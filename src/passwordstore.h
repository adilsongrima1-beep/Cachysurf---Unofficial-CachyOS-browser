#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

struct PasswordEntry
{
    QString id;
    QString host;
    QString username;
};

class PasswordStore final : public QObject
{
    Q_OBJECT

public:
    explicit PasswordStore(QObject *parent = nullptr);

    QVector<PasswordEntry> entries() const;
    void save(const QString &host,
              const QString &username,
              const QString &password,
              std::function<void(bool, const QString &)> callback);
    void upsert(const QString &host,
                const QString &username,
                const QString &password,
                std::function<void(bool, bool, const QString &)> callback);
    void read(const QString &id,
              std::function<void(bool, const QString &, const QString &)> callback);
    void remove(const QString &id,
                std::function<void(bool, const QString &)> callback);

private:
    void loadMetadata();
    void saveMetadata() const;
    int indexForId(const QString &id) const;

    QVector<PasswordEntry> m_entries;
};
