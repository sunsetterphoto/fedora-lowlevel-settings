// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "unitmodel.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

UnitModel::UnitModel(QObject *parent)
    : QAbstractListModel(parent)
{
    loadUnits();
}

int UnitModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_filteredUnits.size();
}

QVariant UnitModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredUnits.size())
        return {};

    const auto &unit = m_filteredUnits.at(index.row());

    switch (role) {
    case NameRole:
        return unit.name;
    case DescriptionRole:
        return unit.description;
    case ActiveStateRole:
        return unit.activeState;
    case SubStateRole:
        return unit.subState;
    case UnitFileStateRole:
        return unit.unitFileState;
    case TypeRole:
        return unit.type;
    case LoadStateRole:
        return unit.loadState;
    }
    return {};
}

QHash<int, QByteArray> UnitModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {ActiveStateRole, "activeState"},
        {SubStateRole, "subState"},
        {UnitFileStateRole, "unitFileState"},
        {TypeRole, "type"},
        {LoadStateRole, "loadState"},
    };
}

QString UnitModel::typeFilter() const
{
    return m_typeFilter;
}

void UnitModel::setTypeFilter(const QString &filter)
{
    if (m_typeFilter == filter)
        return;
    m_typeFilter = filter;
    Q_EMIT typeFilterChanged();
    applyFilters();
}

QString UnitModel::searchText() const
{
    return m_searchText;
}

void UnitModel::setSearchText(const QString &text)
{
    if (m_searchText == text)
        return;
    m_searchText = text;
    Q_EMIT searchTextChanged();
    applyFilters();
}

bool UnitModel::showUserUnits() const
{
    return m_showUserUnits;
}

void UnitModel::setShowUserUnits(bool show)
{
    if (m_showUserUnits == show)
        return;
    m_showUserUnits = show;
    Q_EMIT showUserUnitsChanged();
    loadUnits();
}

void UnitModel::refresh()
{
    loadUnits();
}

QString UnitModel::typeFromName(const QString &name)
{
    int dotIndex = name.lastIndexOf(QLatin1Char('.'));
    if (dotIndex >= 0)
        return name.mid(dotIndex + 1);
    return QString();
}

void UnitModel::loadUnits()
{
    QDBusConnection bus = m_showUserUnits
        ? QDBusConnection::sessionBus()
        : QDBusConnection::systemBus();

    // Load unit file states first
    m_unitFileStates.clear();
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.systemd1"),
            QStringLiteral("/org/freedesktop/systemd1"),
            QStringLiteral("org.freedesktop.systemd1.Manager"),
            QStringLiteral("ListUnitFiles"));
        msg.setInteractiveAuthorizationAllowed(true);

        QDBusMessage reply = bus.call(msg, QDBus::Block, 30000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
            arg.beginArray();
            while (!arg.atEnd()) {
                arg.beginStructure();
                QString unitFile;
                QString state;
                arg >> unitFile >> state;
                arg.endStructure();

                // Extract just the filename from the full path
                QString baseName = unitFile.mid(unitFile.lastIndexOf(QLatin1Char('/')) + 1);
                m_unitFileStates[baseName] = state;
            }
            arg.endArray();
        }
    }

    // Load units
    QVector<UnitInfo> units;
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.systemd1"),
            QStringLiteral("/org/freedesktop/systemd1"),
            QStringLiteral("org.freedesktop.systemd1.Manager"),
            QStringLiteral("ListUnits"));
        msg.setInteractiveAuthorizationAllowed(true);

        QDBusMessage reply = bus.call(msg, QDBus::Block, 30000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
            arg.beginArray();
            while (!arg.atEnd()) {
                arg.beginStructure();

                // ListUnits returns: (name, description, loadState, activeState, subState,
                //                     followed, unitPath, jobId, jobType, jobPath)
                QString name, description, loadState, activeState, subState;
                QString followed, unitPath, jobType;
                QDBusObjectPath jobPath, unitObjPath;
                quint32 jobId;

                arg >> name >> description >> loadState >> activeState >> subState
                    >> followed >> unitObjPath >> jobId >> jobType >> jobPath;

                arg.endStructure();

                UnitInfo info;
                info.name = name;
                info.description = description;
                info.loadState = loadState;
                info.activeState = activeState;
                info.subState = subState;
                info.type = typeFromName(name);
                info.unitFileState = m_unitFileStates.value(name, QStringLiteral(""));

                units.append(info);
            }
            arg.endArray();
        }
    }

    beginResetModel();
    m_allUnits = units;
    endResetModel();

    applyFilters();
}

void UnitModel::applyFilters()
{
    beginResetModel();

    m_filteredUnits.clear();
    for (const auto &unit : std::as_const(m_allUnits)) {
        // Type filter
        if (!m_typeFilter.isEmpty() && unit.type != m_typeFilter)
            continue;
        // Search filter
        if (!m_searchText.isEmpty()
            && !unit.name.contains(m_searchText, Qt::CaseInsensitive)
            && !unit.description.contains(m_searchText, Qt::CaseInsensitive))
            continue;

        m_filteredUnits.append(unit);
    }

    endResetModel();
}
