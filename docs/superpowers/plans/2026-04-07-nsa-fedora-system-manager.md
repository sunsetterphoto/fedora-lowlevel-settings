# NSA Fedora System Manager Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build 12 KDE System Settings KCM modules for comprehensive Fedora system administration, appearing under a custom "System-Administration" category.

**Architecture:** Monorepo with shared library (libnsa-fsm-common), single KAuth helper (nsa-fsm-helper) for privileged file operations, and 12 independent QML-based KCM plugins. D-Bus direct calls for services with own polkit integration, KAuth for /etc/ file writes.

**Tech Stack:** C++20, Qt 6.10, KDE Frameworks 6.24, QML/Kirigami, CMake, KAuth/polkit, D-Bus, sd-journal API

**Spec:** `docs/superpowers/specs/2026-04-07-nsa-fedora-system-manager-design.md`

---

## Task 1: Install Build Dependencies

**Files:** None (system setup)

- [ ] **Step 1: Install all required -devel packages**

```bash
sudo dnf5 install \
  extra-cmake-modules \
  kf6-kcmutils-devel \
  kf6-kauth-devel \
  kf6-ki18n-devel \
  kf6-kcoreaddons-devel \
  kf6-kconfig-devel \
  kf6-kconfigwidgets-devel \
  kf6-kservice-devel \
  kf6-kirigami-devel \
  kf6-kdbusaddons-devel \
  kf6-knotifications-devel \
  qt6-qtbase-devel \
  qt6-qtdeclarative-devel \
  systemd-devel \
  cmake \
  gcc-c++
```

Expected: All packages install successfully.

- [ ] **Step 2: Verify cmake can find KF6**

```bash
cd ~/nsa-fedora-system-manager
cmake -B build-test -DCMAKE_INSTALL_PREFIX=/usr 2>&1 | head -5
rm -rf build-test
```

Expected: cmake runs (will fail on missing CMakeLists, that's fine -- we're just checking cmake itself works).

---

## Task 2: Project Skeleton + Top-Level CMake

**Files:**
- Create: `CMakeLists.txt`
- Create: `.gitignore`

- [ ] **Step 1: Create top-level CMakeLists.txt**

```cmake
# ~/nsa-fedora-system-manager/CMakeLists.txt
cmake_minimum_required(VERSION 3.22)
project(nsa-fedora-system-manager VERSION 0.1.0)

set(QT_MIN_VERSION "6.8.0")
set(KF6_MIN_VERSION "6.22.0")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core Quick DBus
)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons I18n Auth KCMUtils Config Service DBusAddons Notifications
)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

add_subdirectory(common)
add_subdirectory(helper)
add_subdirectory(kcms)

# Install category desktop file
install(FILES categories/nsa-system-administration.desktop
        DESTINATION ${KDE_INSTALL_DATADIR}/systemsettings/categories)

# Backup directory creation
install(DIRECTORY DESTINATION /var/lib/nsa-fsm/backups)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
```

- [ ] **Step 2: Create .gitignore**

```gitignore
# ~/nsa-fedora-system-manager/.gitignore
build/
build-*/
.cache/
CMakeLists.txt.user
*.autosave
compile_commands.json
```

- [ ] **Step 3: Create category desktop file**

```ini
# ~/nsa-fedora-system-manager/categories/nsa-system-administration.desktop
[Desktop Entry]
Type=Service
X-KDE-System-Settings-Category=nsa-system-administration
X-KDE-System-Settings-Parent-Category=
X-KDE-Weight=100
Icon=preferences-system

Name=System Administration
Name[de]=System-Administration
```

- [ ] **Step 4: Create stub CMakeLists for subdirectories**

```cmake
# ~/nsa-fedora-system-manager/common/CMakeLists.txt
# Placeholder - will be populated in Task 3
```

```cmake
# ~/nsa-fedora-system-manager/helper/CMakeLists.txt
# Placeholder - will be populated in Task 5
```

```cmake
# ~/nsa-fedora-system-manager/kcms/CMakeLists.txt
# Populated as modules are added
```

- [ ] **Step 5: Verify build system**

```bash
cd ~/nsa-fedora-system-manager
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
```

Expected: cmake configures successfully with no errors.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: project skeleton with CMake build system and category registration"
```

---

## Task 3: Shared Library - BackupManager

**Files:**
- Create: `common/CMakeLists.txt`
- Create: `common/backupmanager.h`
- Create: `common/backupmanager.cpp`
- Create: `tests/common/test_backupmanager.cpp`
- Create: `tests/CMakeLists.txt`

- [ ] **Step 1: Write CMakeLists for common library**

```cmake
# ~/nsa-fedora-system-manager/common/CMakeLists.txt
add_library(nsa-fsm-common STATIC
    backupmanager.cpp
    backupmanager.h
    dbushelper.cpp
    dbushelper.h
    configparser.cpp
    configparser.h
    statusnotifier.cpp
    statusnotifier.h
)

target_link_libraries(nsa-fsm-common PUBLIC
    Qt6::Core
    Qt6::DBus
    KF6::CoreAddons
    KF6::Notifications
)

