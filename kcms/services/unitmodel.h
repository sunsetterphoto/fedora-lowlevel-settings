// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

struct UnitInfo {
    QString name;
    QString description;
    QString loadState;
    QString activeState;
    QString subState;
    QString unitFileState;
    QString type;
};

class UnitModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY typeFilterChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(bool showUserUnits READ showUserUnits WRITE setShowUserUnits NOTIFY showUserUnitsChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        ActiveStateRole,
        SubStateRole,
        UnitFileStateRole,
        TypeRole,
        LoadStateRole,
    };

    explicit UnitModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString typeFilter() const;
    void setTypeFilter(const QString &filter);

    QString searchText() const;
    void setSearchText(const QString &text);

    bool showUserUnits() const;
    void setShowUserUnits(bool show);

    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void typeFilterChanged();
    void searchTextChanged();
    void showUserUnitsChanged();

private:
    void loadUnits();
    void applyFilters();
    static QString typeFromName(const QString &name);

    QVector<UnitInfo> m_allUnits;
    QVector<UnitInfo> m_filteredUnits;
    QHash<QString, QString> m_unitFileStates;
    QString m_typeFilter;
    QString m_searchText;
    bool m_showUserUnits = false;
};
