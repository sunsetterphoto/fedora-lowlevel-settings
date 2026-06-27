# Harden the fls-helper (privileged KAuth helper) — Design

**Goal:** Close three confirmed privilege-escalation weaknesses in
`helper/flshelper.cpp` (the root-running KAuth helper) by enforcing a
whitelist-by-default policy on every path, command, and argument it accepts —
without changing the KAuth action schema or breaking any of the 12 KCM modules.

## Background — the vulnerabilities

The helper runs as root and exposes 5 generic actions
(`org.kde.fls.{write,read,execute,backup,restore}`, polkit `auth_admin`).
Three issues were confirmed by reading the code:

1. **Arbitrary file read as root (CRITICAL).** `FlsHelper::read()` has *no*
   path check. Any caller passing the `auth_admin` gate can read any file
   (`/etc/shadow`, private keys) via `filePath`, or any directory via
   `dirPath`.
2. **Arbitrary command execution as root (CRITICAL).** `runCommand()` checks
   only the *binary*, not its arguments. Whitelisted binaries (`dnf5`,
   `systemctl`, `modprobe`, `grubby`, `grub2-mkconfig`) each permit full root
   code execution with attacker-chosen arguments. `write()` compounds this by
   accepting a caller-supplied `postCommand` + `postArgs`.
3. **Path prefix-match bypass (MEDIUM).** `isPathAllowed()` uses
   `startsWith()` on `absoluteFilePath()`, which does not resolve symlinks and
   matches file entries as prefixes (`/etc/hosts` also matches
   `/etc/hosts.evil`).

## Legitimate call surface (must keep working)

Mapped from all 12 modules. The hardening must allow exactly these and reject
everything else.

**`read` (directory only):** `/boot/loader/entries/`, `/etc/sudoers.d/`

**`write` (file, with server-derived post-command):**

| File | Post-command (now server-side) |
|---|---|
| `/etc/default/grub` | `grub2-mkconfig -o /boot/grub2/grub.cfg` |
| `/etc/fstab` | `systemctl daemon-reload` |
| `/etc/systemd/resolved.conf` | `systemctl restart systemd-resolved` |
| `/etc/sysctl.d/99-fls.conf` | `sysctl --system` |
| `/etc/hosts` | — |
| `/etc/environment` | — |
| `/etc/modprobe.d/fls-blacklist.conf` | — |
| `/etc/sudoers.d/<name>` (visudo-validated) | — |
| `/etc/systemd/zram-generator.conf` | — |
| `/etc/yum.repos.d/<name>` | — |

**`execute` (command + args):** `grubby --set-default /boot/vmlinuz-<v>` ·
`dnf5 copr enable|disable <project> -y` · `dnf5 makecache` ·
`modprobe <module>` · `rmmod <module>` · `setenforce <0|1>` ·
`setsebool -P <name> <on|off>`

`depmod`, `findmnt`, `systemctl`, `grub2-mkconfig`, `sysctl` are **never**
invoked through generic `execute` (the latter three are post-commands only;
the first two are unused) and are therefore dropped from the `execute`
whitelist.

## Architecture

All changes are confined to the `helper/` directory plus one new test. The
KAuth action schema (`helper/fls.actions`), the polkit policy, and 11 of 12
modules are unchanged. Only the 4 `write` call sites that pass `postCommand`
are simplified.

### Component 1 — `helper/helperpolicy.{h,cpp}` (new, pure, testable)

A new static library `fls-helperpolicy` containing **only pure functions** (no
KAuth, no root, no I/O beyond path canonicalization). This isolates all
security logic so it can be unit-tested without privileges.

```cpp
namespace Fls::Policy {

// Path classification entry
enum class PathKind { File, Dir };
struct PathRule { QString path; PathKind kind; };

// Returns canonicalized absolute path, or empty QString if the path is
// malformed (relative, contains "..") or its parent cannot be resolved.
QString canonicalize(const QString &path);

bool isWritePathAllowed(const QString &path);   // canonicalizes internally
bool isReadPathAllowed(const QString &path);    // file OR dir read targets

// Command + argument validation. Returns true iff (binary, args) matches
// exactly one allowed pattern.
bool isExecAllowed(const QString &binary, const QStringList &args);

// Server-side post-command for a validated write path.
struct PostCommand { QString binary; QStringList args; };
std::optional<PostCommand> postCommandFor(const QString &canonicalPath);

} // namespace Fls::Policy
```

### Component 2 — Path validation (fixes #1 partial, #3)

`canonicalize(path)`:
- Strip a trailing `/` first, so directory targets (`/etc/sudoers.d/`) and the
  `dirPath` read argument canonicalize the same way as files.
- Reject if `path` is empty, relative, or contains a `..` segment after
  `QDir::cleanPath`.
- Resolve the **parent directory** with `QFileInfo(parent).canonicalFilePath()`
  (follows symlinks); if the parent does not resolve (does not exist), reject.
- Return `parentCanonical + "/" + fileName`.
- This deliberately allows a **new** file whose parent exists (e.g. a new
  `/etc/sudoers.d/<name>` or `/etc/yum.repos.d/<name>`): the parent is
  canonicalized, the leaf need not pre-exist. Symlink swaps are still defeated
  because the *parent* is resolved.

