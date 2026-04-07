// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVector>

struct JournalEntry {
    QString timestamp;
    QString message;
    QString unit;
    int priority = 6;
    QString priorityName;
};

class JournalModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int priorityFilter READ priorityFilter WRITE setPriorityFilter NOTIFY priorityFilterChanged)
    Q_PROPERTY(QString unitFilter READ unitFilter WRITE setUnitFilter NOTIFY unitFilterChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(bool kernelOnly READ kernelOnly WRITE setKernelOnly NOTIFY kernelOnlyChanged)

public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        MessageRole,
        UnitRole,
        PriorityRole,
        PriorityNameRole,
    };

    explicit JournalModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int priorityFilter() const;
    void setPriorityFilter(int priority);

    QString unitFilter() const;
    void setUnitFilter(const QString &unit);

    QString searchText() const;
    void setSearchText(const QString &text);

    bool kernelOnly() const;
    void setKernelOnly(bool kernel);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QStringList availableUnits();

Q_SIGNALS:
    void priorityFilterChanged();
    void unitFilterChanged();
    void searchTextChanged();
    void kernelOnlyChanged();

private:
    void loadEntries();
    static QString priorityToName(int priority);

    QVector<JournalEntry> m_entries;
    int m_priorityFilter = 6;
    QString m_unitFilter;
    QString m_searchText;
    bool m_kernelOnly = false;
};
