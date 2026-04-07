// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>
#include <QVariantList>

#include "configparser.h"

class FstabKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QVariantList fstabEntries READ fstabEntries NOTIFY fstabEntriesChanged)

public:
    explicit FstabKcm(QObject *parent, const KPluginMetaData &metaData);

    QVariantList fstabEntries() const { return m_entries; }

    Q_INVOKABLE void updateEntry(int index, const QVariantMap &entry);
    Q_INVOKABLE void removeEntry(int index);
    Q_INVOKABLE void addEntry(const QVariantMap &entry);

public Q_SLOTS:
    void save() override;
    void load() override;

Q_SIGNALS:
    void fstabEntriesChanged();

private:
    void parseProcMounts();
    QVariantMap fstabEntryToMap(const struct NsaFsm::FstabEntry &entry) const;

    QVariantList m_entries;
    QSet<QString> m_mountedPoints;
};
