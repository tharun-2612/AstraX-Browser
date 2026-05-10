#include "BrowserWindow.h"

#include "BrowserView.h"
#include "EngineLabWindow.h"
#include "../core/RequestBlocker.h"
#include "../core/UrlTools.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCompleter>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QIcon>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QRegularExpression>
#include <QShortcut>
#include <QSize>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringListModel>
#include <QStringList>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTimer>
#include <QUrlQuery>
#include <QWebEngineDownloadRequest>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWidget>

#include <algorithm>
#include <functional>
#include <utility>

#ifndef ASTRA_VERSION
#define ASTRA_VERSION "dev"
#endif

namespace
{
QString storageRoot()
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
        root = QDir::home().filePath(QStringLiteral(".astrax-browser"));
    }

    QDir().mkpath(root);
    return root;
}

QString storageFile(const QString &fileName)
{
    return QDir(storageRoot()).filePath(fileName);
}

class AstraPage final : public QWebEnginePage
{
public:
    using NavigationHandler = std::function<bool(const QUrl &)>;

    explicit AstraPage(QWebEngineProfile *profile, QObject *parent = nullptr)
        : QWebEnginePage(profile, parent)
    {
    }

    void setNavigationHandler(NavigationHandler handler)
    {
        m_navigationHandler = std::move(handler);
    }

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
    {
        if (isMainFrame && m_navigationHandler && m_navigationHandler(url)) {
            return false;
        }

        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }

private:
    NavigationHandler m_navigationHandler;
};

QString elidedTitle(const QString &title)
{
    const QString cleanTitle = title.trimmed();
    if (cleanTitle.isEmpty()) {
        return QStringLiteral("New tab");
    }

    constexpr qsizetype MaxTitleLength = 32;
    if (cleanTitle.size() <= MaxTitleLength) {
        return cleanTitle;
    }

    return cleanTitle.left(MaxTitleLength - 1) + QStringLiteral("...");
}

QString humanSize(qint64 bytes)
{
    constexpr double KiB = 1024.0;
    constexpr double MiB = KiB * 1024.0;
    constexpr double GiB = MiB * 1024.0;

    if (bytes >= GiB) {
        return QStringLiteral("%1 GB").arg(bytes / GiB, 0, 'f', 1);
    }
    if (bytes >= MiB) {
        return QStringLiteral("%1 MB").arg(bytes / MiB, 0, 'f', 1);
    }
    if (bytes >= KiB) {
        return QStringLiteral("%1 KB").arg(bytes / KiB, 0, 'f', 1);
    }

    return QStringLiteral("%1 B").arg(bytes);
}

QString pageHostText(const QUrl &url)
{
    if (!url.isValid() || url.scheme() == QStringLiteral("about")) {
        return QStringLiteral("New tab");
    }

    if (url.host().isEmpty()) {
        return url.toDisplayString();
    }

    return url.host();
}

QString pageSecurityText(const QUrl &url)
{
    if (url.scheme() == QStringLiteral("https")) {
        return QStringLiteral("Secure");
    }

    if (url.scheme() == QStringLiteral("http")) {
        return QStringLiteral("Not secure");
    }

    return QStringLiteral("Local");
}

bool looksLikeUrlInput(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QUrl explicitUrl(trimmed);
    if (explicitUrl.isValid() && !explicitUrl.scheme().isEmpty()) {
        return true;
    }

    static const QRegularExpression whitespace(QStringLiteral("\\s"));
    if (trimmed.contains(whitespace)) {
        return false;
    }

    return trimmed.contains(QLatin1Char('.'))
        || trimmed.startsWith(QStringLiteral("localhost"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("127."))
        || trimmed.startsWith(QStringLiteral("[::1]"));
}

bool isAstraSearchUrl(const QUrl &url)
{
    return (url.scheme() == QStringLiteral("astrax")
            && (url.host() == QStringLiteral("search") || url.path() == QStringLiteral("/search")))
        || url.host() == QStringLiteral("search.astrax.local");
}

QUrl astraSearchUrl(const QString &query)
{
    QUrl url(QStringLiteral("astrax://search"));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("q"), query.trimmed());
    url.setQuery(urlQuery);
    return url;
}

QString astraSearchQuery(const QUrl &url)
{
    return QUrlQuery(url).queryItemValue(QStringLiteral("q")).trimmed();
}

QString systemRegionText()
{
    const QLocale locale = QLocale::system();
    QString country = QLocale::territoryToString(locale.territory());
    if (country.trimmed().isEmpty()) {
        country = locale.name();
    }

    return QStringLiteral("%1 - %2").arg(country, locale.name());
}

struct InternetLocation
{
    bool valid = false;
    QString main;
    QString detail;
    QString badge;
};

struct SearchResult
{
    QString type;
    QString title;
    QUrl url;
    QString detail;
    int score = 0;
};

QString joinedLocation(const QStringList &parts)
{
    QStringList cleaned;
    for (const QString &part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            cleaned.append(trimmed);
        }
    }

    return cleaned.join(QStringLiteral(", "));
}

InternetLocation parseInternetLocation(const QByteArray &payload, const QString &host)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonObject object = document.object();
    if (host == QStringLiteral("ipwho.is") && object.value(QStringLiteral("success")).isBool()
        && !object.value(QStringLiteral("success")).toBool()) {
        return {};
    }

    InternetLocation location;
    const QString ip = object.value(QStringLiteral("ip")).toString();
    QString city = object.value(QStringLiteral("city")).toString();
    QString region = object.value(QStringLiteral("region")).toString();
    QString country = object.value(QStringLiteral("country")).toString();
    QString countryCode = object.value(QStringLiteral("country_code")).toString();
    QString timezone;
    QString isp;

    if (host == QStringLiteral("ipapi.co")) {
        country = object.value(QStringLiteral("country_name")).toString(country);
        countryCode = object.value(QStringLiteral("country_code")).toString(countryCode);
        timezone = object.value(QStringLiteral("timezone")).toString();
        isp = object.value(QStringLiteral("org")).toString();
    } else {
        const QJsonObject timezoneObject = object.value(QStringLiteral("timezone")).toObject();
        timezone = timezoneObject.value(QStringLiteral("id")).toString();
        const QJsonObject connectionObject = object.value(QStringLiteral("connection")).toObject();
        isp = connectionObject.value(QStringLiteral("isp")).toString();
    }

    location.main = joinedLocation({ city, region, country });
    if (location.main.isEmpty()) {
        location.main = country;
    }

    QStringList detailParts;
    if (!ip.isEmpty()) {
        detailParts.append(QStringLiteral("Public IP %1").arg(ip));
    }
    if (!timezone.isEmpty()) {
        detailParts.append(QStringLiteral("Timezone %1").arg(timezone));
    }
    if (!isp.isEmpty()) {
        detailParts.append(QStringLiteral("Network %1").arg(isp));
    }

    location.detail = detailParts.join(QStringLiteral(" - "));
    if (location.detail.isEmpty()) {
        location.detail = QStringLiteral("Internet location detected");
    }

    location.badge = countryCode.isEmpty() ? QStringLiteral("Live") : countryCode;
    location.valid = !location.main.isEmpty();
    return location;
}

QString compactText(QString text, qsizetype maxLength)
{
    text = text.simplified();
    if (text.size() <= maxLength) {
        return text;
    }

    return text.left(maxLength - 3) + QStringLiteral("...");
}

QString suggestionLabel(const QString &type, const QString &title, const QUrl &url)
{
    const QString cleanTitle = title.trimmed().isEmpty() ? url.toDisplayString() : title.trimmed();
    return QStringLiteral("%1: %2 - %3").arg(
        type,
        compactText(cleanTitle, 58),
        compactText(url.toDisplayString(), 92));
}

int resultScore(const QString &title, const QString &url, const QString &query)
{
    const QString cleanQuery = query.trimmed().toCaseFolded();
    if (cleanQuery.isEmpty()) {
        return 0;
    }

    const QString foldedTitle = title.toCaseFolded();
    const QString foldedUrl = url.toCaseFolded();
    int score = 0;

    if (foldedTitle == cleanQuery) {
        score += 140;
    }
    if (foldedTitle.contains(cleanQuery)) {
        score += 90;
    }
    if (foldedUrl.contains(cleanQuery)) {
        score += 55;
    }

    const QStringList terms = cleanQuery.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &term : terms) {
        if (foldedTitle.contains(term)) {
            score += 24;
        }
        if (foldedUrl.contains(term)) {
            score += 12;
        }
    }

    return score;
}

QString searchResultHtml(const SearchResult &result)
{
    return QStringLiteral(R"(
          <a class="result" href="%1">
            <span class="result-type">%2</span>
            <strong>%3</strong>
            <span class="result-url">%4</span>
            <span class="result-detail">%5</span>
          </a>
)")
        .arg(
            result.url.toString(QUrl::FullyEncoded).toHtmlEscaped(),
            result.type.toHtmlEscaped(),
            compactText(result.title, 90).toHtmlEscaped(),
            compactText(result.url.toDisplayString(), 110).toHtmlEscaped(),
            result.detail.toHtmlEscaped());
}

