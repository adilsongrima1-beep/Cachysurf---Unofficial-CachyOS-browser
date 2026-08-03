#include "adaptivetabbar.h"

#include <QColor>
#include <QEasingCurve>
#include <QPainter>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kMinimumTabWidth = 92;
constexpr int kTabHeight = 38;
constexpr int kOuterAllowance = 0;
}

AdaptiveTabBar::AdaptiveTabBar(QWidget *parent)
    : QTabBar(parent)
{
    setUsesScrollButtons(true);
    setExpanding(false);
    setElideMode(Qt::ElideRight);
    setDrawBase(false);
    setMouseTracking(true);

    m_widthAnimation = new QVariantAnimation(this);
    m_widthAnimation->setDuration(240);
    m_widthAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_widthAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) { setAnimatedTabWidth(value.toReal()); });

    m_indicatorAnimation = new QVariantAnimation(this);
    m_indicatorAnimation->setDuration(210);
    m_indicatorAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_indicatorAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
        m_indicatorRect = value.toRectF();
        update();
    });
    connect(m_indicatorAnimation, &QVariantAnimation::finished, this, [this] {
        m_indicatorAnimating = false;
        m_indicatorRect = selectedRect(currentIndex());
        update();
    });

    m_hoverRectAnimation = new QVariantAnimation(this);
    m_hoverRectAnimation->setDuration(135);
    m_hoverRectAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverRectAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
        m_hoverRect = value.toRectF();
        update();
    });

    m_hoverOpacityAnimation = new QVariantAnimation(this);
    m_hoverOpacityAnimation->setDuration(120);
    m_hoverOpacityAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverOpacityAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
        m_hoverOpacity = value.toReal();
        update();
    });

    connect(this, &QTabBar::currentChanged, this,
            [this](int index) { animateIndicator(index); });
    connect(this, &QTabBar::tabMoved, this, [this](int, int) {
        animateIndicator(currentIndex());
        animateHover(m_hoveredIndex);
    });
}

qreal AdaptiveTabBar::animatedTabWidth() const
{
    return m_animatedTabWidth;
}

void AdaptiveTabBar::setAnimatedTabWidth(qreal width)
{
    if (qFuzzyCompare(m_animatedTabWidth, width))
        return;

    m_animatedTabWidth = width;

    // QTabBar caches each tab rect. Send its resize handler a same-size event
    // so it re-reads tabSizeHint() on every animation frame.
    QResizeEvent layoutEvent(size(), size());
    QTabBar::resizeEvent(&layoutEvent);

    updateGeometry();
    if (!m_indicatorAnimating)
        m_indicatorRect = selectedRect(currentIndex());
    if (m_hoveredIndex >= 0 && !m_hoverRectAnimation->state())
        m_hoverRect = selectedRect(m_hoveredIndex);
    update();
}

QSize AdaptiveTabBar::tabSizeHint(int index) const
{
    Q_UNUSED(index)
    return QSize(std::max(kMinimumTabWidth, qRound(m_animatedTabWidth)), kTabHeight);
}

int AdaptiveTabBar::targetTabWidth() const
{
    const int tabCount = std::max(1, count());
    const int available = std::max(kMinimumTabWidth, width() - kOuterAllowance);
    // Safari-style adaptive tabs: one tab consumes the complete strip, two
    // tabs split it in half, and so on. Only stop shrinking once the minimum
    // readable width is reached; QTabBar then exposes its scroll buttons.
    return std::max(kMinimumTabWidth, available / tabCount);
}

void AdaptiveTabBar::refreshAnimatedLayout()
{
    animateWidths();
}

void AdaptiveTabBar::animateWidths()
{
    const qreal target = targetTabWidth();
    if (std::abs(target - m_animatedTabWidth) < 0.5) {
        setAnimatedTabWidth(target);
        return;
    }

    m_widthAnimation->stop();
    m_widthAnimation->setStartValue(m_animatedTabWidth);
    m_widthAnimation->setEndValue(target);
    m_widthAnimation->start();
}

QRectF AdaptiveTabBar::selectedRect(int index) const
{
    if (index < 0 || index >= count())
        return {};
    const QRectF rect(tabRect(index));
    return count() == 1 ? rect.adjusted(0.0, 2.0, 0.0, -2.0)
                        : rect.adjusted(2.0, 3.0, -2.0, -3.0);
}

void AdaptiveTabBar::animateIndicator(int index)
{
    if (index < 0)
        return;

    QTimer::singleShot(0, this, [this, index] {
        const QRectF destination = selectedRect(index);
        if (!destination.isValid())
            return;

        const QRectF start = m_indicatorRect.isValid() ? m_indicatorRect : destination;
        m_indicatorAnimation->stop();
        m_indicatorAnimating = true;
        m_indicatorAnimation->setStartValue(start);
        m_indicatorAnimation->setEndValue(destination);
        m_indicatorAnimation->start();
    });
}

void AdaptiveTabBar::animateHover(int index)
{
    m_hoveredIndex = index;
    const QRectF destination = selectedRect(index);

    m_hoverOpacityAnimation->stop();
    m_hoverOpacityAnimation->setStartValue(m_hoverOpacity);
    m_hoverOpacityAnimation->setEndValue(index >= 0 ? 1.0 : 0.0);
    m_hoverOpacityAnimation->start();

    if (index < 0 || !destination.isValid())
        return;

    const QRectF start = m_hoverRect.isValid() ? m_hoverRect : destination;
    m_hoverRectAnimation->stop();
    m_hoverRectAnimation->setStartValue(start);
    m_hoverRectAnimation->setEndValue(destination);
    m_hoverRectAnimation->start();
}

void AdaptiveTabBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (!m_indicatorRect.isValid())
        m_indicatorRect = selectedRect(currentIndex());

    QColor indicator(property("indicatorColor").toString());
    if (!indicator.isValid())
        indicator = palette().color(QPalette::Base);

    QColor hover(property("hoverColor").toString());
    if (!hover.isValid())
        hover = palette().color(QPalette::Mid);
    hover.setAlphaF(std::clamp(0.24 * m_hoverOpacity, 0.0, 1.0));

    if (m_hoverRect.isValid() && m_hoverOpacity > 0.001 && m_hoveredIndex != currentIndex()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(hover);
        painter.drawRoundedRect(m_hoverRect, 11.0, 11.0);
    }

    if (m_indicatorRect.isValid()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(indicator);
        painter.drawRoundedRect(m_indicatorRect, 11.0, 11.0);
    }

    painter.end();
    QTabBar::paintEvent(event);
}

void AdaptiveTabBar::mouseMoveEvent(QMouseEvent *event)
{
    const int index = tabAt(event->position().toPoint());
    if (index != m_hoveredIndex)
        animateHover(index);
    QTabBar::mouseMoveEvent(event);
}

void AdaptiveTabBar::leaveEvent(QEvent *event)
{
    animateHover(-1);
    QTabBar::leaveEvent(event);
}

void AdaptiveTabBar::resizeEvent(QResizeEvent *event)
{
    QTabBar::resizeEvent(event);
    animateWidths();
}

void AdaptiveTabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    QTimer::singleShot(0, this, [this] {
        setTabsClosable(count() > 1);
        animateWidths();
        animateIndicator(currentIndex());
    });
}

void AdaptiveTabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);
    QTimer::singleShot(0, this, [this] {
        setTabsClosable(count() > 1);
        animateWidths();
        animateIndicator(currentIndex());
    });
}
