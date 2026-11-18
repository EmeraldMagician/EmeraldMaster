#ifndef MINESWEEPER_H
#define MINESWEEPER_H

#include <QHash>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

class QComboBox;
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
    void cellRightClicked(int index);
    void panRequested(const QPoint &delta);
    void zoomRequested(int tileSize, const QPoint &anchor);
    void zoomGestureFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    void handleTouch(QTouchEvent *event);
    void resetTouch();
    int cellAtPoint(const QPointF &position) const;

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
    void setDifficulty(int index);
    void toggleFlagMode(bool enabled);
    void onButtonClicked(int index);
    void tick();

private:
    using Cell = MinesweeperCell;

    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void buildBoard();
    void placeMines(int safeIndex);
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

    QComboBox *m_difficulty = nullptr;
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
    int m_previousDifficulty = 0;
    bool m_ignoreNextActivation = false;

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
