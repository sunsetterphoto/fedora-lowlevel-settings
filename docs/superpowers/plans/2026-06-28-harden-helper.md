# Harden the fls-helper — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enforce whitelist-by-default on every path, command, and argument the root `fls-helper` accepts, closing three confirmed privilege-escalation weaknesses without changing the KAuth action schema or breaking any module.

**Architecture:** Extract all security logic into a new pure, unit-tested static library `fls-helperpolicy` (`helper/helperpolicy.{h,cpp}`). `flshelper.cpp` calls it to validate read paths, write paths, and command+arguments, and to derive post-commands server-side. Four module write call sites drop their now-ignored `postCommand`/`postArgs`.

**Tech Stack:** C++20, Qt6 (Core, Test), KF6 KAuth, CMake/CTest. Tests use QTest.

## Global Constraints

- **Whitelist-by-default:** every path/command/argument not explicitly allowed is rejected with `ActionReply::HelperErrorReply()`.
- **No action-schema change:** `helper/fls.actions`, the polkit policy, and the 5 action names (`org.kde.fls.{write,read,execute,backup,restore}`) are unchanged.
- **Do not break the mapped call surface** (these exact patterns must keep working):
  - write files: `/etc/default/grub`, `/etc/fstab`, `/etc/hosts`, `/etc/environment`, `/etc/systemd/resolved.conf`, `/etc/systemd/zram-generator.conf`; write under dirs: `/etc/sysctl.d/`, `/etc/modprobe.d/`, `/etc/sudoers.d/`, `/etc/yum.repos.d/`
  - read dirs: `/boot/loader/entries/`, `/etc/sudoers.d/`
  - execute: `grubby --set-default /boot/vmlinuz-<v>`; `dnf5 copr enable|disable <project> -y`; `dnf5 makecache`; `modprobe <module>`; `rmmod <module>`; `setenforce 0|1`; `setsebool -P <name> on|off`
  - server-side post-commands: grub→`grub2-mkconfig -o /boot/grub2/grub.cfg`; fstab→`systemctl daemon-reload`; resolved.conf→`systemctl restart systemd-resolved`; sysctl 99-fls.conf→`sysctl --system`
- **Test pattern:** QTest single-file, ending with `QTEST_MAIN(Class)` then `#include "<file>.moc"` (AUTOMOC is enabled).
- Tests assume a standard Fedora filesystem layout (real system dirs like `/etc/sysctl.d`, `/etc/sudoers.d`, `/boot/loader/entries` exist), consistent with the existing `test_backupmanager`.

---

### Task 1: `fls-helperpolicy` library — path validation + scaffolding

**Files:**
- Create: `helper/helperpolicy.h`
- Create: `helper/helperpolicy.cpp`
- Create: `tests/helper/test_helperpolicy.cpp`
- Modify: `helper/CMakeLists.txt` (add the static lib)
- Modify: `tests/CMakeLists.txt` (add the test target)

**Interfaces:**
- Produces (this task): `Fls::Policy::canonicalize(const QString&) -> QString`; `Fls::Policy::isWritePathAllowed(const QString&) -> bool`; `Fls::Policy::isReadDirAllowed(const QString&) -> bool`; `Fls::Policy::isReadFileAllowed(const QString&) -> bool`; types `Fls::Policy::PathKind`, `Fls::Policy::PathRule`, `Fls::Policy::PostCommand`.
- Produces (declared now, implemented Task 2): `Fls::Policy::isExecAllowed(const QString&, const QStringList&) -> bool`; `Fls::Policy::postCommandFor(const QString&) -> std::optional<PostCommand>`.

- [ ] **Step 1: Create the header `helper/helperpolicy.h`**