QString appLogoMarkup()
{
    QFile logoFile(QStringLiteral(":/branding/AstraX.ico"));
    if (!logoFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QString dataUri = QStringLiteral("data:image/x-icon;base64,")
        + QString::fromLatin1(logoFile.readAll().toBase64());
    return QStringLiteral("<img src=\"%1\" alt=\"AstraX logo\">").arg(dataUri);
}

QString newTabHtml(
    bool privateMode,
    const QString &searchUrlTemplate,
    bool blockTrackers,
    bool restoreSession,
    qsizetype bookmarkCount,
    qsizetype historyCount,
    const QString &regionText)
{
    const QString logoMarkup = appLogoMarkup();
    const QString modeText = privateMode ? QStringLiteral("Private") : QStringLiteral("Standard");
    const QString shieldText = blockTrackers ? QStringLiteral("Shield on") : QStringLiteral("Shield off");
    const QString sessionText = restoreSession ? QStringLiteral("Restore on") : QStringLiteral("Restore off");

    QString html = QStringLiteral(R"(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AstraX</title>
  <style>
    :root {
      color-scheme: light dark;
      font-family: Inter, Segoe UI, system-ui, sans-serif;
      --bg: #eef4f8;
      --ink: #102033;
      --muted: #627387;
      --panel: rgba(255, 255, 255, .88);
      --line: rgba(113, 128, 150, .26);
      --accent: #0f766e;
      --accent-strong: #155e75;
      --warm: #b45309;
      --rose: #be123c;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background:
        linear-gradient(135deg, rgba(248, 250, 252, .96), rgba(226, 232, 240, .94)),
        url("data:image/svg+xml,%3Csvg width='120' height='120' viewBox='0 0 120 120' xmlns='http://www.w3.org/2000/svg'%3E%3Cg fill='none' stroke='%2394a3b8' stroke-opacity='.18'%3E%3Cpath d='M0 30h120M0 90h120M30 0v120M90 0v120'/%3E%3C/g%3E%3C/svg%3E");
      color: var(--ink);
      display: grid;
      place-items: center;
      padding: 40px 20px;
    }
    main {
      width: min(1080px, 100%);
      display: grid;
      gap: 22px;
    }
    .topbar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 18px;
    }
    .brand { display: flex; align-items: center; gap: 16px; min-width: 0; }
    .brand img {
      width: 72px;
      height: 72px;
      object-fit: contain;
      filter: drop-shadow(0 12px 18px rgba(15, 23, 42, .18));
    }
    h1 { margin: 0; font-size: 56px; letter-spacing: 0; line-height: 1; }
    .subtitle { margin-top: 5px; color: var(--muted); font-size: 15px; }
    .clock {
      text-align: right;
      color: var(--muted);
      min-width: 150px;
    }
    .clock strong {
      display: block;
      color: var(--ink);
      font-size: 28px;
      line-height: 1.1;
      font-weight: 800;
    }
    .search-panel {
      border: 1px solid var(--line);
      border-radius: 18px;
      background: var(--panel);
      box-shadow: 0 24px 70px rgba(15, 23, 42, .12);
      padding: 22px;
      backdrop-filter: blur(18px);
    }
    form { display: grid; grid-template-columns: 1fr auto; gap: 12px; }
    input {
      min-width: 0;
      border: 1px solid #b8c5d5;
      border-radius: 12px;
      padding: 16px 18px;
      font-size: 18px;
      background: #ffffff;
      color: var(--ink);
      outline: none;
    }
    input:focus { border-color: #2563eb; box-shadow: 0 0 0 4px rgba(37, 99, 235, .13); }
    button {
      border: 0;
      border-radius: 12px;
      padding: 0 24px;
      font-size: 16px;
      font-weight: 700;
      background: var(--accent);
      color: white;
      cursor: pointer;
      min-width: 96px;
    }
    button:hover { background: var(--accent-strong); }
    .chips {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 10px;
      margin-top: 16px;
    }
    .chip {
      border: 1px solid var(--line);
      border-radius: 12px;
      padding: 11px 12px;
      background: rgba(248, 250, 252, .78);
      color: var(--muted);
      font-size: 13px;
    }
    .chip strong { display: block; color: var(--ink); font-size: 16px; margin-bottom: 2px; }
    .grid {
      display: grid;
      grid-template-columns: 1.35fr .65fr;
      gap: 22px;
    }
    .panel {
      border: 1px solid var(--line);
      border-radius: 18px;
      background: rgba(255, 255, 255, .78);
      padding: 18px;
    }
    .panel h2 {
      margin: 0 0 14px;
      font-size: 18px;
      letter-spacing: 0;
    }
    .quick-links {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 12px;
    }
    .tile {
      display: grid;
      gap: 8px;
      align-content: center;
      min-height: 94px;
      border: 1px solid var(--line);
      border-radius: 14px;
      background: #ffffff;
      color: var(--ink);
      text-decoration: none;
      padding: 14px;
      transition: transform .16s ease, border-color .16s ease, box-shadow .16s ease;
    }
    .tile:hover {
      transform: translateY(-2px);
      border-color: rgba(37, 99, 235, .42);
      box-shadow: 0 14px 34px rgba(15, 23, 42, .12);
    }
    .tile strong { font-size: 15px; font-weight: 800; }
    .site-icon {
      width: 34px;
      height: 34px;
      display: grid;
      place-items: center;
      border-radius: 10px;
      background: #dbeafe;
      color: #1d4ed8;
      font-weight: 900;
    }
    .site-icon img {
      width: 24px;
      height: 24px;
      object-fit: contain;
    }
    .site-icon .fallback { display: none; }
    .tile:nth-child(2) .site-icon { background: #ccfbf1; color: #0f766e; }
    .tile:nth-child(3) .site-icon { background: #fef3c7; color: var(--warm); }
    .tile:nth-child(4) .site-icon { background: #ffe4e6; color: var(--rose); }
    .metric {
      display: grid;
      grid-template-columns: 1fr auto;
      gap: 12px;
      border-bottom: 1px solid var(--line);
      padding: 12px 0;
      color: var(--muted);
    }
    .metric:first-of-type { padding-top: 0; }
    .metric:last-child { border-bottom: 0; padding-bottom: 0; }
    .metric strong { color: var(--ink); font-size: 22px; }
    .private {
      color: var(--rose);
      font-weight: 800;
    }
    .location-panel {
      display: grid;
      grid-template-columns: 1fr auto;
      gap: 14px;
      align-items: center;
      border: 1px solid var(--line);
      border-radius: 18px;
      background: rgba(255, 255, 255, .72);
      padding: 16px 18px;
    }
    .location-panel h2 {
      display: flex;
      align-items: center;
      gap: 8px;
      margin: 0 0 4px;
      font-size: 18px;
      letter-spacing: 0;
    }
    .location-panel p {
      margin: 0;
      color: var(--muted);
      line-height: 1.45;
    }
    .location-badge {
      min-width: 128px;
      border: 1px solid rgba(15, 118, 110, .24);
      border-radius: 14px;
      background: rgba(204, 251, 241, .62);
      color: #115e59;
      font-weight: 900;
      padding: 12px 14px;
      text-align: center;
    }
    .location-badge small {
      display: block;
      margin-top: 3px;
      color: #4b6f69;
      font-weight: 700;
    }
    .connection-dot {
      width: 10px;
      height: 10px;
      border-radius: 999px;
      background: #14b8a6;
      box-shadow: 0 0 0 5px rgba(20, 184, 166, .15);
    }
    body[data-connection-status="fallback"] .connection-dot,
    body[data-connection-status="fallback"] .location-badge {
      background: rgba(254, 243, 199, .82);
      color: #92400e;
      border-color: rgba(180, 83, 9, .28);
    }
    body[data-connection-status="offline"] .connection-dot,
    body[data-connection-status="offline"] .location-badge {
      background: rgba(255, 228, 230, .84);
      color: #be123c;
      border-color: rgba(190, 18, 60, .28);
    }
    .footer {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      color: var(--muted);
      font-size: 13px;
      padding: 0 4px;
    }
    .footer strong {
      color: var(--ink);
      font-weight: 800;
    }
    @media (prefers-color-scheme: dark) {
      :root {
        --bg: #07111f;
        --ink: #ecfdf5;
        --muted: #9fb0c6;
        --panel: rgba(15, 23, 42, .86);
        --line: rgba(148, 163, 184, .22);
      }
      body {
        background:
          linear-gradient(135deg, rgba(2, 6, 23, .96), rgba(17, 24, 39, .94)),
          url("data:image/svg+xml,%3Csvg width='120' height='120' viewBox='0 0 120 120' xmlns='http://www.w3.org/2000/svg'%3E%3Cg fill='none' stroke='%2364748b' stroke-opacity='.16'%3E%3Cpath d='M0 30h120M0 90h120M30 0v120M90 0v120'/%3E%3C/g%3E%3C/svg%3E");
      }
      input, .tile { background: rgba(15, 23, 42, .92); color: #f8fafc; border-color: #475569; }
      .panel { background: rgba(15, 23, 42, .74); }
      .chip { background: rgba(15, 23, 42, .64); }
      .location-panel { background: rgba(15, 23, 42, .70); }
      .location-badge { background: rgba(20, 184, 166, .16); color: #99f6e4; border-color: rgba(45, 212, 191, .28); }
      .location-badge small { color: #9fb0c6; }
      body[data-connection-status="fallback"] .connection-dot,
      body[data-connection-status="fallback"] .location-badge { background: rgba(245, 158, 11, .16); color: #fde68a; border-color: rgba(245, 158, 11, .30); }
      body[data-connection-status="offline"] .connection-dot,
      body[data-connection-status="offline"] .location-badge { background: rgba(244, 63, 94, .16); color: #fecdd3; border-color: rgba(244, 63, 94, .30); }
    }
    @media (max-width: 820px) {
      body { padding: 24px 14px; }
      .topbar, .grid { grid-template-columns: 1fr; display: grid; }
      .clock { text-align: left; }
      form, .chips, .quick-links { grid-template-columns: 1fr; }
      .location-panel { grid-template-columns: 1fr; }
      .location-badge { text-align: left; }
      .footer { display: grid; }
      h1 { font-size: 42px; }
    }
  </style>
</head>
<body data-search-template="__SEARCH_TEMPLATE__">
  <main>
    <section class="topbar">
      <div class="brand">
        __LOGO__
        <div>
          <h1>AstraX</h1>
          <div class="subtitle">Start</div>
        </div>
      </div>
      <div class="clock"><strong id="time">--:--</strong><span id="date">Today</span></div>
    </section>

    <section class="search-panel">
      <form id="search-form">
        <input name="q" autofocus autocomplete="off" spellcheck="false" placeholder="Search or enter a web address">
        <button>Go</button>
      </form>
      <div class="chips">
        <div class="chip"><strong class="__PRIVATE_CLASS__">__MODE__</strong>Profile</div>
        <div class="chip"><strong>__SHIELD__</strong>Requests</div>
        <div class="chip"><strong>__SESSION__</strong>Tabs</div>
        <div class="chip"><strong>AstraX</strong>Release build</div>
      </div>
    </section>

    <section class="grid">
      <div class="panel">
        <h2>Quick Launch</h2>
        <div class="quick-links">
          <a class="tile" href="https://github.com">
            <span class="site-icon"><img src="https://github.githubassets.com/favicons/favicon.svg" alt=""><span class="fallback">G</span></span>
            <strong>GitHub</strong>
          </a>
          <a class="tile" href="https://doc.qt.io">
            <span class="site-icon"><img src="https://doc.qt.io/favicon.ico" alt=""><span class="fallback">Q</span></span>
            <strong>Qt Docs</strong>
          </a>
          <a class="tile" href="https://stackoverflow.com">
            <span class="site-icon"><img src="https://cdn.sstatic.net/Sites/stackoverflow/Img/favicon.ico" alt=""><span class="fallback">S</span></span>
            <strong>Stack Overflow</strong>
          </a>
          <a class="tile" href="https://developer.mozilla.org">
            <span class="site-icon"><img src="https://developer.mozilla.org/favicon-48x48.png" alt=""><span class="fallback">M</span></span>
            <strong>MDN</strong>
          </a>
        </div>
      </div>

      <div class="panel">
        <h2>Workspace</h2>
        <div class="metric"><span>Bookmarks</span><strong>__BOOKMARKS__</strong></div>
        <div class="metric"><span>History entries</span><strong>__HISTORY__</strong></div>
        <div class="metric"><span>Start mode</span><strong>__MODE__</strong></div>
      </div>
    </section>

    <section class="location-panel">
      <div>
        <h2><span class="connection-dot" aria-hidden="true"></span>Live Connection</h2>
        <p id="location-line">Checking internet location...</p>
        <p id="network-line">System region fallback: __REGION__</p>
        <p id="last-check-line">Waiting for network status...</p>
      </div>
      <div class="location-badge" id="country-badge"><span id="country-label">Online</span><small id="country-code">IP lookup</small></div>
    </section>

    <footer class="footer">
      <span>Location <strong id="footer-region">__REGION__</strong></span>
      <span id="connection-source">Waiting for internet location</span>
    </footer>
  </main>
  <script>
    const form = document.getElementById('search-form');
    form.addEventListener('submit', event => {
      event.preventDefault();
      const input = form.elements.q.value.trim();
      if (!input) return;
      window.location.href = `astrax://search?q=${encodeURIComponent(input)}`;
    });
    const updateClock = () => {
      const now = new Date();
      document.getElementById('time').textContent = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
      document.getElementById('date').textContent = now.toLocaleDateString([], { weekday: 'long', month: 'short', day: 'numeric' });
    };
    updateClock();
    setInterval(updateClock, 15000);
    const locationElements = {
      main: document.getElementById('location-line'),
      detail: document.getElementById('network-line'),
      checked: document.getElementById('last-check-line'),
      badge: document.getElementById('country-label'),
      code: document.getElementById('country-code'),
      footer: document.getElementById('footer-region'),
      source: document.getElementById('connection-source'),
    };
    const lastChecked = () => new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    const setLocation = (main, detail, badge, source, status, codeText) => {
      document.body.dataset.connectionStatus = status;
      locationElements.main.textContent = main;
      locationElements.detail.textContent = detail;
      locationElements.checked.textContent = `Last checked ${lastChecked()}`;
      locationElements.badge.textContent = badge;
      locationElements.code.textContent = codeText;
      locationElements.footer.textContent = main;
      locationElements.source.textContent = source;
    };
    window.astraxSetLocation = location => {
      const data = location || {};
      setLocation(
        data.main || '__REGION__',
        data.detail || 'System region fallback: __REGION__',
        data.badge || 'Fallback',
        data.source || 'Waiting for internet location',
        data.status || 'fallback',
        data.codeText || 'system'
      );
    };
    window.addEventListener('offline', () => {
      window.astraxSetLocation({
        main: '__REGION__',
        detail: 'Offline: using system region fallback',
        badge: 'Offline',
        source: 'No internet connection detected',
        status: 'offline',
        codeText: 'fallback',
      });
    });
    window.addEventListener('online', () => {
      locationElements.source.textContent = 'Internet connection restored. Refreshing location soon...';
    });
    document.querySelectorAll('.site-icon img').forEach(img => {
      img.addEventListener('error', () => {
        img.style.display = 'none';
        const fallback = img.nextElementSibling;
        if (fallback) fallback.style.display = 'inline';
      });
    });
  </script>
</body>
</html>
)");

    html.replace(QStringLiteral("__SEARCH_TEMPLATE__"), searchUrlTemplate.toHtmlEscaped());
    html.replace(QStringLiteral("__LOGO__"), logoMarkup);
    html.replace(QStringLiteral("__PRIVATE_CLASS__"), privateMode ? QStringLiteral("private") : QString());
    html.replace(QStringLiteral("__MODE__"), modeText);
    html.replace(QStringLiteral("__SHIELD__"), shieldText);
    html.replace(QStringLiteral("__SESSION__"), sessionText);
    html.replace(QStringLiteral("__BOOKMARKS__"), QString::number(bookmarkCount));
    html.replace(QStringLiteral("__HISTORY__"), QString::number(historyCount));
    html.replace(QStringLiteral("__REGION__"), regionText.toHtmlEscaped());
    return html;
}
}

namespace astra
{
BrowserWindow::BrowserWindow(bool privateMode, QWidget *parent)
    : QMainWindow(parent)
    , m_privateMode(privateMode)
    , m_settingsStore(storageFile(QStringLiteral("settings.json")), this)
    , m_sessionStore(storageFile(QStringLiteral("session.json")), this)
    , m_bookmarks(storageFile(QStringLiteral("bookmarks.json")), this)
    , m_history(storageFile(QStringLiteral("history.json")), this)
{
    m_settingsStore.load();
    if (!m_privateMode) {
        m_sessionStore.load();
    }

    setupProfile();
    setupUi();
    setupMenus();
    setupShortcuts();
    connectStoreSignals();

    m_bookmarks.load();
    if (!m_privateMode) {
        m_history.load();
    }
    rebuildSearchSuggestions();

    m_networkAccess = new QNetworkAccessManager(this);
    m_locationMain = systemRegionText();
    m_locationDetail = QStringLiteral("System region fallback while AstraX checks the internet connection");
    m_locationBadge = QStringLiteral("Fallback");
    m_locationSource = QStringLiteral("Waiting for internet location");
    m_locationStatus = QStringLiteral("fallback");
    m_locationCodeText = QStringLiteral("system");

    createTab(QUrl(), true);
    restorePreviousSession();
    updateWindowForCurrentTab();
    refreshInternetLocation();
}

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    saveSession();
    QMainWindow::closeEvent(event);
}

void BrowserWindow::setupProfile()
{
    if (m_privateMode) {
        m_profile = new QWebEngineProfile(this);
        m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        m_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    } else {
        m_profile = new QWebEngineProfile(QStringLiteral("AstraX"), this);
        m_profile->setCachePath(QDir(storageRoot()).filePath(QStringLiteral("cache")));
        m_profile->setPersistentStoragePath(QDir(storageRoot()).filePath(QStringLiteral("web-storage")));
        m_profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
    }

    m_profile->setHttpUserAgent(m_profile->httpUserAgent() + QStringLiteral(" AstraXBrowser/") + QStringLiteral(ASTRA_VERSION));

    m_requestBlocker = new RequestBlocker(this);
    m_requestBlocker->setEnabled(m_settingsStore.settings().blockTrackers);
    m_profile->setUrlRequestInterceptor(m_requestBlocker);

    QWebEngineSettings *settings = m_profile->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    settings->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);

    connect(m_profile, &QWebEngineProfile::downloadRequested, this, &BrowserWindow::handleDownload);
}

void BrowserWindow::applyModernStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow {
            background: #f6f8fb;
            color: #172033;
        }

        QMenuBar {
            background: #eef2f7;
            color: #172033;
            padding: 3px 6px;
        }

        QMenuBar::item {
            border-radius: 6px;
            padding: 5px 10px;
        }

        QMenuBar::item:selected {
            background: #dbe6f3;
        }

        QMenu {
            background: #ffffff;
            border: 1px solid #d6dee9;
            border-radius: 8px;
            padding: 6px;
        }

        QMenu::item {
            border-radius: 6px;
            padding: 7px 22px;
        }

        QMenu::item:selected {
            background: #e8f0fb;
            color: #0f3f73;
        }

        QToolBar#NavigationBar {
            background: #eef2f7;
            border: 0;
            border-bottom: 1px solid #d7e0eb;
            spacing: 6px;
            padding: 8px;
        }

        QToolBar#FindBar {
            background: #f8fafc;
            border: 0;
            border-bottom: 1px solid #d7e0eb;
            spacing: 6px;
            padding: 6px 8px;
        }

        QToolButton {
            border: 1px solid transparent;
            border-radius: 8px;
            padding: 6px 8px;
            background: transparent;
        }

        QToolButton:hover {
            background: #dfe8f3;
            border-color: #cad6e4;
        }

        QToolButton:pressed {
            background: #cddaea;
        }

        QToolButton#PageInfoButton {
            background: #ffffff;
            border: 1px solid #ccd7e4;
            color: #31546f;
            padding: 7px 12px;
        }

        QLineEdit#AddressBar {
            min-height: 34px;
            border: 1px solid #c8d4e3;
            border-radius: 8px;
            padding: 0 14px;
            background: #ffffff;
            selection-background-color: #2563eb;
        }

        QLineEdit#AddressBar:focus {
            border-color: #3b82f6;
        }

        QAbstractItemView#AddressSuggestions {
            background: #ffffff;
            border: 1px solid #c8d4e3;
            border-radius: 10px;
            padding: 6px;
            selection-background-color: #e0f2fe;
            selection-color: #0f172a;
            outline: 0;
        }

        QAbstractItemView#AddressSuggestions::item {
            min-height: 30px;
            border-radius: 7px;
            padding: 6px 10px;
        }

        QTabWidget::pane {
            border: 0;
            background: #ffffff;
        }

        QTabBar::tab {
            background: #e8edf5;
            border: 1px solid #d1dbe8;
            border-bottom: 0;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            padding: 8px 16px;
            margin-right: 3px;
            min-width: 120px;
        }

        QTabBar::tab:selected {
            background: #ffffff;
            color: #0f3f73;
        }

        QDockWidget {
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
            background: #f8fafc;
            border: 1px solid #d7e0eb;
        }

        QDockWidget::title {
            background: #edf3f9;
            border-bottom: 1px solid #d7e0eb;
            padding: 8px;
            text-align: left;
        }

        QListWidget {
            background: #ffffff;
            border: 0;
            outline: 0;
        }

        QListWidget::item {
            border-bottom: 1px solid #eef2f7;
            padding: 8px;
        }

        QListWidget::item:selected {
            background: #dbeafe;
            color: #0f3f73;
        }

        QStatusBar {
            background: #eef2f7;
            border-top: 1px solid #d7e0eb;
            color: #496174;
        }

        QProgressBar {
            border: 1px solid #c8d4e3;
            border-radius: 5px;
            background: #ffffff;
            text-align: center;
        }

        QProgressBar::chunk {
            background: #2563eb;
            border-radius: 4px;
        }
    )"));
}

