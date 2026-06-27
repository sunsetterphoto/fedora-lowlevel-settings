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

} // namespace Fls::Policy
