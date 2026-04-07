// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "scheduledtaskskcm.h"

#include <KPluginFactory>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QTextStream>
#include <QTimer>

#include "dbushelper.h"

ScheduledTasksKcm::ScheduledTasksKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    setButtons(NoAdditionalButton);
    load();
}

void ScheduledTasksKcm::setUserCrontab(const QString &text)
{
    if (m_userCrontab != text) {
        m_userCrontab = text;
        Q_EMIT userCrontabChanged();
    }
}

void ScheduledTasksKcm::load()
{
    loadTimers();
    loadUserCrontab();
    loadSystemCronJobs();
}

void ScheduledTasksKcm::loadTimers()
{
    m_timers.clear();

    QDBusConnection bus = QDBusConnection::systemBus();

    // Get unit file states for timers
    QHash<QString, QString> unitFileStates;
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.systemd1"),
            QStringLiteral("/org/freedesktop/systemd1"),
            QStringLiteral("org.freedesktop.systemd1.Manager"),
            QStringLiteral("ListUnitFiles"));
        msg.setInteractiveAuthorizationAllowed(true);

        QDBusMessage reply = bus.call(msg, QDBus::Block, 30000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
            const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
            arg.beginArray();
            while (!arg.atEnd()) {
                arg.beginStructure();
                QString unitFile;
                QString state;
                arg >> unitFile >> state;
                arg.endStructure();

                QString baseName = unitFile.mid(unitFile.lastIndexOf(QLatin1Char('/')) + 1);
                if (baseName.endsWith(QLatin1String(".timer"))) {
                    unitFileStates[baseName] = state;
                }
            }
            arg.endArray();
        }
    }

    // List loaded units, filter to .timer
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.systemd1"),
        QStringLiteral("/org/freedesktop/systemd1"),
        QStringLiteral("org.freedesktop.systemd1.Manager"),
        QStringLiteral("ListUnits"));
    msg.setInteractiveAuthorizationAllowed(true);

    QDBusMessage reply = bus.call(msg, QDBus::Block, 30000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        Q_EMIT timersChanged();
        return;
    }

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) {
        arg.beginStructure();

        QString name, description, loadState, activeState, subState;
        QString followed, jobType;
        QDBusObjectPath unitObjPath, jobPath;
        quint32 jobId;

        arg >> name >> description >> loadState >> activeState >> subState
            >> followed >> unitObjPath >> jobId >> jobType >> jobPath;

        arg.endStructure();

        if (!name.endsWith(QLatin1String(".timer"))) {
            continue;
        }

        QVariantMap timer;
        timer[QStringLiteral("name")] = name;
        timer[QStringLiteral("description")] = description;
        timer[QStringLiteral("activeState")] = activeState;
        timer[QStringLiteral("unitFileState")] = unitFileStates.value(name, QString());

        // Try to get NextElapseUSecRealtime and LastTriggerUSec from the timer unit
        QString nextRun;
        QString lastRun;

        QDBusMessage propMsg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.systemd1"),
            unitObjPath.path(),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"));

        // NextElapseUSecRealtime
        propMsg.setArguments({
            QStringLiteral("org.freedesktop.systemd1.Timer"),
            QStringLiteral("NextElapseUSecRealtime")
        });
        QDBusMessage propReply = bus.call(propMsg, QDBus::Block, 5000);
        if (propReply.type() == QDBusMessage::ReplyMessage && !propReply.arguments().isEmpty()) {
            quint64 usec = propReply.arguments().at(0).value<QDBusVariant>().variant().toULongLong();
            if (usec > 0) {
                QDateTime dt = QDateTime::fromMSecsSinceEpoch(usec / 1000);
                nextRun = QLocale().toString(dt, QLocale::ShortFormat);
            }
        }

        // LastTriggerUSec
        propMsg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.systemd1"),
            unitObjPath.path(),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get"));
        propMsg.setArguments({
            QStringLiteral("org.freedesktop.systemd1.Timer"),
            QStringLiteral("LastTriggerUSec")
        });
        propReply = bus.call(propMsg, QDBus::Block, 5000);
        if (propReply.type() == QDBusMessage::ReplyMessage && !propReply.arguments().isEmpty()) {
            quint64 usec = propReply.arguments().at(0).value<QDBusVariant>().variant().toULongLong();
            if (usec > 0) {
                QDateTime dt = QDateTime::fromMSecsSinceEpoch(usec / 1000);
                lastRun = QLocale().toString(dt, QLocale::ShortFormat);
            }
        }

        timer[QStringLiteral("nextRun")] = nextRun;
        timer[QStringLiteral("lastRun")] = lastRun;

        m_timers.append(timer);
    }
    arg.endArray();

    // Sort by name
    std::sort(m_timers.begin(), m_timers.end(),
        [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("name")).toString()
                < b.toMap().value(QStringLiteral("name")).toString();
        });

    Q_EMIT timersChanged();
}

