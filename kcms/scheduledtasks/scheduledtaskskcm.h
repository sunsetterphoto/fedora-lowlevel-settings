// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>
#include <QVariantList>

class ScheduledTasksKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QVariantList timers READ timers NOTIFY timersChanged)
    Q_PROPERTY(QString userCrontab READ userCrontab WRITE setUserCrontab NOTIFY userCrontabChanged)
    Q_PROPERTY(QVariantList systemCronJobs READ systemCronJobs NOTIFY systemCronJobsChanged)

public:
    explicit ScheduledTasksKcm(QObject *parent, const KPluginMetaData &metaData);

    QVariantList timers() const { return m_timers; }
    QString userCrontab() const { return m_userCrontab; }
    QVariantList systemCronJobs() const { return m_systemCronJobs; }

    void setUserCrontab(const QString &text);

    Q_INVOKABLE void enableTimer(const QString &name);
    Q_INVOKABLE void disableTimer(const QString &name);
    Q_INVOKABLE void startTimer(const QString &name);
    Q_INVOKABLE void stopTimer(const QString &name);
    Q_INVOKABLE void saveUserCrontab();
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void timersChanged();
    void userCrontabChanged();
    void systemCronJobsChanged();

public Q_SLOTS:
    void load() override;

private:
    void loadTimers();
    void loadUserCrontab();
    void loadSystemCronJobs();

    QVariantList m_timers;
    QString m_userCrontab;
    QVariantList m_systemCronJobs;
};
