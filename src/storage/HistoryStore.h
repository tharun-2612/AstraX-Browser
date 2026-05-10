#pragma once

#include <QDateTime>
#include <QObject>
#include <QUrl>
#include <QVector>

namespace astra
{
struct HistoryEntry
{
    QString title;
    QUrl url;
    QDateTime visitedAt;
};

class HistoryStore final : public QObject
{
    Q_OBJECT

public:
    explicit HistoryStore(QString filePath, QObject *parent = nullptr);

    [[nodiscard]] const QVector<HistoryEntry> &entries() const;

    bool load();
    bool save() const;
    void appendVisit(QString title, QUrl url);
    void clear();

signals:
    void changed();

private:
    static constexpr qsizetype MaxEntries = 500;

    QString m_filePath;
    QVector<HistoryEntry> m_entries;
};
}
