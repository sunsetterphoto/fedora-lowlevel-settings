#pragma once

#include <QString>
#include <QList>
#include <QDateTime>
#include <optional>

namespace Fls {

struct BackupEntry {
    QString path;
    QString originalFile;
    QDateTime timestamp;
};

class BackupManager {
public:
    static constexpr const char *BACKUP_DIR = "/var/lib/fls/backups";
    static constexpr int MAX_BACKUPS_PER_FILE = 10;

    static std::optional<QString> backup(const QString &filePath);
    static std::optional<QString> restore(const QString &backupPath, const QString &originalPath);
    static QList<BackupEntry> listBackups(const QString &originalFilePath);
    static void rotate(const QString &originalFilePath);

private:
    static QString backupFileName(const QString &originalFilePath);
    static bool ensureBackupDir();
};

} // namespace Fls
