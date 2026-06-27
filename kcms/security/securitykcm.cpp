// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "securitykcm.h"

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

SecurityKcm::SecurityKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    setButtons(NoAdditionalButton);
    load();
}

void SecurityKcm::load()
{
    loadSudoersFiles();
    loadSelinuxMode();
    loadSelinuxBooleans();
    loadPolkitActions();
}

void SecurityKcm::refresh()
{
    load();
}

// ---- Sudoers ----

void SecurityKcm::loadSudoersFiles()
{
    m_sudoersFiles.clear();

    KAuth::Action action(QStringLiteral("org.kde.fcse.read"));
    action.setHelperId(QStringLiteral("org.kde.fcse"));

    QVariantMap args;
    args[QStringLiteral("dirPath")] = QStringLiteral("/etc/sudoers.d/");
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "Failed to read sudoers.d:" << job->errorString();
        } else {
            const QVariantMap files = job->data().value(QStringLiteral("files")).toMap();
            for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
                QVariantMap entry;
                entry[QStringLiteral("filename")] = it.key();
                entry[QStringLiteral("content")] = QString::fromUtf8(it.value().toByteArray());
                m_sudoersFiles.append(entry);
            }

            // Sort by filename
            std::sort(m_sudoersFiles.begin(), m_sudoersFiles.end(),
                [](const QVariant &a, const QVariant &b) {
                    return a.toMap().value(QStringLiteral("filename")).toString()
                        < b.toMap().value(QStringLiteral("filename")).toString();
                });
        }
        Q_EMIT sudoersFilesChanged();
    });
    job->start();
}

void SecurityKcm::saveSudoersFile(const QString &filename, const QString &content)
{
    // Sanitize filename: only allow alphanumeric, dash, underscore, dot
    static const QRegularExpression safeNameRe(QStringLiteral("^[a-zA-Z0-9._-]+$"));
    if (!safeNameRe.match(filename).hasMatch()) {
        qWarning() << "Invalid sudoers filename:" << filename;
        return;
    }

    QVariantMap args;
    args[QStringLiteral("filePath")] = QString(QStringLiteral("/etc/sudoers.d/") + filename);
    args[QStringLiteral("content")] = content.toUtf8();
    args[QStringLiteral("validateAsSudoers")] = true;

    KAuth::Action action(QStringLiteral("org.kde.fcse.write"));
    action.setHelperId(QStringLiteral("org.kde.fcse"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "Failed to save sudoers file:" << job->errorString();
        } else {
            loadSudoersFiles();
        }
    });
    job->start();
}

void SecurityKcm::deleteSudoersFile(const QString &filename)
{
    // To delete, we write a single comment line (empty content is rejected by helper)
    static const QRegularExpression safeNameRe(QStringLiteral("^[a-zA-Z0-9._-]+$"));
    if (!safeNameRe.match(filename).hasMatch()) {
        qWarning() << "Invalid sudoers filename:" << filename;
        return;
    }

    QVariantMap args;
    args[QStringLiteral("filePath")] = QString(QStringLiteral("/etc/sudoers.d/") + filename);
    args[QStringLiteral("content")] = QByteArray("# Removed by Fedora Core Setting Extension\n");
    args[QStringLiteral("validateAsSudoers")] = true;

    KAuth::Action action(QStringLiteral("org.kde.fcse.write"));
    action.setHelperId(QStringLiteral("org.kde.fcse"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "Failed to delete sudoers file:" << job->errorString();
        } else {
            loadSudoersFiles();
        }
    });
    job->start();
}

// ---- SELinux ----

void SecurityKcm::loadSelinuxMode()
{
    QProcess proc;
    proc.start(QStringLiteral("getenforce"), {});
    proc.waitForFinished(5000);

    if (proc.exitCode() == 0) {
        m_selinuxMode = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    } else {
        m_selinuxMode = QStringLiteral("Unknown");
    }
    Q_EMIT selinuxModeChanged();
}

