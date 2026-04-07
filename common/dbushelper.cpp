#include "dbushelper.h"

namespace NsaFsm {

QDBusPendingCall DBusHelper::asyncSystemCall(
    const QString &service, const QString &path, const QString &interface,
    const QString &method, const QVariantList &args, bool interactiveAuth)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(service, path, interface, method);
    msg.setArguments(args);
    msg.setInteractiveAuthorizationAllowed(interactiveAuth);
    return QDBusConnection::systemBus().asyncCall(msg);
}

QDBusMessage DBusHelper::syncSystemCall(
    const QString &service, const QString &path, const QString &interface,
    const QString &method, const QVariantList &args, bool interactiveAuth)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(service, path, interface, method);
    msg.setArguments(args);
    msg.setInteractiveAuthorizationAllowed(interactiveAuth);
    return QDBusConnection::systemBus().call(msg, QDBus::Block, 30000);
}

QDBusPendingCall DBusHelper::systemdCall(const QString &method, const QVariantList &args)
{
    return asyncSystemCall(
        QStringLiteral("org.freedesktop.systemd1"),
        QStringLiteral("/org/freedesktop/systemd1"),
        QStringLiteral("org.freedesktop.systemd1.Manager"),
        method, args);
}

QDBusPendingCall DBusHelper::hostname1Call(const QString &method, const QVariantList &args)
{
    return asyncSystemCall(
        QStringLiteral("org.freedesktop.hostname1"),
        QStringLiteral("/org/freedesktop/hostname1"),
        QStringLiteral("org.freedesktop.hostname1"),
        method, args);
}

} // namespace NsaFsm
