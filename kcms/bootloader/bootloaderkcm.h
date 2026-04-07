// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>

class BootloaderKcm : public KQuickConfigModule
{
    Q_OBJECT

    // GRUB settings
    Q_PROPERTY(QString grubTimeout READ grubTimeout WRITE setGrubTimeout NOTIFY grubTimeoutChanged)
    Q_PROPERTY(QString grubDefault READ grubDefault WRITE setGrubDefault NOTIFY grubDefaultChanged)
    Q_PROPERTY(QString grubCmdlineDefault READ grubCmdlineDefault WRITE setGrubCmdlineDefault NOTIFY grubCmdlineDefaultChanged)
    Q_PROPERTY(QString grubCmdlineLinux READ grubCmdlineLinux WRITE setGrubCmdlineLinux NOTIFY grubCmdlineLinuxChanged)

    // BLS kernel entries
    Q_PROPERTY(QVariantList kernelEntries READ kernelEntries NOTIFY kernelEntriesChanged)

public:
    explicit BootloaderKcm(QObject *parent, const KPluginMetaData &metaData);

    QString grubTimeout() const { return m_grubTimeout; }
    void setGrubTimeout(const QString &value);

    QString grubDefault() const { return m_grubDefault; }
    void setGrubDefault(const QString &value);

    QString grubCmdlineDefault() const { return m_grubCmdlineDefault; }
    void setGrubCmdlineDefault(const QString &value);

    QString grubCmdlineLinux() const { return m_grubCmdlineLinux; }
    void setGrubCmdlineLinux(const QString &value);

    QVariantList kernelEntries() const { return m_kernelEntries; }

    Q_INVOKABLE void setDefaultKernel(const QString &version);

public Q_SLOTS:
    void save() override;
    void load() override;

Q_SIGNALS:
    void grubTimeoutChanged();
    void grubDefaultChanged();
    void grubCmdlineDefaultChanged();
    void grubCmdlineLinuxChanged();
    void kernelEntriesChanged();

private:
    void loadGrubDefaults();
    void loadKernelEntries();
    QVariantMap parseBlsEntry(const QString &content, const QString &filename);

    void saveGrubDefaults();

    // GRUB settings
    QString m_grubTimeout;
    QString m_grubDefault;
    QString m_grubCmdlineDefault;
    QString m_grubCmdlineLinux;

    // Saved originals
    QString m_origGrubTimeout;
    QString m_origGrubDefault;
    QString m_origGrubCmdlineDefault;
    QString m_origGrubCmdlineLinux;

    // Full GRUB key-value map (to preserve keys we don't expose)
    QMap<QString, QString> m_grubValues;

    // BLS kernel entries
    QVariantList m_kernelEntries;
};
