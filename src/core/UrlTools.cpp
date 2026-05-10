#include "UrlTools.h"

#include <QRegularExpression>
#include <QUrlQuery>

namespace
{
bool looksLikeHostName(const QString &text)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s"));
    if (text.contains(whitespace)) {
        return false;
    }

    return text.contains(QLatin1Char('.'))
        || text.startsWith(QStringLiteral("localhost"), Qt::CaseInsensitive)
        || text.startsWith(QStringLiteral("127."))
        || text.startsWith(QStringLiteral("[::1]"));
}
}

namespace astra::UrlTools
{
QString defaultSearchUrlTemplate()
{
    return QStringLiteral("https://duckduckgo.com/?q={query}");
}

QUrl searchUrlFromTemplate(const QString &searchUrlTemplate, const QString &query)
{
    QString resolvedTemplate = searchUrlTemplate.trimmed();
    if (!resolvedTemplate.contains(QStringLiteral("{query}"))) {
        resolvedTemplate = defaultSearchUrlTemplate();
    }

    const QString encodedQuery = QString::fromLatin1(QUrl::toPercentEncoding(query.trimmed()));
    return QUrl(resolvedTemplate.replace(QStringLiteral("{query}"), encodedQuery));
}

QUrl resolveUserInput(const QString &input)
{
    return resolveUserInput(input, defaultSearchUrlTemplate());
}

QUrl resolveUserInput(const QString &input, const QString &searchUrlTemplate)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return QUrl(QStringLiteral("about:blank"));
    }

    const QUrl explicitUrl(trimmed);
    if (explicitUrl.isValid() && !explicitUrl.scheme().isEmpty()) {
        return explicitUrl;
    }

    if (looksLikeHostName(trimmed)) {
        return QUrl::fromUserInput(trimmed);
    }

    return searchUrlFromTemplate(searchUrlTemplate, trimmed);
}

QString displayUrl(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty() || url.scheme() == QStringLiteral("about")) {
        return {};
    }

    return url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile);
}
}
