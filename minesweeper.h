#ifndef MINESWEEPER_H
#define MINESWEEPER_H

#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QDialog;
class QHideEvent;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QShowEvent;
class QTimer;
class QTouchEvent;
class QWheelEvent;

struct MinesweeperCell
{
    bool mine = false;
    bool revealed = false;
    bool flagged = false;
    bool wrong = false;
    int adjacent = 0;
};

/**
 * The whole minefield as one widget.
 *
 * The old version used one QPushButton per tile, which was ten thousand
 * widgets on the Insane board and made panning crawl. This widget paints only
 * the cells that intersect the paint event, so the cost of a frame depends on
 * the visible area instead of the board size. It also allows any tile size,
 * including the few-pixel overview used by Fit and pinch zoom.
 */
class MinesweeperBoard : public QWidget
{
    Q_OBJECT

public:
    explicit MinesweeperBoard(QWidget *parent = nullptr);

    void setCells(const QVector<MinesweeperCell> *cells) { m_cells = cells; }
    void setUsesDoubleClick(bool uses);
    void setGrid(int rows, int columns, int tileSize);
    void setTileSize(int tileSize);
    int tileSize() const { return m_tileSize; }

    int cellAt(const QPoint &position) const;
    QRect cellRect(int index) const;

    static int spacingFor(int tileSize);
    static int marginFor(int tileSize);
    static QSize documentSize(int rows, int columns, int tileSize);

signals:
    void cellClicked(int index);
    void cellLongClicked(int index);
    void cellDoubleClicked(int index);
    void cellRightClicked(int index);
    void panRequested(const QPoint &delta);
    void zoomRequested(int tileSize, const QPoint &anchor);
    void zoomGestureFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    void handleTouch(QTouchEvent *event);
    void resetTouch();
    int cellAtPoint(const QPointF &position) const;
    void startLongPress(int index);
    void cancelLongPress();

    const QVector<MinesweeperCell> *m_cells = nullptr;
    int m_rows = 0;
    int m_columns = 0;
    int m_tileSize = 32;

    int m_hoverIndex = -1;
    int m_pressedIndex = -1;
    QPoint m_pressPosition;
    QPoint m_lastDragPosition;
    bool m_dragging = false;

    QHash<int, QPointF> m_touchPositions;
    QHash<int, QPointF> m_touchGlobals;
    QPointF m_touchStart;
    QPointF m_touchLastGlobal;
    bool m_touchMoved = false;
    bool m_touchPinching = false;
    qreal m_pinchStartDistance = 1.0;
    int m_pinchStartTileSize = 32;
    int m_touchCell = -1;

    QTimer *m_longPressTimer = nullptr;
    bool m_longPressTriggered = false;

    // The chosen inverse gesture: long press by default, or double click.
    // Double click is detected here so the second release can be suppressed
    // instead of firing a second flag toggle before the open action.
    bool m_usesDoubleClick = false;
    int m_lastClickIndex = -1;
    QElapsedTimer m_lastClickTime;
};

/** A self-contained Minesweeper game, ready to be hosted by the main window. */
class Minesweeper : public QWidget
{
    Q_OBJECT

public:
    explicit Minesweeper(QWidget *parent = nullptr);

    /**
     * Handles a Back/Esc request addressed to this app.
     *
     * Returns true when the request was consumed (for example by closing the
     * custom-game prompt); false means "nothing to close, leave the app".
     */
    bool handleBack();

private slots:
    void newGame();
    void showSettings();
    void toggleFlagMode(bool enabled);
    void onButtonClicked(int index);
    void onCellLongClicked(int index);
    void onCellDoubleClicked(int index);
    void tick();
    void updateNewButton();

private:
    using Cell = MinesweeperCell;

    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void buildBoard();
    void placeMines(int safeIndex);
    void handleCellAction(int index, bool flagAction, bool allowFlaggedOpen);
    void openCell(int index, bool clearFlag);
    void reveal(int index);
    void revealNeighbours(int index);
    void toggleFlag(int index);
    void checkWin();
    void finish(bool won);
    void refreshBoard();
    void updateCounters();
    bool askCustomGame();
    void setStatus(const QString &text);
    void onCellRightClicked(int index);

    void setTileSize(int size);
    void zoomToAnchor(int size, const QPoint &anchor, bool persist);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitBoard();
    void panBoard(const QPoint &delta);
    void zoomBoardAt(int size, const QPoint &anchor);
    void saveZoom();
    void updateZoomLabel();
    QPoint viewportCenterInBoard() const;

    QPushButton *m_newButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QLabel *m_minesLeft = nullptr;
    QLabel *m_time = nullptr;
    QLabel *m_bestTime = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_flagMode = nullptr;
    QPushButton *m_zoomOut = nullptr;
    QPushButton *m_zoomReset = nullptr;
    QPushButton *m_zoomIn = nullptr;
    QPushButton *m_fit = nullptr;
    QScrollArea *m_scroll = nullptr;
    MinesweeperBoard *m_boardView = nullptr;
    QTimer *m_timer = nullptr;

    QVector<Cell> m_cells;

    int m_rows = 9;
    int m_columns = 9;
    int m_mineCount = 10;
    int m_flagCount = 0;
    int m_seconds = 0;
    int m_tileSize = 32;
    bool m_firstClick = true;
    bool m_gameOver = false;

    // Parameters of the last custom game, kept between sessions.
    int m_customRows = 20;
    int m_customColumns = 20;
    int m_customMines = 80;

    // Difficulty chosen in Settings is applied only when New is pressed. The
    // active index describes the board currently on screen and drives best
    // times, so changing the choice never silently replaces a game in progress.
    int m_selectedDifficulty = 0;
    int m_activeDifficulty = 0;
    int m_previousDifficulty = 0;
    QStringList m_difficultyNames;
    bool m_useDoubleClick = false;

    // The clock pauses while the app is not on screen; reopening it must not
    // charge the player for the time spent in the launcher.
    bool m_resumeTimer = false;

    // True while Fit is the active zoom: the board re-fits when the window or
    // the phone's orientation changes.
    bool m_fitMode = false;
    bool m_fitPending = false;

    // The in-window custom-game prompt, if it is open right now.
    QPointer<QDialog> m_activeDialog;

    // Best time per difficulty, in seconds; a missing key means "no record".
    QHash<int, int> m_bestTimes;
};

#endif // MINESWEEPER_H
