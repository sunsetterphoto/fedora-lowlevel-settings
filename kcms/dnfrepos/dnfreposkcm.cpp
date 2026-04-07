// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#include "dnfreposkcm.h"
#include "configparser.h"

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KPluginFactory>

#include <QDir>
#include <QFile>

DnfReposKcm::DnfReposKcm(QObject *parent, const KPluginMetaData &metaData)
    : KQuickConfigModule(parent, metaData)
{
    setButtons(NoAdditionalButton);
    load();
}

void DnfReposKcm::setSearchText(const QString &text)
{
    if (m_searchText != text) {
        m_searchText = text;
        Q_EMIT searchTextChanged();
        applyFilter();
    }
}

void DnfReposKcm::load()
{
    m_allRepos.clear();

    QDir repoDir(QStringLiteral("/etc/yum.repos.d"));
    if (!repoDir.exists()) {
        qWarning() << "/etc/yum.repos.d does not exist";
        applyFilter();
        return;
    }

    const QStringList repoFiles = repoDir.entryList(
        {QStringLiteral("*.repo")}, QDir::Files | QDir::Readable, QDir::Name);

    for (const QString &filename : repoFiles) {
        const QString filePath = repoDir.absoluteFilePath(filename);
        const auto sections = NsaFsm::ConfigParser::parseIni(filePath);

        for (auto it = sections.constBegin(); it != sections.constEnd(); ++it) {
            const QString &repoId = it.key();
            const auto &kvMap = it.value();

            QVariantMap repo;
            repo[QStringLiteral("id")] = repoId;
            repo[QStringLiteral("name")] = kvMap.value(QStringLiteral("name"), repoId);
            repo[QStringLiteral("baseurl")] = kvMap.value(QStringLiteral("baseurl"));
            repo[QStringLiteral("metalink")] = kvMap.value(QStringLiteral("metalink"));
            repo[QStringLiteral("enabled")] = (kvMap.value(QStringLiteral("enabled"), QStringLiteral("1")) == QLatin1String("1"));
            repo[QStringLiteral("gpgcheck")] = (kvMap.value(QStringLiteral("gpgcheck"), QStringLiteral("0")) == QLatin1String("1"));
            repo[QStringLiteral("gpgkey")] = kvMap.value(QStringLiteral("gpgkey"));
            repo[QStringLiteral("filename")] = filename;

            // Detect COPR repos
            const bool isCopr = repoId.contains(QStringLiteral("copr"), Qt::CaseInsensitive)
                             || kvMap.value(QStringLiteral("name")).contains(QStringLiteral("copr"), Qt::CaseInsensitive);
            repo[QStringLiteral("isCopr")] = isCopr;

            m_allRepos.append(repo);
        }
    }

    // Sort by name
    std::sort(m_allRepos.begin(), m_allRepos.end(),
        [](const QVariant &a, const QVariant &b) {
            const QString nameA = a.toMap().value(QStringLiteral("name")).toString();
            const QString nameB = b.toMap().value(QStringLiteral("name")).toString();
            return nameA.compare(nameB, Qt::CaseInsensitive) < 0;
        });

    applyFilter();
}

void DnfReposKcm::applyFilter()
{
    if (m_searchText.isEmpty()) {
        m_filteredRepos = m_allRepos;
    } else {
        m_filteredRepos.clear();
        for (const QVariant &repo : std::as_const(m_allRepos)) {
            const QVariantMap map = repo.toMap();
            const QString name = map.value(QStringLiteral("name")).toString();
            const QString id = map.value(QStringLiteral("id")).toString();
            if (name.contains(m_searchText, Qt::CaseInsensitive)
                || id.contains(m_searchText, Qt::CaseInsensitive)) {
                m_filteredRepos.append(repo);
            }
        }
    }
    Q_EMIT reposChanged();
}