void BrowserWindow::setupUi()
{
    applyModernStyle();

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("MainTabs"));
    m_tabs->setDocumentMode(true);
    m_tabs->setMovable(true);
    m_tabs->setTabsClosable(true);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        updateWindowForCurrentTab();
        updateNavigationState();
    });
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &BrowserWindow::closeTab);

    QToolBar *navigationBar = addToolBar(QStringLiteral("Navigation"));
    navigationBar->setObjectName(QStringLiteral("NavigationBar"));
    navigationBar->setMovable(false);
    navigationBar->setIconSize(QSize(18, 18));

    m_backAction = navigationBar->addAction(style()->standardIcon(QStyle::SP_ArrowBack), QStringLiteral("Back"), this, [this] {
        if (BrowserView *view = currentView()) {
            view->back();
        }
    });
    m_backAction->setShortcut(QKeySequence(QStringLiteral("Alt+Left")));

    m_forwardAction = navigationBar->addAction(style()->standardIcon(QStyle::SP_ArrowForward), QStringLiteral("Forward"), this, [this] {
        if (BrowserView *view = currentView()) {
            view->forward();
        }
    });
    m_forwardAction->setShortcut(QKeySequence(QStringLiteral("Alt+Right")));

    m_reloadAction = navigationBar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), QStringLiteral("Reload"), this, [this] {
        if (BrowserView *view = currentView()) {
            view->reload();
        }
    });
    m_reloadAction->setShortcut(QKeySequence::Refresh);

    m_stopAction = navigationBar->addAction(style()->standardIcon(QStyle::SP_BrowserStop), QStringLiteral("Stop"), this, [this] {
        if (BrowserView *view = currentView()) {
            view->stop();
        }
    });
    m_stopAction->setEnabled(false);

    navigationBar->addAction(style()->standardIcon(QStyle::SP_DirHomeIcon), QStringLiteral("Home"), this, &BrowserWindow::navigateHome);

    m_pageInfoButton = new QToolButton(this);
    m_pageInfoButton->setObjectName(QStringLiteral("PageInfoButton"));
    m_pageInfoButton->setText(QStringLiteral("Page"));
    m_pageInfoButton->setToolTip(QStringLiteral("Page information"));
    m_pageInfoButton->setCursor(Qt::PointingHandCursor);
    navigationBar->addWidget(m_pageInfoButton);
    connect(m_pageInfoButton, &QToolButton::clicked, this, &BrowserWindow::openPageInfoDialog);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setObjectName(QStringLiteral("AddressBar"));
    m_urlEdit->setClearButtonEnabled(true);
    m_urlEdit->setPlaceholderText(QStringLiteral("Search or enter address"));
    m_urlEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_suggestionModel = new QStringListModel(this);
    m_addressCompleter = new QCompleter(m_suggestionModel, this);
    m_addressCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_addressCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_addressCompleter->setFilterMode(Qt::MatchContains);
    m_addressCompleter->setMaxVisibleItems(10);
    m_addressCompleter->popup()->setObjectName(QStringLiteral("AddressSuggestions"));
    m_urlEdit->setCompleter(m_addressCompleter);
    navigationBar->addWidget(m_urlEdit);
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &BrowserWindow::navigateToUserInput);
    connect(m_addressCompleter, qOverload<const QString &>(&QCompleter::activated), this, [this](const QString &suggestion) {
        const QUrl url = m_suggestionTargets.value(suggestion);
        if (url.isValid() && !url.isEmpty()) {
            if (BrowserView *view = currentView()) {
                view->load(url);
            }
        }
    });

    m_addBookmarkAction = navigationBar->addAction(style()->standardIcon(QStyle::SP_DialogYesButton), QStringLiteral("Bookmark"), this, &BrowserWindow::addCurrentBookmark);
    m_addBookmarkAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));

    m_securityLabel = new QLabel(this);
    m_securityLabel->setMinimumWidth(180);
    statusBar()->addPermanentWidget(m_securityLabel);

    m_progress = new QProgressBar(this);
    m_progress->setMaximumWidth(160);
    m_progress->setRange(0, 100);
    m_progress->hide();
    statusBar()->addPermanentWidget(m_progress);

    m_findToolBar = addToolBar(QStringLiteral("Find"));
    m_findToolBar->setObjectName(QStringLiteral("FindBar"));
    m_findToolBar->setMovable(false);
    m_findToolBar->hide();

    m_findEdit = new QLineEdit(this);
    m_findEdit->setClearButtonEnabled(true);
    m_findEdit->setPlaceholderText(QStringLiteral("Find in page"));
    m_findToolBar->addWidget(m_findEdit);
    m_findToolBar->addAction(QStringLiteral("Previous"), this, [this] { findInPage(true); });
    m_findToolBar->addAction(QStringLiteral("Next"), this, [this] { findInPage(false); });
    m_findToolBar->addAction(QStringLiteral("Close"), this, [this] {
        m_findToolBar->hide();
        if (BrowserView *view = currentView()) {
            view->findText(QString());
        }
    });

    connect(m_findEdit, &QLineEdit::textChanged, this, [this] { findInPage(false); });
    connect(m_findEdit, &QLineEdit::returnPressed, this, [this] { findInPage(false); });

    setupLibrarySidebar();
}

