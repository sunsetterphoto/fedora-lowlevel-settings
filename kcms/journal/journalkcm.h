// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>

#include "journalmodel.h"

class JournalKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(JournalModel *journalModel READ journalModel CONSTANT)

public:
    explicit JournalKcm(QObject *parent, const KPluginMetaData &metaData);

    JournalModel *journalModel() const { return m_model; }

private:
    JournalModel *m_model;
};
