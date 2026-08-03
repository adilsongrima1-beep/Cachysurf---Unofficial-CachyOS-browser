#include "trackerblocker.h"

#include <QWebEngineUrlRequestInfo>

#include <utility>

TrackerBlocker::TrackerBlocker(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent),
      m_blockedHosts({
          QStringLiteral("doubleclick.net"),
          QStringLiteral("googletagmanager.com"),
          QStringLiteral("google-analytics.com"),
          QStringLiteral("adnxs.com"),
          QStringLiteral("scorecardresearch.com"),
          QStringLiteral("facebook.net"),
          QStringLiteral("hotjar.com"),
          QStringLiteral("segment.io"),
          QStringLiteral("mixpanel.com")
      }),
      m_compatibilityHosts({
          QStringLiteral("youtube.com"),
          QStringLiteral("googlevideo.com"),
          QStringLiteral("ytimg.com"),
          QStringLiteral("google.com"),
          QStringLiteral("gstatic.com")
      })
{
}

void TrackerBlocker::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (!m_enabled)
        return;

    const QString host = info.requestUrl().host().toLower();
    for (const QString &compatibilityHost : std::as_const(m_compatibilityHosts)) {
        if (host == compatibilityHost || host.endsWith(QStringLiteral(".") + compatibilityHost)) {
            info.setHttpHeader("DNT", "1");
            return;
        }
    }

    for (const QString &blockedHost : std::as_const(m_blockedHosts)) {
        if (host == blockedHost || host.endsWith(QStringLiteral(".") + blockedHost)) {
            info.block(true);
            return;
        }
    }

    info.setHttpHeader("DNT", "1");
}

void TrackerBlocker::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool TrackerBlocker::isEnabled() const
{
    return m_enabled;
}
