// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "sysctlmodel.h"

#include "configparser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>

SysctlModel::SysctlModel(QObject *parent)
    : QAbstractListModel(parent)
{
    reload();
}

int SysctlModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_filteredParams.size();
}

QVariant SysctlModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredParams.size())
        return {};

    const auto &param = m_filteredParams.at(index.row());

    switch (role) {
    case KeyRole:
        return param.key;
    case CurrentValueRole:
        return param.currentValue;
    case CustomValueRole:
        return param.customValue;
    case CategoryRole:
        return param.category;
    case DescriptionRole:
        return param.description;
    case AllowedValuesRole:
        return param.allowedValues;
    case IsModifiedRole:
        return param.isModified;
    }
    return {};
}

QHash<int, QByteArray> SysctlModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {CurrentValueRole, "currentValue"},
        {CustomValueRole, "customValue"},
        {CategoryRole, "category"},
        {DescriptionRole, "description"},
        {AllowedValuesRole, "allowedValues"},
        {IsModifiedRole, "isModified"},
    };
}

QString SysctlModel::searchText() const
{
    return m_searchText;
}

void SysctlModel::setSearchText(const QString &text)
{
    if (m_searchText == text)
        return;
    m_searchText = text;
    Q_EMIT searchTextChanged();
    applyFilters();
}

QString SysctlModel::categoryFilter() const
{
    return m_categoryFilter;
}

void SysctlModel::setCategoryFilter(const QString &filter)
{
    if (m_categoryFilter == filter)
        return;
    m_categoryFilter = filter;
    Q_EMIT categoryFilterChanged();
    applyFilters();
}

bool SysctlModel::onlyModified() const
{
    return m_onlyModified;
}

void SysctlModel::setOnlyModified(bool only)
{
    if (m_onlyModified == only)
        return;
    m_onlyModified = only;
    Q_EMIT onlyModifiedChanged();
    applyFilters();
}

void SysctlModel::setValue(const QString &key, const QString &value)
{
    m_customValues[key] = value;

    // Update the matching entry in m_allParams
    for (auto &param : m_allParams) {
        if (param.key == key) {
            param.customValue = value;
            param.isModified = true;
            break;
        }
    }

    applyFilters();
}

void SysctlModel::resetValue(const QString &key)
{
    m_customValues.remove(key);

    for (auto &param : m_allParams) {
        if (param.key == key) {
            param.customValue.clear();
            param.isModified = false;
            break;
        }
    }

    applyFilters();
}

QVariantList SysctlModel::modifiedEntries() const
{
    QVariantList result;
    for (auto it = m_customValues.constBegin(); it != m_customValues.constEnd(); ++it) {
        QVariantMap entry;
        entry[QStringLiteral("key")] = it.key();
        entry[QStringLiteral("value")] = it.value();
        result.append(entry);
    }
    return result;
}

void SysctlModel::reload()
{
    loadFromProc();
    loadCustomValues();
    applyFilters();
}

void SysctlModel::loadFromProc()
{
    m_allParams.clear();

    const QString procSysPath = QStringLiteral("/proc/sys");
    QDirIterator it(procSysPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    int count = 0;
    while (it.hasNext() && count < MaxParameters) {
        it.next();

        const QString filePath = it.filePath();

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue; // Skip files we can't read (permission denied)
        }

        QTextStream in(&file);
        const QString value = in.readAll().trimmed();
        file.close();

        // Convert path to sysctl key: strip /proc/sys/ prefix and replace / with .
        QString key = filePath.mid(procSysPath.length() + 1); // +1 for trailing /
        key.replace(QLatin1Char('/'), QLatin1Char('.'));

        // Category is the first component
        const int dotPos = key.indexOf(QLatin1Char('.'));
        const QString category = dotPos > 0 ? key.left(dotPos) : key;

        SysctlParam param;
        param.key = key;
        param.currentValue = value;
        param.category = category;
        param.isModified = false;

        populateHelpText(param);
        m_allParams.append(param);
        ++count;
    }

    // Sort by key for consistent ordering
    std::sort(m_allParams.begin(), m_allParams.end(), [](const SysctlParam &a, const SysctlParam &b) {
        return a.key < b.key;
    });
}