void BrowserWindow::setupLibrarySidebar()
{
    m_libraryTabs = new QTabWidget(this);
    m_libraryTabs->setDocumentMode(true);

    m_bookmarksList = new QListWidget(this);
    m_bookmarksList->setAlternatingRowColors(false);
    m_bookmarksList->setUniformItemSizes(false);
    m_bookmarksList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_bookmarksList, &QListWidget::itemActivated, this, &BrowserWindow::openLibraryUrl);

    m_historyList = new QListWidget(this);
    m_historyList->setAlternatingRowColors(false);
    m_historyList->setUniformItemSizes(false);
    m_historyList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_historyList, &QListWidget::itemActivated, this, &BrowserWindow::openLibraryUrl);

    m_downloadsList = new QListWidget(this);
    m_downloadsList->setAlternatingRowColors(false);
    m_downloadsList->setUniformItemSizes(false);

    m_libraryTabs->addTab(m_bookmarksList, style()->standardIcon(QStyle::SP_DialogYesButton), QStringLiteral("Bookmarks"));
    m_libraryTabs->addTab(m_historyList, style()->standardIcon(QStyle::SP_FileDialogDetailedView), QStringLiteral("History"));
    m_libraryTabs->addTab(m_downloadsList, style()->standardIcon(QStyle::SP_ArrowDown), QStringLiteral("Downloads"));

    m_libraryDock = new QDockWidget(QStringLiteral("Library"), this);
    m_libraryDock->setWidget(m_libraryTabs);
    m_libraryDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_libraryDock->setMinimumWidth(280);
    addDockWidget(Qt::LeftDockWidgetArea, m_libraryDock);
    m_libraryDock->hide();
}

void BrowserWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    fileMenu->addAction(QStringLiteral("New Tab"), QKeySequence(QStringLiteral("Ctrl+T")), this, [this] { createTab(QUrl(), true); });
    fileMenu->addAction(QStringLiteral("New Private Window"), QKeySequence(QStringLiteral("Ctrl+Shift+N")), this, &BrowserWindow::openPrivateWindow);
    fileMenu->addAction(QStringLiteral("Close Tab"), QKeySequence(QStringLiteral("Ctrl+W")), this, [this] { closeTab(m_tabs->currentIndex()); });
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("View"));
    viewMenu->addAction(m_backAction);
    viewMenu->addAction(m_forwardAction);
    viewMenu->addAction(m_reloadAction);
    viewMenu->addAction(m_stopAction);
    viewMenu->addSeparator();
    viewMenu->addAction(QStringLiteral("Zoom In"), QKeySequence::ZoomIn, this, [this] {
        if (BrowserView *view = currentView()) {
            view->setZoomFactor(view->zoomFactor() + 0.1);
        }
    });
    viewMenu->addAction(QStringLiteral("Zoom Out"), QKeySequence::ZoomOut, this, [this] {
        if (BrowserView *view = currentView()) {
            view->setZoomFactor(std::max(0.25, view->zoomFactor() - 0.1));
        }
    });
    viewMenu->addAction(QStringLiteral("Reset Zoom"), QKeySequence(QStringLiteral("Ctrl+0")), this, [this] {
        if (BrowserView *view = currentView()) {
            view->setZoomFactor(1.0);
        }
    });
    viewMenu->addSeparator();
    viewMenu->addAction(QStringLiteral("Find"), QKeySequence::Find, this, &BrowserWindow::showFindBar);
    viewMenu->addAction(QStringLiteral("Library"), QKeySequence(QStringLiteral("Ctrl+B")), this, [this] {
        showLibraryTab(m_libraryTabs ? m_libraryTabs->currentIndex() : 0);
    });
    viewMenu->addAction(QStringLiteral("Bookmarks Sidebar"), this, [this] { showLibraryTab(0); });
    viewMenu->addAction(QStringLiteral("History Sidebar"), this, [this] { showLibraryTab(1); });
    viewMenu->addAction(QStringLiteral("Downloads"), QKeySequence(QStringLiteral("Ctrl+J")), this, [this] { showLibraryTab(2); });

    m_bookmarksMenu = menuBar()->addMenu(QStringLiteral("Bookmarks"));
    m_historyMenu = menuBar()->addMenu(QStringLiteral("History"));

    QMenu *toolsMenu = menuBar()->addMenu(QStringLiteral("Tools"));
    toolsMenu->addAction(QStringLiteral("Page Info"), QKeySequence(QStringLiteral("Ctrl+I")), this, &BrowserWindow::openPageInfoDialog);
    toolsMenu->addAction(QStringLiteral("AstraX Engine Lab"), QKeySequence(QStringLiteral("Ctrl+Shift+E")), this, &BrowserWindow::openEngineLab);
    toolsMenu->addAction(QStringLiteral("Developer Tools"), QKeySequence(QStringLiteral("F12")), this, &BrowserWindow::openDevTools);
    m_blockTrackersAction = toolsMenu->addAction(QStringLiteral("Block Trackers"));
    m_blockTrackersAction->setCheckable(true);
    m_blockTrackersAction->setChecked(m_settingsStore.settings().blockTrackers);
    connect(m_blockTrackersAction, &QAction::toggled, this, [this](bool checked) {
        BrowserSettings settings = m_settingsStore.settings();
        settings.blockTrackers = checked;
        m_settingsStore.update(settings);
    });
    toolsMenu->addAction(QStringLiteral("Settings"), QKeySequence(QStringLiteral("Ctrl+,")), this, &BrowserWindow::openSettingsDialog);
    toolsMenu->addAction(QStringLiteral("Open Profile Folder"), this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(storageRoot()));
    });

    rebuildBookmarksMenu();
    rebuildHistoryMenu();
}

