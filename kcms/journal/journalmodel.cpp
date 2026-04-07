// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "journalmodel.h"

#include <QDateTime>
#include <QSet>
#include <QTimeZone>

#include <systemd/sd-journal.h>

JournalModel::JournalModel(QObject *parent)
    : QAbstractListModel(parent)
{
    loadEntries();
}

int JournalModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant JournalModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const auto &entry = m_entries.at(index.row());

    switch (role) {
    case TimestampRole:
        return entry.timestamp;
    case MessageRole:
        return entry.message;
    case UnitRole:
        return entry.unit;
    case PriorityRole:
        return entry.priority;
    case PriorityNameRole:
        return entry.priorityName;
    default:
        return {};
    }
}

QHash<int, QByteArray> JournalModel::roleNames() const
{
    return {
        {TimestampRole, "timestamp"},
        {MessageRole, "message"},
        {UnitRole, "unit"},
        {PriorityRole, "priority"},
        {PriorityNameRole, "priorityName"},
    };
}

int JournalModel::priorityFilter() const
{
    return m_priorityFilter;
}

void JournalModel::setPriorityFilter(int priority)
{
    if (m_priorityFilter == priority) {
        return;
    }
    m_priorityFilter = priority;
    Q_EMIT priorityFilterChanged();
    loadEntries();
}

QString JournalModel::unitFilter() const
{
    return m_unitFilter;
}

void JournalModel::setUnitFilter(const QString &unit)
{
    if (m_unitFilter == unit) {
        return;
    }
    m_unitFilter = unit;
    Q_EMIT unitFilterChanged();
    loadEntries();
}

QString JournalModel::searchText() const
{
    return m_searchText;
}

void JournalModel::setSearchText(const QString &text)
{
    if (m_searchText == text) {
        return;
    }
    m_searchText = text;
    Q_EMIT searchTextChanged();
    loadEntries();
}

bool JournalModel::kernelOnly() const
{
    return m_kernelOnly;
}

void JournalModel::setKernelOnly(bool kernel)
{
    if (m_kernelOnly == kernel) {
        return;
    }
    m_kernelOnly = kernel;
    Q_EMIT kernelOnlyChanged();
    loadEntries();
}

void JournalModel::refresh()
{
    loadEntries();
}

QStringList JournalModel::availableUnits()
{
    QSet<QString> units;

    sd_journal *journal = nullptr;
    int r = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
    if (r < 0) {
        return {};
    }

    // Enumerate unique _SYSTEMD_UNIT values
    const void *data = nullptr;
    size_t length = 0;
    r = sd_journal_query_unique(journal, "_SYSTEMD_UNIT");
    if (r >= 0) {
        SD_JOURNAL_FOREACH_UNIQUE(journal, data, length) {
            // Data format: _SYSTEMD_UNIT=value
            const char *str = static_cast<const char *>(data);
            // Skip past "_SYSTEMD_UNIT="
            if (length > 14) {
                units.insert(QString::fromUtf8(str + 14, static_cast<int>(length) - 14));
            }
        }
    }

    sd_journal_close(journal);

    QStringList result(units.begin(), units.end());
    result.sort();
    return result;
}

void JournalModel::loadEntries()
{
    beginResetModel();
    m_entries.clear();

    sd_journal *journal = nullptr;
    int r = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
    if (r < 0) {
        endResetModel();
        return;
    }

    // Apply journal-level filters
    if (m_kernelOnly) {
        sd_journal_add_match(journal, "_TRANSPORT=kernel", 0);
    }

    if (!m_unitFilter.isEmpty()) {
        const QByteArray match = QStringLiteral("_SYSTEMD_UNIT=%1").arg(m_unitFilter).toUtf8();
        sd_journal_add_match(journal, match.constData(), 0);
    }

    // Priority filter: add a match for each priority level 0..priorityFilter
    // sd-journal OR's matches on the same field, so this shows entries at
    // the specified priority and all more severe levels
    if (m_priorityFilter < 7) {
        for (int p = 0; p <= m_priorityFilter; ++p) {
            const QByteArray match = QStringLiteral("PRIORITY=%1").arg(p).toUtf8();
            sd_journal_add_match(journal, match.constData(), 0);
        }
    }

    // Seek to end and read backwards
    sd_journal_seek_tail(journal);

    constexpr int maxEntries = 2000;
    QVector<JournalEntry> rawEntries;
    rawEntries.reserve(maxEntries);

    for (int i = 0; i < maxEntries; ++i) {
        r = sd_journal_previous(journal);
        if (r <= 0) {
            break; // No more entries or error
        }

        JournalEntry entry;

        // Get timestamp
        uint64_t usec = 0;
        if (sd_journal_get_realtime_usec(journal, &usec) >= 0) {
            const QDateTime dt = QDateTime::fromMSecsSinceEpoch(
                static_cast<qint64>(usec / 1000), QTimeZone::systemTimeZone());
            entry.timestamp = dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        }

        // Get MESSAGE
        const void *data = nullptr;
        size_t length = 0;
        if (sd_journal_get_data(journal, "MESSAGE", &data, &length) >= 0) {
            const char *str = static_cast<const char *>(data);
            // Format is "MESSAGE=value", skip 8 chars
            if (length > 8) {
                entry.message = QString::fromUtf8(str + 8, static_cast<int>(length) - 8);
            }
        }

        // Get _SYSTEMD_UNIT
        if (sd_journal_get_data(journal, "_SYSTEMD_UNIT", &data, &length) >= 0) {
            const char *str = static_cast<const char *>(data);
            // Format is "_SYSTEMD_UNIT=value", skip 14 chars
            if (length > 14) {
                entry.unit = QString::fromUtf8(str + 14, static_cast<int>(length) - 14);
            }
        }

        // Get PRIORITY
        if (sd_journal_get_data(journal, "PRIORITY", &data, &length) >= 0) {
            const char *str = static_cast<const char *>(data);
            // Format is "PRIORITY=value", skip 9 chars
            if (length > 9) {
                entry.priority = QString::fromUtf8(str + 9, static_cast<int>(length) - 9).toInt();
            }
        }
        entry.priorityName = priorityToName(entry.priority);

        // Client-side search text filter
        if (!m_searchText.isEmpty()) {
            if (!entry.message.contains(m_searchText, Qt::CaseInsensitive)) {
                continue;
            }
        }

        rawEntries.append(entry);
    }

    sd_journal_close(journal);

    m_entries = rawEntries;
    endResetModel();
}

QString JournalModel::priorityToName(int priority)
{
    switch (priority) {
    case 0: return QStringLiteral("EMERG");
    case 1: return QStringLiteral("ALERT");
    case 2: return QStringLiteral("CRIT");
    case 3: return QStringLiteral("ERR");
    case 4: return QStringLiteral("WARNING");
    case 5: return QStringLiteral("NOTICE");
    case 6: return QStringLiteral("INFO");
    case 7: return QStringLiteral("DEBUG");
    default: return QStringLiteral("UNKNOWN");
    }
}
