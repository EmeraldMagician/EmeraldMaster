#ifndef CELLWIDGET_H
#define CELLWIDGET_H

#include <QFrame>
#include <QPoint>

class QIcon;
class QLabel;
class QVBoxLayout;

/**
 * One tile of the main grid.
 *
 * It shows an app name under the icon (or under the number for placeholders)
 * and reports clicks to the main window. When a tile becomes a real mini
 * application, hand the app's widget to
 * setContentWidget(); the tile then hosts it instead of the placeholder text.
 */
class CellWidget : public QFrame
{
    Q_OBJECT

public:
    explicit CellWidget(int number, QWidget *parent = nullptr);

    int number() const { return m_number; }

    /// The app name shown under the icon, also used for search and titles.
    QString name() const { return m_name; }

    /// Shows an icon instead of the big tile number (used by real mini apps).
    void setIcon(const QIcon &icon);

    /// Sets the app name shown under the icon and in the tooltip.
    void setTitle(const QString &title);

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
    QString m_name;
    QPoint m_pressPosition;
    QLabel *m_numberLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_layout = nullptr;
};

#endif // CELLWIDGET_H