void BrowserWindow::setupShortcuts()
{
    auto *focusLocation = new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this);
    connect(focusLocation, &QShortcut::activated, this, [this] {
        m_urlEdit->setFocus(Qt::ShortcutFocusReason);
        m_urlEdit->selectAll();
    });

    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escapeShortcut, &QShortcut::activated, this, [this] {
        if (m_findToolBar->isVisible()) {
            m_findToolBar->hide();
        }
    });
}

void BrowserWindow::connectStoreSignals()
{
    connect(&m_bookmarks, &BookmarksStore::changed, this, &BrowserWindow::rebuildBookmarksMenu);
    connect(&m_bookmarks, &BookmarksStore::changed, this, &BrowserWindow::rebuildBookmarksList);
    connect(&m_bookmarks, &BookmarksStore::changed, this, &BrowserWindow::rebuildSearchSuggestions);
    connect(&m_history, &HistoryStore::changed, this, &BrowserWindow::rebuildHistoryMenu);
    connect(&m_history, &HistoryStore::changed, this, &BrowserWindow::rebuildHistoryList);
    connect(&m_history, &HistoryStore::changed, this, &BrowserWindow::rebuildSearchSuggestions);
    connect(&m_settingsStore, &SettingsStore::changed, this, &BrowserWindow::applySettings);
}

BrowserView *BrowserWindow::createTab(const QUrl &url, bool makeCurrent)
{
    auto *view = new BrowserView(this);
    auto *page = new AstraPage(m_profile, view);
    page->setNavigationHandler([this, view](const QUrl &targetUrl) {
        if (!isAstraSearchUrl(targetUrl)) {
            return false;
        }

        showAstraSearch(view, astraSearchQuery(targetUrl));
        return true;
    });
    view->setPage(page);
    view->setProperty("astraStartDashboard", false);
    view->setProperty("astraSearchPage", false);
    view->setTabFactory([this] {
        return createTab(QUrl(), true);
    });

    const int index = m_tabs->addTab(view, QStringLiteral("New tab"));
    m_tabs->setTabToolTip(index, QStringLiteral("New tab"));

    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString &title) {
        const int tabIndex = m_tabs->indexOf(view);
        if (tabIndex >= 0) {
            m_tabs->setTabText(tabIndex, elidedTitle(title));
            m_tabs->setTabToolTip(tabIndex, title);
        }
        if (view == currentView()) {
            updateWindowForCurrentTab();
        }
    });

    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &changedUrl) {
        if (changedUrl != QUrl(QStringLiteral("about:blank"))) {
            view->setProperty("astraStartDashboard", false);
            if (!isAstraSearchUrl(changedUrl)) {
                view->setProperty("astraSearchPage", false);
            }
        }
        if (view == currentView()) {
            m_urlEdit->setText(UrlTools::displayUrl(changedUrl));
            updateWindowForCurrentTab();
        }
    });

    connect(view, &QWebEngineView::loadStarted, this, [this] {
        m_stopAction->setEnabled(true);
        m_progress->setValue(0);
        m_progress->show();
    });

    connect(view, &QWebEngineView::loadProgress, this, [this, view](int progress) {
        if (view == currentView()) {
            m_progress->setValue(progress);
            m_progress->setVisible(progress > 0 && progress < 100);
        }
    });

    connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
        if (view == currentView()) {
            m_stopAction->setEnabled(false);
            m_progress->hide();
            updateNavigationState();
        }

        if (ok && isStartDashboard(view)) {
            applyDashboardLocationTo(view);
            QTimer::singleShot(250, view, [this, view] {
                applyDashboardLocationTo(view);
            });
            QTimer::singleShot(1000, view, [this, view] {
                applyDashboardLocationTo(view);
            });
        }

        if (ok && !m_privateMode && !isInternalPage(view)) {
            m_history.appendVisit(view->title(), view->url());
        }
    });

    if (makeCurrent) {
        m_tabs->setCurrentIndex(index);
    }

    if (isAstraSearchUrl(url)) {
        showAstraSearch(view, astraSearchQuery(url));
    } else if (url.isValid() && !url.isEmpty()) {
        view->load(url);
    } else {
        view->setProperty("astraStartDashboard", true);
        view->setHtml(
            newTabHtml(
                m_privateMode,
                m_settingsStore.settings().searchUrlTemplate,
                m_settingsStore.settings().blockTrackers,
                m_settingsStore.settings().restoreSession,
                m_bookmarks.bookmarks().size(),
                m_history.entries().size(),
                systemRegionText()),
            QUrl(QStringLiteral("about:blank")));
    }

    return view;
}

BrowserView *BrowserWindow::currentView() const
{
    return qobject_cast<BrowserView *>(m_tabs->currentWidget());
}

void BrowserWindow::navigateToUserInput()
{
    if (BrowserView *view = currentView()) {
        const QString input = m_urlEdit->text().trimmed();
        if (input.isEmpty()) {
            return;
        }

        if (input.compare(QStringLiteral("search.astrax.local"), Qt::CaseInsensitive) == 0) {
            showAstraSearch(view, {});
            return;
        }

        const QUrl explicitUrl(input);
        if (isAstraSearchUrl(explicitUrl)) {
            showAstraSearch(view, astraSearchQuery(explicitUrl));
            return;
        }

        if (!looksLikeUrlInput(input)) {
            showAstraSearch(view, input);
            return;
        }

        view->load(UrlTools::resolveUserInput(input, m_settingsStore.settings().searchUrlTemplate));
    }
}

void BrowserWindow::navigateHome()
{
    if (BrowserView *view = currentView()) {
        view->load(m_settingsStore.settings().homePage);
    }
}

void BrowserWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabs->count()) {
        return;
    }

    if (m_tabs->count() == 1) {
        createTab(QUrl(), true);
    }

    QWidget *tab = m_tabs->widget(index);
    m_tabs->removeTab(index);
    tab->deleteLater();
}

void BrowserWindow::updateWindowForCurrentTab()
{
    BrowserView *view = currentView();
    const QString title = view ? view->title() : QString();
    const QString prefix = m_privateMode ? QStringLiteral("Private - ") : QString();
    setWindowTitle(prefix + (title.trimmed().isEmpty() ? QStringLiteral("AstraX") : title + QStringLiteral(" - AstraX")));

    if (!view) {
        m_urlEdit->clear();
        m_securityLabel->setText(QStringLiteral("Ready"));
        if (m_pageInfoButton) {
            m_pageInfoButton->setText(QStringLiteral("Page"));
            m_pageInfoButton->setIcon(QIcon());
        }
        return;
    }

    m_urlEdit->setText(UrlTools::displayUrl(view->url()));
    const QString securityText = pageSecurityText(view->url());
    m_securityLabel->setText(QStringLiteral("%1 - %2").arg(securityText, pageHostText(view->url())));
    if (m_pageInfoButton) {
        m_pageInfoButton->setText(securityText);
        m_pageInfoButton->setIcon(style()->standardIcon(view->url().scheme() == QStringLiteral("https")
            ? QStyle::SP_DialogApplyButton
            : QStyle::SP_MessageBoxWarning));
        m_pageInfoButton->setToolTip(QStringLiteral("Page information for %1").arg(pageHostText(view->url())));
    }
}

void BrowserWindow::updateNavigationState()
{
    BrowserView *view = currentView();
    const bool hasView = view != nullptr;
    m_backAction->setEnabled(hasView && view->history()->canGoBack());
    m_forwardAction->setEnabled(hasView && view->history()->canGoForward());
    m_reloadAction->setEnabled(hasView);
    m_addBookmarkAction->setEnabled(hasView && view->url().isValid() && view->url().scheme().startsWith(QStringLiteral("http")));
}

void BrowserWindow::applySettings(const BrowserSettings &settings)
{
    if (m_requestBlocker) {
        m_requestBlocker->setEnabled(settings.blockTrackers);
    }

    if (m_blockTrackersAction && m_blockTrackersAction->isChecked() != settings.blockTrackers) {
        m_blockTrackersAction->setChecked(settings.blockTrackers);
    }

    if (!settings.restoreSession && !m_privateMode) {
        m_sessionStore.clear();
    }
}

void BrowserWindow::addCurrentBookmark()
{
    if (BrowserView *view = currentView()) {
        m_bookmarks.addBookmark(view->title(), view->url());
    }
}

void BrowserWindow::rebuildBookmarksMenu()
{
    if (!m_bookmarksMenu) {
        return;
    }

    m_bookmarksMenu->clear();
    m_bookmarksMenu->addAction(m_addBookmarkAction);

    if (m_bookmarks.bookmarks().isEmpty()) {
        QAction *emptyAction = m_bookmarksMenu->addAction(QStringLiteral("No bookmarks yet"));
        emptyAction->setEnabled(false);
        return;
    }

    m_bookmarksMenu->addSeparator();
    for (const Bookmark &bookmark : m_bookmarks.bookmarks()) {
        QAction *action = m_bookmarksMenu->addAction(elidedTitle(bookmark.title), this, [this, url = bookmark.url] {
            createTab(url, true);
        });
        action->setToolTip(bookmark.url.toDisplayString());
    }
}

