#include "flshelper.h"
#include "backupmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>

using namespace Fls;

// ---- Path / command whitelists ----

static const QStringList allowedPathPrefixes = {
    QStringLiteral("/etc/sysctl.d/"),
    QStringLiteral("/etc/default/grub"),
    QStringLiteral("/etc/fstab"),
    QStringLiteral("/etc/modprobe.d/"),
    QStringLiteral("/etc/cron.d/"),
    QStringLiteral("/etc/sudoers.d/"),
    QStringLiteral("/etc/selinux/config"),
    QStringLiteral("/etc/yum.repos.d/"),
    QStringLiteral("/etc/hosts"),
    QStringLiteral("/etc/environment"),
    QStringLiteral("/etc/profile.d/"),
    QStringLiteral("/etc/systemd/resolved.conf"),
    QStringLiteral("/etc/systemd/zram-generator.conf"),
    QStringLiteral("/boot/loader/entries/"),
};

static const QStringList allowedCommands = {
    QStringLiteral("/usr/sbin/sysctl"),
    QStringLiteral("/usr/sbin/grub2-mkconfig"),
    QStringLiteral("/usr/sbin/grubby"),
    QStringLiteral("/usr/sbin/modprobe"),
    QStringLiteral("/usr/sbin/rmmod"),
    QStringLiteral("/usr/sbin/depmod"),
    QStringLiteral("/usr/sbin/setenforce"),
    QStringLiteral("/usr/sbin/setsebool"),
    QStringLiteral("/usr/bin/systemctl"),
    QStringLiteral("/usr/bin/dnf5"),
    QStringLiteral("/usr/bin/findmnt"),
};

static bool isPathAllowed(const QString &path)
{
    const QString canonical = QFileInfo(path).absoluteFilePath();
    for (const auto &prefix : allowedPathPrefixes) {
        if (canonical.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

static bool isCommandAllowed(const QString &command)
{
    return allowedCommands.contains(command);
}

// ---- Private helpers ----

bool FlsHelper::validateSudoers(const QString &tempPath)
{
    QProcess proc;
    proc.start(QStringLiteral("/usr/sbin/visudo"), {QStringLiteral("-c"), QStringLiteral("-f"), tempPath});
    proc.waitForFinished(10000);
    return proc.exitCode() == 0;
}

ActionReply FlsHelper::writeFileWithBackup(const QString &filePath, const QByteArray &content)
{
    // Back up existing file (if it exists)
    QString backupPath;
    if (QFileInfo::exists(filePath)) {
        auto result = BackupManager::backup(filePath);
        if (!result) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(QStringLiteral("Failed to create backup of %1").arg(filePath));
            return reply;
        }
        backupPath = result.value();
    }

    // Write new content
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to open %1 for writing: %2").arg(filePath, file.errorString()));
        return reply;
    }
    file.write(content);
    file.close();

    auto reply = ActionReply::SuccessReply();
    if (!backupPath.isEmpty()) {
        reply.addData(QStringLiteral("backupPath"), backupPath);
    }
    return reply;
}

ActionReply FlsHelper::runCommand(const QString &command, const QStringList &args)
{
    if (!isCommandAllowed(command)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Command not in whitelist: %1").arg(command));
        return reply;
    }

    QProcess proc;
    proc.start(command, args);
    if (!proc.waitForFinished(30000)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Command timed out: %1").arg(command));
        return reply;
    }

    auto reply = ActionReply::SuccessReply();
    reply.addData(QStringLiteral("stdout"), proc.readAllStandardOutput());
    reply.addData(QStringLiteral("stderr"), proc.readAllStandardError());
    reply.addData(QStringLiteral("exitCode"), proc.exitCode());
    return reply;
}

// ---- Public action slots ----

ActionReply FlsHelper::write(const QVariantMap &args)
{
    const QString filePath = args.value(QStringLiteral("filePath")).toString();
    const QByteArray content = args.value(QStringLiteral("content")).toByteArray();
    const QString postCommand = args.value(QStringLiteral("postCommand")).toString();
    const QStringList postArgs = args.value(QStringLiteral("postArgs")).toStringList();
    const bool validateAsSudoers = args.value(QStringLiteral("validateAsSudoers"), false).toBool();

    if (filePath.isEmpty() || content.isEmpty()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("filePath and content are required"));
        return reply;
    }

    if (!isPathAllowed(filePath)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Path not in whitelist: %1").arg(filePath));
        return reply;
    }

    // Sudoers validation: write to temp, validate, reject on failure
    if (validateAsSudoers) {
        QTemporaryFile tmp;
        if (!tmp.open()) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(QStringLiteral("Failed to create temporary file for sudoers validation"));
            return reply;
        }
        tmp.write(content);
        tmp.flush();

        if (!validateSudoers(tmp.fileName())) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(QStringLiteral("Sudoers syntax validation failed"));
            return reply;
        }
    }

    // Write with automatic backup
    ActionReply writeReply = writeFileWithBackup(filePath, content);
    if (writeReply.failed()) {
        return writeReply;
    }

    // Run post-command if specified
    if (!postCommand.isEmpty()) {
        ActionReply cmdReply = runCommand(postCommand, postArgs);
        if (cmdReply.failed() || cmdReply.data().value(QStringLiteral("exitCode")).toInt() != 0) {
            // Post-command failed -- restore from backup
            const QString backupPath = writeReply.data().value(QStringLiteral("backupPath")).toString();
            if (!backupPath.isEmpty()) {
                BackupManager::restore(backupPath, filePath);
            }

            auto reply = ActionReply::HelperErrorReply();
            const QString stderr = cmdReply.data().value(QStringLiteral("stderr")).toString();
            reply.setErrorDescription(
                QStringLiteral("Post-command failed (%1), restored backup. stderr: %2")
                    .arg(postCommand, stderr));
            return reply;
        }

        // Merge post-command output into the success reply
        writeReply.addData(QStringLiteral("postStdout"), cmdReply.data().value(QStringLiteral("stdout")));
        writeReply.addData(QStringLiteral("postStderr"), cmdReply.data().value(QStringLiteral("stderr")));
    }

    return writeReply;
}

