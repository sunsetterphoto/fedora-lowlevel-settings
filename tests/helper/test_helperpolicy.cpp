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
};

QTEST_MAIN(TestHelperPolicy)
#include "test_helperpolicy.moc"