void BrowserWindow::rebuildHistoryMenu()
{
    if (!m_historyMenu) {
        return;
    }

    m_historyMenu->clear();

    QAction *clearAction = m_historyMenu->addAction(QStringLiteral("Clear History"), &m_history, &HistoryStore::clear);
    clearAction->setEnabled(!m_history.entries().isEmpty());

    if (m_privateMode) {
        QAction *privateAction = m_historyMenu->addAction(QStringLiteral("History is paused in private mode"));
        privateAction->setEnabled(false);
        return;
    }

    if (m_history.entries().isEmpty()) {
        QAction *emptyAction = m_historyMenu->addAction(QStringLiteral("No history yet"));
        emptyAction->setEnabled(false);
        return;
    }

    m_historyMenu->addSeparator();
    constexpr qsizetype MaxMenuEntries = 25;
    qsizetype count = 0;
    for (const HistoryEntry &entry : m_history.entries()) {
        QAction *action = m_historyMenu->addAction(elidedTitle(entry.title), this, [this, url = entry.url] {
            createTab(url, true);
        });
        action->setToolTip(entry.url.toDisplayString());

        ++count;
        if (count >= MaxMenuEntries) {
            break;
        }
    }
}

void BrowserWindow::rebuildLibrarySidebar()
{
    rebuildBookmarksList();
    rebuildHistoryList();
}

void BrowserWindow::rebuildBookmarksList()
{
    if (!m_bookmarksList) {
        return;
    }

    m_bookmarksList->clear();

    if (m_bookmarks.bookmarks().isEmpty()) {
        auto *item = new QListWidgetItem(QStringLiteral("No bookmarks yet"), m_bookmarksList);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        return;
    }

    for (const Bookmark &bookmark : m_bookmarks.bookmarks()) {
        auto *item = new QListWidgetItem(
            style()->standardIcon(QStyle::SP_DialogYesButton),
            QStringLiteral("%1\n%2").arg(bookmark.title, bookmark.url.toDisplayString()),
            m_bookmarksList);
        item->setToolTip(bookmark.url.toDisplayString());
        item->setData(Qt::UserRole, bookmark.url);
    }
}

void BrowserWindow::rebuildHistoryList()
{
    if (!m_historyList) {
        return;
    }

    m_historyList->clear();

    if (m_privateMode) {
        auto *item = new QListWidgetItem(QStringLiteral("History is paused in private mode"), m_historyList);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        return;
    }

    if (m_history.entries().isEmpty()) {
        auto *item = new QListWidgetItem(QStringLiteral("No history yet"), m_historyList);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        return;
    }

    for (const HistoryEntry &entry : m_history.entries()) {
        const QString visited = entry.visitedAt.toLocalTime().toString(QStringLiteral("dd MMM yyyy, HH:mm"));
        auto *item = new QListWidgetItem(
            style()->standardIcon(QStyle::SP_FileDialogDetailedView),
            QStringLiteral("%1\n%2 - %3").arg(entry.title, visited, entry.url.toDisplayString()),
            m_historyList);
        item->setToolTip(entry.url.toDisplayString());
        item->setData(Qt::UserRole, entry.url);
    }
}

void BrowserWindow::rebuildSearchSuggestions()
{
    if (!m_suggestionModel) {
        return;
    }

    QStringList suggestions;
    m_suggestionTargets.clear();

    const auto addSuggestion = [&](const QString &type, const QString &title, const QUrl &url) {
        if (!url.isValid() || url.isEmpty()) {
            return;
        }

        const QString label = suggestionLabel(type, title, url);
        if (m_suggestionTargets.contains(label)) {
            return;
        }

        suggestions.append(label);
        m_suggestionTargets.insert(label, url);
    };

    for (const Bookmark &bookmark : m_bookmarks.bookmarks()) {
        addSuggestion(QStringLiteral("Bookmark"), bookmark.title, bookmark.url);
    }

    if (!m_privateMode) {
        constexpr qsizetype MaxHistorySuggestions = 120;
        qsizetype count = 0;
        for (const HistoryEntry &entry : m_history.entries()) {
            addSuggestion(QStringLiteral("History"), entry.title, entry.url);
            ++count;
            if (count >= MaxHistorySuggestions) {
                break;
            }
        }
    }

    m_suggestionModel->setStringList(suggestions);
}

void BrowserWindow::openLibraryUrl(QListWidgetItem *item)
{
    if (!item) {
        return;
    }

    const QUrl url = item->data(Qt::UserRole).toUrl();
    if (url.isValid() && !url.isEmpty()) {
        createTab(url, true);
    }
}

void BrowserWindow::showLibraryTab(int index)
{
    if (!m_libraryDock || !m_libraryTabs) {
        return;
    }

    m_libraryTabs->setCurrentIndex(std::clamp(index, 0, m_libraryTabs->count() - 1));
    m_libraryDock->show();
    m_libraryDock->raise();
}

void BrowserWindow::showFindBar()
{
    m_findToolBar->show();
    m_findEdit->setFocus(Qt::ShortcutFocusReason);
    m_findEdit->selectAll();
}

void BrowserWindow::findInPage(bool backwards)
{
    BrowserView *view = currentView();
    if (!view) {
        return;
    }

    const QString needle = m_findEdit->text();
    if (needle.isEmpty()) {
        view->findText(QString());
        return;
    }

    QWebEnginePage::FindFlags flags;
    if (backwards) {
        flags |= QWebEnginePage::FindBackward;
    }

    view->findText(needle, flags);
}

void BrowserWindow::openEngineLab()
{
    auto *engineLab = new EngineLabWindow;
    engineLab->setAttribute(Qt::WA_DeleteOnClose);
    engineLab->show();
    engineLab->raise();
    engineLab->activateWindow();
}

void BrowserWindow::openDevTools()
{
    BrowserView *view = currentView();
    if (!view) {
        return;
    }

    auto *devTools = new QWebEngineView;
    auto *devToolsPage = new QWebEnginePage(m_profile, devTools);
    devTools->setPage(devToolsPage);
    devTools->setAttribute(Qt::WA_DeleteOnClose);
    devTools->resize(1000, 700);
    devTools->setWindowTitle(QStringLiteral("AstraX Developer Tools"));
    view->page()->setDevToolsPage(devTools->page());
    devTools->show();
}

void BrowserWindow::openPageInfoDialog()
{
    BrowserView *view = currentView();
    if (!view) {
        return;
    }

    const QUrl url = view->url();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Page Info"));
    dialog.resize(560, 260);

    auto *layout = new QFormLayout(&dialog);
    layout->setLabelAlignment(Qt::AlignRight);

    auto *titleValue = new QLabel(view->title().trimmed().isEmpty() ? QStringLiteral("New tab") : view->title(), &dialog);
    titleValue->setWordWrap(true);
    titleValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addRow(QStringLiteral("Title"), titleValue);

    auto *urlValue = new QLabel(UrlTools::displayUrl(url).isEmpty() ? url.toDisplayString() : UrlTools::displayUrl(url), &dialog);
    urlValue->setWordWrap(true);
    urlValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addRow(QStringLiteral("URL"), urlValue);

    layout->addRow(QStringLiteral("Connection"), new QLabel(pageSecurityText(url), &dialog));
    layout->addRow(QStringLiteral("Host"), new QLabel(pageHostText(url), &dialog));
    layout->addRow(QStringLiteral("Tracker blocking"), new QLabel(m_settingsStore.settings().blockTrackers ? QStringLiteral("Enabled") : QStringLiteral("Disabled"), &dialog));
    layout->addRow(QStringLiteral("Profile"), new QLabel(m_privateMode ? QStringLiteral("Private memory profile") : storageRoot(), &dialog));
    layout->addRow(QStringLiteral("Zoom"), new QLabel(QStringLiteral("%1%").arg(static_cast<int>(view->zoomFactor() * 100.0)), &dialog));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    dialog.exec();
}

void BrowserWindow::openSettingsDialog()
{
    const BrowserSettings current = m_settingsStore.settings();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("AstraX Settings"));
    dialog.resize(520, 220);

    auto *layout = new QFormLayout(&dialog);

    auto *homePageEdit = new QLineEdit(current.homePage.toString(), &dialog);
    homePageEdit->setClearButtonEnabled(true);
    layout->addRow(QStringLiteral("Homepage"), homePageEdit);

    auto *searchTemplateEdit = new QLineEdit(current.searchUrlTemplate, &dialog);
    searchTemplateEdit->setClearButtonEnabled(true);
    layout->addRow(QStringLiteral("Search URL"), searchTemplateEdit);

    auto *restoreSessionBox = new QCheckBox(&dialog);
    restoreSessionBox->setChecked(current.restoreSession);
    layout->addRow(QStringLiteral("Restore previous tabs"), restoreSessionBox);

    auto *blockTrackersBox = new QCheckBox(&dialog);
    blockTrackersBox->setChecked(current.blockTrackers);
    layout->addRow(QStringLiteral("Block known trackers"), blockTrackersBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (!searchTemplateEdit->text().contains(QStringLiteral("{query}"))) {
            QMessageBox::warning(&dialog, QStringLiteral("Search URL"), QStringLiteral("Search URL must include {query}."));
            return;
        }

        BrowserSettings settings;
        settings.homePage = QUrl::fromUserInput(homePageEdit->text().trimmed());
        settings.searchUrlTemplate = searchTemplateEdit->text().trimmed();
        settings.restoreSession = restoreSessionBox->isChecked();
        settings.blockTrackers = blockTrackersBox->isChecked();

        m_settingsStore.update(settings);
        statusBar()->showMessage(QStringLiteral("Settings saved"), 3000);
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

void BrowserWindow::openPrivateWindow()
{
    auto *window = new BrowserWindow(true);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->resize(size());
    window->show();
}

void BrowserWindow::handleDownload(QWebEngineDownloadRequest *download)
{
    QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadsPath.isEmpty()) {
        downloadsPath = QDir::homePath();
    }

    download->setDownloadDirectory(downloadsPath);
    download->setDownloadFileName(download->suggestedFileName());

    showLibraryTab(2);
    auto *item = new QListWidgetItem(download->downloadFileName(), m_downloadsList);

    const auto refreshItem = [download, item] {
        const qint64 received = download->receivedBytes();
        const qint64 total = download->totalBytes();
        const QString progress = total > 0
            ? QStringLiteral("%1 / %2").arg(humanSize(received), humanSize(total))
            : humanSize(received);
        item->setText(QStringLiteral("%1 - %2").arg(download->downloadFileName(), progress));
    };

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this, refreshItem);
    connect(download, &QWebEngineDownloadRequest::totalBytesChanged, this, refreshItem);
    connect(download, &QWebEngineDownloadRequest::stateChanged, this, [download, item](QWebEngineDownloadRequest::DownloadState state) {
        switch (state) {
        case QWebEngineDownloadRequest::DownloadCompleted:
            item->setText(QStringLiteral("%1 - complete").arg(download->downloadFileName()));
            break;
        case QWebEngineDownloadRequest::DownloadCancelled:
            item->setText(QStringLiteral("%1 - cancelled").arg(download->downloadFileName()));
            break;
        case QWebEngineDownloadRequest::DownloadInterrupted:
            item->setText(QStringLiteral("%1 - interrupted").arg(download->downloadFileName()));
            break;
        case QWebEngineDownloadRequest::DownloadRequested:
        case QWebEngineDownloadRequest::DownloadInProgress:
            break;
        }
    });

    download->accept();
}

