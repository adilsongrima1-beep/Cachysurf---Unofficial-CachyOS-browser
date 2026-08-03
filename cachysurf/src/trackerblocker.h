#pragma once

#include <QSet>
#include <QWebEngineUrlRequestInterceptor>

class TrackerBlocker final : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    explicit TrackerBlocker(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

    void setEnabled(bool enabled);
    bool isEnabled() const;

private:
    bool m_enabled = true;
    QSet<QString> m_blockedHosts;
    QSet<QString> m_compatibilityHosts;
};
