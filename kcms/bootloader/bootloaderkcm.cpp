// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "bootloaderkcm.h"

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>

#include "configparser.h"

BootloaderKcm::BootloaderKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    setButtons(Apply | Default);
    setAuthActionName(QStringLiteral("org.kde.nsafsm.write"));
    load();
}

// --- Property setters ---

void BootloaderKcm::setGrubTimeout(const QString &value)
{
    if (m_grubTimeout != value) {
        m_grubTimeout = value;
        Q_EMIT grubTimeoutChanged();
        setNeedsSave(true);
    }
}

void BootloaderKcm::setGrubDefault(const QString &value)
{
    if (m_grubDefault != value) {
        m_grubDefault = value;
        Q_EMIT grubDefaultChanged();
        setNeedsSave(true);
    }
}

void BootloaderKcm::setGrubCmdlineDefault(const QString &value)
{
    if (m_grubCmdlineDefault != value) {
        m_grubCmdlineDefault = value;
        Q_EMIT grubCmdlineDefaultChanged();
        setNeedsSave(true);
    }
}

void BootloaderKcm::setGrubCmdlineLinux(const QString &value)
{
    if (m_grubCmdlineLinux != value) {
        m_grubCmdlineLinux = value;
        Q_EMIT grubCmdlineLinuxChanged();
        setNeedsSave(true);
    }
}

// --- Load ---

void BootloaderKcm::load()
{
    loadGrubDefaults();
    loadKernelEntries();
    setNeedsSave(false);
}

void BootloaderKcm::loadGrubDefaults()
{
    m_grubValues = NsaFsm::ConfigParser::parseKeyValue(
        QStringLiteral("/etc/default/grub"));

    m_grubTimeout = m_grubValues.value(QStringLiteral("GRUB_TIMEOUT"));
    m_grubDefault = m_grubValues.value(QStringLiteral("GRUB_DEFAULT"));
    m_grubCmdlineDefault = m_grubValues.value(QStringLiteral("GRUB_CMDLINE_LINUX_DEFAULT"));
    m_grubCmdlineLinux = m_grubValues.value(QStringLiteral("GRUB_CMDLINE_LINUX"));

    m_origGrubTimeout = m_grubTimeout;
    m_origGrubDefault = m_grubDefault;
    m_origGrubCmdlineDefault = m_grubCmdlineDefault;
    m_origGrubCmdlineLinux = m_grubCmdlineLinux;

    Q_EMIT grubTimeoutChanged();
    Q_EMIT grubDefaultChanged();
    Q_EMIT grubCmdlineDefaultChanged();
    Q_EMIT grubCmdlineLinuxChanged();
}

void BootloaderKcm::loadKernelEntries()
{
    QVariantMap args;
    args[QStringLiteral("dirPath")] = QStringLiteral("/boot/loader/entries/");

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.read"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "KAuth read /boot/loader/entries/ failed:" << job->errorString();
            return;
        }

        const QVariantMap files = job->data().value(QStringLiteral("files")).toMap();
        m_kernelEntries.clear();

        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            const QString filename = it.key();
            const QString content = QString::fromUtf8(it.value().toByteArray());
            QVariantMap entry = parseBlsEntry(content, filename);
            if (!entry.isEmpty()) {
                m_kernelEntries.append(entry);
            }
        }

        // Sort by version descending (newest first)
        std::sort(m_kernelEntries.begin(), m_kernelEntries.end(),
            [](const QVariant &a, const QVariant &b) {
                return a.toMap().value(QStringLiteral("version")).toString()
                    > b.toMap().value(QStringLiteral("version")).toString();
            });

        Q_EMIT kernelEntriesChanged();
    });
    job->start();
}

QVariantMap BootloaderKcm::parseBlsEntry(const QString &content, const QString &filename)
{
    QVariantMap entry;
    entry[QStringLiteral("filename")] = filename;

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        // BLS format: key<space>rest-of-line
        const int spacePos = trimmed.indexOf(QLatin1Char(' '));
        if (spacePos < 0) {
            continue;
        }

        const QString key = trimmed.left(spacePos);
        const QString value = trimmed.mid(spacePos + 1);

        entry[key] = value;
    }

    // Only return entries that have at least a version or title
    if (entry.contains(QStringLiteral("version")) || entry.contains(QStringLiteral("title"))) {
        return entry;
    }

    return {};
}

// --- Save ---

void BootloaderKcm::save()
{
    saveGrubDefaults();
}

void BootloaderKcm::saveGrubDefaults()
{
    if (m_grubTimeout == m_origGrubTimeout
        && m_grubDefault == m_origGrubDefault
        && m_grubCmdlineDefault == m_origGrubCmdlineDefault
        && m_grubCmdlineLinux == m_origGrubCmdlineLinux) {
        return;
    }

    // Update the stored values map with our exposed properties
    m_grubValues[QStringLiteral("GRUB_TIMEOUT")] = m_grubTimeout;
    m_grubValues[QStringLiteral("GRUB_DEFAULT")] = m_grubDefault;
    if (!m_grubCmdlineDefault.isEmpty()) {
        m_grubValues[QStringLiteral("GRUB_CMDLINE_LINUX_DEFAULT")] = m_grubCmdlineDefault;
    }
    m_grubValues[QStringLiteral("GRUB_CMDLINE_LINUX")] = m_grubCmdlineLinux;

    // Reconstruct the file content
    QString content;
    QTextStream out(&content);
    for (auto it = m_grubValues.constBegin(); it != m_grubValues.constEnd(); ++it) {
        const QString &val = it.value();
        if (val.contains(QLatin1Char(' ')) || val.contains(QLatin1Char('\t'))
            || val.contains(QLatin1Char('$')) || val.contains(QLatin1Char('('))) {
            out << it.key() << QStringLiteral("=\"") << val << QStringLiteral("\"\n");
        } else {
            out << it.key() << QLatin1Char('=') << val << QLatin1Char('\n');
        }
    }

    QVariantMap args;
    args[QStringLiteral("filePath")] = QStringLiteral("/etc/default/grub");
    args[QStringLiteral("content")] = content.toUtf8();
    args[QStringLiteral("postCommand")] = QStringLiteral("/usr/sbin/grub2-mkconfig");
    args[QStringLiteral("postArgs")] = QStringList{QStringLiteral("-o"), QStringLiteral("/boot/grub2/grub.cfg")};

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.write"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "KAuth write /etc/default/grub failed:" << job->errorString();
        } else {
            m_origGrubTimeout = m_grubTimeout;
            m_origGrubDefault = m_grubDefault;
            m_origGrubCmdlineDefault = m_grubCmdlineDefault;
            m_origGrubCmdlineLinux = m_grubCmdlineLinux;
        }
        setNeedsSave(false);
    });
    job->start();
}

// --- Q_INVOKABLE methods ---

void BootloaderKcm::setDefaultKernel(const QString &version)
{
    QVariantMap args;
    args[QStringLiteral("command")] = QStringLiteral("/usr/sbin/grubby");
    args[QStringLiteral("args")] = QStringList{
        QStringLiteral("--set-default"),
        QStringLiteral("/boot/vmlinuz-") + version
    };

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.execute"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [job, version] {
        if (job->error()) {
            qWarning() << "Failed to set default kernel:" << job->errorString();
        } else {
            qInfo() << "Default kernel set to" << version;
        }
    });
    job->start();
}

K_PLUGIN_CLASS_WITH_JSON(BootloaderKcm, "kcm_nsa_bootloader.json")

#include "bootloaderkcm.moc"
