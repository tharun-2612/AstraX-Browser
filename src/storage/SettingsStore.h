#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace astra
{
struct BrowserSettings
{
    QUrl homePage = QUrl(QStringLiteral("https://duckduckgo.com/"));
    QString searchUrlTemplate = QStringLiteral("https://duckduckgo.com/?q={query}");
    bool restoreSession = true;
    bool blockTrackers = true;
};

class SettingsStore final : public QObject
{
    Q_OBJECT

public:
    explicit SettingsStore(QString filePath, QObject *parent = nullptr);

    [[nodiscard]] const BrowserSettings &settings() const;

    bool load();
    bool save() const;
    bool update(BrowserSettings settings);

signals:
    void changed(const BrowserSettings &settings);

private:
    QString m_filePath;
    BrowserSettings m_settings;
};
}
