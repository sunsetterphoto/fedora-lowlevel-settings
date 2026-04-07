// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "fstabkcm.h"

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>

#include <QFile>
#include <QTextStream>

#include "configparser.h"

FstabKcm::FstabKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    setButtons(Apply | Default);
    setAuthActionName(QStringLiteral("org.kde.nsafsm.write"));
    load();
}

void FstabKcm::parseProcMounts()
{
    m_mountedPoints.clear();
    QFile file(QStringLiteral("/proc/mounts"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            m_mountedPoints.insert(parts.at(1));
        }
    }
}

QVariantMap FstabKcm::fstabEntryToMap(const NsaFsm::FstabEntry &entry) const
{
    QVariantMap map;

    // Comment-only line
    if (entry.device.isEmpty() && !entry.comment.isEmpty()) {
        map[QStringLiteral("isComment")] = true;
        map[QStringLiteral("comment")] = entry.comment;
        map[QStringLiteral("device")] = QString();
        map[QStringLiteral("mountpoint")] = QString();
        map[QStringLiteral("fstype")] = QString();
        map[QStringLiteral("options")] = QString();
        map[QStringLiteral("dump")] = 0;
        map[QStringLiteral("pass")] = 0;
        map[QStringLiteral("isMounted")] = false;
        map[QStringLiteral("isRootFs")] = false;
        return map;
    }

    map[QStringLiteral("isComment")] = false;
    map[QStringLiteral("comment")] = entry.comment;
    map[QStringLiteral("device")] = entry.device;
    map[QStringLiteral("mountpoint")] = entry.mountpoint;
    map[QStringLiteral("fstype")] = entry.fstype;
    map[QStringLiteral("options")] = entry.options;
    map[QStringLiteral("dump")] = entry.dump;
    map[QStringLiteral("pass")] = entry.pass;

    const bool isRootFs = (entry.mountpoint == QStringLiteral("/"));
    map[QStringLiteral("isRootFs")] = isRootFs;

    // Check if this mountpoint is currently mounted
    // For swap entries, check if swap is active (they don't have a mountpoint in /proc/mounts)
    if (entry.fstype == QStringLiteral("swap")) {
        map[QStringLiteral("isMounted")] = m_mountedPoints.contains(QStringLiteral("none"))
            || QFile::exists(QStringLiteral("/proc/swaps"));
    } else {
        map[QStringLiteral("isMounted")] = m_mountedPoints.contains(entry.mountpoint);
    }

    return map;
}

void FstabKcm::load()
{
    parseProcMounts();

    const auto fstabEntries = NsaFsm::ConfigParser::parseFstab(QStringLiteral("/etc/fstab"));

    m_entries.clear();
    for (const auto &entry : fstabEntries) {
        m_entries.append(fstabEntryToMap(entry));
    }

    Q_EMIT fstabEntriesChanged();
    setNeedsSave(false);
}

void FstabKcm::save()
{
    // Reconstruct FstabEntry list from QVariantList (preserves comments)
    QList<NsaFsm::FstabEntry> entries;
    for (const auto &variant : std::as_const(m_entries)) {
        const QVariantMap map = variant.toMap();
        NsaFsm::FstabEntry entry;

        if (map[QStringLiteral("isComment")].toBool()) {
            entry.comment = map[QStringLiteral("comment")].toString();
            entries.append(entry);
            continue;
        }

        entry.device = map[QStringLiteral("device")].toString();
        entry.mountpoint = map[QStringLiteral("mountpoint")].toString();
        entry.fstype = map[QStringLiteral("fstype")].toString();
        entry.options = map[QStringLiteral("options")].toString();
        entry.dump = map[QStringLiteral("dump")].toInt();
        entry.pass = map[QStringLiteral("pass")].toInt();
        entries.append(entry);
    }

    // Build fstab content using the same format as ConfigParser::writeFstab
    QString content;
    QTextStream out(&content);
    for (const auto &entry : std::as_const(entries)) {
        if (!entry.comment.isEmpty() && entry.device.isEmpty()) {
            out << entry.comment << QLatin1Char('\n');
            continue;
        }
        out << entry.device << QLatin1Char('\t')
            << entry.mountpoint << QLatin1Char('\t')
            << entry.fstype << QLatin1Char('\t')
            << entry.options << QLatin1Char('\t')
            << entry.dump << QLatin1Char('\t')
            << entry.pass << QLatin1Char('\n');
    }

    QVariantMap args;
    args[QStringLiteral("filePath")] = QStringLiteral("/etc/fstab");
    args[QStringLiteral("content")] = content.toUtf8();
    args[QStringLiteral("postCommand")] = QStringLiteral("/usr/bin/systemctl");
    args[QStringLiteral("postArgs")] = QStringList{QStringLiteral("daemon-reload")};

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.write"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "KAuth write /etc/fstab failed:" << job->errorString();
        } else {
            load();
        }
        setNeedsSave(false);
    });
    job->start();
}

