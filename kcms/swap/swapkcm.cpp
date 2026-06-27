// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "swapkcm.h"

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>
#include <QFile>
#include <QTextStream>

#include "configparser.h"

SwapKcm::SwapKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    setButtons(Apply | Default);
    setAuthActionName(QStringLiteral("org.kde.fls.write"));
    load();
}

// --- Property setters ---

void SwapKcm::setZramSize(const QString &value)
{
    if (m_zramSize != value) {
        m_zramSize = value;
        Q_EMIT zramSizeChanged();
        setNeedsSave(true);
    }
}

void SwapKcm::setZramAlgorithm(const QString &value)
{
    if (m_zramAlgorithm != value) {
        m_zramAlgorithm = value;
        Q_EMIT zramAlgorithmChanged();
        setNeedsSave(true);
    }
}

// --- Format helper ---

QString SwapKcm::formatBytes(qint64 bytes)
{
    if (bytes < 0) {
        return QStringLiteral("N/A");
    }

    const double kib = 1024.0;
    const double mib = kib * 1024.0;
    const double gib = mib * 1024.0;
    const double tib = gib * 1024.0;

    if (bytes >= tib) {
        return QStringLiteral("%1 TiB").arg(bytes / tib, 0, 'f', 1);
    } else if (bytes >= gib) {
        return QStringLiteral("%1 GiB").arg(bytes / gib, 0, 'f', 1);
    } else if (bytes >= mib) {
        return QStringLiteral("%1 MiB").arg(bytes / mib, 0, 'f', 1);
    } else if (bytes >= kib) {
        return QStringLiteral("%1 KiB").arg(bytes / kib, 0, 'f', 1);
    } else {
        return QStringLiteral("%1 B").arg(bytes);
    }
}

// --- Load ---

void SwapKcm::load()
{
    loadSwapEntries();
    loadZramConfig();
    loadAvailableAlgorithms();
    loadZramStats();
    loadMemInfo();
    loadSwappiness();
    setNeedsSave(false);
}

void SwapKcm::loadSwapEntries()
{
    m_swapEntries.clear();

    QFile file(QStringLiteral("/proc/swaps"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Q_EMIT swapEntriesChanged();
        return;
    }

    QTextStream in(&file);
    // Skip header line
    if (!in.atEnd()) {
        in.readLine();
    }

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        // Columns: Filename Type Size Used Priority (whitespace-separated)
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")));
        if (parts.size() < 5) {
            continue;
        }

        QVariantMap entry;
        entry[QStringLiteral("filename")] = parts[0];
        entry[QStringLiteral("type")] = parts[1];
        // /proc/swaps reports size in KiB
        const qint64 sizeKib = parts[2].toLongLong();
        const qint64 usedKib = parts[3].toLongLong();
        entry[QStringLiteral("size")] = sizeKib * 1024;
        entry[QStringLiteral("used")] = usedKib * 1024;
        entry[QStringLiteral("sizeFormatted")] = formatBytes(sizeKib * 1024);
        entry[QStringLiteral("usedFormatted")] = formatBytes(usedKib * 1024);
        entry[QStringLiteral("priority")] = parts[4];
        m_swapEntries.append(entry);
    }

    Q_EMIT swapEntriesChanged();
}

void SwapKcm::loadZramConfig()
{
    // Try /etc first, then fall back to /usr/lib
    auto sections = Fls::ConfigParser::parseIni(
        QStringLiteral("/etc/systemd/zram-generator.conf"));

    if (sections.isEmpty()) {
        sections = Fls::ConfigParser::parseIni(
            QStringLiteral("/usr/lib/systemd/zram-generator.conf"));
    }

    const auto zram0 = sections.value(QStringLiteral("zram0"));

    m_zramSize = zram0.value(QStringLiteral("zram-size"));
    m_zramAlgorithm = zram0.value(QStringLiteral("compression-algorithm"));

    if (m_zramSize.isEmpty()) {
        m_zramSize = QStringLiteral("min(ram, 8192)");
    }

    m_origZramSize = m_zramSize;
    m_origZramAlgorithm = m_zramAlgorithm;

    Q_EMIT zramSizeChanged();
    Q_EMIT zramAlgorithmChanged();
}

