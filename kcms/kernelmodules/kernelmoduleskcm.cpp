// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "kernelmoduleskcm.h"

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTextStream>

KernelModulesKcm::KernelModulesKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    setButtons(NoAdditionalButton);
    load();
}

void KernelModulesKcm::setSearchText(const QString &text)
{
    if (m_searchText != text) {
        m_searchText = text;
        Q_EMIT searchTextChanged();
        applyFilter();
    }
}

void KernelModulesKcm::load()
{
    loadBlacklists();
    loadProcModules();
}

void KernelModulesKcm::loadProcModules()
{
    m_allModules.clear();

    QFile procModules(QStringLiteral("/proc/modules"));
    if (!procModules.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open /proc/modules";
        applyFilter();
        return;
    }

    QTextStream in(&procModules);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        // Format: name size usedCount usedBy state address
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 4) {
            continue;
        }

        QVariantMap mod;
        mod[QStringLiteral("name")] = parts[0];
        mod[QStringLiteral("size")] = parts[1].toLongLong();
        mod[QStringLiteral("usedCount")] = parts[2].toInt();

        // usedBy is a comma-separated list with trailing comma, or "-"
        QString usedByRaw = parts[3];
        if (usedByRaw == QLatin1String("-")) {
            mod[QStringLiteral("usedBy")] = QString();
        } else {
            // Remove trailing comma if present
            if (usedByRaw.endsWith(QLatin1Char(','))) {
                usedByRaw.chop(1);
            }
            mod[QStringLiteral("usedBy")] = usedByRaw;
        }

        mod[QStringLiteral("isBlacklisted")] = m_blacklistedNames.contains(parts[0]);

        m_allModules.append(mod);
    }

    // Sort by name
    std::sort(m_allModules.begin(), m_allModules.end(),
        [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("name")).toString()
                < b.toMap().value(QStringLiteral("name")).toString();
        });

    applyFilter();
}

void KernelModulesKcm::loadBlacklists()
{
    m_blacklistedNames.clear();

    QDir modprobeDir(QStringLiteral("/etc/modprobe.d"));
    if (!modprobeDir.exists()) {
        return;
    }

    const QStringList confFiles = modprobeDir.entryList(
        {QStringLiteral("*.conf")}, QDir::Files | QDir::Readable);

    for (const QString &filename : confFiles) {
        QFile file(modprobeDir.absoluteFilePath(filename));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.startsWith(QLatin1String("blacklist "))) {
                const QString moduleName = line.mid(10).trimmed();
                if (!moduleName.isEmpty() && !m_blacklistedNames.contains(moduleName)) {
                    m_blacklistedNames.append(moduleName);
                }
            }
        }
    }
}

void KernelModulesKcm::applyFilter()
{
    if (m_searchText.isEmpty()) {
        m_filteredModules = m_allModules;
    } else {
        m_filteredModules.clear();
        for (const QVariant &mod : std::as_const(m_allModules)) {
            const QString name = mod.toMap().value(QStringLiteral("name")).toString();
            if (name.contains(m_searchText, Qt::CaseInsensitive)) {
                m_filteredModules.append(mod);
            }
        }
    }
    Q_EMIT modulesChanged();
}

void KernelModulesKcm::getModuleInfo(const QString &name)
{
    QProcess proc;
    proc.start(QStringLiteral("modinfo"), {name});
    proc.waitForFinished(5000);

    m_selectedModuleInfo = QString::fromUtf8(proc.readAllStandardOutput());
    if (m_selectedModuleInfo.isEmpty()) {
        m_selectedModuleInfo = QString::fromUtf8(proc.readAllStandardError());
    }
    Q_EMIT selectedModuleInfoChanged();
}