void FstabKcm::updateEntry(int index, const QVariantMap &entry)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }

    const QVariantMap existing = m_entries.at(index).toMap();

    // Protect root filesystem: device and mountpoint cannot be changed
    if (existing[QStringLiteral("isRootFs")].toBool()) {
        QVariantMap updated = entry;
        updated[QStringLiteral("device")] = existing[QStringLiteral("device")];
        updated[QStringLiteral("mountpoint")] = existing[QStringLiteral("mountpoint")];
        updated[QStringLiteral("isRootFs")] = true;
        updated[QStringLiteral("isMounted")] = existing[QStringLiteral("isMounted")];
        updated[QStringLiteral("isComment")] = false;
        m_entries[index] = updated;
    } else {
        QVariantMap updated = entry;
        updated[QStringLiteral("isComment")] = false;
        updated[QStringLiteral("isMounted")] = existing[QStringLiteral("isMounted")];
        updated[QStringLiteral("isRootFs")] = false;
        m_entries[index] = updated;
    }

    Q_EMIT fstabEntriesChanged();
    setNeedsSave(true);
}

void FstabKcm::removeEntry(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }

    const QVariantMap entry = m_entries.at(index).toMap();

    // Refuse to remove root filesystem entry
    if (entry[QStringLiteral("isRootFs")].toBool()) {
        qWarning() << "Cannot remove root filesystem entry";
        return;
    }

    m_entries.removeAt(index);
    Q_EMIT fstabEntriesChanged();
    setNeedsSave(true);
}

void FstabKcm::addEntry(const QVariantMap &entry)
{
    QVariantMap newEntry = entry;
    newEntry[QStringLiteral("isComment")] = false;
    newEntry[QStringLiteral("isMounted")] = false;
    newEntry[QStringLiteral("isRootFs")] = (entry[QStringLiteral("mountpoint")].toString() == QStringLiteral("/"));
    m_entries.append(newEntry);
    Q_EMIT fstabEntriesChanged();
    setNeedsSave(true);
}

QString FstabKcm::fsTypeDescription(const QString &fstype) const
{
    static const QHash<QString, QString> descriptions = {
        {QStringLiteral("ext4"), QStringLiteral("Standard Linux filesystem, good all-around performance")},
        {QStringLiteral("btrfs"), QStringLiteral("Copy-on-write filesystem with snapshots and compression support")},
        {QStringLiteral("xfs"), QStringLiteral("High-performance filesystem, excellent for large files")},
        {QStringLiteral("vfat"), QStringLiteral("FAT32, for EFI system partition and USB drives")},
        {QStringLiteral("ntfs"), QStringLiteral("Windows filesystem (read/write via ntfs3 driver)")},
        {QStringLiteral("swap"), QStringLiteral("Swap space for virtual memory")},
        {QStringLiteral("nfs"), QStringLiteral("Network filesystem for remote file access")},
        {QStringLiteral("tmpfs"), QStringLiteral("RAM-based temporary filesystem")},
    };
    return descriptions.value(fstype);
}

K_PLUGIN_CLASS_WITH_JSON(FstabKcm, "kcm_nsa_fstab.json")

#include "fstabkcm.moc"