target_include_directories(nsa-fsm-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
```

- [ ] **Step 2: Write BackupManager header**

```cpp
// ~/nsa-fedora-system-manager/common/backupmanager.h
#pragma once

#include <QString>
#include <QList>
#include <QDateTime>
#include <optional>

namespace NsaFsm {

struct BackupEntry {
    QString path;
    QString originalFile;
    QDateTime timestamp;
};

class BackupManager {
public:
    static constexpr const char *BACKUP_DIR = "/var/lib/nsa-fsm/backups";
    static constexpr int MAX_BACKUPS_PER_FILE = 10;

    // Create backup of filePath, returns backup path or error string
    static std::optional<QString> backup(const QString &filePath);

    // Restore backup to original location (backs up current state first)
    static std::optional<QString> restore(const QString &backupPath, const QString &originalPath);

    // List all backups for a given original file path, newest first
    static QList<BackupEntry> listBackups(const QString &originalFilePath);

    // Remove old backups beyond MAX_BACKUPS_PER_FILE
    static void rotate(const QString &originalFilePath);

private:
    static QString backupFileName(const QString &originalFilePath);
    static bool ensureBackupDir();
};

} // namespace NsaFsm
```

- [ ] **Step 3: Write BackupManager implementation**

```cpp
// ~/nsa-fedora-system-manager/common/backupmanager.cpp
#include "backupmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace NsaFsm {

bool BackupManager::ensureBackupDir()
{
    QDir dir(BACKUP_DIR);
    if (!dir.exists()) {
        return dir.mkpath(QStringLiteral("."));
    }
    return true;
}

QString BackupManager::backupFileName(const QString &originalFilePath)
{
    // /etc/fstab -> etc_fstab
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

    // Backup the current state before restoring
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
    QDir dir(BACKUP_DIR);

    const auto files = dir.entryInfoList(
        {baseName + QStringLiteral(".*.bak")},
        QDir::Files,
        QDir::Time // newest first
    );

    for (const auto &fi : files) {
        // Parse timestamp from filename: etc_fstab.2026-04-07T14-30-00.bak
        const QString name = fi.fileName();
        const int firstDot = name.indexOf(QLatin1Char('.'));
        const int lastDot = name.lastIndexOf(QLatin1Char('.'));
        if (firstDot < 0 || lastDot <= firstDot) continue;

        const QString tsStr = name.mid(firstDot + 1, lastDot - firstDot - 1);
        const QDateTime ts = QDateTime::fromString(tsStr, QStringLiteral("yyyy-MM-ddTHH-mm-ss"));
        if (!ts.isValid()) continue;

        entries.append({fi.absoluteFilePath(), originalFilePath, ts});
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

} // namespace NsaFsm
```

- [ ] **Step 4: Create stub files for other common classes (so cmake works)**

```cpp
// ~/nsa-fedora-system-manager/common/dbushelper.h
#pragma once
namespace NsaFsm {
class DBusHelper {};
} // namespace NsaFsm

// ~/nsa-fedora-system-manager/common/dbushelper.cpp
#include "dbushelper.h"

// ~/nsa-fedora-system-manager/common/configparser.h
#pragma once
namespace NsaFsm {
class ConfigParser {};
} // namespace NsaFsm

// ~/nsa-fedora-system-manager/common/configparser.cpp
#include "configparser.h"

// ~/nsa-fedora-system-manager/common/statusnotifier.h
#pragma once
namespace NsaFsm {
class StatusNotifier {};
} // namespace NsaFsm

// ~/nsa-fedora-system-manager/common/statusnotifier.cpp
#include "statusnotifier.h"
```

- [ ] **Step 5: Write BackupManager test**

```cmake
# ~/nsa-fedora-system-manager/tests/CMakeLists.txt
find_package(Qt6 REQUIRED COMPONENTS Test)
enable_testing()

add_executable(test_backupmanager common/test_backupmanager.cpp)
target_link_libraries(test_backupmanager
    nsa-fsm-common
    Qt6::Test
)
add_test(NAME test_backupmanager COMMAND test_backupmanager)
```

Add to top-level CMakeLists.txt after the subdirectories:
```cmake
# Add at end of top-level CMakeLists.txt, before feature_summary:
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

```cpp
// ~/nsa-fedora-system-manager/tests/common/test_backupmanager.cpp
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "backupmanager.h"

using namespace NsaFsm;

class TestBackupManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBackupFileName()
    {
        // Test via backup/restore round-trip since backupFileName is private
        // Covered by other tests
    }

    void testBackupCreatesFile()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a test file
        const QString testFile = tmpDir.path() + QStringLiteral("/testfile.conf");
        QFile f(testFile);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("key = value\n");
        f.close();

        auto result = BackupManager::backup(testFile);
        // This may fail if /var/lib/nsa-fsm/backups/ doesn't exist and we're not root
        // In that case, skip gracefully
        if (!result) {
            QSKIP("Backup directory not writable (run as root or create /var/lib/nsa-fsm/backups/)");
        }

        QVERIFY(QFileInfo::exists(*result));
        QVERIFY(result->contains(QStringLiteral(".bak")));
    }

    void testListBackups()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const QString testFile = tmpDir.path() + QStringLiteral("/testfile.conf");
        QFile f(testFile);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("original");
        f.close();

        auto result = BackupManager::backup(testFile);
        if (!result) {
            QSKIP("Backup directory not writable");
        }

        auto backups = BackupManager::listBackups(testFile);
        QVERIFY(backups.size() >= 1);
        QCOMPARE(backups.first().originalFile, testFile);
    }

    void testBackupNonexistentFile()
    {
        auto result = BackupManager::backup(QStringLiteral("/nonexistent/file"));
        QVERIFY(!result.has_value());
    }
};

QTEST_MAIN(TestBackupManager)
#include "test_backupmanager.moc"
```

- [ ] **Step 6: Build and run tests**

```bash
cd ~/nsa-fedora-system-manager
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
cmake --build build
cd build && ctest --output-on-failure
```

Expected: Tests compile and run (some may skip if not root).

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: add BackupManager with backup/restore/rotate functionality"
```

---

## Task 4: Shared Library - DBusHelper, ConfigParser, StatusNotifier

**Files:**
- Modify: `common/dbushelper.h` and `common/dbushelper.cpp`
- Modify: `common/configparser.h` and `common/configparser.cpp`
- Modify: `common/statusnotifier.h` and `common/statusnotifier.cpp`

- [ ] **Step 1: Implement DBusHelper**

```cpp
// ~/nsa-fedora-system-manager/common/dbushelper.h
#pragma once

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QVariantList>

namespace NsaFsm {

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

    // Convenience: systemd1 Manager interface
    static QDBusPendingCall systemdCall(const QString &method, const QVariantList &args = {});

    // Convenience: hostname1 interface
    static QDBusPendingCall hostname1Call(const QString &method, const QVariantList &args = {});
};

} // namespace NsaFsm
```

```cpp
// ~/nsa-fedora-system-manager/common/dbushelper.cpp
#include "dbushelper.h"

namespace NsaFsm {

QDBusPendingCall DBusHelper::asyncSystemCall(
    const QString &service,
    const QString &path,
    const QString &interface,
    const QString &method,
    const QVariantList &args,
    bool interactiveAuth)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(service, path, interface, method);
    msg.setArguments(args);
    msg.setInteractiveAuthorizationAllowed(interactiveAuth);
    return QDBusConnection::systemBus().asyncCall(msg);
}

QDBusMessage DBusHelper::syncSystemCall(
    const QString &service,
    const QString &path,
    const QString &interface,
    const QString &method,
    const QVariantList &args,
    bool interactiveAuth)
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
```

- [ ] **Step 2: Implement ConfigParser (sysctl + hosts parsers as first two)**

```cpp
// ~/nsa-fedora-system-manager/common/configparser.h
#pragma once

#include <QMap>
#include <QList>
#include <QString>

namespace NsaFsm {

struct HostsEntry {
    QString ip;
    QStringList hostnames;
    QString comment;
};

struct FstabEntry {
    QString device;
    QString mountpoint;
    QString fstype;
    QString options;
    int dump = 0;
    int pass = 0;
    QString comment;
};

struct SysctlEntry {
    QString key;
    QString value;
};

class ConfigParser {
public:
    // sysctl.d: key = value
    static QList<SysctlEntry> parseSysctl(const QString &filePath);
    static bool writeSysctl(const QString &filePath, const QList<SysctlEntry> &entries);

    // /etc/hosts: IP hostname [aliases...]
    static QList<HostsEntry> parseHosts(const QString &filePath);
    static bool writeHosts(const QString &filePath, const QList<HostsEntry> &entries);

    // /etc/fstab: device mountpoint type options dump pass
    static QList<FstabEntry> parseFstab(const QString &filePath);
    static bool writeFstab(const QString &filePath, const QList<FstabEntry> &entries);

    // Generic KEY=VALUE or KEY="VALUE" (for /etc/default/grub, /etc/environment)
    static QMap<QString, QString> parseKeyValue(const QString &filePath);
    static bool writeKeyValue(const QString &filePath, const QMap<QString, QString> &values);

    // INI-style (for .repo files, zram-generator.conf, resolved.conf)
    static QMap<QString, QMap<QString, QString>> parseIni(const QString &filePath);
    static bool writeIni(const QString &filePath, const QMap<QString, QMap<QString, QString>> &sections);
};

} // namespace NsaFsm
```

```cpp
// ~/nsa-fedora-system-manager/common/configparser.cpp
#include "configparser.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

namespace NsaFsm {

QList<SysctlEntry> ConfigParser::parseSysctl(const QString &filePath)
{
    QList<SysctlEntry> entries;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return entries;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;

        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0) continue;

        entries.append({
            line.left(eq).trimmed(),
            line.mid(eq + 1).trimmed()
        });
    }
    return entries;
}