void KernelModulesKcm::blacklistModule(const QString &name)
{
    // Read existing nsa-fsm-blacklist.conf content (if any)
    QFile existing(QStringLiteral("/etc/modprobe.d/nsa-fsm-blacklist.conf"));
    QString content;
    if (existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(existing.readAll());
        existing.close();
    }

    // Check if already blacklisted in our file
    if (content.contains(QStringLiteral("blacklist ") + name)) {
        return;
    }

    // Append the new blacklist entry
    if (!content.isEmpty() && !content.endsWith(QLatin1Char('\n'))) {
        content += QLatin1Char('\n');
    }
    if (content.isEmpty()) {
        content = QStringLiteral("# Generated by NSA Fedora System Manager\n");
    }
    content += QStringLiteral("blacklist ") + name + QLatin1Char('\n');

    QVariantMap args;
    args[QStringLiteral("filePath")] = QStringLiteral("/etc/modprobe.d/nsa-fsm-blacklist.conf");
    args[QStringLiteral("content")] = content.toUtf8();

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.write"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "KAuth blacklist write failed:" << job->errorString();
        } else {
            refresh();
        }
    });
    job->start();
}

void KernelModulesKcm::unblacklistModule(const QString &name)
{
    // Read existing nsa-fsm-blacklist.conf
    QFile existing(QStringLiteral("/etc/modprobe.d/nsa-fsm-blacklist.conf"));
    if (!existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot read nsa-fsm-blacklist.conf for unblacklisting";
        return;
    }
    const QString content = QString::fromUtf8(existing.readAll());
    existing.close();

    // Remove the blacklist line for this module
    QStringList lines = content.split(QLatin1Char('\n'));
    QStringList newLines;
    const QString blacklistLine = QStringLiteral("blacklist ") + name;
    for (const QString &line : lines) {
        if (line.trimmed() != blacklistLine) {
            newLines.append(line);
        }
    }

    QString newContent = newLines.join(QLatin1Char('\n'));
    // Ensure file ends with newline if it has content
    if (!newContent.isEmpty() && !newContent.endsWith(QLatin1Char('\n'))) {
        newContent += QLatin1Char('\n');
    }

    QVariantMap args;
    args[QStringLiteral("filePath")] = QStringLiteral("/etc/modprobe.d/nsa-fsm-blacklist.conf");
    args[QStringLiteral("content")] = newContent.toUtf8();

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.write"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "KAuth unblacklist write failed:" << job->errorString();
        } else {
            refresh();
        }
    });
    job->start();
}

void KernelModulesKcm::loadModule(const QString &name)
{
    QVariantMap args;
    args[QStringLiteral("command")] = QStringLiteral("/usr/sbin/modprobe");
    args[QStringLiteral("args")] = QStringList{name};

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.execute"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job, name] {
        if (job->error()) {
            qWarning() << "Failed to load module" << name << ":" << job->errorString();
        } else {
            const int exitCode = job->data().value(QStringLiteral("exitCode")).toInt();
            if (exitCode != 0) {
                qWarning() << "modprobe" << name << "exited with code" << exitCode
                           << QString::fromUtf8(job->data().value(QStringLiteral("stderr")).toByteArray());
            } else {
                qInfo() << "Module" << name << "loaded successfully";
            }
            refresh();
        }
    });
    job->start();
}

void KernelModulesKcm::unloadModule(const QString &name)
{
    QVariantMap args;
    args[QStringLiteral("command")] = QStringLiteral("/usr/sbin/rmmod");
    args[QStringLiteral("args")] = QStringList{name};

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.execute"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job, name] {
        if (job->error()) {
            qWarning() << "Failed to unload module" << name << ":" << job->errorString();
        } else {
            const int exitCode = job->data().value(QStringLiteral("exitCode")).toInt();
            if (exitCode != 0) {
                qWarning() << "rmmod" << name << "exited with code" << exitCode
                           << QString::fromUtf8(job->data().value(QStringLiteral("stderr")).toByteArray());
            } else {
                qInfo() << "Module" << name << "unloaded successfully";
            }
            refresh();
        }
    });
    job->start();
}

void KernelModulesKcm::refresh()
{
    load();
}

K_PLUGIN_CLASS_WITH_JSON(KernelModulesKcm, "kcm_nsa_kernelmodules.json")

#include "kernelmoduleskcm.moc"
