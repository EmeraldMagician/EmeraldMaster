#include "minesweeper.h"

#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
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

bool isDarkTheme()
{
    return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
}

QColor numberColor(int number)
{
    static const QRgb lightColors[] = {
        0x00000000,
        0xff1976d2, // 1 blue
        0xff388e3c, // 2 green
        0xffd32f2f, // 3 red
        0xff303f9f, // 4 dark blue
        0xff8d6e63, // 5 brown
        0xff00838f, // 6 teal
        0xff424242, // 7 dark grey
        0xff616161, // 8 grey
    };
    static const QRgb darkColors[] = {
        0x00000000,
        0xff64b5f6, // 1 blue
        0xff81c784, // 2 green
        0xffe57373, // 3 red
        0xff7986cb, // 4 indigo
        0xffa1887f, // 5 brown
        0xff4dd0e1, // 6 cyan
        0xffbdbdbd, // 7 light grey
        0xff9e9e9e, // 8 grey
    };

    const int index = qBound(1, number, 8);
    return QColor::fromRgb(isDarkTheme() ? darkColors[index] : lightColors[index]);
}

QColor warningColor()
{
    return isDarkTheme() ? QColor(QStringLiteral("#ffb74d"))
                         : QColor(QStringLiteral("#e65100"));
}

// Explicit greys, not palette roles: on some styles (and on Android)
// QPalette::Button and QPalette::Base differ by only a few levels, which made
// opened and unopened tiles look identical.
struct TilePalette
{
    QColor hidden;
    QColor hiddenBorder;
    QColor hiddenHover;
    QColor hiddenPressed;
    QColor flagged;
    QColor open;
    QColor openBorder;
    QColor text;
};

TilePalette tilePalette()
{
    if (isDarkTheme()) {
        return {QColor(QStringLiteral("#4a4a4a")),
                QColor(QStringLiteral("#707070")),
                QColor(QStringLiteral("#585858")),
                QColor(QStringLiteral("#3c3c3c")),
                QColor(QStringLiteral("#6b5424")),
                QColor(QStringLiteral("#242424")),
                QColor(QStringLiteral("#3c3c3c")),
                QColor(QStringLiteral("#e0e0e0"))};
    }
    return {QColor(QStringLiteral("#c8c8c8")),
            QColor(QStringLiteral("#8e8e8e")),
            QColor(QStringLiteral("#d3d3d3")),
            QColor(QStringLiteral("#b0b0b0")),
            QColor(QStringLiteral("#ffd9a0")),
            QColor(QStringLiteral("#f7f7f7")),
            QColor(QStringLiteral("#bcbcbc")),
            QColor(QStringLiteral("#1f1f1f"))};
}

// An unopened tile: raised medium grey, clearly darker than an opened cell.
QString hiddenTileStyle(bool flagged)
{
    const TilePalette palette = tilePalette();
    const QString background = flagged ? palette.flagged.name() : palette.hidden.name();
    const QString border = flagged ? warningColor().name() : palette.hiddenBorder.name();
    const QString text = flagged ? warningColor().name() : palette.text.name();
    QString style = QStringLiteral(
        "QPushButton {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 3px;"
        "  color: %3;"
        "  font-weight: bold;"
        "  padding: 0px;"
        "}").arg(background, border, text);
#ifndef Q_OS_ANDROID
    style += QStringLiteral(
        "QPushButton:hover { background-color: %1; }"
        "QPushButton:pressed { background-color: %2; }")
                 .arg(palette.hiddenHover.name(), palette.hiddenPressed.name());
#endif
    return style;
}

// An opened tile: flat, light and blending into the board, with only a grid
// line. `background`/`border` override the defaults for mines and wrong flags.
QString revealedTileStyle(const QString &textColor,
                          const QString &background = QString(),
                          const QString &border = QString())
{
    const TilePalette palette = tilePalette();
    const QString bg = background.isEmpty() ? palette.open.name() : background;
    const QString line = border.isEmpty() ? palette.openBorder.name() : border;
    return QStringLiteral(
        "QPushButton {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 0px;"
        "  color: %3;"
        "  font-weight: bold;"
        "  padding: 0px;"
        "}"
        // Qt paints disabled buttons with the disabled palette, so spell the
        // opened-tile colours out for that state too.
        "QPushButton:disabled {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  color: %3;"
        "}").arg(bg, line, textColor);
}
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
    // Opened tiles blend into this background; unopened ones stay raised.
    m_board->setAutoFillBackground(true);
    m_board->setBackgroundRole(QPalette::Base);    m_boardLayout = new QGridLayout(m_board);
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

    // Match the board to the opened-tile colour so a revealed area reads as
    // one flat surface.
    QPalette boardPalette = m_board->palette();
    boardPalette.setColor(QPalette::Base, tilePalette().open);
    m_board->setPalette(boardPalette);

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
            // Children created after the board is already visible start out
            // hidden. Without this, the layout's size hint is just the margins
            // and the board collapses to a few pixels on New/difficulty change.
            button->show();
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
        button->setStyleSheet(revealedTileStyle(warningColor().name(),
                                                QStringLiteral("rgba(245, 124, 0, 60)"),
                                                warningColor().name()));
        return;
    }

    if (!cell.revealed) {
        button->setText(cell.flagged ? QStringLiteral("F") : QString());
        button->setEnabled(!m_gameOver || cell.flagged);
        button->setStyleSheet(hiddenTileStyle(cell.flagged));
        return;
    }

    button->setEnabled(false);
    if (cell.mine) {
        button->setText(QStringLiteral("*"));
        button->setStyleSheet(revealedTileStyle(numberColor(3).name(),
                                                QStringLiteral("rgba(211, 47, 47, 70)"),
                                                numberColor(3).name()));
    } else if (cell.adjacent > 0) {
        button->setText(QString::number(cell.adjacent));
        button->setStyleSheet(revealedTileStyle(numberColor(cell.adjacent).name()));
    } else {
        button->setText(QString());
        button->setStyleSheet(revealedTileStyle(tilePalette().text.name()));
    }
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
