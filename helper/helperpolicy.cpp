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
