#include "minesweeper.h"

#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_ANDROID
#include <QScroller>
#endif

#include <algorithm>
#include <cstdlib>

namespace {
struct Difficulty
{
    int rows;
    int columns;
    int mines;
};

// Classic presets.
constexpr Difficulty kDifficulties[] = {
    {9, 9, 10},
    {16, 16, 40},
    {16, 30, 99},
};

constexpr int kTileSize = 34;
constexpr int kTileSpacing = 2;
} // namespace

Minesweeper::Minesweeper(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    auto *controls = new QHBoxLayout;
    m_difficulty = new QComboBox(this);
    m_difficulty->addItem(tr("Beginner"), 0);
    m_difficulty->addItem(tr("Intermediate"), 1);
    m_difficulty->addItem(tr("Expert"), 2);
    connect(m_difficulty, &QComboBox::currentIndexChanged,
            this, &Minesweeper::setDifficulty);

    m_minesLeft = new QLabel(this);
    m_time = new QLabel(this);
    m_flagMode = new QPushButton(tr("Flag: off"), this);
    m_flagMode->setCheckable(true);
    auto *restart = new QPushButton(tr("New"), this);

    controls->addWidget(m_difficulty);
    controls->addWidget(m_minesLeft);
    controls->addWidget(m_time);
    controls->addStretch(1);
    controls->addWidget(m_flagMode);
    controls->addWidget(restart);
    layout->addLayout(controls);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(false);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_board = new QWidget(m_scroll);
    m_boardLayout = new QGridLayout(m_board);
    m_boardLayout->setSpacing(kTileSpacing);
    m_boardLayout->setContentsMargins(4, 4, 4, 4);
    m_scroll->setWidget(m_board);
    layout->addWidget(m_scroll, 1);

#ifdef Q_OS_ANDROID
    // Expert is wider than a phone screen: let a finger drag pan the board.
    QScroller::grabGesture(m_scroll->viewport(), QScroller::LeftMouseButtonGesture);
#endif

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &Minesweeper::tick);

    connect(m_flagMode, &QPushButton::toggled, this, &Minesweeper::toggleFlagMode);
    connect(restart, &QPushButton::clicked, this, &Minesweeper::newGame);

    newGame();
}

void Minesweeper::newGame()
{
    m_timer->stop();
    m_seconds = 0;
    m_flagCount = 0;
    m_firstClick = true;
    m_gameOver = false;
    m_flagMode->setChecked(false);

    const Difficulty difficulty = kDifficulties[m_difficulty->currentIndex()];
    m_rows = difficulty.rows;
    m_columns = difficulty.columns;
    m_mineCount = difficulty.mines;

    buildBoard();
    m_status->setText(tr("Click a tile to start. Right-click, or turn on Flag "
                         "mode on a touch screen, to mark a mine."));
    updateCounters();
}

void Minesweeper::buildBoard()
{
    while (m_boardLayout->count() > 0) {
        QLayoutItem *item = m_boardLayout->takeAt(0);
        delete item->widget();
        delete item;
    }

    m_buttons.clear();
    m_buttons.resize(m_rows * m_columns);
    m_cells.clear();
    m_cells.resize(m_rows * m_columns);

    for (int row = 0; row < m_rows; ++row) {
        for (int column = 0; column < m_columns; ++column) {
            const int index = row * m_columns + column;
            auto *button = new QPushButton(m_board);
            button->setFixedSize(kTileSize, kTileSize);
            button->setProperty("index", index);
            button->installEventFilter(this);
#ifdef Q_OS_ANDROID
            // Same reasoning as the main grid: touch devices have no keyboard,
            // so keep the tiles out of the focus chain.
            button->setFocusPolicy(Qt::NoFocus);
#endif
            connect(button, &QPushButton::clicked,
                    this, [this, index] { onButtonClicked(index); });
            m_buttons[index] = button;
            m_boardLayout->addWidget(button, row, column);
        }
    }

    m_board->adjustSize();
    for (int i = 0; i < m_cells.size(); ++i)
        updateButton(i);
}

void Minesweeper::placeMines(int safeIndex)
{
    // Keep the first click (and its neighbours) mine-free, so the game always
    // starts with a safe opening.
    const int safeRow = safeIndex / m_columns;
    const int safeColumn = safeIndex % m_columns;
    QVector<int> candidates;
    candidates.reserve(m_cells.size());
    for (int i = 0; i < m_cells.size(); ++i) {
        const int row = i / m_columns;
        const int column = i % m_columns;
        if (std::abs(row - safeRow) <= 1 && std::abs(column - safeColumn) <= 1)
            continue;
        candidates.append(i);
    }

    auto *random = QRandomGenerator::global();
    for (int i = candidates.size() - 1; i > 0; --i)
        std::swap(candidates[i], candidates[random->bounded(i + 1)]);

    for (int i = 0; i < m_mineCount && i < candidates.size(); ++i)
        m_cells[candidates.at(i)].mine = true;

    for (int i = 0; i < m_cells.size(); ++i) {
        if (m_cells.at(i).mine)
            continue;

        const int row = i / m_columns;
        const int column = i % m_columns;
        int adjacent = 0;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0)
                    continue;
                const int r = row + dr;
                const int c = column + dc;
                if (r < 0 || r >= m_rows || c < 0 || c >= m_columns)
                    continue;
                if (m_cells.at(r * m_columns + c).mine)
                    ++adjacent;
            }
        }
        m_cells[i].adjacent = adjacent;
    }
}