```cpp
#pragma once

#include <QString>
#include <QStringList>
#include <optional>

namespace Fls::Policy {

enum class PathKind { File, Dir };

struct PathRule {
    QString path;
    PathKind kind;
};

struct PostCommand {
    QString binary;
    QStringList args;
};

// Canonicalize an absolute path for whitelist matching. Returns an empty
// QString if the path is empty, relative, contains a ".." segment, or its
// parent directory cannot be resolved. The leaf need not exist (new files are
// allowed); the parent is resolved through symlinks.
QString canonicalize(const QString &path);

// True iff `path` canonicalizes to an allowed write target.
bool isWritePathAllowed(const QString &path);

// True iff `dirPath` canonicalizes to exactly an allowed read directory.
bool isReadDirAllowed(const QString &dirPath);

// True iff `filePath` canonicalizes to a direct child of an allowed read dir.
bool isReadFileAllowed(const QString &filePath);

// True iff (binary, args) matches exactly one allowed execute pattern.
bool isExecAllowed(const QString &binary, const QStringList &args);

// Fixed server-side post-command for a validated, canonicalized write path,
// or std::nullopt if that path has no post-command.
std::optional<PostCommand> postCommandFor(const QString &canonicalPath);

} // namespace Fls::Policy
```

- [ ] **Step 2: Create `helper/helperpolicy.cpp` with canonicalize + path functions**

(Stubs for `isExecAllowed`/`postCommandFor` are added in Task 2; do not declare them here beyond what the header has — but the file must compile, so include minimal stubs returning `false`/`std::nullopt` now and replace them in Task 2.)

```cpp
#include "helperpolicy.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace Fls::Policy {

namespace {

const QList<PathRule> &writeRules()
{
    static const QList<PathRule> rules = {
        {QStringLiteral("/etc/default/grub"), PathKind::File},
        {QStringLiteral("/etc/fstab"), PathKind::File},
        {QStringLiteral("/etc/hosts"), PathKind::File},
        {QStringLiteral("/etc/environment"), PathKind::File},
        {QStringLiteral("/etc/systemd/resolved.conf"), PathKind::File},
        {QStringLiteral("/etc/systemd/zram-generator.conf"), PathKind::File},
        {QStringLiteral("/etc/sysctl.d/"), PathKind::Dir},
        {QStringLiteral("/etc/modprobe.d/"), PathKind::Dir},
        {QStringLiteral("/etc/sudoers.d/"), PathKind::Dir},
        {QStringLiteral("/etc/yum.repos.d/"), PathKind::Dir},
    };
    return rules;
}

// Allowed read directories (canonical form, no trailing slash).
const QStringList &readDirs()
{
    static const QStringList dirs = {
        QStringLiteral("/boot/loader/entries"),
        QStringLiteral("/etc/sudoers.d"),
    };
    return dirs;
}

bool matchesWrite(const QString &canonical, const PathRule &rule)
{
    if (rule.kind == PathKind::File) {
        return canonical == rule.path;
    }
    // Dir rule: rule.path ends with '/'; canonical must be a strict child.
    return canonical.startsWith(rule.path) && canonical.length() > rule.path.length();
}

} // namespace

QString canonicalize(const QString &path)
{
    if (path.isEmpty() || !path.startsWith(QLatin1Char('/'))) {
        return QString();
    }

    // Lexically normalize, then strip a trailing slash so directory targets
    // compare the same way as files.
    QString clean = QDir::cleanPath(path);
    if (clean.size() > 1 && clean.endsWith(QLatin1Char('/'))) {
        clean.chop(1);
    }

    // Belt and suspenders: reject any ".." that survived cleanPath.
    const auto segments = clean.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.contains(QStringLiteral(".."))) {
        return QString();
    }

    const QFileInfo info(clean);
    const QString canonicalParent = QFileInfo(info.absolutePath()).canonicalFilePath();
    if (canonicalParent.isEmpty()) {
        return QString(); // parent does not exist / cannot be resolved
    }

    QString result = canonicalParent;
    if (!result.endsWith(QLatin1Char('/'))) {
        result += QLatin1Char('/');
    }
    result += info.fileName();
    return result;
}

bool isWritePathAllowed(const QString &path)
{
    const QString canonical = canonicalize(path);
    if (canonical.isEmpty()) {
        return false;
    }
    for (const auto &rule : writeRules()) {
        if (matchesWrite(canonical, rule)) {
            return true;
        }
    }
    return false;
}

bool isReadDirAllowed(const QString &dirPath)
{
    const QString canonical = canonicalize(dirPath);
    return !canonical.isEmpty() && readDirs().contains(canonical);
}

bool isReadFileAllowed(const QString &filePath)
{
    const QString canonical = canonicalize(filePath);
    if (canonical.isEmpty()) {
        return false;
    }
    const QString parent = canonical.left(canonical.lastIndexOf(QLatin1Char('/')));
    return readDirs().contains(parent);
}

// --- Replaced in Task 2 ---
bool isExecAllowed(const QString &, const QStringList &)
{
    return false;
}

std::optional<PostCommand> postCommandFor(const QString &)
{
    return std::nullopt;
}

} // namespace Fls::Policy
```

