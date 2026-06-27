#include "statusnotifier.h"

#include <KNotification>

namespace Fls {

static void notify(const QString &title, const QString &message, const QString &iconName)
{
    auto *notification = new KNotification(QStringLiteral("fls"));
    notification->setTitle(title);
    notification->setText(message);
    notification->setIconName(iconName);
    notification->sendEvent();
}

void StatusNotifier::success(const QString &message)
{
    notify(QStringLiteral("Fedora Lowlevel Settings"), message, QStringLiteral("dialog-positive"));
}

void StatusNotifier::error(const QString &message)
{
    notify(QStringLiteral("Fedora Lowlevel Settings — Error"), message, QStringLiteral("dialog-error"));
}

void StatusNotifier::warning(const QString &message)
{
    notify(QStringLiteral("Fedora Lowlevel Settings — Warning"), message, QStringLiteral("dialog-warning"));
}

void StatusNotifier::backupCreated(const QString &path)
{
    notify(QStringLiteral("Backup Created"),
           QStringLiteral("Configuration backed up to:\n%1").arg(path),
           QStringLiteral("document-save"));
}

} // namespace Fls
