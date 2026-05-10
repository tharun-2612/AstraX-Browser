#include "HistoryStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace
{
QString historyKey(QUrl url)
{
    url.setFragment({});
    return url.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
}
}

namespace astra
{
HistoryStore::HistoryStore(QString filePath, QObject *parent)
    : QObject(parent)
    , m_filePath(std::move(filePath))
{
}

const QVector<HistoryEntry> &HistoryStore::entries() const
{
    return m_entries;
}

bool HistoryStore::load()
{
    QFile file(m_filePath);
    if (!file.exists()) {
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
        return false;
    }

    QVector<HistoryEntry> loaded;
    const QJsonArray entries = document.array();
    loaded.reserve(entries.size());

    for (const QJsonValue &value : entries) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        const QUrl url(object.value(QStringLiteral("url")).toString());
        if (!url.isValid() || url.isEmpty()) {
            continue;
        }

        loaded.push_back(HistoryEntry{
            object.value(QStringLiteral("title")).toString(url.toDisplayString()),
            url,
            QDateTime::fromString(object.value(QStringLiteral("visitedAt")).toString(), Qt::ISODate),
        });
    }

    m_entries = std::move(loaded);
    emit changed();
    return true;
}

bool HistoryStore::save() const
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    QJsonArray entries;
    for (const HistoryEntry &entry : m_entries) {
        QJsonObject object;
        object.insert(QStringLiteral("title"), entry.title);
        object.insert(QStringLiteral("url"), entry.url.toString());
        object.insert(QStringLiteral("visitedAt"), entry.visitedAt.toUTC().toString(Qt::ISODate));
        entries.push_back(object);
    }

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(QJsonDocument(entries).toJson(QJsonDocument::Indented));
    return file.commit();
}

void HistoryStore::appendVisit(QString title, QUrl url)
{
    if (!url.isValid() || url.isEmpty() || url.scheme() == QStringLiteral("about")) {
        return;
    }

    if (title.trimmed().isEmpty()) {
        title = url.toDisplayString();
    }

    const QString key = historyKey(url);
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](const HistoryEntry &entry) {
        return historyKey(entry.url) == key;
    }), m_entries.end());

    m_entries.prepend(HistoryEntry{std::move(title), std::move(url), QDateTime::currentDateTimeUtc()});
    while (m_entries.size() > MaxEntries) {
        m_entries.removeLast();
    }

    save();
    emit changed();
}

void HistoryStore::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }

    m_entries.clear();
    save();
    emit changed();
}
}