void SwapKcm::loadAvailableAlgorithms()
{
    m_availableAlgorithms.clear();

    QFile file(QStringLiteral("/sys/block/zram0/comp_algorithm"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Q_EMIT availableAlgorithmsChanged();
        return;
    }

    const QString content = QString::fromUtf8(file.readAll()).trimmed();
    const QStringList parts = content.split(QRegularExpression(QStringLiteral("\\s+")));

    for (const QString &part : parts) {
        QString algo = part;
        // The currently active algorithm is enclosed in [brackets]
        if (algo.startsWith(QLatin1Char('[')) && algo.endsWith(QLatin1Char(']'))) {
            algo = algo.mid(1, algo.length() - 2);
            // If no algorithm was set in config, use the current one
            if (m_zramAlgorithm.isEmpty()) {
                m_zramAlgorithm = algo;
                m_origZramAlgorithm = algo;
                Q_EMIT zramAlgorithmChanged();
            }
        }
        m_availableAlgorithms.append(algo);
    }

    Q_EMIT availableAlgorithmsChanged();
}

void SwapKcm::loadZramStats()
{
    // Read disksize
    {
        QFile file(QStringLiteral("/sys/block/zram0/disksize"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const qint64 disksize = QString::fromUtf8(file.readAll()).trimmed().toLongLong();
            m_zramCurrentSize = formatBytes(disksize);
        } else {
            m_zramCurrentSize = QStringLiteral("N/A");
        }
        Q_EMIT zramCurrentSizeChanged();
    }

    // Read mm_stat: orig_data_size compr_data_size mem_used_total mem_limit
    //               mem_used_max same_pages pages_compacted huge_pages huge_pages_since
    {
        QFile file(QStringLiteral("/sys/block/zram0/mm_stat"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(file.readAll()).trimmed();
            const QStringList parts = content.split(QRegularExpression(QStringLiteral("\\s+")));
            if (parts.size() >= 2) {
                const qint64 origData = parts[0].toLongLong();
                const qint64 comprData = parts[1].toLongLong();
                m_zramOrigDataSize = formatBytes(origData);
                m_zramComprDataSize = formatBytes(comprData);
            } else {
                m_zramOrigDataSize = QStringLiteral("N/A");
                m_zramComprDataSize = QStringLiteral("N/A");
            }
        } else {
            m_zramOrigDataSize = QStringLiteral("N/A");
            m_zramComprDataSize = QStringLiteral("N/A");
        }
        Q_EMIT zramOrigDataSizeChanged();
        Q_EMIT zramComprDataSizeChanged();
    }
}

void SwapKcm::loadMemInfo()
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_totalRam = QStringLiteral("N/A");
        Q_EMIT totalRamChanged();
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith(QStringLiteral("MemTotal:"))) {
            // MemTotal:       57195444 kB
            const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")));
            if (parts.size() >= 2) {
                const qint64 kbytes = parts[1].toLongLong();
                m_totalRam = formatBytes(kbytes * 1024);
            }
            break;
        }
    }

    Q_EMIT totalRamChanged();
}

void SwapKcm::loadSwappiness()
{
    QFile file(QStringLiteral("/proc/sys/vm/swappiness"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_swappiness = QString::fromUtf8(file.readAll()).trimmed();
    } else {
        m_swappiness = QStringLiteral("N/A");
    }
    Q_EMIT swappinessChanged();
}

// --- Save ---

void SwapKcm::save()
{
    if (m_zramSize == m_origZramSize && m_zramAlgorithm == m_origZramAlgorithm) {
        setNeedsSave(false);
        return;
    }

    QString content = QStringLiteral("[zram0]\nzram-size = %1\ncompression-algorithm = %2\n")
                          .arg(m_zramSize, m_zramAlgorithm);

    QVariantMap args;
    args[QStringLiteral("filePath")] = QStringLiteral("/etc/systemd/zram-generator.conf");
    args[QStringLiteral("content")] = content.toUtf8();

    KAuth::Action action(QStringLiteral("org.kde.fls.write"));
    action.setHelperId(QStringLiteral("org.kde.fls"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "KAuth write zram-generator.conf failed:" << job->errorString();
        } else {
            m_origZramSize = m_zramSize;
            m_origZramAlgorithm = m_zramAlgorithm;
        }
        setNeedsSave(false);
    });
    job->start();
}

K_PLUGIN_CLASS_WITH_JSON(SwapKcm, "kcm_fls_swap.json")

#include "swapkcm.moc"