bool ConfigParser::writeSysctl(const QString &filePath, const QList<SysctlEntry> &entries)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    out << "# Generated by NSA Fedora System Manager\n";
    out << "# Do not edit manually\n\n";
    for (const auto &e : entries) {
        out << e.key << " = " << e.value << "\n";
    }
    return true;
}

QList<HostsEntry> ConfigParser::parseHosts(const QString &filePath)
{
    QList<HostsEntry> entries;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return entries;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString comment;
        const int hashPos = line.indexOf(QLatin1Char('#'));
        if (hashPos == 0) {
            entries.append({QString(), QStringList(), line});
            continue;
        }
        if (hashPos > 0) {
            comment = line.mid(hashPos);
            line = line.left(hashPos);
        }

        const QStringList parts = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;

        entries.append({parts.first(), parts.mid(1), comment});
    }
    return entries;
}

bool ConfigParser::writeHosts(const QString &filePath, const QList<HostsEntry> &entries)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    for (const auto &e : entries) {
        if (e.ip.isEmpty()) {
            out << e.comment << "\n";
            continue;
        }
        out << e.ip << "\t" << e.hostnames.join(QLatin1Char(' '));
        if (!e.comment.isEmpty()) {
            out << " " << e.comment;
        }
        out << "\n";
    }
    return true;
}

QList<FstabEntry> ConfigParser::parseFstab(const QString &filePath)
{
    QList<FstabEntry> entries;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return entries;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            entries.append({QString(), QString(), QString(), QString(), 0, 0, line});
            continue;
        }

        const QStringList parts = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 4) continue;

        FstabEntry e;
        e.device = parts[0];
        e.mountpoint = parts[1];
        e.fstype = parts[2];
        e.options = parts[3];
        e.dump = parts.size() > 4 ? parts[4].toInt() : 0;
        e.pass = parts.size() > 5 ? parts[5].toInt() : 0;
        entries.append(e);
    }
    return entries;
}

bool ConfigParser::writeFstab(const QString &filePath, const QList<FstabEntry> &entries)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    for (const auto &e : entries) {
        if (e.device.isEmpty()) {
            out << e.comment << "\n";
            continue;
        }
        out << e.device << "\t" << e.mountpoint << "\t" << e.fstype
            << "\t" << e.options << "\t" << e.dump << " " << e.pass << "\n";
    }
    return true;
}

QMap<QString, QString> ConfigParser::parseKeyValue(const QString &filePath)
{
    QMap<QString, QString> map;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return map;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;

        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0) continue;

        QString key = line.left(eq).trimmed();
        QString value = line.mid(eq + 1).trimmed();
        // Strip quotes
        if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
            value = value.mid(1, value.size() - 2);
        }
        map[key] = value;
    }
    return map;
}

bool ConfigParser::writeKeyValue(const QString &filePath, const QMap<QString, QString> &values)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    out << "# Generated by NSA Fedora System Manager\n\n";
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        // Quote values that contain spaces
        if (it.value().contains(QLatin1Char(' '))) {
            out << it.key() << "=\"" << it.value() << "\"\n";
        } else {
            out << it.key() << "=" << it.value() << "\n";
        }
    }
    return true;
}

QMap<QString, QMap<QString, QString>> ConfigParser::parseIni(const QString &filePath)
{
    QMap<QString, QMap<QString, QString>> sections;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return sections;

    QString currentSection;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char(';'))) continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentSection = line.mid(1, line.size() - 2);
            continue;
        }

        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0 || currentSection.isEmpty()) continue;

        sections[currentSection][line.left(eq).trimmed()] = line.mid(eq + 1).trimmed();
    }
    return sections;
}

bool ConfigParser::writeIni(const QString &filePath, const QMap<QString, QMap<QString, QString>> &sections)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    out << "# Generated by NSA Fedora System Manager\n\n";
    for (auto secIt = sections.constBegin(); secIt != sections.constEnd(); ++secIt) {
        out << "[" << secIt.key() << "]\n";
        for (auto kvIt = secIt.value().constBegin(); kvIt != secIt.value().constEnd(); ++kvIt) {
            out << kvIt.key() << "=" << kvIt.value() << "\n";
        }
        out << "\n";
    }
    return true;
}

} // namespace NsaFsm
```

- [ ] **Step 3: Implement StatusNotifier**

```cpp
// ~/nsa-fedora-system-manager/common/statusnotifier.h
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
```

```cpp
// ~/nsa-fedora-system-manager/common/statusnotifier.cpp
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
    notify(QStringLiteral("NSA FSM"), message, QStringLiteral("dialog-positive"));
}

void StatusNotifier::error(const QString &message)
{
    notify(QStringLiteral("NSA FSM - Fehler"), message, QStringLiteral("dialog-error"));
}

void StatusNotifier::warning(const QString &message)
{
    notify(QStringLiteral("NSA FSM - Warnung"), message, QStringLiteral("dialog-warning"));
}

void StatusNotifier::backupCreated(const QString &path)
{
    notify(QStringLiteral("NSA FSM"),
           QStringLiteral("Backup erstellt: %1").arg(path),
           QStringLiteral("document-save"));
}

} // namespace NsaFsm
```

- [ ] **Step 4: Write ConfigParser tests**

```cpp
// ~/nsa-fedora-system-manager/tests/common/test_configparser.cpp
#include <QTest>
#include <QTemporaryFile>
#include "configparser.h"

using namespace NsaFsm;

