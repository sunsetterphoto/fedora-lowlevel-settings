// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>

#include "unitmodel.h"

class ServicesKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(UnitModel *unitModel READ unitModel CONSTANT)

public:
    explicit ServicesKcm(QObject *parent, const KPluginMetaData &metaData);

    UnitModel *unitModel() const { return m_model; }

    Q_INVOKABLE void startUnit(const QString &name);
    Q_INVOKABLE void stopUnit(const QString &name);
    Q_INVOKABLE void restartUnit(const QString &name);
    Q_INVOKABLE void enableUnit(const QString &name);
    Q_INVOKABLE void disableUnit(const QString &name);

private:
    void systemdAction(const QString &method, const QVariantList &args);
    UnitModel *m_model;
};
