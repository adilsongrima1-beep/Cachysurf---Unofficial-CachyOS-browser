#include "animatedaddressbar.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QKeySequence>

AnimatedAddressBar::AnimatedAddressBar(QWidget *parent)
    : QLineEdit(parent)
{
}

void AnimatedAddressBar::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    emit focusStateChanged(true);
}

void AnimatedAddressBar::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);
    emit focusStateChanged(false);
}

void AnimatedAddressBar::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::SelectAll)) {
        // Safari keeps text selection independent from the suggestion list.
        emit dismissSuggestionsRequested();
        QLineEdit::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit submitRequested();
        event->accept();
        return;
    case Qt::Key_Escape:
        emit dismissSuggestionsRequested();
        event->accept();
        return;
    case Qt::Key_Down:
        emit suggestionStepRequested(1);
        event->accept();
        return;
    case Qt::Key_Up:
        emit suggestionStepRequested(-1);
        event->accept();
        return;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        emit dismissSuggestionsRequested();
        break;
    default:
        break;
    }

    QLineEdit::keyPressEvent(event);
}