- [ ] **Step 3: Add the static library to `helper/CMakeLists.txt`**

Insert at the **top** of `helper/CMakeLists.txt` (before `add_executable(fls-helper ...)`):

```cmake
add_library(fls-helperpolicy STATIC
    helperpolicy.cpp
    helperpolicy.h
)
set_target_properties(fls-helperpolicy PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(fls-helperpolicy PUBLIC Qt6::Core)
target_include_directories(fls-helperpolicy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
```

Then add `fls-helperpolicy` to the existing `target_link_libraries(fls-helper ...)` list (it will be used in Task 3). After this step `fls-helper` links: `fls-common fls-helperpolicy KF6::AuthCore Qt6::Core Qt6::DBus`.

- [ ] **Step 4: Create the failing test `tests/helper/test_helperpolicy.cpp`**

```cpp
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "helperpolicy.h"

using namespace Fls::Policy;

class TestHelperPolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testWritePathsAllowed()
    {
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/default/grub")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/fstab")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/hosts")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/environment")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/systemd/resolved.conf")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/systemd/zram-generator.conf")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/sysctl.d/99-fls.conf")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/modprobe.d/fls-blacklist.conf")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/sudoers.d/myrule")));
        QVERIFY(isWritePathAllowed(QStringLiteral("/etc/yum.repos.d/copr.repo")));
    }

    void testWritePathsRejected()
    {
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/shadow")));
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/hosts.evil")));          // prefix bypass
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/sysctl.d/../shadow")));  // traversal
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/sysctl.d/")));           // dir itself, not a child
        QVERIFY(!isWritePathAllowed(QStringLiteral("etc/fstab")));               // relative
        QVERIFY(!isWritePathAllowed(QString()));                                  // empty
        QVERIFY(!isWritePathAllowed(QStringLiteral("/etc/selinux/config")));      // not a write target
    }

    void testReadDirsAllowed()
    {
        QVERIFY(isReadDirAllowed(QStringLiteral("/boot/loader/entries/")));
        QVERIFY(isReadDirAllowed(QStringLiteral("/etc/sudoers.d/")));
        QVERIFY(isReadDirAllowed(QStringLiteral("/etc/sudoers.d")));   // trailing slash optional
    }

    void testReadRejected()
    {
        QVERIFY(!isReadDirAllowed(QStringLiteral("/etc")));
        QVERIFY(!isReadDirAllowed(QStringLiteral("/root")));
        QVERIFY(!isReadFileAllowed(QStringLiteral("/etc/shadow")));
        QVERIFY(!isReadFileAllowed(QStringLiteral("/etc/passwd")));
        QVERIFY(isReadFileAllowed(QStringLiteral("/etc/sudoers.d/myrule"))); // direct child OK
    }

    void testCanonicalizeResolvesSymlinks()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString real = tmp.path() + QStringLiteral("/realdir");
        QVERIFY(QDir().mkpath(real));
        const QString link = tmp.path() + QStringLiteral("/link");
        QVERIFY(QFile::link(real, link)); // link -> realdir
        const QString canon = canonicalize(link + QStringLiteral("/file.conf"));
        QCOMPARE(canon, real + QStringLiteral("/file.conf"));
    }

    void testCanonicalizeRejectsNonexistentParent()
    {
        QVERIFY(canonicalize(QStringLiteral("/nonexistent-xyz-12345/foo")).isEmpty());
    }
};

QTEST_MAIN(TestHelperPolicy)
#include "test_helperpolicy.moc"
```

- [ ] **Step 5: Wire the test into `tests/CMakeLists.txt`**

Append:

