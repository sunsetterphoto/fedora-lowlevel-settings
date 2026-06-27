#include <QTest>
#include <QTemporaryFile>
#include <QTextStream>
#include "configparser.h"

using namespace Fcse;

class TestConfigParser : public QObject
{
    Q_OBJECT

private:
    static bool writeTemp(QTemporaryFile &tmp, const QString &content)
    {
        tmp.setAutoRemove(true);
        if (!tmp.open()) {
            return false;
        }
        QTextStream out(&tmp);
        out << content;
        out.flush();
        tmp.close();
        return true;
    }

private Q_SLOTS:
    void testParseSysctl()
    {
        QTemporaryFile tmp;
        QVERIFY(writeTemp(tmp,
            QStringLiteral("# Sysctl config\n"
                           "vm.swappiness = 60\n"
                           "\n"
                           "net.ipv4.ip_forward = 1\n")));

        auto entries = ConfigParser::parseSysctl(tmp.fileName());
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).key, QStringLiteral("vm.swappiness"));
        QCOMPARE(entries.at(0).value, QStringLiteral("60"));
        QCOMPARE(entries.at(1).key, QStringLiteral("net.ipv4.ip_forward"));
        QCOMPARE(entries.at(1).value, QStringLiteral("1"));
    }

    void testParseHosts()
    {
        QTemporaryFile tmp;
        QVERIFY(writeTemp(tmp,
            QStringLiteral("# /etc/hosts\n"
                           "127.0.0.1\tlocalhost\n"
                           "::1\tlocalhost ip6-localhost\n")));

        auto entries = ConfigParser::parseHosts(tmp.fileName());
        QCOMPARE(entries.size(), 3);

        // Comment-only line
        QVERIFY(entries.at(0).ip.isEmpty());
        QCOMPARE(entries.at(0).comment, QStringLiteral("# /etc/hosts"));

        // IPv4 entry
        QCOMPARE(entries.at(1).ip, QStringLiteral("127.0.0.1"));
        QCOMPARE(entries.at(1).hostnames.size(), 1);
        QCOMPARE(entries.at(1).hostnames.at(0), QStringLiteral("localhost"));

        // IPv6 entry with alias
        QCOMPARE(entries.at(2).ip, QStringLiteral("::1"));
        QCOMPARE(entries.at(2).hostnames.size(), 2);
        QCOMPARE(entries.at(2).hostnames.at(0), QStringLiteral("localhost"));
        QCOMPARE(entries.at(2).hostnames.at(1), QStringLiteral("ip6-localhost"));
    }

    void testParseFstab()
    {
        QTemporaryFile tmp;
        QVERIFY(writeTemp(tmp,
            QStringLiteral("# /etc/fstab\n"
                           "UUID=abcd-1234\t/\text4\tdefaults\t1\t1\n"
                           "/dev/sda2\t/home\text4\tdefaults,noatime\t0\t2\n")));

        auto entries = ConfigParser::parseFstab(tmp.fileName());
        QCOMPARE(entries.size(), 3);

        // Comment line
        QCOMPARE(entries.at(0).comment, QStringLiteral("# /etc/fstab"));
        QVERIFY(entries.at(0).device.isEmpty());

        // First mount
        QCOMPARE(entries.at(1).device, QStringLiteral("UUID=abcd-1234"));
        QCOMPARE(entries.at(1).mountpoint, QStringLiteral("/"));
        QCOMPARE(entries.at(1).fstype, QStringLiteral("ext4"));
        QCOMPARE(entries.at(1).options, QStringLiteral("defaults"));
        QCOMPARE(entries.at(1).dump, 1);
        QCOMPARE(entries.at(1).pass, 1);

        // Second mount
        QCOMPARE(entries.at(2).device, QStringLiteral("/dev/sda2"));
        QCOMPARE(entries.at(2).mountpoint, QStringLiteral("/home"));
        QCOMPARE(entries.at(2).fstype, QStringLiteral("ext4"));
        QCOMPARE(entries.at(2).options, QStringLiteral("defaults,noatime"));
        QCOMPARE(entries.at(2).dump, 0);
        QCOMPARE(entries.at(2).pass, 2);
    }

    void testParseKeyValue()
    {
        QTemporaryFile tmp;
        QVERIFY(writeTemp(tmp,
            QStringLiteral("# GRUB config\n"
                           "GRUB_TIMEOUT=5\n"
                           "GRUB_CMDLINE_LINUX=\"quiet splash\"\n"
                           "SINGLE=value\n")));

        auto values = ConfigParser::parseKeyValue(tmp.fileName());
        QCOMPARE(values.size(), 3);
        QCOMPARE(values.value(QStringLiteral("GRUB_TIMEOUT")), QStringLiteral("5"));
        QCOMPARE(values.value(QStringLiteral("GRUB_CMDLINE_LINUX")), QStringLiteral("quiet splash"));
        QCOMPARE(values.value(QStringLiteral("SINGLE")), QStringLiteral("value"));
    }

    void testParseKeyValueSingleQuotes()
    {
        QTemporaryFile tmp;
        QVERIFY(writeTemp(tmp,
            QStringLiteral("KEY='single quoted value'\n")));

        auto values = ConfigParser::parseKeyValue(tmp.fileName());
        QCOMPARE(values.size(), 1);
        QCOMPARE(values.value(QStringLiteral("KEY")), QStringLiteral("single quoted value"));
    }

    void testParseIni()
    {
        QTemporaryFile tmp;
        QVERIFY(writeTemp(tmp,
            QStringLiteral("[section]\n"
                           "key = value\n"
                           "# comment line\n"
                           "another = test\n"
                           "\n"
                           "[other]\n"
                           "; semicolon comment\n"
                           "foo = bar\n")));

        auto sections = ConfigParser::parseIni(tmp.fileName());
        QCOMPARE(sections.size(), 2);
        QVERIFY(sections.contains(QStringLiteral("section")));
        QVERIFY(sections.contains(QStringLiteral("other")));
        QCOMPARE(sections[QStringLiteral("section")].value(QStringLiteral("key")), QStringLiteral("value"));
        QCOMPARE(sections[QStringLiteral("section")].value(QStringLiteral("another")), QStringLiteral("test"));
        QCOMPARE(sections[QStringLiteral("other")].value(QStringLiteral("foo")), QStringLiteral("bar"));
    }

    void testRoundTripSysctl()
    {
        // Write entries to a temp file, read them back, verify identical
        QList<SysctlEntry> original;
        original.append({QStringLiteral("vm.swappiness"), QStringLiteral("10")});
        original.append({QStringLiteral("net.core.somaxconn"), QStringLiteral("4096")});
        original.append({QStringLiteral("kernel.panic"), QStringLiteral("60")});

        QTemporaryFile tmp;
        tmp.setAutoRemove(true);
        QVERIFY(tmp.open());
        tmp.close();

        QVERIFY(ConfigParser::writeSysctl(tmp.fileName(), original));

        auto readBack = ConfigParser::parseSysctl(tmp.fileName());
        QCOMPARE(readBack.size(), original.size());
        for (int i = 0; i < original.size(); ++i) {
            QCOMPARE(readBack.at(i).key, original.at(i).key);
            QCOMPARE(readBack.at(i).value, original.at(i).value);
        }
    }

    void testRoundTripHosts()
    {
        QList<HostsEntry> original;
        original.append({QString(), {}, QStringLiteral("# Managed by Fedora Core Setting Extension")});
        original.append({QStringLiteral("127.0.0.1"), {QStringLiteral("localhost")}, QString()});
        original.append({QStringLiteral("::1"), {QStringLiteral("localhost"), QStringLiteral("ip6-localhost")}, QStringLiteral("# IPv6")});

        QTemporaryFile tmp;
        tmp.setAutoRemove(true);
        QVERIFY(tmp.open());
        tmp.close();

        QVERIFY(ConfigParser::writeHosts(tmp.fileName(), original));

        auto readBack = ConfigParser::parseHosts(tmp.fileName());
        QCOMPARE(readBack.size(), original.size());
        QCOMPARE(readBack.at(0).comment, original.at(0).comment);
        QCOMPARE(readBack.at(1).ip, original.at(1).ip);
        QCOMPARE(readBack.at(1).hostnames, original.at(1).hostnames);
        QCOMPARE(readBack.at(2).ip, original.at(2).ip);
        QCOMPARE(readBack.at(2).hostnames, original.at(2).hostnames);
        QCOMPARE(readBack.at(2).comment, original.at(2).comment);
    }

    void testRoundTripKeyValue()
    {
        QMap<QString, QString> original;
        original.insert(QStringLiteral("GRUB_TIMEOUT"), QStringLiteral("5"));
        original.insert(QStringLiteral("GRUB_CMDLINE_LINUX"), QStringLiteral("quiet splash"));

        QTemporaryFile tmp;
        tmp.setAutoRemove(true);
        QVERIFY(tmp.open());
        tmp.close();

        QVERIFY(ConfigParser::writeKeyValue(tmp.fileName(), original));

        auto readBack = ConfigParser::parseKeyValue(tmp.fileName());
        QCOMPARE(readBack.size(), original.size());
        QCOMPARE(readBack.value(QStringLiteral("GRUB_TIMEOUT")), QStringLiteral("5"));
        // "quiet splash" has a space, so it should be quoted on write and stripped on read
        QCOMPARE(readBack.value(QStringLiteral("GRUB_CMDLINE_LINUX")), QStringLiteral("quiet splash"));
    }

    void testRoundTripIni()
    {
        QMap<QString, QMap<QString, QString>> original;
        original[QStringLiteral("zram0")].insert(QStringLiteral("zram-size"), QStringLiteral("ram / 2"));
        original[QStringLiteral("zram0")].insert(QStringLiteral("compression-algorithm"), QStringLiteral("zstd"));

        QTemporaryFile tmp;
        tmp.setAutoRemove(true);
        QVERIFY(tmp.open());
        tmp.close();

        QVERIFY(ConfigParser::writeIni(tmp.fileName(), original));

        auto readBack = ConfigParser::parseIni(tmp.fileName());
        QCOMPARE(readBack.size(), 1);
        QVERIFY(readBack.contains(QStringLiteral("zram0")));
        QCOMPARE(readBack[QStringLiteral("zram0")].value(QStringLiteral("zram-size")), QStringLiteral("ram / 2"));
        QCOMPARE(readBack[QStringLiteral("zram0")].value(QStringLiteral("compression-algorithm")), QStringLiteral("zstd"));
    }

    void testWriteKeyValueHeader()
    {
        QMap<QString, QString> values;
        values.insert(QStringLiteral("KEY"), QStringLiteral("val"));

        QTemporaryFile tmp;
        tmp.setAutoRemove(true);
        QVERIFY(tmp.open());
        tmp.close();

        QVERIFY(ConfigParser::writeKeyValue(tmp.fileName(), values));

        QFile file(tmp.fileName());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QTextStream(&file).readAll();
        QVERIFY(content.startsWith(QStringLiteral("# Generated by Fedora Core Setting Extension")));
    }

    void testParseNonexistentFile()
    {
        auto sysctl = ConfigParser::parseSysctl(QStringLiteral("/nonexistent/path"));
        QVERIFY(sysctl.isEmpty());

        auto hosts = ConfigParser::parseHosts(QStringLiteral("/nonexistent/path"));
        QVERIFY(hosts.isEmpty());

        auto fstab = ConfigParser::parseFstab(QStringLiteral("/nonexistent/path"));
        QVERIFY(fstab.isEmpty());

        auto kv = ConfigParser::parseKeyValue(QStringLiteral("/nonexistent/path"));
        QVERIFY(kv.isEmpty());

        auto ini = ConfigParser::parseIni(QStringLiteral("/nonexistent/path"));
        QVERIFY(ini.isEmpty());
    }
};

QTEST_MAIN(TestConfigParser)
#include "test_configparser.moc"