class TestConfigParser : public QObject
{
    Q_OBJECT

private:
    QString writeTempFile(const QByteArray &content) {
        auto *f = new QTemporaryFile(this);
        f->open();
        f->write(content);
        f->close();
        return f->fileName();
    }

private Q_SLOTS:
    void testParseSysctl()
    {
        auto path = writeTempFile(
            "# comment\n"
            "vm.swappiness = 60\n"
            "net.ipv4.ip_forward = 1\n"
        );
        auto entries = ConfigParser::parseSysctl(path);
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].key, QStringLiteral("vm.swappiness"));
        QCOMPARE(entries[0].value, QStringLiteral("60"));
        QCOMPARE(entries[1].key, QStringLiteral("net.ipv4.ip_forward"));
    }

    void testParseHosts()
    {
        auto path = writeTempFile(
            "# /etc/hosts\n"
            "127.0.0.1\tlocalhost\n"
            "::1\tlocalhost ip6-localhost\n"
        );
        auto entries = ConfigParser::parseHosts(path);
        QCOMPARE(entries.size(), 3); // 1 comment + 2 entries
        QCOMPARE(entries[1].ip, QStringLiteral("127.0.0.1"));
        QCOMPARE(entries[1].hostnames.first(), QStringLiteral("localhost"));
        QCOMPARE(entries[2].hostnames.size(), 2);
    }

    void testParseFstab()
    {
        auto path = writeTempFile(
            "# /etc/fstab\n"
            "UUID=abc-123\t/\text4\tdefaults\t1 1\n"
            "/dev/sda2\t/home\text4\tnoatime,discard\t0 2\n"
        );
        auto entries = ConfigParser::parseFstab(path);
        QCOMPARE(entries.size(), 3); // 1 comment + 2 entries
        QCOMPARE(entries[1].mountpoint, QStringLiteral("/"));
        QCOMPARE(entries[2].options, QStringLiteral("noatime,discard"));
        QCOMPARE(entries[2].pass, 2);
    }

    void testParseKeyValue()
    {
        auto path = writeTempFile(
            "GRUB_TIMEOUT=5\n"
            "GRUB_CMDLINE_LINUX_DEFAULT=\"quiet splash\"\n"
        );
        auto map = ConfigParser::parseKeyValue(path);
        QCOMPARE(map[QStringLiteral("GRUB_TIMEOUT")], QStringLiteral("5"));
        QCOMPARE(map[QStringLiteral("GRUB_CMDLINE_LINUX_DEFAULT")], QStringLiteral("quiet splash"));
    }

    void testParseIni()
    {
        auto path = writeTempFile(
            "[zram0]\n"
            "zram-size = min(ram, 8192)\n"
            "compression-algorithm = zstd\n"
        );
        auto sections = ConfigParser::parseIni(path);
        QVERIFY(sections.contains(QStringLiteral("zram0")));
        QCOMPARE(sections[QStringLiteral("zram0")][QStringLiteral("zram-size")],
                 QStringLiteral("min(ram, 8192)"));
    }

    void testRoundTripSysctl()
    {
        QTemporaryFile out;
        out.open();
        QString outPath = out.fileName();
        out.close();

        QList<SysctlEntry> entries = {
            {QStringLiteral("vm.swappiness"), QStringLiteral("10")},
            {QStringLiteral("net.core.somaxconn"), QStringLiteral("4096")},
        };

        QVERIFY(ConfigParser::writeSysctl(outPath, entries));
        auto parsed = ConfigParser::parseSysctl(outPath);
        QCOMPARE(parsed.size(), 2);
        QCOMPARE(parsed[0].key, QStringLiteral("vm.swappiness"));
        QCOMPARE(parsed[0].value, QStringLiteral("10"));
    }
};

QTEST_MAIN(TestConfigParser)
#include "test_configparser.moc"
```

Add to tests/CMakeLists.txt:

```cmake
add_executable(test_configparser common/test_configparser.cpp)
target_link_libraries(test_configparser nsa-fsm-common Qt6::Test)
add_test(NAME test_configparser COMMAND test_configparser)
```

- [ ] **Step 5: Build and run all tests**

```bash
cd ~/nsa-fedora-system-manager
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
cmake --build build
cd build && ctest --output-on-failure
```

Expected: All ConfigParser tests PASS.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat: add DBusHelper, ConfigParser, and StatusNotifier to common library"
```

---

## Task 5: KAuth Helper

**Files:**
- Create: `helper/CMakeLists.txt`
- Create: `helper/nsafsmhelper.h`
- Create: `helper/nsafsmhelper.cpp`
- Create: `helper/nsa-fsm.actions`

- [ ] **Step 1: Write .actions file for polkit policies**

```ini
# ~/nsa-fedora-system-manager/helper/nsa-fsm.actions

[Domain]
Name=NSA Fedora System Manager
Icon=preferences-system

[org.kde.nsa-fsm.sysctl.write]
Name=Write sysctl configuration
Description=Modify kernel parameters in /etc/sysctl.d/
Policy=auth_admin

[org.kde.nsa-fsm.grub.write]
Name=Write GRUB configuration
Description=Modify bootloader configuration
Policy=auth_admin

[org.kde.nsa-fsm.bls.read]
Name=Read boot loader entries
Description=Read kernel boot entries from /boot/loader/entries/
Policy=auth_admin

[org.kde.nsa-fsm.bls.write]
Name=Write boot loader entry
Description=Modify kernel boot entries
Policy=auth_admin

[org.kde.nsa-fsm.bls.setdefault]
Name=Set default kernel
Description=Change the default boot kernel
Policy=auth_admin

[org.kde.nsa-fsm.fstab.write]
Name=Write fstab configuration
Description=Modify filesystem mount configuration
Policy=auth_admin

[org.kde.nsa-fsm.modprobe.write]
Name=Write modprobe configuration
Description=Modify kernel module configuration
Policy=auth_admin

[org.kde.nsa-fsm.modprobe.load]
Name=Load or unload kernel module
Description=Load or remove a kernel module
Policy=auth_admin

[org.kde.nsa-fsm.cron.write]
Name=Write system cron job
Description=Modify system scheduled tasks
Policy=auth_admin

[org.kde.nsa-fsm.sudoers.write]
Name=Write sudoers configuration
Description=Modify sudo rules
Policy=auth_admin

[org.kde.nsa-fsm.selinux.setmode]
Name=Change SELinux mode
Description=Switch between Enforcing and Permissive mode
Policy=auth_admin

[org.kde.nsa-fsm.selinux.setbool]
Name=Change SELinux boolean
Description=Toggle an SELinux boolean value
Policy=auth_admin

[org.kde.nsa-fsm.repos.write]
Name=Write DNF repository configuration
Description=Modify package repository settings
Policy=auth_admin

[org.kde.nsa-fsm.repos.copr]
Name=Manage COPR repository
Description=Enable or disable a COPR repository
Policy=auth_admin

[org.kde.nsa-fsm.hosts.write]
Name=Write hosts file
Description=Modify /etc/hosts
Policy=auth_admin

[org.kde.nsa-fsm.env.write]
Name=Write environment configuration
Description=Modify system environment variables
Policy=auth_admin

[org.kde.nsa-fsm.resolved.write]
Name=Write DNS resolver configuration
Description=Modify systemd-resolved settings
Policy=auth_admin

[org.kde.nsa-fsm.zram.write]
Name=Write zram configuration
Description=Modify swap/zram settings
Policy=auth_admin

[org.kde.nsa-fsm.generic.backup]
Name=Create configuration backup
Description=Backup a system configuration file
Policy=auth_admin

[org.kde.nsa-fsm.generic.restore]
Name=Restore configuration backup
Description=Restore a previously backed up configuration file
Policy=auth_admin
```

- [ ] **Step 2: Write KAuth Helper**

```cpp
// ~/nsa-fedora-system-manager/helper/nsafsmhelper.h
#pragma once

#include <KAuth/ActionReply>
#include <KAuth/HelperSupport>
#include <QObject>

using namespace KAuth;

class NsaFsmHelper : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    // Each slot name corresponds to the last part of the action ID
    // e.g. org.kde.nsa-fsm.sysctl.write -> uses writeFile with "sysctl" context

    // Generic file write with backup
    ActionReply write(const QVariantMap &args);

    // Generic file read (for root-only files like BLS entries)
    ActionReply read(const QVariantMap &args);

    // Execute a post-write command
    ActionReply execute(const QVariantMap &args);

    // Backup management
    ActionReply backup(const QVariantMap &args);
    ActionReply restore(const QVariantMap &args);

private:
    ActionReply writeFileWithBackup(const QString &filePath, const QByteArray &content);
    ActionReply runCommand(const QString &command, const QStringList &args);
    bool validateSudoers(const QString &tempPath);
};
```

