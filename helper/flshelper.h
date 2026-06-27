#pragma once

#include <KAuth/ActionReply>
#include <KAuth/HelperSupport>
#include <QObject>

using namespace KAuth;

class FlsHelper : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    ActionReply write(const QVariantMap &args);
    ActionReply read(const QVariantMap &args);
    ActionReply execute(const QVariantMap &args);
    ActionReply backup(const QVariantMap &args);
    ActionReply restore(const QVariantMap &args);

private:
    ActionReply writeFileWithBackup(const QString &filePath, const QByteArray &content);
    ActionReply runProcess(const QString &command, const QStringList &args);
    ActionReply runCommand(const QString &command, const QStringList &args);
    bool validateSudoers(const QString &tempPath);
};