```cmake
add_executable(test_helperpolicy helper/test_helperpolicy.cpp)
target_link_libraries(test_helperpolicy
    fls-helperpolicy
    Qt6::Test
)
add_test(NAME test_helperpolicy COMMAND test_helperpolicy)
```

- [ ] **Step 6: Configure + build + run, verify the path tests pass**

Run:
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)
ctest --test-dir build -R test_helperpolicy --output-on-failure
```
Expected: `test_helperpolicy` PASS (all path/canonicalize cases). `test_backupmanager` and `test_configparser` still build.

- [ ] **Step 7: Commit**

```bash
git add helper/helperpolicy.h helper/helperpolicy.cpp tests/helper/test_helperpolicy.cpp helper/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(helper): add fls-helperpolicy with path validation"
```

---

### Task 2: `fls-helperpolicy` — command/argument validation + post-command map

**Files:**
- Modify: `helper/helperpolicy.cpp` (replace the two stubs from Task 1)
- Modify: `tests/helper/test_helperpolicy.cpp` (add exec + post-command tests)

**Interfaces:**
- Consumes: the file/types from Task 1.
- Produces: working `isExecAllowed` and `postCommandFor` (signatures already in the header).

- [ ] **Step 1: Add the failing tests to `tests/helper/test_helperpolicy.cpp`**

Insert these slots inside the `private Q_SLOTS:` section (before the closing `};`):

```cpp
    void testExecAllowed()
    {
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/grubby"),
            {QStringLiteral("--set-default"), QStringLiteral("/boot/vmlinuz-6.14.0-1.fc44.x86_64")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("copr"), QStringLiteral("enable"), QStringLiteral("user/project"), QStringLiteral("-y")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("copr"), QStringLiteral("disable"), QStringLiteral("user/project"), QStringLiteral("-y")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/bin/dnf5"), {QStringLiteral("makecache")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/modprobe"), {QStringLiteral("kvm_intel")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/rmmod"), {QStringLiteral("pcspkr")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/setenforce"), {QStringLiteral("0")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/setenforce"), {QStringLiteral("1")}));
        QVERIFY(isExecAllowed(QStringLiteral("/usr/sbin/setsebool"),
            {QStringLiteral("-P"), QStringLiteral("httpd_can_network_connect"), QStringLiteral("on")}));
    }

    void testExecRejected()
    {
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("install"), QStringLiteral("evil")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/bin/dnf5"),
            {QStringLiteral("copr"), QStringLiteral("enable"), QStringLiteral("a; rm -rf /"), QStringLiteral("-y")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/modprobe"), {QStringLiteral("../evil")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/rmmod"),
            {QStringLiteral("a"), QStringLiteral("b")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/setenforce"), {QStringLiteral("2")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/setsebool"),
            {QStringLiteral("httpd_can_network_connect"), QStringLiteral("on")})); // missing -P
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/bin/systemctl"),
            {QStringLiteral("link"), QStringLiteral("/x")}));                      // systemctl not in execute set
        QVERIFY(!isExecAllowed(QStringLiteral("/usr/sbin/grubby"),
            {QStringLiteral("--update-kernel=ALL")}));
        QVERIFY(!isExecAllowed(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("evil")}));
    }

    void testPostCommandFor()
    {
        const auto grub = postCommandFor(QStringLiteral("/etc/default/grub"));
        QVERIFY(grub.has_value());
        QCOMPARE(grub->binary, QStringLiteral("/usr/sbin/grub2-mkconfig"));
        QCOMPARE(grub->args, (QStringList{QStringLiteral("-o"), QStringLiteral("/boot/grub2/grub.cfg")}));

        const auto fstab = postCommandFor(QStringLiteral("/etc/fstab"));
        QVERIFY(fstab.has_value());
        QCOMPARE(fstab->binary, QStringLiteral("/usr/bin/systemctl"));
        QCOMPARE(fstab->args, (QStringList{QStringLiteral("daemon-reload")}));

        const auto resolved = postCommandFor(QStringLiteral("/etc/systemd/resolved.conf"));
        QVERIFY(resolved.has_value());
        QCOMPARE(resolved->args, (QStringList{QStringLiteral("restart"), QStringLiteral("systemd-resolved")}));

        const auto sysctl = postCommandFor(QStringLiteral("/etc/sysctl.d/99-fls.conf"));
        QVERIFY(sysctl.has_value());
        QCOMPARE(sysctl->binary, QStringLiteral("/usr/sbin/sysctl"));
        QCOMPARE(sysctl->args, (QStringList{QStringLiteral("--system")}));

        QVERIFY(!postCommandFor(QStringLiteral("/etc/hosts")).has_value());
    }
