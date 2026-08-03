#pragma once

#include <QKeyEvent>
#include <QLineEdit>

class AnimatedAddressBar final : public QLineEdit
{
    Q_OBJECT

public:
    explicit AnimatedAddressBar(QWidget *parent = nullptr);

signals:
    void focusStateChanged(bool focused);
    void submitRequested();
    void dismissSuggestionsRequested();
    void suggestionStepRequested(int delta);

protected:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};
