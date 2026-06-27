#include "backupmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Fls {

bool BackupManager::ensureBackupDir()
{
    QDir dir{QLatin1String(BACKUP_DIR)};
    if (!dir.exists()) {
        return dir.mkpath(QStringLiteral("."));
    }
    return true;
}

QString BackupManager::backupFileName(const QString &originalFilePath)
{
    QString sanitized = originalFilePath;
    sanitized.replace(QLatin1Char('/'), QLatin1Char('_'));
    if (sanitized.startsWith(QLatin1Char('_'))) {
        sanitized = sanitized.mid(1);
    }
    return sanitized;
}

std::optional<QString> BackupManager::backup(const QString &filePath)
{
    if (!ensureBackupDir()) {
        return std::nullopt;
    }

    QFileInfo sourceInfo(filePath);
    if (!sourceInfo.exists()) {
        return std::nullopt;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH-mm-ss"));
    const QString baseName = backupFileName(filePath);
    const QString backupPath = QStringLiteral("%1/%2.%3.bak")
        .arg(QLatin1String(BACKUP_DIR), baseName, timestamp);

    if (!QFile::copy(filePath, backupPath)) {
        return std::nullopt;
    }

    rotate(filePath);
    return backupPath;
}

std::optional<QString> BackupManager::restore(const QString &backupPath, const QString &originalPath)
{
    QFileInfo backupInfo(backupPath);
    if (!backupInfo.exists()) {
        return std::nullopt;
    }

    if (QFileInfo::exists(originalPath)) {
        auto currentBackup = backup(originalPath);
        if (!currentBackup) {
            return std::nullopt;
        }
    }

    QFile::remove(originalPath);
    if (!QFile::copy(backupPath, originalPath)) {
        return std::nullopt;
    }

    return originalPath;
}

QList<BackupEntry> BackupManager::listBackups(const QString &originalFilePath)
{
    QList<BackupEntry> entries;
    const QString baseName = backupFileName(originalFilePath);
    QDir dir{QLatin1String(BACKUP_DIR)};

    const auto files = dir.entryInfoList(
        {baseName + QStringLiteral(".*.bak")},
        QDir::Files,
        QDir::Time
    );

    for (const auto &fi : files) {
        const QString name = fi.fileName();
        // Format: <baseName>.<timestamp>.bak
        // Find ".bak" at the end, then the timestamp before it
        if (!name.endsWith(QLatin1String(".bak"))) continue;

        const int bakDot = name.length() - 4; // position of last '.'
        const int tsDot = name.lastIndexOf(QLatin1Char('.'), bakDot - 1);
        if (tsDot < 0) continue;

        const QString tsStr = name.mid(tsDot + 1, bakDot - tsDot - 1);
        const QDateTime ts = QDateTime::fromString(tsStr, QStringLiteral("yyyy-MM-ddTHH-mm-ss"));
        if (!ts.isValid()) continue;

        entries.append(BackupEntry{fi.absoluteFilePath(), originalFilePath, ts});
    }

    return entries;
}

void BackupManager::rotate(const QString &originalFilePath)
{
    auto entries = listBackups(originalFilePath);
    while (entries.size() > MAX_BACKUPS_PER_FILE) {
        QFile::remove(entries.takeLast().path);
    }
}

} // namespace Fls
