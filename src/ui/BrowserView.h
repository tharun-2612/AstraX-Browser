#pragma once

#include <QWebEngineView>

#include <functional>

namespace astra
{
class BrowserView final : public QWebEngineView
{
    Q_OBJECT

public:
    using TabFactory = std::function<BrowserView *()>;

    explicit BrowserView(QWidget *parent = nullptr);

    void setTabFactory(TabFactory factory);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

private:
    TabFactory m_tabFactory;
};
}