void DnfReposKcm::toggleRepo(const QString &filename, const QString &repoId, bool enabled)
{
    const QString filePath = QStringLiteral("/etc/yum.repos.d/") + filename;

    // Read the entire .repo file, modify the relevant section, write back
    auto sections = NsaFsm::ConfigParser::parseIni(filePath);

    if (!sections.contains(repoId)) {
        qWarning() << "Repo section" << repoId << "not found in" << filePath;
        return;
    }

    sections[repoId][QStringLiteral("enabled")] = enabled ? QStringLiteral("1") : QStringLiteral("0");

    // Serialize the modified INI content to a string
    QString content;
    bool first = true;
    for (auto secIt = sections.constBegin(); secIt != sections.constEnd(); ++secIt) {
        if (!first) {
            content += QLatin1Char('\n');
        }
        first = false;

        content += QLatin1Char('[') + secIt.key() + QStringLiteral("]\n");
        const auto &kvMap = secIt.value();
        for (auto kvIt = kvMap.constBegin(); kvIt != kvMap.constEnd(); ++kvIt) {
            content += kvIt.key() + QStringLiteral(" = ") + kvIt.value() + QLatin1Char('\n');
        }
    }

    QVariantMap args;
    args[QStringLiteral("filePath")] = filePath;
    args[QStringLiteral("content")] = content.toUtf8();

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.write"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "KAuth write failed (toggleRepo):" << job->errorString();
        } else {
            refresh();
        }
    });
    job->start();
}

void DnfReposKcm::addCoprRepo(const QString &project)
{
    if (project.isEmpty()) {
        return;
    }

    QVariantMap args;
    args[QStringLiteral("command")] = QStringLiteral("/usr/bin/dnf5");
    args[QStringLiteral("args")] = QStringList{
        QStringLiteral("copr"),
        QStringLiteral("enable"),
        project,
        QStringLiteral("-y")
    };

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.execute"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job, project] {
        if (job->error()) {
            qWarning() << "Failed to enable COPR" << project << ":" << job->errorString();
        } else {
            const int exitCode = job->data().value(QStringLiteral("exitCode")).toInt();
            if (exitCode != 0) {
                qWarning() << "dnf5 copr enable" << project << "exited with code" << exitCode
                           << QString::fromUtf8(job->data().value(QStringLiteral("stderr")).toByteArray());
            } else {
                qInfo() << "COPR" << project << "enabled successfully";
            }
            refresh();
        }
    });
    job->start();
}

void DnfReposKcm::removeCoprRepo(const QString &project)
{
    if (project.isEmpty()) {
        return;
    }

    QVariantMap args;
    args[QStringLiteral("command")] = QStringLiteral("/usr/bin/dnf5");
    args[QStringLiteral("args")] = QStringList{
        QStringLiteral("copr"),
        QStringLiteral("disable"),
        project,
        QStringLiteral("-y")
    };

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.execute"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job, project] {
        if (job->error()) {
            qWarning() << "Failed to disable COPR" << project << ":" << job->errorString();
        } else {
            const int exitCode = job->data().value(QStringLiteral("exitCode")).toInt();
            if (exitCode != 0) {
                qWarning() << "dnf5 copr disable" << project << "exited with code" << exitCode
                           << QString::fromUtf8(job->data().value(QStringLiteral("stderr")).toByteArray());
            } else {
                qInfo() << "COPR" << project << "disabled successfully";
            }
            refresh();
        }
    });
    job->start();
}

void DnfReposKcm::refreshCache()
{
    QVariantMap args;
    args[QStringLiteral("command")] = QStringLiteral("/usr/bin/dnf5");
    args[QStringLiteral("args")] = QStringList{QStringLiteral("makecache")};

    KAuth::Action action(QStringLiteral("org.kde.nsafsm.execute"));
    action.setHelperId(QStringLiteral("org.kde.nsafsm"));
    action.setArguments(args);

    KAuth::ExecuteJob *job = action.execute();
    connect(job, &KJob::result, this, [this, job] {
        if (job->error()) {
            qWarning() << "Failed to refresh DNF cache:" << job->errorString();
        } else {
            const int exitCode = job->data().value(QStringLiteral("exitCode")).toInt();
            if (exitCode != 0) {
                qWarning() << "dnf5 makecache exited with code" << exitCode
                           << QString::fromUtf8(job->data().value(QStringLiteral("stderr")).toByteArray());
            } else {
                qInfo() << "DNF cache refreshed successfully";
            }
        }
    });
    job->start();
}

void DnfReposKcm::refresh()
{
    load();
}

K_PLUGIN_CLASS_WITH_JSON(DnfReposKcm, "kcm_nsa_dnfrepos.json")

#include "dnfreposkcm.moc"
