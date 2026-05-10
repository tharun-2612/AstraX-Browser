#pragma once

#include <QSet>
#include <QStringList>
#include <QWebEngineUrlRequestInterceptor>

#include <atomic>

namespace astra
{
class RequestBlocker final : public QWebEngineUrlRequestInterceptor
{
public:
    explicit RequestBlocker(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] bool shouldBlock(const QUrl &url) const;

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

private:
    std::atomic_bool m_enabled = true;
    QSet<QString> m_blockedHosts;
    QStringList m_blockedPathTokens;
};
}