```

- [ ] **Step 2: Run the test, verify the new cases FAIL**

Run:
```bash
cmake --build build -j$(nproc)
ctest --test-dir build -R test_helperpolicy --output-on-failure
```
Expected: FAIL — `isExecAllowed` stub returns false (allowed cases fail) and `postCommandFor` stub returns nullopt (post-command cases fail).

- [ ] **Step 3: Replace the two stubs in `helper/helperpolicy.cpp`**

Add these regex helpers inside the existing anonymous `namespace { ... }` (after `matchesWrite`):

```cpp
bool isModuleName(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9._-]+$"));
    return re.match(s).hasMatch();
}

bool isCoprProject(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9._@/-]+$"));
    return re.match(s).hasMatch();
}

bool isKernelImage(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^/boot/vmlinuz-[A-Za-z0-9._+-]+$"));
    return re.match(s).hasMatch();
}
```

Replace the `isExecAllowed` stub with:

```cpp
bool isExecAllowed(const QString &binary, const QStringList &args)
{
    if (binary == QStringLiteral("/usr/sbin/grubby")) {
        return args.size() == 2
            && args.at(0) == QStringLiteral("--set-default")
            && isKernelImage(args.at(1));
    }
    if (binary == QStringLiteral("/usr/bin/dnf5")) {
        if (args == QStringList{QStringLiteral("makecache")}) {
            return true;
        }
        return args.size() == 4
            && args.at(0) == QStringLiteral("copr")
            && (args.at(1) == QStringLiteral("enable") || args.at(1) == QStringLiteral("disable"))
            && isCoprProject(args.at(2))
            && args.at(3) == QStringLiteral("-y");
    }
    if (binary == QStringLiteral("/usr/sbin/modprobe") || binary == QStringLiteral("/usr/sbin/rmmod")) {
        return args.size() == 1 && isModuleName(args.at(0));
    }
    if (binary == QStringLiteral("/usr/sbin/setenforce")) {
        return args == QStringList{QStringLiteral("0")} || args == QStringList{QStringLiteral("1")};
    }
    if (binary == QStringLiteral("/usr/sbin/setsebool")) {
        return args.size() == 3
            && args.at(0) == QStringLiteral("-P")
            && isModuleName(args.at(1))
            && (args.at(2) == QStringLiteral("on") || args.at(2) == QStringLiteral("off"));
    }
    return false;
}
```

Replace the `postCommandFor` stub with:

```cpp
std::optional<PostCommand> postCommandFor(const QString &canonicalPath)
{
    if (canonicalPath == QStringLiteral("/etc/default/grub")) {
        return PostCommand{QStringLiteral("/usr/sbin/grub2-mkconfig"),
                           {QStringLiteral("-o"), QStringLiteral("/boot/grub2/grub.cfg")}};
    }
    if (canonicalPath == QStringLiteral("/etc/fstab")) {
        return PostCommand{QStringLiteral("/usr/bin/systemctl"),
                           {QStringLiteral("daemon-reload")}};
    }
    if (canonicalPath == QStringLiteral("/etc/systemd/resolved.conf")) {
        return PostCommand{QStringLiteral("/usr/bin/systemctl"),
                           {QStringLiteral("restart"), QStringLiteral("systemd-resolved")}};
    }
    if (canonicalPath == QStringLiteral("/etc/sysctl.d/99-fls.conf")) {
        return PostCommand{QStringLiteral("/usr/sbin/sysctl"),
                           {QStringLiteral("--system")}};
    }
    return std::nullopt;
}
```

- [ ] **Step 4: Run the full policy test, verify PASS**

Run:
```bash
cmake --build build -j$(nproc)
ctest --test-dir build -R test_helperpolicy --output-on-failure
```
Expected: PASS (all path, exec, and post-command cases).

- [ ] **Step 5: Commit**

```bash
git add helper/helperpolicy.cpp tests/helper/test_helperpolicy.cpp
git commit -m "feat(helper): validate command arguments and map post-commands"
```

---

### Task 3: Integrate the policy into `flshelper.cpp`

**Files:**
- Modify: `helper/flshelper.h` (add `runProcess` declaration)
- Modify: `helper/flshelper.cpp` (use policy in `write`, `read`, `runCommand`; factor `runProcess`)

**Interfaces:**
- Consumes: all `Fls::Policy::*` functions from Tasks 1-2.
- Produces: hardened action slots. Action argument contract unchanged except `write` now **ignores** caller-supplied `postCommand`/`postArgs`.

- [ ] **Step 1: Add the `runProcess` declaration to `helper/flshelper.h`**

In the `private:` section, add above `runCommand`:

```cpp
    ActionReply runProcess(const QString &command, const QStringList &args);
