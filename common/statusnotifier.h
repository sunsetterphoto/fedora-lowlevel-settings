#pragma once

#include <QString>

namespace NsaFsm {

class StatusNotifier {
public:
    static void success(const QString &message);
    static void error(const QString &message);
    static void warning(const QString &message);
    static void backupCreated(const QString &path);
};

} // namespace NsaFsm