```cpp
// ~/nsa-fedora-system-manager/helper/nsafsmhelper.cpp
#include "nsafsmhelper.h"
#include "backupmanager.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryFile>

ActionReply NsaFsmHelper::writeFileWithBackup(const QString &filePath, const QByteArray &content)
{
    // Step 1: Backup existing file
    if (QFile::exists(filePath)) {
        auto backupResult = NsaFsm::BackupManager::backup(filePath);
        if (!backupResult) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(
                QStringLiteral("Failed to create backup of %1").arg(filePath));
            return reply;
        }
    }

    // Step 2: Write new content
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(
            QStringLiteral("Failed to open %1 for writing: %2").arg(filePath, file.errorString()));
        return reply;
    }

    file.write(content);
    file.close();

    auto reply = ActionReply::SuccessReply();
    reply.addData(QStringLiteral("filePath"), filePath);
    return reply;
}

ActionReply NsaFsmHelper::runCommand(const QString &command, const QStringList &args)
{
    QProcess proc;
    proc.start(command, args);
    proc.waitForFinished(30000);

    if (proc.exitCode() != 0) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(
            QStringLiteral("Command failed: %1 %2\nOutput: %3\nError: %4")
                .arg(command, args.join(QLatin1Char(' ')),
                     QString::fromUtf8(proc.readAllStandardOutput()),
                     QString::fromUtf8(proc.readAllStandardError())));
        return reply;
    }

    auto reply = ActionReply::SuccessReply();
    reply.addData(QStringLiteral("stdout"), proc.readAllStandardOutput());
    reply.addData(QStringLiteral("stderr"), proc.readAllStandardError());
    return reply;
}

bool NsaFsmHelper::validateSudoers(const QString &tempPath)
{
    QProcess proc;
    proc.start(QStringLiteral("visudo"), {QStringLiteral("-c"), QStringLiteral("-f"), tempPath});
    proc.waitForFinished(5000);
    return proc.exitCode() == 0;
}

ActionReply NsaFsmHelper::write(const QVariantMap &args)
{
    const QString filePath = args[QStringLiteral("filePath")].toString();
    const QByteArray content = args[QStringLiteral("content")].toByteArray();
    const QString postCommand = args.value(QStringLiteral("postCommand")).toString();
    const QStringList postArgs = args.value(QStringLiteral("postArgs")).toStringList();
    const bool validateAsSudoers = args.value(QStringLiteral("validateAsSudoers")).toBool();

    // Safety: reject writes outside allowed paths
    static const QStringList allowedPrefixes = {
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

    bool allowed = false;
    for (const auto &prefix : allowedPrefixes) {
        if (filePath.startsWith(prefix)) {
            allowed = true;
            break;
        }
    }
    if (!allowed) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(
            QStringLiteral("Write to %1 is not allowed").arg(filePath));
        return reply;
    }

    // Sudoers validation
    if (validateAsSudoers) {
        QTemporaryFile temp;
        temp.open();
        temp.write(content);
        temp.close();
        if (!validateSudoers(temp.fileName())) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(
                QStringLiteral("Sudoers syntax validation failed. File not written."));
            return reply;
        }
    }

    // Write with backup
    auto writeReply = writeFileWithBackup(filePath, content);
    if (writeReply.failed()) {
        return writeReply;
    }

    // Execute post-write command if specified
    if (!postCommand.isEmpty()) {
        auto cmdReply = runCommand(postCommand, postArgs);
        if (cmdReply.failed()) {
            // Post-command failed -- restore backup
            auto backups = NsaFsm::BackupManager::listBackups(filePath);
            if (!backups.isEmpty()) {
                QFile::remove(filePath);
                QFile::copy(backups.first().path, filePath);
            }
            return cmdReply;
        }
    }

    return writeReply;
}

ActionReply NsaFsmHelper::read(const QVariantMap &args)
{
    const QString dirPath = args.value(QStringLiteral("dirPath")).toString();
    const QString filePath = args.value(QStringLiteral("filePath")).toString();

    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(QStringLiteral("Cannot read %1").arg(filePath));
            return reply;
        }
        auto reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("content"), file.readAll());
        return reply;
    }

    if (!dirPath.isEmpty()) {
        QDir dir(dirPath);
        QVariantMap filesMap;
        const auto entries = dir.entryInfoList(QDir::Files);
        for (const auto &entry : entries) {
            QFile f(entry.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                filesMap[entry.fileName()] = f.readAll();
            }
        }
        auto reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("files"), filesMap);
        return reply;
    }

    return ActionReply::HelperErrorReply();
}

ActionReply NsaFsmHelper::execute(const QVariantMap &args)
{
    const QString command = args[QStringLiteral("command")].toString();
    const QStringList cmdArgs = args.value(QStringLiteral("args")).toStringList();

    // Whitelist of allowed commands
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

    if (!allowedCommands.contains(command)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(
            QStringLiteral("Command %1 is not in the allowed list").arg(command));
        return reply;
    }

    return runCommand(command, cmdArgs);
}

ActionReply NsaFsmHelper::backup(const QVariantMap &args)
{
    const QString filePath = args[QStringLiteral("filePath")].toString();
    auto result = NsaFsm::BackupManager::backup(filePath);
    if (!result) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Backup failed for %1").arg(filePath));
        return reply;
    }
    auto reply = ActionReply::SuccessReply();
    reply.addData(QStringLiteral("backupPath"), *result);
    return reply;
}

ActionReply NsaFsmHelper::restore(const QVariantMap &args)
{
    const QString backupPath = args[QStringLiteral("backupPath")].toString();
    const QString originalPath = args[QStringLiteral("originalPath")].toString();
    auto result = NsaFsm::BackupManager::restore(backupPath, originalPath);
    if (!result) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Restore failed"));
        return reply;
    }
    return ActionReply::SuccessReply();
}

KAUTH_HELPER_MAIN("org.kde.nsa-fsm", NsaFsmHelper)
```

- [ ] **Step 3: Write helper CMakeLists.txt**

```cmake
# ~/nsa-fedora-system-manager/helper/CMakeLists.txt
add_executable(nsa-fsm-helper
    nsafsmhelper.cpp
    nsafsmhelper.h
)

target_link_libraries(nsa-fsm-helper
    nsa-fsm-common
    KF6::AuthCore
    Qt6::Core
    Qt6::DBus
)

# Install polkit actions
kauth_install_actions(org.kde.nsa-fsm nsa-fsm.actions)

# Install helper with D-Bus activation
kauth_install_helper_files(nsa-fsm-helper org.kde.nsa-fsm root)
install(TARGETS nsa-fsm-helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
```

- [ ] **Step 4: Build**

```bash
cd ~/nsa-fedora-system-manager
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
```

