#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "backupmanager.h"

using namespace Fcse;

class TestBackupManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBackupCreatesFile()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString testFile = tmpDir.path() + QStringLiteral("/testfile.conf");
        QFile f(testFile);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("key = value\n");
        f.close();

        auto result = BackupManager::backup(testFile);
        if (!result) {
            QSKIP("Backup directory not writable (run as root or create /var/lib/fcse/backups/)");
        }

        QVERIFY(QFileInfo::exists(*result));
        QVERIFY(result->contains(QStringLiteral(".bak")));
    }

    void testListBackups()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString testFile = tmpDir.path() + QStringLiteral("/testfile.conf");
        QFile f(testFile);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("original");
        f.close();

        auto result = BackupManager::backup(testFile);
        if (!result) {
            QSKIP("Backup directory not writable");
        }

        auto backups = BackupManager::listBackups(testFile);
        QVERIFY(backups.size() >= 1);
        QCOMPARE(backups.first().originalFile, testFile);
    }

    void testBackupNonexistentFile()
    {
        auto result = BackupManager::backup(QStringLiteral("/nonexistent/file"));
        QVERIFY(!result.has_value());
    }
};

QTEST_MAIN(TestBackupManager)
#include "test_backupmanager.moc"
