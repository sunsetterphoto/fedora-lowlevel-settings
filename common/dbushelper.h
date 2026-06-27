#pragma once

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QVariantList>

namespace Fcse {

class DBusHelper {
public:
    static QDBusPendingCall asyncSystemCall(
        const QString &service,
        const QString &path,
        const QString &interface,
        const QString &method,
        const QVariantList &args = {},
        bool interactiveAuth = true
    );

    static QDBusMessage syncSystemCall(
        const QString &service,
        const QString &path,
        const QString &interface,
        const QString &method,
        const QVariantList &args = {},
        bool interactiveAuth = true
    );

    static QDBusPendingCall systemdCall(const QString &method, const QVariantList &args = {});
    static QDBusPendingCall hostname1Call(const QString &method, const QVariantList &args = {});
};

} // namespace Fcse
