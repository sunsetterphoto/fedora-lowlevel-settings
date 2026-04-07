// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "journalkcm.h"

#include <KPluginFactory>

JournalKcm::JournalKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
    , m_model(new JournalModel(this))
{
    setButtons(NoAdditionalButton);
}

K_PLUGIN_CLASS_WITH_JSON(JournalKcm, "kcm_nsa_journal.json")

#include "journalkcm.moc"
