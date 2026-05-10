#include "SessionStore.h"

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
bool isRestorableUrl(const QUrl &url)
{
    return url.isValid()
        && !url.isEmpty()
        && (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"));
}
}

namespace astra
{
SessionStore::SessionStore(QString filePath, QObject *parent)
    : QObject(parent)
    , m_filePath(std::move(filePath))
{
}

const BrowserSession &SessionStore::session() const
{
    return m_session;
}

bool SessionStore::load()
{
    QFile file(m_filePath);
    if (!file.exists()) {
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return false;
    }

    const QJsonObject object = document.object();
    const QJsonArray tabs = object.value(QStringLiteral("tabs")).toArray();

    BrowserSession loaded;
    loaded.tabs.reserve(tabs.size());
    for (const QJsonValue &value : tabs) {
        const QUrl url(value.toString());
        if (isRestorableUrl(url)) {
            loaded.tabs.push_back(url);
        }
    }

    loaded.currentIndex = object.value(QStringLiteral("currentIndex")).toInt(0);
    if (!loaded.tabs.isEmpty()) {
        loaded.currentIndex = std::clamp(loaded.currentIndex, 0, static_cast<int>(loaded.tabs.size()) - 1);
    } else {
        loaded.currentIndex = 0;
    }

    m_session = std::move(loaded);
    return true;
}

bool SessionStore::save(const BrowserSession &session)
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    BrowserSession sanitized;
    sanitized.currentIndex = session.currentIndex;

    for (const QUrl &url : session.tabs) {
        if (isRestorableUrl(url)) {
            sanitized.tabs.push_back(url);
        }
    }

    if (!sanitized.tabs.isEmpty()) {
        sanitized.currentIndex = std::clamp(sanitized.currentIndex, 0, static_cast<int>(sanitized.tabs.size()) - 1);
    } else {
        sanitized.currentIndex = 0;
    }

    QJsonArray tabs;
    for (const QUrl &url : sanitized.tabs) {
        tabs.push_back(url.toString());
    }

    QJsonObject object;
    object.insert(QStringLiteral("currentIndex"), sanitized.currentIndex);
    object.insert(QStringLiteral("tabs"), tabs);

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    const bool saved = file.commit();
    if (saved) {
        m_session = std::move(sanitized);
    }

    return saved;
}

void SessionStore::clear()
{
    m_session = {};
    save(m_session);
}
}