void SysctlModel::loadCustomValues()
{
    m_customValues.clear();

    const QString confPath = QStringLiteral("/etc/sysctl.d/99-nsa-fsm.conf");
    const auto entries = NsaFsm::ConfigParser::parseSysctl(confPath);

    for (const auto &entry : entries) {
        m_customValues[entry.key] = entry.value;
    }

    // Apply custom values to parameters
    for (auto &param : m_allParams) {
        if (m_customValues.contains(param.key)) {
            param.customValue = m_customValues[param.key];
            param.isModified = true;
        } else {
            param.customValue.clear();
            param.isModified = false;
        }
    }
}

void SysctlModel::applyFilters()
{
    beginResetModel();

    m_filteredParams.clear();
    for (const auto &param : std::as_const(m_allParams)) {
        // Category filter
        if (!m_categoryFilter.isEmpty() && param.category != m_categoryFilter)
            continue;

        // Search filter
        if (!m_searchText.isEmpty()
            && !param.key.contains(m_searchText, Qt::CaseInsensitive))
            continue;

        // Only modified filter
        if (m_onlyModified && !param.isModified)
            continue;

        m_filteredParams.append(param);
    }

    endResetModel();
}

void SysctlModel::populateHelpText(SysctlParam &param)
{
    // Build once
    static const QHash<QString, std::pair<QString, QString>> helpDB = {
        // VM subsystem
        {QStringLiteral("vm.swappiness"),
         {QStringLiteral("Controls how aggressively the kernel swaps memory pages to disk. "
                         "Lower values keep more data in RAM, higher values swap more aggressively. "
                         "Desktop: 10-30, Server: 60 (default)."),
          QStringLiteral("0-200 (default: 60)")}},
        {QStringLiteral("vm.dirty_ratio"),
         {QStringLiteral("Maximum percentage of system memory that can be filled with dirty pages "
                         "before the writing process is forced to write them out. "
                         "Lower values reduce data loss risk on crash but may reduce write performance."),
          QStringLiteral("0-100 (default: 20)")}},
        {QStringLiteral("vm.dirty_background_ratio"),
         {QStringLiteral("Percentage of system memory at which background writeback of dirty pages starts. "
                         "Lower values start flushing to disk sooner, reducing burst writes."),
          QStringLiteral("0-100 (default: 10)")}},
        {QStringLiteral("vm.dirty_expire_centisecs"),
         {QStringLiteral("How long dirty pages can stay in memory before they must be written to disk. "
                         "Value in centiseconds (1/100 of a second). Lower = more frequent writes, less data loss risk."),
          QStringLiteral("Centiseconds (default: 3000 = 30s)")}},
        {QStringLiteral("vm.dirty_writeback_centisecs"),
         {QStringLiteral("How often the kernel writeback daemon wakes up to check for dirty pages. "
                         "Lower = more responsive flushing, slightly more CPU usage."),
          QStringLiteral("Centiseconds (default: 500 = 5s)")}},
        {QStringLiteral("vm.vfs_cache_pressure"),
         {QStringLiteral("Controls the tendency of the kernel to reclaim inode/dentry caches vs page cache. "
                         "Higher values reclaim directory/inode caches more aggressively. "
                         "Lower values keep filesystem metadata in memory longer (good for many small files)."),
          QStringLiteral("0-1000 (default: 100)")}},
        {QStringLiteral("vm.overcommit_memory"),
         {QStringLiteral("Controls memory overcommit behavior. "
                         "0 = heuristic (default, allows reasonable overcommit). "
                         "1 = always overcommit (never fail malloc). "
                         "2 = strict, only allow up to swap + overcommit_ratio% of RAM."),
          QStringLiteral("0, 1, or 2 (default: 0)")}},
        {QStringLiteral("vm.overcommit_ratio"),
         {QStringLiteral("When overcommit_memory=2, percentage of RAM that can be overcommitted. "
                         "Total available memory = swap + RAM * (overcommit_ratio / 100)."),
          QStringLiteral("0-100 (default: 50)")}},
        {QStringLiteral("vm.min_free_kbytes"),
         {QStringLiteral("Minimum amount of free memory (in KB) the kernel tries to keep available. "
                         "Too low: risk of OOM in interrupt context. Too high: wastes memory."),
          QStringLiteral("KB (default: varies by RAM size)")}},
        {QStringLiteral("vm.max_map_count"),
         {QStringLiteral("Maximum number of memory map areas a process can have. "
                         "Some applications (e.g., Elasticsearch, games) need higher values."),
          QStringLiteral("Integer (default: 65530)")}},

        // Network
        {QStringLiteral("net.ipv4.ip_forward"),
         {QStringLiteral("Enable or disable IPv4 packet forwarding between interfaces. "
                         "Required for routing, NAT, VPN servers, and container networking."),
          QStringLiteral("0 = disabled, 1 = enabled")}},
        {QStringLiteral("net.ipv4.tcp_syncookies"),
         {QStringLiteral("Enable TCP SYN cookies to protect against SYN flood attacks. "
                         "When the SYN queue is full, the server responds with a cookie instead of allocating resources."),
          QStringLiteral("0 = disabled, 1 = enabled (default: 1)")}},
        {QStringLiteral("net.ipv4.tcp_timestamps"),
         {QStringLiteral("Enable TCP timestamps (RFC 1323). Improves performance measurement and PAWS. "
                         "Disabling may prevent some fingerprinting but reduces TCP reliability."),
          QStringLiteral("0 = disabled, 1 = enabled (default: 1)")}},
        {QStringLiteral("net.core.somaxconn"),
         {QStringLiteral("Maximum number of pending connections in the listen queue. "
                         "Increase for high-traffic servers. Low values cause connection drops under load."),
          QStringLiteral("Integer (default: 4096)")}},
        {QStringLiteral("net.core.rmem_max"),
         {QStringLiteral("Maximum receive socket buffer size in bytes. "
                         "Increase for high-bandwidth applications and network-intensive workloads."),
          QStringLiteral("Bytes (default: 212992)")}},
        {QStringLiteral("net.core.wmem_max"),
         {QStringLiteral("Maximum send socket buffer size in bytes. "
                         "Increase for high-bandwidth applications."),
          QStringLiteral("Bytes (default: 212992)")}},
        {QStringLiteral("net.core.netdev_max_backlog"),
         {QStringLiteral("Maximum number of packets queued on the INPUT side when the interface receives faster than the kernel can process. "
                         "Increase on high-speed networks."),
          QStringLiteral("Integer (default: 1000)")}},
        {QStringLiteral("net.ipv4.tcp_max_syn_backlog"),
         {QStringLiteral("Maximum number of remembered connection requests that have not yet been acknowledged. "
                         "Increase for servers handling many connections."),
          QStringLiteral("Integer (default: 1024)")}},
        {QStringLiteral("net.ipv4.tcp_fin_timeout"),
         {QStringLiteral("Time in seconds to hold a socket in FIN-WAIT-2 state. "
                         "Lower values free up resources faster but may break slow connections."),
          QStringLiteral("Seconds (default: 60)")}},
        {QStringLiteral("net.ipv4.tcp_keepalive_time"),
         {QStringLiteral("How long (seconds) a connection must be idle before TCP starts sending keepalive probes. "
                         "Lower values detect dead connections faster."),
          QStringLiteral("Seconds (default: 7200 = 2h)")}},
        {QStringLiteral("net.ipv6.conf.all.disable_ipv6"),
         {QStringLiteral("Disable IPv6 on all network interfaces. "
                         "0 = IPv6 enabled, 1 = IPv6 disabled. Only disable if IPv6 causes problems."),
          QStringLiteral("0 or 1 (default: 0)")}},
        {QStringLiteral("net.ipv4.conf.all.rp_filter"),
         {QStringLiteral("Reverse path filtering. Validates that incoming packets arrive on the expected interface. "
                         "0 = off, 1 = strict (recommended), 2 = loose."),
          QStringLiteral("0, 1, or 2 (default: 2)")}},

        // Kernel
        {QStringLiteral("kernel.sysrq"),
         {QStringLiteral("Controls which SysRq key functions are allowed. "
                         "SysRq provides emergency keyboard shortcuts (e.g., REISUB for safe reboot). "
                         "0 = disabled, 1 = all enabled, bitmask for selective."),
          QStringLiteral("0-511 bitmask (default: 16)")}},
        {QStringLiteral("kernel.panic"),
         {QStringLiteral("Number of seconds the kernel waits after a panic before automatically rebooting. "
                         "0 = never reboot (hang on panic). Useful for servers that should auto-recover."),
          QStringLiteral("Seconds, 0 = no reboot (default: 0)")}},
        {QStringLiteral("kernel.pid_max"),
         {QStringLiteral("Maximum process ID value. When reached, PIDs wrap around. "
                         "Increase on systems running many processes/containers."),
          QStringLiteral("Integer (default: 4194304)")}},
        {QStringLiteral("kernel.threads-max"),
         {QStringLiteral("Maximum number of threads the system can have. "
                         "Increase for applications spawning many threads."),
          QStringLiteral("Integer (default: varies by RAM)")}},
        {QStringLiteral("kernel.printk"),
         {QStringLiteral("Controls kernel log message levels shown on console. "
                         "Format: console_loglevel default_message_loglevel minimum_console_loglevel default_console_loglevel."),
          QStringLiteral("4 space-separated integers")}},
        {QStringLiteral("kernel.dmesg_restrict"),
         {QStringLiteral("Restrict access to kernel log (dmesg) to users with CAP_SYSLOG. "
                         "1 = restricted (more secure), 0 = any user can read dmesg."),
          QStringLiteral("0 or 1 (default: 1 on Fedora)")}},
        {QStringLiteral("kernel.yama.ptrace_scope"),
         {QStringLiteral("Controls which processes can ptrace (debug) other processes. "
                         "0 = any process, 1 = only parent (default), 2 = admin only, 3 = disabled completely."),
          QStringLiteral("0-3 (default: 1)")}},

        // FS
        {QStringLiteral("fs.file-max"),
         {QStringLiteral("Maximum number of file handles the kernel will allocate. "
                         "Increase if you see 'too many open files' errors system-wide."),
          QStringLiteral("Integer (default: varies by RAM)")}},
        {QStringLiteral("fs.inotify.max_user_watches"),
         {QStringLiteral("Maximum number of inotify watches per user. "
                         "IDEs, file sync tools, and build systems need many watches. "
                         "Increase if you see 'inotify watch limit reached' errors."),
          QStringLiteral("Integer (default: 524288)")}},
        {QStringLiteral("fs.inotify.max_user_instances"),
         {QStringLiteral("Maximum number of inotify instances per user. "
                         "Each application monitoring files creates at least one instance."),
          QStringLiteral("Integer (default: 128)")}},
        {QStringLiteral("fs.protected_hardlinks"),
         {QStringLiteral("Prevents creating hard links to files not owned by the user. "
                         "Security feature to prevent hard link attacks."),
          QStringLiteral("0 = disabled, 1 = enabled (default: 1)")}},
        {QStringLiteral("fs.protected_symlinks"),
         {QStringLiteral("Prevents following symlinks in world-writable sticky directories (like /tmp) "
                         "unless the owner matches. Security feature against symlink attacks."),
          QStringLiteral("0 = disabled, 1 = enabled (default: 1)")}},
    };

    auto it = helpDB.find(param.key);
    if (it != helpDB.end()) {
        param.description = it->first;
        param.allowedValues = it->second;
    }
}
