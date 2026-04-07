#include "statusnotifier.h"

#include <KNotification>

namespace NsaFsm {

static void notify(const QString &title, const QString &message, const QString &iconName)
{
    auto *notification = new KNotification(QStringLiteral("nsafsm"));
    notification->setTitle(title);
    notification->setText(message);
    notification->setIconName(iconName);
    notification->sendEvent();
}

void StatusNotifier::success(const QString &message)
{
    notify(QStringLiteral("NSA System Manager"), message, QStringLiteral("dialog-positive"));
}

void StatusNotifier::error(const QString &message)
{
    notify(QStringLiteral("NSA System Manager — Error"), message, QStringLiteral("dialog-error"));
}

void StatusNotifier::warning(const QString &message)
{
    notify(QStringLiteral("NSA System Manager — Warning"), message, QStringLiteral("dialog-warning"));
}

void StatusNotifier::backupCreated(const QString &path)
{
    notify(QStringLiteral("Backup Created"),
           QStringLiteral("Configuration backed up to:\n%1").arg(path),
           QStringLiteral("document-save"));
}

} // namespace NsaFsm
