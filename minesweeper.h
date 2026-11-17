#ifndef MINESWEEPER_H
#define MINESWEEPER_H

#include <QHash>
#include <QVector>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;

/** A self-contained Minesweeper game, ready to be hosted by the main window. */
class Minesweeper : public QWidget
{
    Q_OBJECT

public:
    explicit Minesweeper(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void newGame();
    void setDifficulty(int index);
    void toggleFlagMode(bool enabled);
    void onButtonClicked(int index);
    void tick();

private:
    struct Cell
    {
        bool mine = false;
        bool revealed = false;
        bool flagged = false;
        bool wrong = false;
        int adjacent = 0;
    };

    void buildBoard();
    void placeMines(int safeIndex);
    void reveal(int index);
    void revealNeighbours(int index);
    void toggleFlag(int index);
    void checkWin();
    void finish(bool won);
    void updateButton(int index);
    void updateCounters();
    bool askCustomGame();

    QComboBox *m_difficulty = nullptr;
    QLabel *m_minesLeft = nullptr;
    QLabel *m_time = nullptr;
    QLabel *m_bestTime = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_flagMode = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_board = nullptr;
    QGridLayout *m_boardLayout = nullptr;
    QTimer *m_timer = nullptr;

    QVector<Cell> m_cells;
    QVector<QPushButton *> m_buttons;

    int m_rows = 9;
    int m_columns = 9;
    int m_mineCount = 10;
    int m_flagCount = 0;
    int m_seconds = 0;
    bool m_firstClick = true;
    bool m_gameOver = false;

    // Parameters of the last custom game, kept between sessions.
    int m_customRows = 20;
    int m_customColumns = 20;
    int m_customMines = 80;
    int m_previousDifficulty = 0;
    bool m_ignoreNextActivation = false;

    // Best time per difficulty, in seconds; a missing key means "no record".
    QHash<int, int> m_bestTimes;
};

#endif // MINESWEEPER_H