void Minesweeper::onButtonClicked(int index)
{
    if (m_gameOver)
        return;

    if (m_flagMode->isChecked()) {
        toggleFlag(index);
        return;
    }

    if (m_firstClick) {
        placeMines(index);
        m_firstClick = false;
        m_timer->start();
    }

    reveal(index);
}

void Minesweeper::reveal(int index)
{
    if (index < 0 || index >= m_cells.size())
        return;

    if (m_cells.at(index).mine) {
        finish(false);
        return;
    }

    QVector<int> pending{index};
    while (!pending.isEmpty()) {
        const int current = pending.takeLast();
        Cell &cell = m_cells[current];
        if (cell.revealed || cell.flagged || cell.mine)
            continue;

        cell.revealed = true;
        updateButton(current);

        if (cell.adjacent != 0)
            continue;

        const int row = current / m_columns;
        const int column = current % m_columns;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0)
                    continue;
                const int r = row + dr;
                const int c = column + dc;
                if (r < 0 || r >= m_rows || c < 0 || c >= m_columns)
                    continue;
                const int neighbour = r * m_columns + c;
                const Cell &next = m_cells.at(neighbour);
                if (!next.revealed && !next.flagged && !next.mine)
                    pending.append(neighbour);
            }
        }
    }

    checkWin();
}

void Minesweeper::toggleFlag(int index)
{
    if (index < 0 || index >= m_cells.size() || m_gameOver)
        return;

    Cell &cell = m_cells[index];
    if (cell.revealed)
        return;

    cell.flagged = !cell.flagged;
    m_flagCount += cell.flagged ? 1 : -1;
    updateButton(index);
    updateCounters();
}

void Minesweeper::checkWin()
{
    int revealed = 0;
    for (const Cell &cell : m_cells) {
        if (cell.revealed)
            ++revealed;
    }

    if (revealed == m_cells.size() - m_mineCount)
        finish(true);
}

void Minesweeper::finish(bool won)
{
    m_gameOver = true;
    m_timer->stop();

    if (won) {
        // Mark the remaining mines as flagged, as a finished game should.
        for (int i = 0; i < m_cells.size(); ++i) {
            if (m_cells.at(i).mine && !m_cells.at(i).flagged) {
                m_cells[i].flagged = true;
                ++m_flagCount;
                updateButton(i);
            }
        }
        m_status->setText(tr("You cleared the field in %1 s!").arg(m_seconds));
    } else {
        for (int i = 0; i < m_cells.size(); ++i) {
            Cell &cell = m_cells[i];
            if (cell.mine && !cell.flagged)
                cell.revealed = true;
            else if (!cell.mine && cell.flagged)
                cell.wrong = true;
            updateButton(i);
        }
        m_status->setText(tr("Boom! Game over. Press New to try again."));
    }

    updateCounters();
}

void Minesweeper::updateButton(int index)
{
    QPushButton *button = m_buttons.value(index);
    if (!button)
        return;

    const Cell &cell = m_cells.at(index);
    if (cell.wrong) {
        button->setText(QStringLiteral("\u00d7"));
        button->setEnabled(false);
        button->setStyleSheet(QStringLiteral("color: #d32f2f; font-weight: bold;"));
        return;
    }

    if (!cell.revealed) {
        button->setText(cell.flagged ? QStringLiteral("F") : QString());
        button->setEnabled(!m_gameOver || cell.flagged);
        button->setStyleSheet(cell.flagged
                                  ? QStringLiteral("color: #f57c00; font-weight: bold;")
                                  : QString());
        return;
    }

    button->setEnabled(false);
    if (cell.mine) {
        button->setText(QStringLiteral("*"));
        button->setStyleSheet(QStringLiteral("color: #d32f2f; font-weight: bold;"));
    } else if (cell.adjacent > 0) {
        button->setText(QString::number(cell.adjacent));
        colorNumber(button, cell.adjacent);
    } else {
        button->setText(QString());
        button->setStyleSheet(QString());
    }
}

void Minesweeper::colorNumber(QPushButton *button, int number)
{
    static const char *colors[] = {
        "",
        "#1976d2", // 1 blue
        "#388e3c", // 2 green
        "#d32f2f", // 3 red
        "#303f9f", // 4 dark blue
        "#8d6e63", // 5 brown
        "#00838f", // 6 teal
        "#424242", // 7 dark grey
        "#616161", // 8 grey
    };

    const int index = qBound(1, number, 8);
    button->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;")
                              .arg(QLatin1String(colors[index])));
}

void Minesweeper::updateCounters()
{
    m_minesLeft->setText(tr("Mines: %1").arg(m_mineCount - m_flagCount));
    m_time->setText(tr("%1 s").arg(m_seconds));
}

void Minesweeper::tick()
{
    ++m_seconds;
    updateCounters();
}

void Minesweeper::setDifficulty(int index)
{
    Q_UNUSED(index);
    newGame();
}

void Minesweeper::toggleFlagMode(bool enabled)
{
    m_flagMode->setText(enabled ? tr("Flag: on") : tr("Flag: off"));
}

bool Minesweeper::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::RightButton) {
            if (auto *button = qobject_cast<QPushButton *>(watched)) {
                const QVariant index = button->property("index");
                if (index.isValid()) {
                    toggleFlag(index.toInt());
                    return true;
                }
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}