Whitelist tables (in `helperpolicy.cpp`):
- `writeRules` — the 10 write targets above. `/etc/sudoers.d/`,
  `/etc/sysctl.d/`, `/etc/modprobe.d/`, `/etc/yum.repos.d/`,
  `/boot/loader/entries/` are `Dir`; the rest are `File`.
- `readRules` — the read targets: `/boot/loader/entries/` (Dir),
  `/etc/sudoers.d/` (Dir). (Read is only ever used for these two today; keep
  it minimal.)

Matching rule:
- `File`: `canonical == rule.path`.
- `Dir`: `rule.path` ends with `/` **and** `canonical.startsWith(rule.path)`
  **and** `canonical.length() > rule.path.length()` (a real child).

### Component 3 — Command + argument validation (fixes #2)

`isExecAllowed(binary, args)` switches on `binary` and checks `args` against a
structured rule. Value tokens are validated with these regexes
(`QRegularExpression`, full-string anchored):
- module name: `^[A-Za-z0-9._-]+$`
- selinux boolean name: `^[A-Za-z0-9._-]+$`
- copr project: `^[A-Za-z0-9._@/-]+$`
- kernel image (grubby): `^/boot/vmlinuz-[A-Za-z0-9._+-]+$`

Per-binary rules:
- `/usr/sbin/grubby`: `args == {"--set-default", <kernel-image>}`
- `/usr/bin/dnf5`: `args == {"copr","enable",<project>,"-y"}` OR
  `{"copr","disable",<project>,"-y"}` OR `{"makecache"}`
- `/usr/sbin/modprobe`: `args.size()==1 && <module>`
- `/usr/sbin/rmmod`: `args.size()==1 && <module>`
- `/usr/sbin/setenforce`: `args == {"0"}` OR `{"1"}`
- `/usr/sbin/setsebool`: `args == {"-P", <bool-name>, ("on"|"off")}`
- any other binary: reject.

`isExecAllowed` governs **only** the caller-facing `execute` action — exactly
these 6 binaries. `systemctl`, `grub2-mkconfig`, `sysctl` are **not** in it and
cannot be reached through `execute` at all.

The raw process runner is factored out as a private
`runProcess(binary, args)` (the existing `QProcess` body). `execute` →
`runProcess` *after* `isExecAllowed`. Post-commands (Component 4) are fixed
server-side constants and call `runProcess` **directly**, with no caller
influence, so they need no caller-validator entry. `QProcess` is invoked with a
binary + `QStringList` (no shell), so there is no shell-injection vector; the
regexes additionally constrain token *content*.

### Component 4 — Server-side post-commands (fixes #2)

`write()` ignores any caller-supplied `postCommand`/`postArgs`. After a
successful write it calls `Policy::postCommandFor(canonicalPath)`; if a
`PostCommand` is returned, it runs via `runProcess()` directly. The four
returned constants are exactly: `grub2-mkconfig {"-o","/boot/grub2/grub.cfg"}`,
`systemctl {"daemon-reload"}`, `systemctl {"restart","systemd-resolved"}`,
`sysctl {"--system"}`. Because these are server-side constants keyed off the
already-validated write path — never caller input — they are trusted and do
not pass through `isExecAllowed`. The failure/rollback path (restore backup on
non-zero exit) is preserved.

### Component 5 — Module changes (minimal)

Remove the now-ignored `postCommand`/`postArgs` lines from the 4 write call
sites: `sysctl`, `fstab`, `hostname` (resolved.conf), `bootloader` (grub).
Behavior is identical because the helper now derives them. No other module
changes. `read`/`execute`/`write` action invocations are otherwise untouched.

## Error handling

Every rejection returns `ActionReply::HelperErrorReply()` with a specific
message ("Path not in whitelist", "Command arguments not allowed",
"Read path not in whitelist"). Path validation happens **before** any file is
opened or written, so a rejected request never has side effects. Sudoers
writes keep the existing `visudo -c` validation. A failing server-side
post-command still triggers backup restore, as today.

## Testing

New `tests/helper/test_helperpolicy.cpp` (QTest), linked against
`fls-helperpolicy` only (no root needed), wired into CMake/CTest:

- **Path allow:** each of the 10 write targets and 2 read targets passes.
- **Path reject:** `/etc/shadow`, `/etc/hosts.evil` (prefix bypass),
  `/etc/sysctl.d/../shadow` (traversal), a symlinked parent escaping the
  whitelist, relative path, empty path.
- **Exec allow:** every legitimate pattern from the call-surface table.
- **Exec reject:** `dnf5 install evil`, `dnf5 copr enable x; rm` (token regex),
  `modprobe ../x`, `rmmod` with 2 args, `setenforce 2`, `setsebool` without
  `-P`, `systemctl link x`, `grubby --update-kernel=ALL`, unknown binary.
- **postCommandFor:** the 4 mapped paths return the exact command; other paths
  return `nullopt`.

Existing `test_backupmanager` and `test_configparser` must still pass.

## Delivery

1. Build, run full ctest (existing 3 + new) — all green.
2. Reinstall on the live Fedora 44 system (current install is vulnerable):
   `cmake --build build && sudo cmake --install build && kbuildsycoca6`.
3. Merge `harden-helper` → `main`, push, tag `v0.2.1` (security fix).

## Out of scope (YAGNI)

- No change to the action schema or polkit policy (the `auth_admin` gate
  stays; this work hardens what happens *after* authentication).
- No semantic-operation redesign of the helper interface (considered and
  deferred; the focused validation closes the exploitable paths).
- No new modules or features.
