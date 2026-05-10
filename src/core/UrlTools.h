#pragma once

#include <QUrl>

namespace astra::UrlTools
{
QString defaultSearchUrlTemplate();
QUrl searchUrlFromTemplate(const QString &searchUrlTemplate, const QString &query);
QUrl resolveUserInput(const QString &input);
QUrl resolveUserInput(const QString &input, const QString &searchUrlTemplate);
QString displayUrl(const QUrl &url);
}
