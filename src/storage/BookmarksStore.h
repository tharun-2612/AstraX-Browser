#pragma once

#include <QDateTime>
#include <QObject>
#include <QUrl>
#include <QVector>

namespace astra
{
struct Bookmark
{
    QString title;
    QUrl url;
    QDateTime createdAt;
};

class BookmarksStore final : public QObject
{
    Q_OBJECT

public:
    explicit BookmarksStore(QString filePath, QObject *parent = nullptr);

    [[nodiscard]] const QVector<Bookmark> &bookmarks() const;
    [[nodiscard]] bool contains(const QUrl &url) const;

    bool load();
    bool save() const;
    bool addBookmark(QString title, QUrl url);
    void removeBookmark(const QUrl &url);

signals:
    void changed();

private:
    QString m_filePath;
    QVector<Bookmark> m_bookmarks;
};
}
