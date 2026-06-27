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

// Like isWritePathAllowed but takes an ALREADY-canonical path (does not
// re-resolve). Validate the exact string you will operate on.
bool isWriteTargetCanonical(const QString &canonicalPath);

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
