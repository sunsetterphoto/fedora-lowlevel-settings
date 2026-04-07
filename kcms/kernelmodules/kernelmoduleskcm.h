// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>
#include <QVariantList>

class KernelModulesKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QVariantList modules READ modules NOTIFY modulesChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString selectedModuleInfo READ selectedModuleInfo NOTIFY selectedModuleInfoChanged)

public:
    explicit KernelModulesKcm(QObject *parent, const KPluginMetaData &metaData);

    QVariantList modules() const { return m_filteredModules; }
    QString searchText() const { return m_searchText; }
    QString selectedModuleInfo() const { return m_selectedModuleInfo; }

    void setSearchText(const QString &text);

    Q_INVOKABLE void getModuleInfo(const QString &name);
    Q_INVOKABLE void blacklistModule(const QString &name);
    Q_INVOKABLE void unblacklistModule(const QString &name);
    Q_INVOKABLE void loadModule(const QString &name);
    Q_INVOKABLE void unloadModule(const QString &name);
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void modulesChanged();
    void searchTextChanged();
    void selectedModuleInfoChanged();

public Q_SLOTS:
    void load() override;

private:
    void loadProcModules();
    void loadBlacklists();
    void applyFilter();

    QVariantList m_allModules;
    QVariantList m_filteredModules;
    QStringList m_blacklistedNames;
    QString m_searchText;
    QString m_selectedModuleInfo;
};
