// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QMap>
#include <QString>
#include <QVariantList>
#include <QVector>

struct SysctlParam {
    QString key;
    QString currentValue;
    QString customValue;
    QString category;
    QString description;
    bool isModified = false;
};

class SysctlModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY categoryFilterChanged)
    Q_PROPERTY(bool onlyModified READ onlyModified WRITE setOnlyModified NOTIFY onlyModifiedChanged)

public:
    enum Roles {
        KeyRole = Qt::UserRole + 1,
        CurrentValueRole,
        CustomValueRole,
        CategoryRole,
        DescriptionRole,
        IsModifiedRole,
    };

    explicit SysctlModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString searchText() const;
    void setSearchText(const QString &text);

    QString categoryFilter() const;
    void setCategoryFilter(const QString &filter);

    bool onlyModified() const;
    void setOnlyModified(bool only);

    Q_INVOKABLE void setValue(const QString &key, const QString &value);
    Q_INVOKABLE void resetValue(const QString &key);
    Q_INVOKABLE QVariantList modifiedEntries() const;

    void reload();

Q_SIGNALS:
    void searchTextChanged();
    void categoryFilterChanged();
    void onlyModifiedChanged();

private:
    void loadFromProc();
    void loadCustomValues();
    void applyFilters();

    QVector<SysctlParam> m_allParams;
    QVector<SysctlParam> m_filteredParams;
    QMap<QString, QString> m_customValues; // key -> custom value

    QString m_searchText;
    QString m_categoryFilter;
    bool m_onlyModified = false;

    static constexpr int MaxParameters = 5000;
};