Expected: Helper compiles without errors.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat: add KAuth helper with whitelisted file operations and backup-on-write"
```

---

## Task 6: First KCM - System Journal (Modul 12)

Simplest module -- no KAuth needed, pure read-only. Proves the KCM pipeline works.

**Files:**
- Create: `kcms/journal/CMakeLists.txt`
- Create: `kcms/journal/kcm_nsa_journal.json`
- Create: `kcms/journal/journalkcm.h`
- Create: `kcms/journal/journalkcm.cpp`
- Create: `kcms/journal/journalmodel.h`
- Create: `kcms/journal/journalmodel.cpp`
- Create: `kcms/journal/ui/main.qml`
- Modify: `kcms/CMakeLists.txt`

- [ ] **Step 1: Create KCM metadata JSON**

```json
{
    "KPlugin": {
        "Description": "View and search systemd journal logs",
        "Icon": "text-x-log",
        "Name": "System Logs"
    },
    "X-KDE-System-Settings-Parent-Category": "nsa-system-administration",
    "X-KDE-Weight": 120
}
```

Save as `kcms/journal/kcm_nsa_journal.json`.

- [ ] **Step 2: Write JournalModel (C++ backend for sd-journal)**

```cpp
// ~/nsa-fedora-system-manager/kcms/journal/journalmodel.h
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <systemd/sd-journal.h>

class JournalEntry {
public:
    QDateTime timestamp;
    QString message;
    QString unit;
    int priority = 6; // LOG_INFO
    QString bootId;
};

class JournalModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int priorityFilter READ priorityFilter WRITE setPriorityFilter NOTIFY priorityFilterChanged)
    Q_PROPERTY(QString unitFilter READ unitFilter WRITE setUnitFilter NOTIFY unitFilterChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(bool kernelOnly READ kernelOnly WRITE setKernelOnly NOTIFY kernelOnlyChanged)

public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        MessageRole,
        UnitRole,
        PriorityRole,
        PriorityNameRole,
    };

    explicit JournalModel(QObject *parent = nullptr);
    ~JournalModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int priorityFilter() const { return m_priorityFilter; }
    void setPriorityFilter(int prio);

    QString unitFilter() const { return m_unitFilter; }
    void setUnitFilter(const QString &unit);

    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);

    bool kernelOnly() const { return m_kernelOnly; }
    void setKernelOnly(bool kernel);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QStringList availableUnits() const;

Q_SIGNALS:
    void priorityFilterChanged();
    void unitFilterChanged();
    void searchTextChanged();
    void kernelOnlyChanged();

private:
    void loadEntries();
    static QString priorityName(int prio);

    QList<JournalEntry> m_entries;
    int m_priorityFilter = 6; // LOG_INFO and above
    QString m_unitFilter;
    QString m_searchText;
    bool m_kernelOnly = false;
    static constexpr int MAX_ENTRIES = 2000;
};
```

```cpp
// ~/nsa-fedora-system-manager/kcms/journal/journalmodel.cpp
#include "journalmodel.h"

#include <QTimeZone>

JournalModel::JournalModel(QObject *parent)
    : QAbstractListModel(parent)
{
    loadEntries();
}

JournalModel::~JournalModel() = default;

int JournalModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant JournalModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) return {};

    const auto &entry = m_entries[index.row()];
    switch (role) {
    case TimestampRole: return entry.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    case MessageRole: return entry.message;
    case UnitRole: return entry.unit;
    case PriorityRole: return entry.priority;
    case PriorityNameRole: return priorityName(entry.priority);
    }
    return {};
}

QHash<int, QByteArray> JournalModel::roleNames() const
{
    return {
        {TimestampRole, "timestamp"},
        {MessageRole, "message"},
        {UnitRole, "unit"},
        {PriorityRole, "priority"},
        {PriorityNameRole, "priorityName"},
    };
}

QString JournalModel::priorityName(int prio)
{
    switch (prio) {
    case 0: return QStringLiteral("EMERG");
    case 1: return QStringLiteral("ALERT");
    case 2: return QStringLiteral("CRIT");
    case 3: return QStringLiteral("ERR");
    case 4: return QStringLiteral("WARNING");
    case 5: return QStringLiteral("NOTICE");
    case 6: return QStringLiteral("INFO");
    case 7: return QStringLiteral("DEBUG");
    default: return QStringLiteral("???");
    }
}

void JournalModel::setPriorityFilter(int prio)
{
    if (m_priorityFilter == prio) return;
    m_priorityFilter = prio;
    Q_EMIT priorityFilterChanged();
    loadEntries();
}

void JournalModel::setUnitFilter(const QString &unit)
{
    if (m_unitFilter == unit) return;
    m_unitFilter = unit;
    Q_EMIT unitFilterChanged();
    loadEntries();
}

void JournalModel::setSearchText(const QString &text)
{
    if (m_searchText == text) return;
    m_searchText = text;
    Q_EMIT searchTextChanged();
    loadEntries();
}

void JournalModel::setKernelOnly(bool kernel)
{
    if (m_kernelOnly == kernel) return;
    m_kernelOnly = kernel;
    Q_EMIT kernelOnlyChanged();
    loadEntries();
}

void JournalModel::refresh()
{
    loadEntries();
}

void JournalModel::loadEntries()
{
    beginResetModel();
    m_entries.clear();

    sd_journal *journal = nullptr;
    int r = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
    if (r < 0) {
        endResetModel();
        return;
    }

    // Apply filters
    if (m_kernelOnly) {
        sd_journal_add_match(journal, "_TRANSPORT=kernel", 0);
    }
    if (!m_unitFilter.isEmpty()) {
        const QByteArray match = QStringLiteral("_SYSTEMD_UNIT=%1").arg(m_unitFilter).toUtf8();
        sd_journal_add_match(journal, match.constData(), 0);
    }
    if (m_priorityFilter < 7) {
        for (int p = 0; p <= m_priorityFilter; ++p) {
            const QByteArray match = QStringLiteral("PRIORITY=%1").arg(p).toUtf8();
            sd_journal_add_match(journal, match.constData(), 0);
        }
    }

    // Seek to end and read backwards
    sd_journal_seek_tail(journal);

    int count = 0;
    while (sd_journal_previous(journal) > 0 && count < MAX_ENTRIES) {
        JournalEntry entry;

        // Timestamp
        uint64_t usec = 0;
        sd_journal_get_realtime_usec(journal, &usec);
        entry.timestamp = QDateTime::fromMSecsSinceEpoch(usec / 1000, QTimeZone::systemTimeZone());

        // Message
        const char *data = nullptr;
        size_t len = 0;
        if (sd_journal_get_data(journal, "MESSAGE", (const void **)&data, &len) >= 0) {
            // data is "MESSAGE=actual message"
            entry.message = QString::fromUtf8(data + 8, len - 8);
        }

        // Unit
        if (sd_journal_get_data(journal, "_SYSTEMD_UNIT", (const void **)&data, &len) >= 0) {
            entry.unit = QString::fromUtf8(data + 14, len - 14);
        }

        // Priority
        if (sd_journal_get_data(journal, "PRIORITY", (const void **)&data, &len) >= 0) {
            entry.priority = QString::fromUtf8(data + 9, len - 9).toInt();
        }

        // Search filter (client-side)
        if (!m_searchText.isEmpty() &&
            !entry.message.contains(m_searchText, Qt::CaseInsensitive)) {
            continue;
        }

        m_entries.append(entry);
        ++count;
    }

    sd_journal_close(journal);
    endResetModel();
}

