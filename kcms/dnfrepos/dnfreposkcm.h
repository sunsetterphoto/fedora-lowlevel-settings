// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>
#include <QVariantList>

class DnfReposKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QVariantList repos READ repos NOTIFY reposChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

public:
    explicit DnfReposKcm(QObject *parent, const KPluginMetaData &metaData);

    QVariantList repos() const { return m_filteredRepos; }
    QString searchText() const { return m_searchText; }

    void setSearchText(const QString &text);

    Q_INVOKABLE void toggleRepo(const QString &filename, const QString &repoId, bool enabled);
    Q_INVOKABLE void addCoprRepo(const QString &project);
    Q_INVOKABLE void removeCoprRepo(const QString &project);
    Q_INVOKABLE void refreshCache();
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void reposChanged();
    void searchTextChanged();

public Q_SLOTS:
    void load() override;

private:
    void applyFilter();

    QVariantList m_allRepos;
    QVariantList m_filteredRepos;
    QString m_searchText;
};
