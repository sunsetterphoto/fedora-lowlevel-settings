// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>
#include <QVariantList>

class SecurityKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sudoersFiles READ sudoersFiles NOTIFY sudoersFilesChanged)
    Q_PROPERTY(QString selinuxMode READ selinuxMode NOTIFY selinuxModeChanged)
    Q_PROPERTY(QVariantList selinuxBooleans READ selinuxBooleans NOTIFY selinuxBooleansChanged)
    Q_PROPERTY(QVariantList polkitActions READ polkitActions NOTIFY polkitActionsChanged)

public:
    explicit SecurityKcm(QObject *parent, const KPluginMetaData &metaData);

    QVariantList sudoersFiles() const { return m_sudoersFiles; }
    QString selinuxMode() const { return m_selinuxMode; }
    QVariantList selinuxBooleans() const { return m_selinuxBooleans; }
    QVariantList polkitActions() const { return m_polkitActions; }

    Q_INVOKABLE void setSelinuxMode(const QString &mode);
    Q_INVOKABLE void setSelinuxBool(const QString &name, bool value);
    Q_INVOKABLE void saveSudoersFile(const QString &filename, const QString &content);
    Q_INVOKABLE void deleteSudoersFile(const QString &filename);
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void sudoersFilesChanged();
    void selinuxModeChanged();
    void selinuxBooleansChanged();
    void polkitActionsChanged();

public Q_SLOTS:
    void load() override;

private:
    void loadSudoersFiles();
    void loadSelinuxMode();
    void loadSelinuxBooleans();
    void loadPolkitActions();

    QVariantList m_sudoersFiles;
    QString m_selinuxMode;
    QVariantList m_selinuxBooleans;
    QVariantList m_polkitActions;
};