QStringList JournalModel::availableUnits() const
{
    QStringList units;
    sd_journal *journal = nullptr;
    if (sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY) < 0) return units;

    const char *data = nullptr;
    size_t len = 0;
    SD_JOURNAL_FOREACH_UNIQUE(journal, data, len) {
        // We queried _SYSTEMD_UNIT field unique values
    }
    // Simpler approach: use a set
    QSet<QString> unitSet;
    sd_journal_query_unique(journal, "_SYSTEMD_UNIT");
    SD_JOURNAL_FOREACH_UNIQUE(journal, data, len) {
        unitSet.insert(QString::fromUtf8(data + 14, len - 14));
    }
    sd_journal_close(journal);

    units = unitSet.values();
    units.sort();
    return units;
}
```

- [ ] **Step 3: Write JournalKcm (KCM plugin class)**

```cpp
// ~/nsa-fedora-system-manager/kcms/journal/journalkcm.h
#pragma once

#include <KQuickConfigModule>

class JournalModel;

class JournalKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(JournalModel *journalModel READ journalModel CONSTANT)

public:
    explicit JournalKcm(QObject *parent, const KPluginMetaData &metaData);

    JournalModel *journalModel() const { return m_model; }

private:
    JournalModel *m_model;
};
```

```cpp
// ~/nsa-fedora-system-manager/kcms/journal/journalkcm.cpp
#include "journalkcm.h"
#include "journalmodel.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(JournalKcm, "kcm_nsa_journal.json")

JournalKcm::JournalKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
    , m_model(new JournalModel(this))
{
    setButtons(NoAdditionalButton);
}

