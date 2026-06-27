// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "hostnamekcm.h"

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QFile>
#include <QTextStream>

#include "configparser.h"
#include "dbushelper.h"

static const QString hostname1Service = QStringLiteral("org.freedesktop.hostname1");
static const QString hostname1Path = QStringLiteral("/org/freedesktop/hostname1");
static const QString hostname1Iface = QStringLiteral("org.freedesktop.hostname1");
static const QString dbusPropsIface = QStringLiteral("org.freedesktop.DBus.Properties");

HostnameKcm::HostnameKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    setButtons(Apply | Default);
    load();
}

// --- Property setters ---

void HostnameKcm::setPrettyHostname(const QString &value)
{
    if (m_prettyHostname != value) {
        m_prettyHostname = value;
        Q_EMIT prettyHostnameChanged();
        setNeedsSave(true);
    }
}

void HostnameKcm::setStaticHostname(const QString &value)
{
    if (m_staticHostname != value) {
        m_staticHostname = value;
        Q_EMIT staticHostnameChanged();
        setNeedsSave(true);
    }
}

void HostnameKcm::setIconName(const QString &value)
{
    if (m_iconName != value) {
        m_iconName = value;
        Q_EMIT iconNameChanged();
        setNeedsSave(true);
    }
}

void HostnameKcm::setHostsContent(const QString &value)
{
    if (m_hostsContent != value) {
        m_hostsContent = value;
        Q_EMIT hostsContentChanged();
        setNeedsSave(true);
    }
}

void HostnameKcm::setDnsServers(const QString &value)
{
    if (m_dnsServers != value) {
        m_dnsServers = value;
        Q_EMIT dnsServersChanged();
        setNeedsSave(true);
    }
}

void HostnameKcm::setFallbackDns(const QString &value)
{
    if (m_fallbackDns != value) {
        m_fallbackDns = value;
        Q_EMIT fallbackDnsChanged();
        setNeedsSave(true);
    }
}

void HostnameKcm::setDnssec(const QString &value)
{
    if (m_dnssec != value) {
        m_dnssec = value;
        Q_EMIT dnssecChanged();
        setNeedsSave(true);
    }
}

void HostnameKcm::setDnsOverTls(const QString &value)
{
    if (m_dnsOverTls != value) {
        m_dnsOverTls = value;
        Q_EMIT dnsOverTlsChanged();
        setNeedsSave(true);
    }
}

// --- Load ---

void HostnameKcm::load()
{
    loadHostnameFromDBus();
    loadMachineId();
    loadHostsFile();
    loadResolvedConf();
    setNeedsSave(false);
}

void HostnameKcm::loadHostnameFromDBus()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        hostname1Service, hostname1Path, dbusPropsIface, QStringLiteral("GetAll"));
    msg.setArguments({hostname1Iface});

    QDBusReply<QVariantMap> reply = QDBusConnection::systemBus().call(msg, QDBus::Block, 5000);
    if (reply.isValid()) {
        const QVariantMap props = reply.value();

        m_prettyHostname = props.value(QStringLiteral("PrettyHostname")).toString();
        m_staticHostname = props.value(QStringLiteral("StaticHostname")).toString();
        m_iconName = props.value(QStringLiteral("IconName")).toString();
        m_chassisType = props.value(QStringLiteral("Chassis")).toString();

        m_origPrettyHostname = m_prettyHostname;
        m_origStaticHostname = m_staticHostname;
        m_origIconName = m_iconName;

        Q_EMIT prettyHostnameChanged();
        Q_EMIT staticHostnameChanged();
        Q_EMIT iconNameChanged();
        Q_EMIT chassisTypeChanged();
    } else {
        qWarning() << "Failed to read hostname1 properties:" << reply.error().message();
    }
}

