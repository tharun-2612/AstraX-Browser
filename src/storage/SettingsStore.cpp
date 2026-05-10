#include "SettingsStore.h"

#include "../core/UrlTools.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

namespace
{
QUrl validatedHomePage(const QString &value)
{
    const QUrl url = QUrl::fromUserInput(value.trimmed());
    if (url.isValid() && !url.isEmpty()) {
        return url;
    }

    return QUrl(QStringLiteral("https://duckduckgo.com/"));
}

QString validatedSearchTemplate(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.contains(QStringLiteral("{query}"))) {
        return trimmed;
    }

    return astra::UrlTools::defaultSearchUrlTemplate();
}
}

namespace astra
{
SettingsStore::SettingsStore(QString filePath, QObject *parent)
    : QObject(parent)
    , m_filePath(std::move(filePath))
{
}

const BrowserSettings &SettingsStore::settings() const
{
    return m_settings;
}

bool SettingsStore::load()
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
    m_settings.homePage = validatedHomePage(object.value(QStringLiteral("homePage")).toString(m_settings.homePage.toString()));
    m_settings.searchUrlTemplate = validatedSearchTemplate(object.value(QStringLiteral("searchUrlTemplate")).toString(m_settings.searchUrlTemplate));
    m_settings.restoreSession = object.value(QStringLiteral("restoreSession")).toBool(m_settings.restoreSession);
    m_settings.blockTrackers = object.value(QStringLiteral("blockTrackers")).toBool(m_settings.blockTrackers);

    emit changed(m_settings);
    return true;
}

bool SettingsStore::save() const
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    QJsonObject object;
    object.insert(QStringLiteral("homePage"), m_settings.homePage.toString());
    object.insert(QStringLiteral("searchUrlTemplate"), m_settings.searchUrlTemplate);
    object.insert(QStringLiteral("restoreSession"), m_settings.restoreSession);
    object.insert(QStringLiteral("blockTrackers"), m_settings.blockTrackers);

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool SettingsStore::update(BrowserSettings settings)
{
    settings.homePage = validatedHomePage(settings.homePage.toString());
    settings.searchUrlTemplate = validatedSearchTemplate(settings.searchUrlTemplate);

    m_settings = std::move(settings);
    const bool saved = save();
    emit changed(m_settings);
    return saved;
}
}
