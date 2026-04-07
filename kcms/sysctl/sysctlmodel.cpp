// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "sysctlmodel.h"

#include "configparser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>

SysctlModel::SysctlModel(QObject *parent)
    : QAbstractListModel(parent)
{
    reload();
}

int SysctlModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_filteredParams.size();
}

QVariant SysctlModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredParams.size())
        return {};

    const auto &param = m_filteredParams.at(index.row());

    switch (role) {
    case KeyRole:
        return param.key;
    case CurrentValueRole:
        return param.currentValue;
    case CustomValueRole:
        return param.customValue;
    case CategoryRole:
        return param.category;
    case DescriptionRole:
        return param.description;
    case IsModifiedRole:
        return param.isModified;
    }
    return {};
}

QHash<int, QByteArray> SysctlModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {CurrentValueRole, "currentValue"},
        {CustomValueRole, "customValue"},
        {CategoryRole, "category"},
        {DescriptionRole, "description"},
        {IsModifiedRole, "isModified"},
    };
}

QString SysctlModel::searchText() const
{
    return m_searchText;
}

void SysctlModel::setSearchText(const QString &text)
{
    if (m_searchText == text)
        return;
    m_searchText = text;
    Q_EMIT searchTextChanged();
    applyFilters();
}

QString SysctlModel::categoryFilter() const
{
    return m_categoryFilter;
}

void SysctlModel::setCategoryFilter(const QString &filter)
{
    if (m_categoryFilter == filter)
        return;
    m_categoryFilter = filter;
    Q_EMIT categoryFilterChanged();
    applyFilters();
}

bool SysctlModel::onlyModified() const
{
    return m_onlyModified;
}

void SysctlModel::setOnlyModified(bool only)
{
    if (m_onlyModified == only)
        return;
    m_onlyModified = only;
    Q_EMIT onlyModifiedChanged();
    applyFilters();
}

void SysctlModel::setValue(const QString &key, const QString &value)
{
    m_customValues[key] = value;

    // Update the matching entry in m_allParams
    for (auto &param : m_allParams) {
        if (param.key == key) {
            param.customValue = value;
            param.isModified = true;
            break;
        }
    }

    applyFilters();
}

void SysctlModel::resetValue(const QString &key)
{
    m_customValues.remove(key);

    for (auto &param : m_allParams) {
        if (param.key == key) {
            param.customValue.clear();
            param.isModified = false;
            break;
        }
    }

    applyFilters();
}

QVariantList SysctlModel::modifiedEntries() const
{
    QVariantList result;
    for (auto it = m_customValues.constBegin(); it != m_customValues.constEnd(); ++it) {
        QVariantMap entry;
        entry[QStringLiteral("key")] = it.key();
        entry[QStringLiteral("value")] = it.value();
        result.append(entry);
    }
    return result;
}

void SysctlModel::reload()
{
    loadFromProc();
    loadCustomValues();
    applyFilters();
}

void SysctlModel::loadFromProc()
{
    m_allParams.clear();

    const QString procSysPath = QStringLiteral("/proc/sys");
    QDirIterator it(procSysPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    int count = 0;
    while (it.hasNext() && count < MaxParameters) {
        it.next();

        const QString filePath = it.filePath();

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue; // Skip files we can't read (permission denied)
        }

        QTextStream in(&file);
        const QString value = in.readAll().trimmed();
        file.close();

        // Convert path to sysctl key: strip /proc/sys/ prefix and replace / with .
        QString key = filePath.mid(procSysPath.length() + 1); // +1 for trailing /
        key.replace(QLatin1Char('/'), QLatin1Char('.'));

        // Category is the first component
        const int dotPos = key.indexOf(QLatin1Char('.'));
        const QString category = dotPos > 0 ? key.left(dotPos) : key;

        SysctlParam param;
        param.key = key;
        param.currentValue = value;
        param.category = category;
        param.isModified = false;

        m_allParams.append(param);
        ++count;
    }

    // Sort by key for consistent ordering
    std::sort(m_allParams.begin(), m_allParams.end(), [](const SysctlParam &a, const SysctlParam &b) {
        return a.key < b.key;
    });
}

void SysctlModel::loadCustomValues()
{
    m_customValues.clear();

    const QString confPath = QStringLiteral("/etc/sysctl.d/99-nsa-fsm.conf");
    const auto entries = NsaFsm::ConfigParser::parseSysctl(confPath);

    for (const auto &entry : entries) {
        m_customValues[entry.key] = entry.value;
    }

    // Apply custom values to parameters
    for (auto &param : m_allParams) {
        if (m_customValues.contains(param.key)) {
            param.customValue = m_customValues[param.key];
            param.isModified = true;
        } else {
            param.customValue.clear();
            param.isModified = false;
        }
    }
}

void SysctlModel::applyFilters()
{
    beginResetModel();

    m_filteredParams.clear();
    for (const auto &param : std::as_const(m_allParams)) {
        // Category filter
        if (!m_categoryFilter.isEmpty() && param.category != m_categoryFilter)
            continue;

        // Search filter
        if (!m_searchText.isEmpty()
            && !param.key.contains(m_searchText, Qt::CaseInsensitive))
            continue;

        // Only modified filter
        if (m_onlyModified && !param.isModified)
            continue;

        m_filteredParams.append(param);
    }

    endResetModel();
}
