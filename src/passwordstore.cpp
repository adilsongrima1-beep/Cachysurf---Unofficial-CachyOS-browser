#include "passwordstore.h"

#include <QSettings>
#include <QUuid>
#include <qt6keychain/keychain.h>

namespace {
constexpr auto kServiceName = "CachySurf";
QString keyForId(const QString &id)
{
    return QStringLiteral("login/") + id;
}
}

PasswordStore::PasswordStore(QObject *parent)
    : QObject(parent)
{
    loadMetadata();
}

QVector<PasswordEntry> PasswordStore::entries() const
{
    return m_entries;
}

void PasswordStore::save(const QString &host,
                         const QString &username,
                         const QString &password,
                         std::function<void(bool, const QString &)> callback)
{
    upsert(host, username, password,
           [callback = std::move(callback)](bool ok, bool, const QString &error) {
        callback(ok, error);
    });
}

void PasswordStore::upsert(const QString &host,
                           const QString &username,
                           const QString &password,
                           std::function<void(bool, bool, const QString &)> callback)
{
    const QString normalizedHost = host.trimmed().toLower();
    const QString normalizedUsername = username.trimmed();

    int existingIndex = -1;
    for (int index = 0; index < m_entries.size(); ++index) {
        const PasswordEntry &candidate = m_entries.at(index);
        if (candidate.host.compare(normalizedHost, Qt::CaseInsensitive) == 0
            && candidate.username == normalizedUsername) {
            existingIndex = index;
            break;
        }
    }

    PasswordEntry entry;
    if (existingIndex >= 0)
        entry = m_entries.at(existingIndex);
    else
        entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.host = normalizedHost;
    entry.username = normalizedUsername;

    auto *job = new QKeychain::WritePasswordJob(QString::fromLatin1(kServiceName), this);
    job->setKey(keyForId(entry.id));
    job->setTextData(password);
    job->setAutoDelete(true);
    connect(job, &QKeychain::Job::finished, this,
            [this, job, entry, existingIndex, callback = std::move(callback)] {
        if (job->error()) {
            callback(false, false, job->errorString());
            return;
        }
        const bool updated = existingIndex >= 0;
        if (updated)
            m_entries[existingIndex] = entry;
        else
            m_entries.append(entry);
        saveMetadata();
        callback(true, updated, QString());
    });
    job->start();
}

void PasswordStore::read(const QString &id,
                         std::function<void(bool, const QString &, const QString &)> callback)
{
    const int index = indexForId(id);
    if (index < 0) {
        callback(false, QString(), QStringLiteral("Password entry not found."));
        return;
    }

    auto *job = new QKeychain::ReadPasswordJob(QString::fromLatin1(kServiceName), this);
    job->setKey(keyForId(id));
    job->setAutoDelete(true);
    connect(job, &QKeychain::Job::finished, this, [job, callback = std::move(callback)] {
        if (job->error()) {
            callback(false, QString(), job->errorString());
            return;
        }
        callback(true, job->textData(), QString());
    });
    job->start();
}

void PasswordStore::remove(const QString &id,
                           std::function<void(bool, const QString &)> callback)
{
    const int index = indexForId(id);
    if (index < 0) {
        callback(false, QStringLiteral("Password entry not found."));
        return;
    }

    auto *job = new QKeychain::DeletePasswordJob(QString::fromLatin1(kServiceName), this);
    job->setKey(keyForId(id));
    job->setAutoDelete(true);
    connect(job, &QKeychain::Job::finished, this, [this, job, id, callback = std::move(callback)] {
        if (job->error()) {
            callback(false, job->errorString());
            return;
        }
        const int currentIndex = indexForId(id);
        if (currentIndex >= 0)
            m_entries.removeAt(currentIndex);
        saveMetadata();
        callback(true, QString());
    });
    job->start();
}

void PasswordStore::loadMetadata()
{
    QSettings settings;
    const int count = settings.beginReadArray(QStringLiteral("passwords/entries"));
    for (int index = 0; index < count; ++index) {
        settings.setArrayIndex(index);
        PasswordEntry entry;
        entry.id = settings.value(QStringLiteral("id")).toString();
        entry.host = settings.value(QStringLiteral("host")).toString();
        entry.username = settings.value(QStringLiteral("username")).toString();
        if (!entry.id.isEmpty() && !entry.host.isEmpty())
            m_entries.append(entry);
    }
    settings.endArray();
}

void PasswordStore::saveMetadata() const
{
    QSettings settings;
    settings.beginWriteArray(QStringLiteral("passwords/entries"));
    for (int index = 0; index < m_entries.size(); ++index) {
        settings.setArrayIndex(index);
        const PasswordEntry &entry = m_entries.at(index);
        settings.setValue(QStringLiteral("id"), entry.id);
        settings.setValue(QStringLiteral("host"), entry.host);
        settings.setValue(QStringLiteral("username"), entry.username);
    }
    settings.endArray();
}

int PasswordStore::indexForId(const QString &id) const
{
    for (int index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).id == id)
            return index;
    }
    return -1;
}
