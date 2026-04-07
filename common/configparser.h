#pragma once

#include <QMap>
#include <QList>
#include <QString>
#include <QStringList>

namespace NsaFsm {

struct HostsEntry {
    QString ip;
    QStringList hostnames;
    QString comment;
};

struct FstabEntry {
    QString device;
    QString mountpoint;
    QString fstype;
    QString options;
    int dump = 0;
    int pass = 0;
    QString comment;
};

struct SysctlEntry {
    QString key;
    QString value;
};

class ConfigParser {
public:
    static QList<SysctlEntry> parseSysctl(const QString &filePath);
    static bool writeSysctl(const QString &filePath, const QList<SysctlEntry> &entries);

    static QList<HostsEntry> parseHosts(const QString &filePath);
    static bool writeHosts(const QString &filePath, const QList<HostsEntry> &entries);

    static QList<FstabEntry> parseFstab(const QString &filePath);
    static bool writeFstab(const QString &filePath, const QList<FstabEntry> &entries);

    static QMap<QString, QString> parseKeyValue(const QString &filePath);
    static bool writeKeyValue(const QString &filePath, const QMap<QString, QString> &values);

    static QMap<QString, QMap<QString, QString>> parseIni(const QString &filePath);
    static bool writeIni(const QString &filePath, const QMap<QString, QMap<QString, QString>> &sections);
};

} // namespace NsaFsm
