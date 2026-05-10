#pragma once

#include <QObject>
#include <QUrl>
#include <QVector>

namespace astra
{
struct BrowserSession
{
    QVector<QUrl> tabs;
    int currentIndex = 0;
};

class SessionStore final : public QObject
{
    Q_OBJECT

public:
    explicit SessionStore(QString filePath, QObject *parent = nullptr);

    [[nodiscard]] const BrowserSession &session() const;

    bool load();
    bool save(const BrowserSession &session);
    void clear();

private:
    QString m_filePath;
    BrowserSession m_session;
};
}
