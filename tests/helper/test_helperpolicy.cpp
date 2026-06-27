#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "helperpolicy.h"

using namespace Fls::Policy;

class TestHelperPolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testWritePathsAllowed()
    {
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/default/grub")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/fstab")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/hosts")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/environment")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/systemd/resolved.conf")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/systemd/zram-generator.conf")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/sysctl.d/99-fls.conf")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/modprobe.d/fls-blacklist.conf")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/sudoers.d/myrule")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/yum.repos.d/copr.repo")));
    }

    void testWritePathsRejected()
    {
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/shadow")));
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/hosts.evil")));          // prefix bypass
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/sysctl.d/../shadow")));  // traversal
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/sysctl.d/")));           // dir itself, not a child
        QVERIFY(!isWritePathAllowed(QStringLiteral("etc/fstab")));               // relative
        QVERIFY(!isWritePathAllowed(QString()));                                  // empty
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/selinux/config")));      // not a write target
    }

    void testReadDirsAllowed()
    {
        QVERIFY(isReadDirAllowed(QStringLiteral("/boot/loader/entries/")));
        QVERIFY(isReadDirAllowed(QStringLiteral("/etc/sudoers.d/")));
        QVERIFY(isReadDirAllowed(QStringLiteral("/etc/sudoers.d")));   // trailing slash optional
    }

    void testReadRejected()
    {
        QVERIFY(!isReadDirAllowed(QStringLiteral("/etc")));
        QVERIFY(!isReadDirAllowed(QStringLiteral("/root")));
        QVERIFY(!isReadFileAllowed(QStringLiteral("/etc/shadow")));
        QVERIFY(!isReadFileAllowed(QStringLiteral("/etc/passwd")));
        QVERIFY(isReadFileAllowed(QStringLiteral("/etc/sudoers.d/myrule"))); // direct child OK
    }

    void testCanonicalizeResolvesSymlinks()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString real = tmp.path() + QStringLiteral("/realdir");
        QVERIFY(QDir().mkpath(real));
        const QString link = tmp.path() + QStringLiteral("/link");
        QVERIFY(QFile::link(real, link)); // link -> realdir
        const QString canon = canonicalize(link + QStringLiteral("/file.conf"));
        QCOMPARE(canon, real + QStringLiteral("/file.conf"));
    }

    void testCanonicalizeRejectsNonexistentParent()
    {
        QVERIFY(canonicalize(QStringLiteral("/nonexistent-xyz-12345/foo")).isEmpty());
    }

    void testExecAllowed()
    {
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/grubby"),
            {QStringLiteral("--set-default"), QStringLiteral("/boot/vmlinuz-6.14.0-1.fc44.x86_64")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("copr"), QStringLiteral("enable"), QStringLiteral("user/project"), QStringLiteral("-y")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("copr"), QStringLiteral("disable"), QStringLiteral("user/project"), QStringLiteral("-y")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/bin/dnf5"), {QStringLiteral("makecache")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/modprobe"), {QStringLiteral("kvm_intel")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/rmmod"), {QStringLiteral("pcspkr")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/setenforce"), {QStringLiteral("0")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/setenforce"), {QStringLiteral("1")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/setsebool"),
            {QStringLiteral("-P"), QStringLiteral("httpd_can_network_connect"), QStringLiteral("on")}));
    }

    void testExecRejected()
    {
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("install"), QStringLiteral("evil")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("copr"), QStringLiteral("enable"), QStringLiteral("a; rm -rf /"), QStringLiteral("-y")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/modprobe"), {QStringLiteral("../evil")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/rmmod"),
            {QStringLiteral("a"), QStringLiteral("b")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/setenforce"), {QStringLiteral("2")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/setsebool"),
            {QStringLiteral("httpd_can_network_connect"), QStringLiteral("on")})); // missing -P
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/bin/systemctl"),
            {QStringLiteral("link"), QStringLiteral("/x")}));                      // systemctl not in execute set
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/grubby"),
            {QStringLiteral("--update-kernel=ALL")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("evil")}));
    }

    void testExecRejectsTrailingNewline()
    {
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/modprobe"),
            {QStringLiteral("kvm_intel\n")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/rmmod"),
            {QStringLiteral("pcspkr\n")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("copr"), QStringLiteral("enable"), QStringLiteral("user/project\n"), QStringLiteral("-y")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/grubby"),
            {QStringLiteral("--set-default"), QStringLiteral("/boot/vmlinuz-6.14.0\n")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/setsebool"),
            {QStringLiteral("-P"), QStringLiteral("httpd_can_network_connect\n"), QStringLiteral("on")}));
    }

    void testExecRejectsLeadingDash()
    {
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/modprobe"), {QStringLiteral("-r")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/rmmod"), {QStringLiteral("--all")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/setsebool"),
            {QStringLiteral("-P"), QStringLiteral("-X"), QStringLiteral("on")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("copr"), QStringLiteral("enable"), QStringLiteral("--hub=evil"), QStringLiteral("-y")}));
        // Internal dash is still legitimate:
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/modprobe"), {QStringLiteral("snd-hda-intel")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("copr"), QStringLiteral("enable"), QStringLiteral("user/my-project"), QStringLiteral("-y")}));
    }

    void testPostCommandFor()
    {
        const auto grub = postCommandFor(QStringLiteral("/etc/default/grub"));
        QVERIFY(grub.has_value());
        QCOMPARE(grub->binary, QStringLiteral("/usr/sbin/grub2-mkconfig"));
        QCOMPARE(grub->args, (QStringList{QStringLiteral("-o"), QStringLiteral("/boot/grub2/grub.cfg")}));

        const auto fstab = postCommandFor(QStringLiteral("/etc/fstab"));
        QVERIFY(fstab.has_value());
        QCOMPARE(fstab->binary, QStringLiteral("/usr/bin/systemctl"));
        QCOMPARE(fstab->args, (QStringList{QStringLiteral("daemon-reload")}));

        const auto resolved = postCommandFor(QStringLiteral("/etc/systemd/resolved.conf"));
        QVERIFY(resolved.has_value());
        QCOMPARE(resolved->args, (QStringList{QStringLiteral("restart"), QStringLiteral("systemd-resolved")}));

        const auto sysctl = postCommandFor(QStringLiteral("/etc/sysctl.d/99-fls.conf"));
        QVERIFY(sysctl.has_value());
        QCOMPARE(sysctl->binary, QStringLiteral("/usr/sbin/sysctl"));
        QCOMPARE(sysctl->args, (QStringList{QStringLiteral("--system")}));

        QVERIFY(!postCommandFor(QStringLiteral("/etc/hosts")).has_value());
    }
};

QTEST_MAIN(TestHelperPolicy)
#include "test_helperpolicy.moc"
