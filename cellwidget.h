#ifndef CELLWIDGET_H
#define CELLWIDGET_H

#include <QFrame>

class QLabel;
class QVBoxLayout;

/**
 * One tile of the main grid.
 *
 * Right now it only shows its number and reports clicks to the main window.
 * When a tile becomes a real mini application, hand the app's widget to
 * setContentWidget(); the tile then hosts it instead of the placeholder text.
 */
class CellWidget : public QFrame
{
    Q_OBJECT

public:
    explicit CellWidget(int number, QWidget *parent = nullptr);

    int number() const { return m_number; }

    /// Replaces the placeholder content with the widget of a mini app.
    /// The tile takes ownership of @p widget.
    void setContentWidget(QWidget *widget);

signals:
    void clicked(int number);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    int m_number;
    QLabel *m_numberLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_layout = nullptr;
};

#endif // CELLWIDGET_H
