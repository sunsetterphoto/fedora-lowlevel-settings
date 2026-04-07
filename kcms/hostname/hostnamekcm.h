// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>

class HostnameKcm : public KQuickConfigModule
{
    Q_OBJECT

    // Hostname section
    Q_PROPERTY(QString prettyHostname READ prettyHostname WRITE setPrettyHostname NOTIFY prettyHostnameChanged)
    Q_PROPERTY(QString staticHostname READ staticHostname WRITE setStaticHostname NOTIFY staticHostnameChanged)
    Q_PROPERTY(QString iconName READ iconName WRITE setIconName NOTIFY iconNameChanged)
    Q_PROPERTY(QString chassisType READ chassisType NOTIFY chassisTypeChanged)
    Q_PROPERTY(QString machineId READ machineId CONSTANT)

    // Hosts section
    Q_PROPERTY(QString hostsContent READ hostsContent WRITE setHostsContent NOTIFY hostsContentChanged)

    // DNS section
    Q_PROPERTY(QString dnsServers READ dnsServers WRITE setDnsServers NOTIFY dnsServersChanged)
    Q_PROPERTY(QString fallbackDns READ fallbackDns WRITE setFallbackDns NOTIFY fallbackDnsChanged)
    Q_PROPERTY(QString dnssec READ dnssec WRITE setDnssec NOTIFY dnssecChanged)
    Q_PROPERTY(QString dnsOverTls READ dnsOverTls WRITE setDnsOverTls NOTIFY dnsOverTlsChanged)

public:
    explicit HostnameKcm(QObject *parent, const KPluginMetaData &metaData);

    QString prettyHostname() const { return m_prettyHostname; }
    void setPrettyHostname(const QString &value);

    QString staticHostname() const { return m_staticHostname; }
    void setStaticHostname(const QString &value);

    QString iconName() const { return m_iconName; }
    void setIconName(const QString &value);

    QString chassisType() const { return m_chassisType; }
    QString machineId() const { return m_machineId; }

    QString hostsContent() const { return m_hostsContent; }
    void setHostsContent(const QString &value);

    QString dnsServers() const { return m_dnsServers; }
    void setDnsServers(const QString &value);

    QString fallbackDns() const { return m_fallbackDns; }
    void setFallbackDns(const QString &value);

    QString dnssec() const { return m_dnssec; }
    void setDnssec(const QString &value);

    QString dnsOverTls() const { return m_dnsOverTls; }
    void setDnsOverTls(const QString &value);

public Q_SLOTS:
    void save() override;
    void load() override;

Q_SIGNALS:
    void prettyHostnameChanged();
    void staticHostnameChanged();
    void iconNameChanged();
    void chassisTypeChanged();
    void hostsContentChanged();
    void dnsServersChanged();
    void fallbackDnsChanged();
    void dnssecChanged();
    void dnsOverTlsChanged();

private:
    void loadHostnameFromDBus();
    void loadMachineId();
    void loadHostsFile();
    void loadResolvedConf();

    void saveHostnameToDBus();
    void saveHostsFile();
    void saveResolvedConf();

    // Hostname
    QString m_prettyHostname;
    QString m_staticHostname;
    QString m_iconName;
    QString m_chassisType;
    QString m_machineId;

    // Saved originals for change detection
    QString m_origPrettyHostname;
    QString m_origStaticHostname;
    QString m_origIconName;

    // Hosts
    QString m_hostsContent;
    QString m_origHostsContent;

    // DNS
    QString m_dnsServers;
    QString m_fallbackDns;
    QString m_dnssec;
    QString m_dnsOverTls;
    QString m_origDnsServers;
    QString m_origFallbackDns;
    QString m_origDnssec;
    QString m_origDnsOverTls;
};
