#include "BrowserView.h"

#include <QWebEnginePage>

#include <utility>

namespace astra
{
BrowserView::BrowserView(QWidget *parent)
    : QWebEngineView(parent)
{
}

void BrowserView::setTabFactory(TabFactory factory)
{
    m_tabFactory = std::move(factory);
}

QWebEngineView *BrowserView::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type)

    if (m_tabFactory) {
        return m_tabFactory();
    }

    return nullptr;
}
}
