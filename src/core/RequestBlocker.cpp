#include "RequestBlocker.h"

#include <QUrl>
#include <QWebEngineUrlRequestInfo>

namespace
{
bool hostMatches(const QString &host, const QString &blockedHost)
{
    return host == blockedHost || host.endsWith(QStringLiteral(".") + blockedHost);
}
}

namespace astra
{
RequestBlocker::RequestBlocker(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
    , m_blockedHosts({
          QStringLiteral("doubleclick.net"),
          QStringLiteral("googlesyndication.com"),
          QStringLiteral("google-analytics.com"),
          QStringLiteral("googletagmanager.com"),
          QStringLiteral("facebook.net"),
          QStringLiteral("connect.facebook.net"),
          QStringLiteral("scorecardresearch.com"),
          QStringLiteral("hotjar.com"),
          QStringLiteral("adsystem.com"),
          QStringLiteral("adnxs.com"),
          QStringLiteral("taboola.com"),
          QStringLiteral("outbrain.com"),
      })
    , m_blockedPathTokens({
          QStringLiteral("/ads/"),
          QStringLiteral("/adserver/"),
          QStringLiteral("/analytics/"),
          QStringLiteral("/track/"),
          QStringLiteral("/tracker/"),
          QStringLiteral("/pixel"),
          QStringLiteral("utm_source="),
      })
{
}

void RequestBlocker::setEnabled(bool enabled)
{
    m_enabled.store(enabled);
}

bool RequestBlocker::isEnabled() const
{
    return m_enabled.load();
}

bool RequestBlocker::shouldBlock(const QUrl &url) const
{
    if (!m_enabled.load() || !url.isValid()) {
        return false;
    }

    const QString host = url.host().toLower();
    for (const QString &blockedHost : m_blockedHosts) {
        if (hostMatches(host, blockedHost)) {
            return true;
        }
    }

    const QString resource = url.path().toLower() + QStringLiteral("?") + url.query().toLower();
    for (const QString &token : m_blockedPathTokens) {
        if (resource.contains(token)) {
            return true;
        }
    }

    return false;
}

void RequestBlocker::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (shouldBlock(info.requestUrl())) {
        info.block(true);
    }
}
}
