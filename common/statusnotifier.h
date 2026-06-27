#pragma once

#include <QString>

namespace Fls {

class StatusNotifier {
public:
    static void success(const QString &message);
    static void error(const QString &message);
    static void warning(const QString &message);
    static void backupCreated(const QString &path);
};

} // namespace Fls
