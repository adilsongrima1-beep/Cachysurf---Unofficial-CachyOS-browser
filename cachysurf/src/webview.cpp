#include "webview.h"
#include "browserwindow.h"

WebView::WebView(BrowserWindow *browserWindow, QWidget *parent)
    : QWebEngineView(parent), m_browserWindow(browserWindow)
{
}

QWebEngineView *WebView::createWindow(QWebEnginePage::WebWindowType type)
{
    const bool activate = type != QWebEnginePage::WebBrowserBackgroundTab;
    return m_browserWindow->createTab(QUrl(), activate);
}