void HostnameKcm::loadMachineId()
{
    QFile file(QStringLiteral("/etc/machine-id"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_machineId = QString::fromUtf8(file.readAll()).trimmed();
    }
}

void HostnameKcm::loadHostsFile()
{
    QFile file(QStringLiteral("/etc/hosts"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_hostsContent = QString::fromUtf8(file.readAll());
        m_origHostsContent = m_hostsContent;
        Q_EMIT hostsContentChanged();
    }
}

void HostnameKcm::loadResolvedConf()
{
    const auto sections = Fcse::ConfigParser::parseIni(
        QStringLiteral("/etc/systemd/resolved.conf"));

    const auto resolve = sections.value(QStringLiteral("Resolve"));

    m_dnsServers = resolve.value(QStringLiteral("DNS"));
    m_fallbackDns = resolve.value(QStringLiteral("FallbackDNS"));
    m_dnssec = resolve.value(QStringLiteral("DNSSEC"));
    m_dnsOverTls = resolve.value(QStringLiteral("DNSOverTLS"));

    // Default empty values to sensible defaults for display
    if (m_dnssec.isEmpty()) {
        m_dnssec = QStringLiteral("allow-downgrade");
    }
    if (m_dnsOverTls.isEmpty()) {
        m_dnsOverTls = QStringLiteral("no");
    }

    m_origDnsServers = m_dnsServers;
    m_origFallbackDns = m_fallbackDns;
    m_origDnssec = m_dnssec;
    m_origDnsOverTls = m_dnsOverTls;

    Q_EMIT dnsServersChanged();
    Q_EMIT fallbackDnsChanged();
    Q_EMIT dnssecChanged();
    Q_EMIT dnsOverTlsChanged();
}

// --- Save ---

void HostnameKcm::save()
{
    saveHostnameToDBus();
    saveHostsFile();
    saveResolvedConf();
}

void HostnameKcm::saveHostnameToDBus()
{
    if (m_prettyHostname != m_origPrettyHostname) {
        Fcse::DBusHelper::hostname1Call(
            QStringLiteral("SetPrettyHostname"),
            {m_prettyHostname, true});
        m_origPrettyHostname = m_prettyHostname;
    }

    if (m_staticHostname != m_origStaticHostname) {
        Fcse::DBusHelper::hostname1Call(
            QStringLiteral("SetStaticHostname"),
            {m_staticHostname, true});
        m_origStaticHostname = m_staticHostname;
    }

    if (m_iconName != m_origIconName) {
        Fcse::DBusHelper::hostname1Call(
            QStringLiteral("SetIconName"),
            {m_iconName, true});
        m_origIconName = m_iconName;
    }
}

void HostnameKcm::saveHostsFile()
{
    if (m_hostsContent == m_origHostsContent) {
        return;
    }

    QVariantMap args;
    args[QStringLiteral("filePath")] = QStringLiteral("/etc/hosts");
    args[QStringLiteral("content")] = m_hostsContent.toUtf8();

    KAuth::Action action(QStringLiteral("org.kde.fcse.write"));
    action.setHelperId(QStringLiteral("org.kde.fcse"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "KAuth write /etc/hosts failed:" << job->errorString();
        } else {
            m_origHostsContent = m_hostsContent;
        }
    });
    job->start();
}

void HostnameKcm::saveResolvedConf()
{
    if (m_dnsServers == m_origDnsServers
        && m_fallbackDns == m_origFallbackDns
        && m_dnssec == m_origDnssec
        && m_dnsOverTls == m_origDnsOverTls) {
        return;
    }

    // Read existing config to preserve other sections/keys
    auto sections = Fcse::ConfigParser::parseIni(
        QStringLiteral("/etc/systemd/resolved.conf"));

    auto &resolve = sections[QStringLiteral("Resolve")];
    resolve[QStringLiteral("DNS")] = m_dnsServers;
    resolve[QStringLiteral("FallbackDNS")] = m_fallbackDns;
    resolve[QStringLiteral("DNSSEC")] = m_dnssec;
    resolve[QStringLiteral("DNSOverTLS")] = m_dnsOverTls;

    // Serialize the INI content
    QString content;
    QTextStream out(&content);
    bool first = true;
    for (auto secIt = sections.constBegin(); secIt != sections.constEnd(); ++secIt) {
        if (!first) {
            out << QLatin1Char('\n');
        }
        first = false;
        out << QLatin1Char('[') << secIt.key() << QStringLiteral("]\n");
        const auto &kvMap = secIt.value();
        for (auto kvIt = kvMap.constBegin(); kvIt != kvMap.constEnd(); ++kvIt) {
            out << kvIt.key() << QStringLiteral(" = ") << kvIt.value() << QLatin1Char('\n');
        }
    }

    QVariantMap args;
    args[QStringLiteral("filePath")] = QStringLiteral("/etc/systemd/resolved.conf");
    args[QStringLiteral("content")] = content.toUtf8();
    args[QStringLiteral("postCommand")] = QStringLiteral("/usr/bin/systemctl");
    args[QStringLiteral("postArgs")] = QStringList{QStringLiteral("restart"), QStringLiteral("systemd-resolved")};

    KAuth::Action action(QStringLiteral("org.kde.fcse.write"));
    action.setHelperId(QStringLiteral("org.kde.fcse"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "KAuth write resolved.conf failed:" << job->errorString();
        } else {
            m_origDnsServers = m_dnsServers;
            m_origFallbackDns = m_fallbackDns;
            m_origDnssec = m_dnssec;
            m_origDnsOverTls = m_dnsOverTls;
        }
        setNeedsSave(false);
    });
    job->start();
}

K_PLUGIN_CLASS_WITH_JSON(HostnameKcm, "kcm_fcse_hostname.json")

#include "hostnamekcm.moc"