```

- [ ] **Step 2: Add the policy include to `helper/flshelper.cpp`**

Below `#include "backupmanager.h"` add:

```cpp
#include "helperpolicy.h"
```

- [ ] **Step 3: Replace `runCommand` with a `runProcess` + validated `runCommand` pair**

Replace the entire existing `FlsHelper::runCommand` function (the one that does the binary whitelist check and `QProcess`) with:

```cpp
ActionReply FlsHelper::runProcess(const QString &command, const QStringList &args)
{
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

ActionReply FlsHelper::runCommand(const QString &command, const QStringList &args)
{
    if (!Fls::Policy::isExecAllowed(command, args)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Command not permitted: %1").arg(command));
        return reply;
    }
    return runProcess(command, args);
}
```

Also delete the now-unused file-scope `allowedCommands` list and `isCommandAllowed` function, and the `allowedPathPrefixes` list and `isPathAllowed` function (all superseded by the policy library). Leave `validateSudoers` and `writeFileWithBackup` intact (but see Step 4 for `writeFileWithBackup`'s caller using the canonical path).

- [ ] **Step 4: Replace `FlsHelper::write` to validate the path and derive the post-command server-side**

Replace the entire `FlsHelper::write` function with:

```cpp
ActionReply FlsHelper::write(const QVariantMap &args)
{
    const QString filePath = args.value(QStringLiteral("filePath")).toString();
    const QByteArray content = args.value(QStringLiteral("content")).toByteArray();
    const bool validateAsSudoers = args.value(QStringLiteral("validateAsSudoers"), false).toBool();

    if (filePath.isEmpty() || content.isEmpty()) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("filePath and content are required"));
        return reply;
    }

    const QString canonical = Fls::Policy::canonicalize(filePath);
    if (canonical.isEmpty() || !Fls::Policy::isWritePathAllowed(filePath)) {
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

    // Write to the canonical (symlink-resolved) path, with automatic backup.
    ActionReply writeReply = writeFileWithBackup(canonical, content);
    if (writeReply.failed()) {
        return writeReply;
    }

    // Post-command is derived server-side from the validated path; callers
    // cannot choose it.
    const auto post = Fls::Policy::postCommandFor(canonical);
    if (post.has_value()) {
        ActionReply cmdReply = runProcess(post->binary, post->args);
        if (cmdReply.failed() || cmdReply.data().value(QStringLiteral("exitCode")).toInt() != 0) {
            const QString backupPath = writeReply.data().value(QStringLiteral("backupPath")).toString();
            if (!backupPath.isEmpty()) {
                BackupManager::restore(backupPath, canonical);
            }

            auto reply = ActionReply::HelperErrorReply();
            const QString stderr = cmdReply.data().value(QStringLiteral("stderr")).toString();
            reply.setErrorDescription(
                QStringLiteral("Post-command failed (%1), restored backup. stderr: %2")
                    .arg(post->binary, stderr));
            return reply;
        }

        writeReply.addData(QStringLiteral("postStdout"), cmdReply.data().value(QStringLiteral("stdout")));
        writeReply.addData(QStringLiteral("postStderr"), cmdReply.data().value(QStringLiteral("stderr")));
    }

    return writeReply;
}
```

- [ ] **Step 5: Replace `FlsHelper::read` to enforce the read whitelist**

Replace the entire `FlsHelper::read` function with:

```cpp
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
        if (!Fls::Policy::isReadFileAllowed(filePath)) {
            auto reply = ActionReply::HelperErrorReply();
            reply.setErrorDescription(QStringLiteral("Read path not in whitelist: %1").arg(filePath));
            return reply;
        }
        QFile file(Fls::Policy::canonicalize(filePath));
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
    if (!Fls::Policy::isReadDirAllowed(dirPath)) {
        auto reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Read directory not in whitelist: %1").arg(dirPath));
        return reply;
    }

    QDir dir(Fls::Policy::canonicalize(dirPath));
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
```

- [ ] **Step 6: Build and run the full suite, verify all green**

Run:
```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```
Expected: `test_helperpolicy`, `test_backupmanager`, `test_configparser`, `appstreamtest` all PASS; full compile clean (no references to the deleted `allowedCommands`/`isPathAllowed`).

- [ ] **Step 7: Commit**

```bash
git add helper/flshelper.h helper/flshelper.cpp
git commit -m "feat(helper): enforce policy on read/write/execute, derive post-commands"
```

---

### Task 4: Simplify the four module write call sites

**Files:**
- Modify: `kcms/sysctl/sysctlkcm.cpp` (remove `postCommand`/`postArgs` lines)
- Modify: `kcms/fstab/fstabkcm.cpp`
- Modify: `kcms/hostname/hostnamekcm.cpp`
- Modify: `kcms/bootloader/bootloaderkcm.cpp`

**Interfaces:**
- Consumes: the server-side post-command behavior from Task 3 (`write` now runs the post-command itself).
- Produces: no behavior change — the helper derives the same post-commands.

- [ ] **Step 1: Remove the post-command lines from each module**

In each file, delete only the two lines that set `postCommand` and `postArgs` in the args map passed to `org.kde.fls.write`. The `filePath`/`content`/`validateAsSudoers` lines stay. Exact lines to remove:

`kcms/sysctl/sysctlkcm.cpp`:
```cpp
    args[QStringLiteral("postCommand")] = QStringLiteral("/usr/sbin/sysctl");
    args[QStringLiteral("postArgs")] = QStringList{QStringLiteral("--system")};
```

`kcms/fstab/fstabkcm.cpp`:
```cpp
    args[QStringLiteral("postCommand")] = QStringLiteral("/usr/bin/systemctl");
    args[QStringLiteral("postArgs")] = QStringList{QStringLiteral("daemon-reload")};
```

`kcms/hostname/hostnamekcm.cpp`:
```cpp
    args[QStringLiteral("postCommand")] = QStringLiteral("/usr/bin/systemctl");
    args[QStringLiteral("postArgs")] = QStringList{QStringLiteral("restart"), QStringLiteral("systemd-resolved")};
```

`kcms/bootloader/bootloaderkcm.cpp`:
```cpp
    args[QStringLiteral("postCommand")] = QStringLiteral("/usr/sbin/grub2-mkconfig");
    args[QStringLiteral("postArgs")] = QStringList{QStringLiteral("-o"), QStringLiteral("/boot/grub2/grub.cfg")};
```

- [ ] **Step 2: Verify no `postCommand`/`postArgs` remain anywhere**

Run:
```bash
git grep -n 'postCommand\|postArgs' -- 'kcms/*.cpp'
```
Expected: no output.

- [ ] **Step 3: Full build + tests**

Run:
```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```
Expected: clean build, all tests PASS.

- [ ] **Step 4: Commit**

```bash
git add kcms/sysctl/sysctlkcm.cpp kcms/fstab/fstabkcm.cpp kcms/hostname/hostnamekcm.cpp kcms/bootloader/bootloaderkcm.cpp
git commit -m "refactor(kcms): drop caller post-commands (now derived server-side)"
```

---

## Delivery (after all tasks pass review)

Not a TDD task — performed by the controller after merge:

1. Reinstall on the live Fedora 44 system (current install is the vulnerable version):
   ```bash
   sudo cmake --install build
   kbuildsycoca6
   ```
2. Merge `harden-helper` → `main`, push to `origin`.
3. Tag `v0.2.1` (security fix) and push the tag.
