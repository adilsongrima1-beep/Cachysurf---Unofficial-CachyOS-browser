#pragma once

#include <QWebEngineView>

class BrowserWindow;

class WebView final : public QWebEngineView
{
    Q_OBJECT

public:
    explicit WebView(BrowserWindow *browserWindow, QWidget *parent = nullptr);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

private:
    BrowserWindow *m_browserWindow;
};
