#pragma once

#include <QString>

namespace Fcse {

class StatusNotifier {
public:
    static void success(const QString &message);
    static void error(const QString &message);
    static void warning(const QString &message);
    static void backupCreated(const QString &path);
};

} // namespace Fcse