void SecurityKcm::loadSelinuxBooleans()
{
    m_selinuxBooleans.clear();

    QProcess proc;
    proc.start(QStringLiteral("getsebool"), {QStringLiteral("-a")});
    proc.waitForFinished(30000);

    if (proc.exitCode() != 0) {
        Q_EMIT selinuxBooleansChanged();
        return;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    static const QRegularExpression boolRe(QStringLiteral("^(\\S+)\\s+-->\\s+(on|off)$"));

    for (const QString &line : lines) {
        const QRegularExpressionMatch match = boolRe.match(line.trimmed());
        if (match.hasMatch()) {
            QVariantMap entry;
            entry[QStringLiteral("name")] = match.captured(1);
            entry[QStringLiteral("value")] = (match.captured(2) == QLatin1String("on"));
            entry[QStringLiteral("description")] = match.captured(1);
            m_selinuxBooleans.append(entry);
        }
    }

    Q_EMIT selinuxBooleansChanged();
}

void SecurityKcm::setSelinuxMode(const QString &mode)
{
    QString arg;
    if (mode == QLatin1String("Enforcing")) {
        arg = QStringLiteral("1");
    } else if (mode == QLatin1String("Permissive")) {
        arg = QStringLiteral("0");
    } else {
        qWarning() << "Invalid SELinux mode:" << mode;
        return;
    }

    QVariantMap args;
    args[QStringLiteral("command")] = QStringLiteral("/usr/sbin/setenforce");
    args[QStringLiteral("args")] = QStringList{arg};

    KAuth::Action action(QStringLiteral("org.kde.fcse.execute"));
    action.setHelperId(QStringLiteral("org.kde.fcse"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "Failed to set SELinux mode:" << job->errorString();
        }
        loadSelinuxMode();
    });
    job->start();
}

void SecurityKcm::setSelinuxBool(const QString &name, bool value)
{
    QVariantMap args;
    args[QStringLiteral("command")] = QStringLiteral("/usr/sbin/setsebool");
    args[QStringLiteral("args")] = QStringList{
        QStringLiteral("-P"),
        name,
        value ? QStringLiteral("on") : QStringLiteral("off")
    };

    KAuth::Action action(QStringLiteral("org.kde.fcse.execute"));
    action.setHelperId(QStringLiteral("org.kde.fcse"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "Failed to set SELinux boolean:" << job->errorString();
        }
        // Note: setsebool -P can take a while, so we reload after
        loadSelinuxBooleans();
    });
    job->start();
}

// ---- Polkit ----

void SecurityKcm::loadPolkitActions()
{
    m_polkitActions.clear();

    QDir policyDir(QStringLiteral("/usr/share/polkit-1/actions"));
    if (!policyDir.exists()) {
        Q_EMIT polkitActionsChanged();
        return;
    }

    const QStringList policyFiles = policyDir.entryList(
        {QStringLiteral("*.policy")}, QDir::Files | QDir::Readable, QDir::Name);

    // Regex patterns for parsing action id and description elements
    static const QRegularExpression actionIdRe(
        QStringLiteral("<action\\s+id=\"([^\"]+)\""));
    // Match the first <description> after an <action> (non-lang-specific)
    static const QRegularExpression descRe(
        QStringLiteral("<description>([^<]*)</description>"));

    for (const QString &filename : policyFiles) {
        QFile file(policyDir.absoluteFilePath(filename));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        const QString content = QString::fromUtf8(file.readAll());
        file.close();

        // Split by <action to process each action block
        const QStringList actionBlocks = content.split(QStringLiteral("<action "), Qt::SkipEmptyParts);

        for (int i = 1; i < actionBlocks.size(); ++i) {
            const QString block = QStringLiteral("<action ") + actionBlocks.at(i);

            const QRegularExpressionMatch idMatch = actionIdRe.match(block);
            if (!idMatch.hasMatch()) {
                continue;
            }

            QString description;
            const QRegularExpressionMatch descMatch = descRe.match(block);
            if (descMatch.hasMatch()) {
                description = descMatch.captured(1).trimmed();
            }

            // Try to find allow_active
            QString allowActive;
            static const QRegularExpression allowActiveRe(
                QStringLiteral("<allow_active>([^<]*)</allow_active>"));
            const QRegularExpressionMatch allowMatch = allowActiveRe.match(block);
            if (allowMatch.hasMatch()) {
                allowActive = allowMatch.captured(1).trimmed();
            }

            QVariantMap entry;
            entry[QStringLiteral("id")] = idMatch.captured(1);
            entry[QStringLiteral("description")] = description;
            entry[QStringLiteral("allowActive")] = allowActive;
            m_polkitActions.append(entry);
        }
    }

    Q_EMIT polkitActionsChanged();
}

K_PLUGIN_CLASS_WITH_JSON(SecurityKcm, "kcm_fcse_security.json")

#include "securitykcm.moc"