void BrowserWindow::showAstraSearch(BrowserView *view, const QString &query)
{
    if (!view) {
        return;
    }

    const QString cleanQuery = query.trimmed();
    QVector<SearchResult> results;

    if (!cleanQuery.isEmpty()) {
        for (const Bookmark &bookmark : m_bookmarks.bookmarks()) {
            const QString urlText = bookmark.url.toDisplayString();
            const int score = resultScore(bookmark.title, urlText, cleanQuery);
            if (score > 0) {
                results.append(SearchResult{
                    QStringLiteral("Bookmark"),
                    bookmark.title,
                    bookmark.url,
                    QStringLiteral("Saved bookmark"),
                    score + 20,
                });
            }
        }

        for (const HistoryEntry &entry : m_history.entries()) {
            const QString urlText = entry.url.toDisplayString();
            const int score = resultScore(entry.title, urlText, cleanQuery);
            if (score > 0) {
                results.append(SearchResult{
                    QStringLiteral("History"),
                    entry.title,
                    entry.url,
                    QStringLiteral("Visited %1").arg(entry.visitedAt.toLocalTime().toString(QStringLiteral("MMM d, h:mm AP"))),
                    score,
                });
            }
        }
    }

    std::sort(results.begin(), results.end(), [](const SearchResult &left, const SearchResult &right) {
        return left.score > right.score;
    });

    QString resultsHtml;
    constexpr qsizetype MaxResults = 24;
    const qsizetype visibleResults = std::min(results.size(), MaxResults);
    for (qsizetype index = 0; index < visibleResults; ++index) {
        resultsHtml += searchResultHtml(results.at(index));
    }

    if (resultsHtml.isEmpty()) {
        resultsHtml = QStringLiteral(R"(
          <div class="empty">
            <strong>No local matches yet</strong>
            <span>AstraX Search currently indexes your bookmarks and browsing history. Use the web fallback, then bookmark useful pages to grow your local index.</span>
          </div>
)");
    }

    const QString resultSummary = cleanQuery.isEmpty()
        ? QStringLiteral("Type a search query to search your AstraX local index.")
        : QStringLiteral("%1 local result%2 from bookmarks and history")
            .arg(QString::number(visibleResults))
            .arg(visibleResults == 1 ? QString() : QStringLiteral("s"));
    const QUrl webSearchUrl = UrlTools::searchUrlFromTemplate(m_settingsStore.settings().searchUrlTemplate, cleanQuery);
    const QString logoMarkup = appLogoMarkup();
    QJsonArray suggestionData;
    QStringList seenSuggestionUrls;

    const auto addSuggestion = [&](const QString &type, const QString &title, const QUrl &url) {
        if (!url.isValid() || url.isEmpty()) {
            return;
        }

        const QString key = url.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
        if (seenSuggestionUrls.contains(key)) {
            return;
        }

        QJsonObject item;
        item.insert(QStringLiteral("type"), type);
        item.insert(QStringLiteral("title"), title.trimmed().isEmpty() ? url.toDisplayString() : title.trimmed());
        item.insert(QStringLiteral("url"), url.toString());
        item.insert(QStringLiteral("label"), suggestionLabel(type, title, url));
        suggestionData.append(item);
        seenSuggestionUrls.append(key);
    };

    for (const Bookmark &bookmark : m_bookmarks.bookmarks()) {
        addSuggestion(QStringLiteral("Bookmark"), bookmark.title, bookmark.url);
    }

    if (!m_privateMode) {
        constexpr qsizetype MaxPageSuggestions = 120;
        qsizetype count = 0;
        for (const HistoryEntry &entry : m_history.entries()) {
            addSuggestion(QStringLiteral("History"), entry.title, entry.url);
            ++count;
            if (count >= MaxPageSuggestions) {
                break;
            }
        }
    }

    const QString suggestionJson = QString::fromUtf8(QJsonDocument(suggestionData).toJson(QJsonDocument::Compact));

    QString html = QStringLiteral(R"(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AstraX Search</title>
  <style>
    :root {
      color-scheme: light dark;
      font-family: Inter, Segoe UI, system-ui, sans-serif;
      --bg: #f4f7fb;
      --ink: #102033;
      --muted: #64748b;
      --panel: rgba(255, 255, 255, .86);
      --line: rgba(100, 116, 139, .24);
      --accent: #0f766e;
      --accent-strong: #155e75;
      --mark: #2563eb;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background:
        linear-gradient(135deg, rgba(248, 250, 252, .98), rgba(226, 232, 240, .94)),
        url("data:image/svg+xml,%3Csvg width='96' height='96' viewBox='0 0 96 96' xmlns='http://www.w3.org/2000/svg'%3E%3Cg fill='none' stroke='%2394a3b8' stroke-opacity='.16'%3E%3Cpath d='M0 24h96M0 72h96M24 0v96M72 0v96'/%3E%3C/g%3E%3C/svg%3E");
      color: var(--ink);
      padding: 34px 20px;
    }
    main {
      width: min(1060px, 100%);
      margin: 0 auto;
      display: grid;
      gap: 18px;
    }
    header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 18px;
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 14px;
      min-width: 0;
    }
    .brand img {
      width: 54px;
      height: 54px;
      object-fit: contain;
      filter: drop-shadow(0 10px 18px rgba(15, 23, 42, .16));
    }
    h1 {
      margin: 0;
      font-size: 34px;
      line-height: 1;
      letter-spacing: 0;
    }
    .subtitle {
      margin-top: 4px;
      color: var(--muted);
      font-size: 14px;
    }
    .engine-chip {
      border: 1px solid rgba(15, 118, 110, .24);
      border-radius: 12px;
      color: #115e59;
      background: rgba(204, 251, 241, .62);
      padding: 10px 12px;
      font-weight: 800;
      white-space: nowrap;
    }
    .search-panel {
      border: 1px solid var(--line);
      border-radius: 18px;
      background: var(--panel);
      box-shadow: 0 24px 70px rgba(15, 23, 42, .10);
      padding: 18px;
      backdrop-filter: blur(18px);
    }
    form {
      display: grid;
      grid-template-columns: 1fr auto auto;
      gap: 10px;
    }
    input {
      min-width: 0;
      border: 1px solid #b8c5d5;
      border-radius: 12px;
      padding: 15px 16px;
      font-size: 18px;
      color: var(--ink);
      background: #fff;
      outline: none;
    }
    input:focus {
      border-color: var(--mark);
      box-shadow: 0 0 0 4px rgba(37, 99, 235, .13);
    }
    button, .web-button {
      border: 0;
      border-radius: 12px;
      padding: 0 18px;
      min-height: 52px;
      color: #fff;
      background: var(--accent);
      font-size: 15px;
      font-weight: 800;
      cursor: pointer;
      text-decoration: none;
      display: inline-grid;
      place-items: center;
    }
    button:hover, .web-button:hover { background: var(--accent-strong); }
    .web-button {
      background: #1e293b;
    }
    .summary {
      color: var(--muted);
      margin-top: 12px;
      font-size: 14px;
    }
    .suggestions {
      display: none;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 8px;
      margin-top: 12px;
    }
    button.suggestion {
      min-height: 0;
      background: rgba(248, 250, 252, .88);
      color: var(--ink);
      border: 1px solid var(--line);
      padding: 10px 12px;
      text-align: left;
      display: grid;
      place-items: stretch;
      gap: 3px;
    }
    button.suggestion:hover {
      background: #e0f2fe;
      border-color: rgba(37, 99, 235, .30);
    }
    button.suggestion strong {
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    button.suggestion span {
      color: var(--muted);
      font-size: 12px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }
    .layout {
      display: grid;
      grid-template-columns: 1fr 300px;
      gap: 18px;
      align-items: start;
    }
    .results {
      display: grid;
      gap: 10px;
    }
    .result {
      border: 1px solid var(--line);
      border-radius: 14px;
      background: rgba(255, 255, 255, .82);
      color: var(--ink);
      padding: 14px 16px;
      text-decoration: none;
      display: grid;
      gap: 5px;
      transition: transform .16s ease, border-color .16s ease, box-shadow .16s ease;
    }
    .result:hover {
      transform: translateY(-1px);
      border-color: rgba(37, 99, 235, .38);
      box-shadow: 0 14px 34px rgba(15, 23, 42, .10);
    }
    .result-type {
      color: var(--accent);
      font-size: 12px;
      font-weight: 900;
      text-transform: uppercase;
      letter-spacing: .04em;
    }
    .result strong {
      font-size: 18px;
      line-height: 1.25;
    }
    .result-url {
      color: #2563eb;
      overflow-wrap: anywhere;
      font-size: 14px;
    }
    .result-detail {
      color: var(--muted);
      font-size: 13px;
    }
    .side {
      border: 1px solid var(--line);
      border-radius: 14px;
      background: rgba(255, 255, 255, .74);
      padding: 16px;
      display: grid;
      gap: 12px;
    }
    .side h2 {
      margin: 0;
      font-size: 17px;
    }
    .side p {
      margin: 0;
      color: var(--muted);
      line-height: 1.45;
    }
    .stat-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }
    .stat {
      border: 1px solid var(--line);
      border-radius: 12px;
      padding: 10px;
      color: var(--muted);
      background: rgba(248, 250, 252, .76);
    }
    .stat strong {
      display: block;
      color: var(--ink);
      font-size: 20px;
    }
    .empty {
      border: 1px dashed rgba(100, 116, 139, .42);
      border-radius: 14px;
      padding: 18px;
      color: var(--muted);
      background: rgba(255, 255, 255, .58);
      display: grid;
      gap: 6px;
    }
    .empty strong {
      color: var(--ink);
      font-size: 18px;
    }
    @media (prefers-color-scheme: dark) {
      :root {
        --bg: #07111f;
        --ink: #ecfdf5;
        --muted: #9fb0c6;
        --panel: rgba(15, 23, 42, .86);
        --line: rgba(148, 163, 184, .22);
      }
      body {
        background:
          linear-gradient(135deg, rgba(2, 6, 23, .96), rgba(17, 24, 39, .94)),
          url("data:image/svg+xml,%3Csvg width='96' height='96' viewBox='0 0 96 96' xmlns='http://www.w3.org/2000/svg'%3E%3Cg fill='none' stroke='%2364748b' stroke-opacity='.16'%3E%3Cpath d='M0 24h96M0 72h96M24 0v96M72 0v96'/%3E%3C/g%3E%3C/svg%3E");
      }
      input, .result { background: rgba(15, 23, 42, .92); color: #f8fafc; border-color: #475569; }
      .search-panel, .side { background: rgba(15, 23, 42, .74); }
      .stat, .empty { background: rgba(15, 23, 42, .58); }
      button.suggestion { background: rgba(15, 23, 42, .72); color: #f8fafc; border-color: #475569; }
      button.suggestion:hover { background: rgba(30, 41, 59, .92); }
      .engine-chip { background: rgba(20, 184, 166, .16); color: #99f6e4; border-color: rgba(45, 212, 191, .28); }
      .result-url { color: #93c5fd; }
    }
    @media (max-width: 820px) {
      body { padding: 24px 14px; }
      header, .layout, form { grid-template-columns: 1fr; display: grid; }
      .suggestions { grid-template-columns: 1fr; }
      .engine-chip { justify-self: start; white-space: normal; }
      button, .web-button { width: 100%; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <div class="brand">
        __LOGO__
        <div>
          <h1>AstraX Search</h1>
          <div class="subtitle">Private local results first, web fallback when you need it</div>
        </div>
      </div>
      <div class="engine-chip">Local Engine</div>
    </header>

    <section class="search-panel">
      <form id="search-form">
        <input name="q" autofocus autocomplete="off" spellcheck="false" value="__QUERY__" placeholder="Search bookmarks, history, or the web">
        <button>Search</button>
        <a class="web-button" href="__WEB_URL__">Search Web</a>
      </form>
      <div class="suggestions" id="suggestions"></div>
      <div class="summary">__SUMMARY__</div>
    </section>

    <section class="layout">
      <div class="results">
__RESULTS__
      </div>
      <aside class="side">
        <h2>Index Status</h2>
        <p>AstraX Search currently ranks your bookmarks and browsing history before sending anything to the web.</p>
        <div class="stat-grid">
          <div class="stat"><strong>__BOOKMARKS__</strong>Bookmarks</div>
          <div class="stat"><strong>__HISTORY__</strong>History</div>
        </div>
        <p>Local suggestions refresh as bookmarks and history change.</p>
      </aside>
    </section>
  </main>
  <script>
    const localSuggestions = __SUGGESTIONS__;
    const form = document.getElementById('search-form');
    const input = form.elements.q;
    const suggestionBox = document.getElementById('suggestions');
    const renderSuggestions = () => {
      const value = input.value.trim().toLowerCase();
      suggestionBox.replaceChildren();
      if (!value) {
        suggestionBox.style.display = 'none';
        return;
      }
      const terms = value.split(/\s+/).filter(Boolean);
      const matches = localSuggestions.filter(item => {
        const haystack = `${item.title} ${item.url}`.toLowerCase();
        return terms.every(term => haystack.includes(term));
      }).slice(0, 6);
      if (!matches.length) {
        suggestionBox.style.display = 'none';
        return;
      }
      matches.forEach(item => {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'suggestion';
        const title = document.createElement('strong');
        title.textContent = `${item.type}: ${item.title}`;
        const url = document.createElement('span');
        url.textContent = item.url;
        button.append(title, url);
        button.addEventListener('click', () => {
          window.location.href = item.url;
        });
        suggestionBox.append(button);
      });
      suggestionBox.style.display = 'grid';
    };
    input.addEventListener('input', renderSuggestions);
    input.addEventListener('focus', renderSuggestions);
    renderSuggestions();
    form.addEventListener('submit', event => {
      event.preventDefault();
      const query = input.value.trim();
      if (!query) return;
      window.location.href = `astrax://search?q=${encodeURIComponent(query)}`;
    });
  </script>
</body>
</html>
)");

    html.replace(QStringLiteral("__LOGO__"), logoMarkup);
    html.replace(QStringLiteral("__QUERY__"), cleanQuery.toHtmlEscaped());
    html.replace(QStringLiteral("__WEB_URL__"), webSearchUrl.toString(QUrl::FullyEncoded).toHtmlEscaped());
    html.replace(QStringLiteral("__SUMMARY__"), resultSummary.toHtmlEscaped());
    html.replace(QStringLiteral("__RESULTS__"), resultsHtml);
    html.replace(QStringLiteral("__SUGGESTIONS__"), suggestionJson);
    html.replace(QStringLiteral("__BOOKMARKS__"), QString::number(m_bookmarks.bookmarks().size()));
    html.replace(QStringLiteral("__HISTORY__"), QString::number(m_history.entries().size()));

    view->setProperty("astraStartDashboard", false);
    view->setProperty("astraSearchPage", true);
    view->setHtml(html, astraSearchUrl(cleanQuery));
}

void BrowserWindow::refreshInternetLocation()
{
    if (m_privateMode || !m_networkAccess || m_locationLookupInFlight) {
        return;
    }

    m_locationLookupInFlight = true;
    m_locationSource = QStringLiteral("Checking live internet location...");
    applyDashboardLocation();
    requestInternetLocation(QUrl(QStringLiteral("https://ipwho.is/")));
}

void BrowserWindow::requestInternetLocation(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QStringLiteral("AstraXBrowser/") + QStringLiteral(ASTRA_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(8000);

    QNetworkReply *reply = m_networkAccess->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleInternetLocationReply(reply);
    });
}

void BrowserWindow::handleInternetLocationReply(QNetworkReply *reply)
{
    const QUrl sourceUrl = reply->request().url();
    const QString host = sourceUrl.host();
    const QString errorText = reply->errorString();
    const bool requestFailed = reply->error() != QNetworkReply::NoError;
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (!requestFailed) {
        const InternetLocation location = parseInternetLocation(payload, host);
        if (location.valid) {
            m_locationMain = location.main;
            m_locationDetail = location.detail;
            m_locationBadge = location.badge;
            m_locationSource = QStringLiteral("Live internet location from %1").arg(host);
            m_locationStatus = QStringLiteral("connected");
            m_locationCodeText = QStringLiteral("connected");
            m_locationLookupInFlight = false;
            applyDashboardLocation();
            QTimer::singleShot(600000, this, &BrowserWindow::refreshInternetLocation);
            return;
        }
    }

    if (host == QStringLiteral("ipwho.is")) {
        requestInternetLocation(QUrl(QStringLiteral("https://ipapi.co/json/")));
        return;
    }

    if (m_locationStatus == QStringLiteral("connected")) {
        m_locationSource = requestFailed
            ? QStringLiteral("Live internet location cached; refresh failed: %1").arg(errorText)
            : QStringLiteral("Live internet location cached; refresh unavailable");
        m_locationCodeText = QStringLiteral("cached");
    } else {
        m_locationMain = systemRegionText();
        m_locationDetail = requestFailed
            ? QStringLiteral("Internet lookup unavailable: %1").arg(errorText)
            : QStringLiteral("Internet lookup unavailable: using system region fallback");
        m_locationBadge = QStringLiteral("Fallback");
        m_locationSource = QStringLiteral("Location service unavailable");
        m_locationStatus = QStringLiteral("fallback");
        m_locationCodeText = QStringLiteral("system");
    }
    m_locationLookupInFlight = false;
    applyDashboardLocation();
    QTimer::singleShot(60000, this, &BrowserWindow::refreshInternetLocation);
}

void BrowserWindow::applyDashboardLocation()
{
    if (!m_tabs) {
        return;
    }

    for (int index = 0; index < m_tabs->count(); ++index) {
        applyDashboardLocationTo(qobject_cast<BrowserView *>(m_tabs->widget(index)));
    }
}

void BrowserWindow::applyDashboardLocationTo(BrowserView *view)
{
    if (!isStartDashboard(view) || !view->page()) {
        return;
    }

    QJsonObject location;
    location.insert(QStringLiteral("main"), m_locationMain);
    location.insert(QStringLiteral("detail"), m_locationDetail);
    location.insert(QStringLiteral("badge"), m_locationBadge);
    location.insert(QStringLiteral("source"), m_locationSource);
    location.insert(QStringLiteral("status"), m_locationStatus);
    location.insert(QStringLiteral("codeText"), m_locationCodeText);

    const QString payload = QString::fromUtf8(QJsonDocument(location).toJson(QJsonDocument::Compact));
    view->page()->runJavaScript(QStringLiteral("if (window.astraxSetLocation) window.astraxSetLocation(%1);").arg(payload));
}

bool BrowserWindow::isStartDashboard(const BrowserView *view) const
{
    return view && view->property("astraStartDashboard").toBool();
}

bool BrowserWindow::isInternalPage(const BrowserView *view) const
{
    return view && (view->property("astraStartDashboard").toBool() || view->property("astraSearchPage").toBool());
}

bool BrowserWindow::restorePreviousSession()
{
    if (m_privateMode || !m_settingsStore.settings().restoreSession) {
        return false;
    }

    const BrowserSession &session = m_sessionStore.session();
    if (session.tabs.isEmpty()) {
        return false;
    }

    for (const QUrl &url : session.tabs) {
        createTab(url, false);
    }
    return true;
}

BrowserSession BrowserWindow::currentSession() const
{
    BrowserSession session;

    const int currentIndex = m_tabs->currentIndex();
    for (int index = 0; index < m_tabs->count(); ++index) {
        const auto *view = qobject_cast<BrowserView *>(m_tabs->widget(index));
        if (!view) {
            continue;
        }

        const QUrl url = view->url();
        if (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")) {
            continue;
        }

        if (index == currentIndex) {
            session.currentIndex = static_cast<int>(session.tabs.size());
        }
        session.tabs.push_back(url);
    }

    return session;
}

void BrowserWindow::saveSession()
{
    if (m_privateMode || !m_settingsStore.settings().restoreSession) {
        return;
    }

    m_sessionStore.save(currentSession());
}
}