void ScheduledTasksKcm::loadUserCrontab()
{
    QProcess proc;
    proc.start(QStringLiteral("crontab"), {QStringLiteral("-l")});
    proc.waitForFinished(5000);

    if (proc.exitCode() == 0) {
        m_userCrontab = QString::fromUtf8(proc.readAllStandardOutput());
    } else {
        // No crontab for user, or error
        m_userCrontab = QString();
    }
    Q_EMIT userCrontabChanged();
}

void ScheduledTasksKcm::loadSystemCronJobs()
{
    m_systemCronJobs.clear();

    QDir cronDir(QStringLiteral("/etc/cron.d"));
    if (!cronDir.exists()) {
        Q_EMIT systemCronJobsChanged();
        return;
    }

    const QStringList files = cronDir.entryList(QDir::Files | QDir::Readable, QDir::Name);
    for (const QString &filename : files) {
        QFile file(cronDir.absoluteFilePath(filename));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QVariantMap job;
        job[QStringLiteral("filename")] = filename;
        job[QStringLiteral("content")] = QString::fromUtf8(file.readAll());
        m_systemCronJobs.append(job);
    }

    Q_EMIT systemCronJobsChanged();
}

void ScheduledTasksKcm::enableTimer(const QString &name)
{
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("EnableUnitFiles"),
        {QStringList{name}, false, true});
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("Reload"), {});
    QTimer::singleShot(500, this, [this] { loadTimers(); });
}

void ScheduledTasksKcm::disableTimer(const QString &name)
{
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("DisableUnitFiles"),
        {QStringList{name}, false});
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("Reload"), {});
    QTimer::singleShot(500, this, [this] { loadTimers(); });
}

void ScheduledTasksKcm::startTimer(const QString &name)
{
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("StartUnit"),
        {name, QStringLiteral("fail")});
    QTimer::singleShot(500, this, [this] { loadTimers(); });
}

void ScheduledTasksKcm::stopTimer(const QString &name)
{
    NsaFsm::DBusHelper::systemdCall(QStringLiteral("StopUnit"),
        {name, QStringLiteral("fail")});
    QTimer::singleShot(500, this, [this] { loadTimers(); });
}

void ScheduledTasksKcm::saveUserCrontab()
{
    QProcess proc;
    proc.start(QStringLiteral("crontab"), {QStringLiteral("-")});
    proc.waitForStarted(3000);
    proc.write(m_userCrontab.toUtf8());
    proc.closeWriteChannel();
    proc.waitForFinished(5000);

    if (proc.exitCode() != 0) {
        qWarning() << "Failed to save user crontab:"
                    << QString::fromUtf8(proc.readAllStandardError());
    }
}

void ScheduledTasksKcm::refresh()
{
    load();
}

K_PLUGIN_CLASS_WITH_JSON(ScheduledTasksKcm, "kcm_nsa_scheduledtasks.json")

#include "scheduledtaskskcm.moc"
