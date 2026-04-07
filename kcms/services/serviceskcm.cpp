// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "serviceskcm.h"

#include <KPluginFactory>
#include <QTimer>

#include "dbushelper.h"

ServicesKcm::ServicesKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
    , m_model(new UnitModel(this))
{
    setButtons(NoAdditionalButton);
}

void ServicesKcm::startUnit(const QString &name)
{
    systemdAction(QStringLiteral("StartUnit"), {name, QStringLiteral("fail")});
}

void ServicesKcm::stopUnit(const QString &name)
{
    systemdAction(QStringLiteral("StopUnit"), {name, QStringLiteral("fail")});
}

void ServicesKcm::restartUnit(const QString &name)
{
    systemdAction(QStringLiteral("RestartUnit"), {name, QStringLiteral("fail")});
}

void ServicesKcm::enableUnit(const QString &name)
{
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("EnableUnitFiles"),
        {QStringList{name}, false, true});
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("Reload"), {});
    QTimer::singleShot(500, m_model, &UnitModel::refresh);
}

void ServicesKcm::disableUnit(const QString &name)
{
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("DisableUnitFiles"),
        {QStringList{name}, false});
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("Reload"), {});
    QTimer::singleShot(500, m_model, &UnitModel::refresh);
}

void ServicesKcm::systemdAction(const QString &method, const QVariantList &args)
{
    NsaFsm::DBusHelper::systemdCall(method, args);
    QTimer::singleShot(500, m_model, &UnitModel::refresh);
}

K_PLUGIN_CLASS_WITH_JSON(ServicesKcm, "kcm_nsa_services.json")

#include "serviceskcm.moc"