#include "journalkcm.moc"
```

- [ ] **Step 4: Write QML UI**

```qml
// ~/nsa-fedora-system-manager/kcms/journal/ui/main.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.ScrollViewKCM {
    id: root

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.SearchField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: i18n("Search logs...")
                onAccepted: kcm.journalModel.searchText = text
            }

            QQC2.ComboBox {
                id: priorityCombo
                model: ["Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug"]
                currentIndex: kcm.journalModel.priorityFilter
                onCurrentIndexChanged: kcm.journalModel.priorityFilter = currentIndex
            }

            QQC2.CheckBox {
                text: i18n("Kernel only")
                checked: kcm.journalModel.kernelOnly
                onCheckedChanged: kcm.journalModel.kernelOnly = checked
            }

            QQC2.Button {
                icon.name: "view-refresh"
                onClicked: kcm.journalModel.refresh()
                QQC2.ToolTip.text: i18n("Refresh")
                QQC2.ToolTip.visible: hovered
            }
        }
    }

    view: ListView {
        id: logView
        model: kcm.journalModel
        clip: true

        delegate: QQC2.ItemDelegate {
            width: logView.width
            padding: Kirigami.Units.smallSpacing

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                // Priority badge
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: {
                        if (model.priority <= 2) return Kirigami.Theme.negativeTextColor
                        if (model.priority === 3) return "#e67e22"
                        if (model.priority === 4) return Kirigami.Theme.neutralTextColor
                        return "transparent"
                    }
                }

                // Timestamp
                QQC2.Label {
                    text: model.timestamp
                    font.family: "monospace"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                    Layout.preferredWidth: 160
                }

                // Unit
                QQC2.Label {
                    text: model.unit || ""
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.activeTextColor
                    Layout.preferredWidth: 200
                    elide: Text.ElideRight
                }

                // Message
                QQC2.Label {
                    text: model.message
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: {
                        if (model.priority <= 2) return Kirigami.Theme.negativeTextColor
                        if (model.priority === 3) return "#e67e22"
                        if (model.priority === 4) return Kirigami.Theme.neutralTextColor
                        return Kirigami.Theme.textColor
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 5: Write CMakeLists for journal KCM**

```cmake
# ~/nsa-fedora-system-manager/kcms/journal/CMakeLists.txt
kcmutils_add_qml_kcm(kcm_nsa_journal
    SOURCES
        journalkcm.cpp journalkcm.h
        journalmodel.cpp journalmodel.h
)

target_link_libraries(kcm_nsa_journal PRIVATE
    nsa-fsm-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    Qt6::Quick
    PkgConfig::SYSTEMD
)
```

```cmake
# ~/nsa-fedora-system-manager/kcms/CMakeLists.txt
add_subdirectory(journal)
```

- [ ] **Step 6: Build and test**

```bash
cd ~/nsa-fedora-system-manager
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
# Test standalone:
sudo cmake --install build
kcmshell6 kcm_nsa_journal
```

Expected: Journal KCM opens showing recent log entries with search and priority filter.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat: add System Journal KCM module with sd-journal integration"
```

---

## Task 7: systemd Services KCM (Modul 1)

**Files:**
- Create: `kcms/services/CMakeLists.txt`
- Create: `kcms/services/kcm_nsa_services.json`
- Create: `kcms/services/serviceskcm.h` and `.cpp`
- Create: `kcms/services/unitmodel.h` and `.cpp`
- Create: `kcms/services/ui/main.qml`
- Modify: `kcms/CMakeLists.txt`

This follows the same pattern as Task 6 but uses D-Bus to `org.freedesktop.systemd1` instead of sd-journal. The UnitModel fetches data via `ListUnits()` and `ListUnitFiles()`, provides roles for name, description, activeState, unitFileState, and type. Actions (start/stop/enable/disable) use `DBusHelper::systemdCall()` with interactive auth.

- [ ] **Step 1: Create metadata JSON** (`kcm_nsa_services.json` with Name "Services", Icon "system-run", Weight 10)
- [ ] **Step 2: Write UnitModel** (QAbstractListModel, roles: name, description, activeState, subState, unitFileState, type; loads via D-Bus ListUnits; filters by type; search by name)
- [ ] **Step 3: Write ServicesKcm** (KQuickConfigModule, exposes UnitModel, Q_INVOKABLE startUnit/stopUnit/restartUnit/enableUnit/disableUnit methods using DBusHelper)
- [ ] **Step 4: Write QML UI** (ScrollViewKCM with SearchField, type-filter ComboBox, ListView with status badges, action buttons per delegate)
- [ ] **Step 5: Write CMakeLists** (same pattern as journal, link Qt6::DBus)
- [ ] **Step 6: Add `add_subdirectory(services)` to kcms/CMakeLists.txt**
- [ ] **Step 7: Build, install, test with `kcmshell6 kcm_nsa_services`**
- [ ] **Step 8: Commit** `"feat: add systemd Services KCM module"`

---

## Task 8: sysctl KCM (Modul 2) - First KAuth Module

**Files:**
- Create: `kcms/sysctl/CMakeLists.txt`
- Create: `kcms/sysctl/kcm_nsa_sysctl.json`
- Create: `kcms/sysctl/sysctlkcm.h` and `.cpp`
- Create: `kcms/sysctl/sysctlmodel.h` and `.cpp`
- Create: `kcms/sysctl/ui/main.qml`

First module that writes via KAuth. The SysctlModel reads from `/proc/sys/` (no root needed) and custom values from `/etc/sysctl.d/*.conf`. Save triggers KAuth action `org.kde.nsa-fsm.write` with filePath `/etc/sysctl.d/99-nsa-fsm.conf` and postCommand `sysctl --system`.

- [ ] **Step 1: Create metadata JSON** (Name "Kernel Parameters", Icon "preferences-system-linux", Weight 20)
- [ ] **Step 2: Write SysctlModel** (TreeModel with categories kernel/vm/net/fs/dev, reads /proc/sys/ recursively, shows current + custom values)
- [ ] **Step 3: Write SysctlKcm** (KQuickConfigModule with save() override that calls KAuth::Action `org.kde.nsa-fsm.write`, setAuthActionName)
- [ ] **Step 4: Write QML UI** (ScrollViewKCM with TreeView, inline editing, preset selector)
- [ ] **Step 5: Write CMakeLists** (link KF6::AuthCore additionally)
- [ ] **Step 6: Build, install, test with `kcmshell6 kcm_nsa_sysctl`**
- [ ] **Step 7: Commit** `"feat: add sysctl Parameters KCM module with KAuth integration"`

---

## Task 9: Hostname & Network Identity KCM (Modul 9)

**Files:** `kcms/hostname/` -- same structure

Mix of D-Bus (hostname1 for hostname get/set) and KAuth (for /etc/hosts, resolved.conf writes). Three sections: Hostname (FormLayout with fields for pretty/static/transient via D-Bus), Hosts table (TableView with add/edit/delete), DNS config (FormLayout for resolved.conf).

- [ ] **Steps 1-7:** Follow established pattern from Tasks 6-8
- [ ] **Commit** `"feat: add Hostname & Network Identity KCM module"`

---

## Task 10: Bootloader KCM (Modul 3)

**Files:** `kcms/bootloader/` -- same structure

Most complex module. Detects GRUB vs systemd-boot. GRUB tab uses parseKeyValue for `/etc/default/grub`. BLS tab reads entries via KAuth read action (root-only `/boot/loader/entries/`). Uses parseBLSEntry pattern. Write uses KAuth with postCommand `grub2-mkconfig` or `grubby --set-default`.

- [ ] **Steps 1-7:** Follow established pattern, additional TagEditor QML component for GRUB_CMDLINE
- [ ] **Commit** `"feat: add Bootloader KCM module with GRUB and BLS support"`

---

## Task 11: fstab / Mounts KCM (Modul 4)

**Files:** `kcms/fstab/` -- same structure

TableView of fstab entries with status from /proc/mounts. Edit dialog as OverlaySheet. UDisks2 D-Bus for device discovery. KAuth write with postCommand `systemctl daemon-reload`. Validation via `findmnt --verify`. Root mountpoint (`/`) is read-only.

- [ ] **Steps 1-7:** Follow established pattern
- [ ] **Commit** `"feat: add fstab/Mounts KCM module"`

---

## Task 12: Kernel Modules KCM (Modul 5)

**Files:** `kcms/kernelmodules/` -- same structure

Reads `/proc/modules` for loaded modules. `modinfo` output via QProcess (no root needed for reading). Blacklist management via KAuth write to `/etc/modprobe.d/nsa-fsm-blacklist.conf`. Load/unload via KAuth execute with `modprobe`/`rmmod`.

- [ ] **Steps 1-7:** Follow established pattern
- [ ] **Commit** `"feat: add Kernel Modules KCM module"`

---

## Task 13: Scheduled Tasks KCM (Modul 6)

**Files:** `kcms/scheduledtasks/` -- same structure

Two tabs: Timer (D-Bus to systemd1 ListUnits filtered by type=timer) and Cron (parse /etc/cron.d/ via KAuth read, user crontab via QProcess `crontab -l`). Timer wizard generates .timer + .service files.

- [ ] **Steps 1-7:** Follow established pattern
- [ ] **Commit** `"feat: add Scheduled Tasks KCM module"`

---

## Task 14: Security KCM (Modul 7)

**Files:** `kcms/security/` -- same structure

Three tabs. Sudoers: read /etc/sudoers.d/ via KAuth, write with validateAsSudoers=true. SELinux: read getenforce via QProcess, toggle via KAuth execute setenforce, booleans via getsebool/setsebool. Polkit: read-only listing of rules files.

- [ ] **Steps 1-7:** Follow established pattern
- [ ] **Commit** `"feat: add Security KCM module (sudoers, SELinux, polkit)"`

---

## Task 15: DNF Repos KCM (Modul 8)

**Files:** `kcms/dnfrepos/` -- same structure

Reads .repo files from /etc/yum.repos.d/ using parseIni. Enable/disable toggle writes modified file via KAuth. COPR via KAuth execute `dnf5 copr enable/disable`. GPG key info via `rpm -qa gpg-pubkey*`.

- [ ] **Steps 1-7:** Follow established pattern
- [ ] **Commit** `"feat: add DNF Repos KCM module with COPR support"`

---

## Task 16: Environment Variables KCM (Modul 10)

**Files:** `kcms/environment/` -- same structure

Two tabs: System (/etc/environment via KAuth, /etc/profile.d/ listing) and User (~/.config/environment.d/ -- no root needed). Uses parseKeyValue for both. Known variable descriptions as inline help text.

- [ ] **Steps 1-7:** Follow established pattern
- [ ] **Commit** `"feat: add Environment Variables KCM module"`

---

## Task 17: Swap & zram KCM (Modul 11)

**Files:** `kcms/swap/` -- same structure

Reads /proc/swaps, /sys/block/zram0/ stats, current zram-generator config. Writes via KAuth to /etc/systemd/zram-generator.conf using writeIni. Shows compression stats and RAM/swap usage bars.

- [ ] **Steps 1-7:** Follow established pattern
- [ ] **Commit** `"feat: add Swap & zram KCM module"`

---

## Task 18: System Install + Integration Test

**Files:** None (system integration)

- [ ] **Step 1: Full build**

```bash
cd ~/nsa-fedora-system-manager
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
cmake --build build
```

- [ ] **Step 2: Run unit tests**

```bash
cd build && ctest --output-on-failure
```

- [ ] **Step 3: Install to system**

```bash
sudo cmake --install build
```

- [ ] **Step 4: Verify category appears in System Settings**

Open KDE System Settings, check that "System-Administration" category appears in the sidebar with all 12 modules listed.

- [ ] **Step 5: Test each module individually**

```bash
kcmshell6 kcm_nsa_journal
kcmshell6 kcm_nsa_services
kcmshell6 kcm_nsa_sysctl
kcmshell6 kcm_nsa_hostname
kcmshell6 kcm_nsa_bootloader
kcmshell6 kcm_nsa_fstab
kcmshell6 kcm_nsa_kernelmodules
kcmshell6 kcm_nsa_scheduledtasks
kcmshell6 kcm_nsa_security
kcmshell6 kcm_nsa_dnfrepos
kcmshell6 kcm_nsa_environment
kcmshell6 kcm_nsa_swap
```

- [ ] **Step 6: Test a write operation** (e.g. add a sysctl parameter, verify backup created in /var/lib/nsa-fsm/backups/)

- [ ] **Step 7: Commit any fixes**

---

## Task 19: Polish & README

**Files:**
- Create: `README.md`
- Modify: Various QML files for UI polish

- [ ] **Step 1: Write README.md** with project description, screenshots section, build instructions, module list, contributing guidelines
- [ ] **Step 2: Add .desktop file metadata translations** (de, en as minimum)
- [ ] **Step 3: Final commit**

```bash
git add -A
git commit -m "docs: add README and polish UI translations"
```

- [ ] **Step 4: Tag release**

```bash
git tag -a v0.1.0 -m "Initial release: 12 KCM modules for Fedora system administration"
```
