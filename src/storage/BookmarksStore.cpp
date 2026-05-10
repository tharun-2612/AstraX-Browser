#include "BookmarksStore.h"

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
QString bookmarkKey(QUrl url)
{
    url.setFragment({});
    return url.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
}
}

namespace astra
{
BookmarksStore::BookmarksStore(QString filePath, QObject *parent)
    : QObject(parent)
    , m_filePath(std::move(filePath))
{
}

const QVector<Bookmark> &BookmarksStore::bookmarks() const
{
    return m_bookmarks;
}

bool BookmarksStore::contains(const QUrl &url) const
{
    const QString key = bookmarkKey(url);
    return std::any_of(m_bookmarks.cbegin(), m_bookmarks.cend(), [&](const Bookmark &bookmark) {
        return bookmarkKey(bookmark.url) == key;
    });
}

bool BookmarksStore::load()
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

    QVector<Bookmark> loaded;
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

        loaded.push_back(Bookmark{
            object.value(QStringLiteral("title")).toString(url.toDisplayString()),
            url,
            QDateTime::fromString(object.value(QStringLiteral("createdAt")).toString(), Qt::ISODate),
        });
    }

    m_bookmarks = std::move(loaded);
    emit changed();
    return true;
}

bool BookmarksStore::save() const
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    QJsonArray entries;
    for (const Bookmark &bookmark : m_bookmarks) {
        QJsonObject object;
        object.insert(QStringLiteral("title"), bookmark.title);
        object.insert(QStringLiteral("url"), bookmark.url.toString());
        object.insert(QStringLiteral("createdAt"), bookmark.createdAt.toUTC().toString(Qt::ISODate));
        entries.push_back(object);
    }

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(QJsonDocument(entries).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool BookmarksStore::addBookmark(QString title, QUrl url)
{
    if (!url.isValid() || url.isEmpty() || contains(url)) {
        return false;
    }

    if (title.trimmed().isEmpty()) {
        title = url.toDisplayString();
    }

    m_bookmarks.prepend(Bookmark{std::move(title), std::move(url), QDateTime::currentDateTimeUtc()});
    const bool saved = save();
    emit changed();
    return saved;
}

void BookmarksStore::removeBookmark(const QUrl &url)
{
    const QString key = bookmarkKey(url);
    const qsizetype previousSize = m_bookmarks.size();

    m_bookmarks.erase(std::remove_if(m_bookmarks.begin(), m_bookmarks.end(), [&](const Bookmark &bookmark) {
        return bookmarkKey(bookmark.url) == key;
    }), m_bookmarks.end());

    if (m_bookmarks.size() != previousSize) {
        save();
        emit changed();
    }
}
}
