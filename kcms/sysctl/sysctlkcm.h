// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>

#include "sysctlmodel.h"

class SysctlKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(SysctlModel *sysctlModel READ sysctlModel CONSTANT)

public:
    explicit SysctlKcm(QObject *parent, const KPluginMetaData &metaData);

    SysctlModel *sysctlModel() const { return m_model; }

public Q_SLOTS:
    void save() override;
    void load() override;

private:
    SysctlModel *m_model;
};