ActionReply FlsHelper::read(const QVariantMap &args)
{
    const QString filePath = args.value(QStringLiteral("filePath")).toString();
    const QString dirPath = args.value(QStringLiteral("dirPath")).toString();

    if (filePath.isEmpty() && dirPath.isEmpty()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Either filePath or dirPath is required"));
        return reply;
    }

    // Single file read
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(QStringLiteral("Failed to read %1: %2").arg(filePath, file.errorString()));
            return reply;
        }

        auto reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("content"), file.readAll());
        return reply;
    }

    // Directory read -- return map of filename -> content
    QDir dir(dirPath);
    if (!dir.exists()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Directory does not exist: %1").arg(dirPath));
        return reply;
    }

    QVariantMap filesMap;
    const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        QFile f(entry.absoluteFilePath());
        if (f.open(QIODevice::ReadOnly)) {
            filesMap.insert(entry.fileName(), f.readAll());
        }
    }

    auto reply = ActionReply::SuccessReply();
    reply.addData(QStringLiteral("files"), filesMap);
    return reply;
}

ActionReply FlsHelper::execute(const QVariantMap &args)
{
    const QString command = args.value(QStringLiteral("command")).toString();
    const QStringList cmdArgs = args.value(QStringLiteral("args")).toStringList();

    if (command.isEmpty()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("command is required"));
        return reply;
    }

    return runCommand(command, cmdArgs);
}

ActionReply FlsHelper::backup(const QVariantMap &args)
{
    const QString filePath = args.value(QStringLiteral("filePath")).toString();

    if (filePath.isEmpty()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("filePath is required"));
        return reply;
    }

    auto result = BackupManager::backup(filePath);
    if (!result) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to create backup of %1").arg(filePath));
        return reply;
    }

    auto reply = ActionReply::SuccessReply();
    reply.addData(QStringLiteral("backupPath"), result.value());
    return reply;
}

ActionReply FlsHelper::restore(const QVariantMap &args)
{
    const QString backupPath = args.value(QStringLiteral("backupPath")).toString();
    const QString originalPath = args.value(QStringLiteral("originalPath")).toString();

    if (backupPath.isEmpty() || originalPath.isEmpty()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("backupPath and originalPath are required"));
        return reply;
    }

    auto result = BackupManager::restore(backupPath, originalPath);
    if (!result) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to restore %1 from %2").arg(originalPath, backupPath));
        return reply;
    }

    return ActionReply::SuccessReply();
}

KAUTH_HELPER_MAIN("org.kde.fls", FlsHelper)
