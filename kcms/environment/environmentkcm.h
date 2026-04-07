// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>
#include <QVariantList>

class EnvironmentKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QVariantList systemVars READ systemVars NOTIFY systemVarsChanged)
    Q_PROPERTY(QVariantList userVars READ userVars NOTIFY userVarsChanged)
    Q_PROPERTY(QStringList profileScripts READ profileScripts NOTIFY profileScriptsChanged)

public:
    explicit EnvironmentKcm(QObject *parent, const KPluginMetaData &metaData);

    QVariantList systemVars() const { return m_systemVars; }
    QVariantList userVars() const { return m_userVars; }
    QStringList profileScripts() const { return m_profileScripts; }

    Q_INVOKABLE void setSystemVar(const QString &key, const QString &value);
    Q_INVOKABLE void removeSystemVar(const QString &key);
    Q_INVOKABLE void addSystemVar(const QString &key, const QString &value);

    Q_INVOKABLE void setUserVar(const QString &key, const QString &value);
    Q_INVOKABLE void removeUserVar(const QString &key);
    Q_INVOKABLE void addUserVar(const QString &key, const QString &value);

Q_SIGNALS:
    void systemVarsChanged();
    void userVarsChanged();
    void profileScriptsChanged();

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

private:
    static QVariantList mapToVarList(const QMap<QString, QString> &map);
    static QMap<QString, QString> varListToMap(const QVariantList &list);
    QString userConfPath() const;

    QVariantList m_systemVars;
    QVariantList m_userVars;
    QStringList m_profileScripts;
};
