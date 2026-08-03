#pragma once

#include <QRectF>
#include <QTabBar>

class QEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QVariantAnimation;

class AdaptiveTabBar final : public QTabBar
{
    Q_OBJECT
    Q_PROPERTY(qreal animatedTabWidth READ animatedTabWidth WRITE setAnimatedTabWidth)

public:
    explicit AdaptiveTabBar(QWidget *parent = nullptr);

    qreal animatedTabWidth() const;
    void setAnimatedTabWidth(qreal width);
    void refreshAnimatedLayout();

protected:
    QSize tabSizeHint(int index) const override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void tabInserted(int index) override;
    void tabRemoved(int index) override;

private:
    int targetTabWidth() const;
    void animateWidths();
    void animateIndicator(int index);
    void animateHover(int index);
    QRectF selectedRect(int index) const;

    qreal m_animatedTabWidth = 180.0;
    QRectF m_indicatorRect;
    QRectF m_hoverRect;
    QVariantAnimation *m_widthAnimation = nullptr;
    QVariantAnimation *m_indicatorAnimation = nullptr;
    QVariantAnimation *m_hoverRectAnimation = nullptr;
    QVariantAnimation *m_hoverOpacityAnimation = nullptr;
    bool m_indicatorAnimating = false;
    qreal m_hoverOpacity = 0.0;
    int m_hoveredIndex = -1;
};
