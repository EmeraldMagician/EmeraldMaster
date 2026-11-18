#include "minesweeper.h"

#include "appicons.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QEventLoop>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QStyledItemDelegate>
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

// Classic presets. The last entry is the deliberately huge Insane board; a
// custom game is not part of this table and is described by the user.
constexpr Difficulty kDifficulties[] = {
    {9, 9, 10},
    {16, 16, 40},
    {16, 30, 99},
    {100, 100, 2000},
};

constexpr int kCustomDifficulty = 4;
constexpr int kMaxCustomSize = 100;

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

constexpr int kBestTimeRole = Qt::UserRole + 1;

QString bestTimeKey(int difficulty)
{
    return QStringLiteral("bestTimes/%1").arg(difficulty);
}

// Gap between the difficulty name and the best time in the open drop-down.
// Both columns are drawn by BestTimeDelegate, so no padding is baked into the
// strings and every best time starts at the same x position.
constexpr int kBestTimeGap = 16;

// The best-time column text shown after a difficulty while the drop-down is
// open. Kept separate from the difficulty name so the two can be aligned.
QString bestTimeSuffix(const QVariant &best)
{
    const QString time = best.toString();
    return time.isEmpty() ? QStringLiteral("no best time yet")
                          : QStringLiteral("best ") + time;
}

// Always hours:minutes:seconds. The fixed-width fields keep the best times in
// the drop-down aligned in one column, with leading zeroes where needed.
QString formatTime(int seconds)
{
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600)
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

// The combo itself stays as narrow as the difficulty name; the best time is
// only added to the entries while the drop-down is open.
class BestTimeDelegate : public QStyledItemDelegate
{
public:
    explicit BestTimeDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem styled(option);
        initStyleOption(&styled, index);

        // Let the style draw the background and the difficulty name, then add
        // the best time in a second, fixed column. Drawing it separately is
        // what keeps all the values below each other.
        QStyle *style = styled.widget ? styled.widget->style()
                                      : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &styled, painter,
                           styled.widget);

        const QRect textRect = style->subElementRect(
            QStyle::SE_ItemViewItemText, &styled, styled.widget);
        if (textRect.isEmpty())
            return;

        const int column = nameColumnWidth(index, styled.fontMetrics)
                           + kBestTimeGap;
        const QRect suffixRect = textRect.adjusted(column, 0, 0, 0);
        if (suffixRect.width() <= 0)
            return;

        painter->save();
        painter->setFont(styled.font);
        painter->setPen(styled.palette.color(
            styled.state & QStyle::State_Selected ? QPalette::HighlightedText
                                                   : QPalette::Text));
        painter->drawText(suffixRect, Qt::AlignLeft | Qt::AlignVCenter,
                          suffix(index));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.rwidth() += kBestTimeGap
                         + option.fontMetrics.horizontalAdvance(suffix(index));
        return size;
    }

private:
    // Width of the longest difficulty name, so the best times can all start
    // after the same column.
    static int nameColumnWidth(const QModelIndex &index,
                               const QFontMetrics &metrics)
    {
        int width = 0;
        if (const QAbstractItemModel *model = index.model()) {
            for (int row = 0; row < model->rowCount(); ++row) {
                width = qMax(width, metrics.horizontalAdvance(
                    model->index(row, 0).data(Qt::DisplayRole).toString()));
            }
        }
        return width;
    }

    static QString suffix(const QModelIndex &index)
    {
        return bestTimeSuffix(index.data(kBestTimeRole));
    }
};

// QComboBox makes its popup exactly as wide as the closed combo, which would
// elide the "best ..." part added by the delegate. Widen the popup after Qt
// has shown it, keeping it inside the screen.
class DifficultyComboBox : public QComboBox
{
public:
    using QComboBox::QComboBox;

    void showPopup() override
    {
        QComboBox::showPopup();

        QWidget *popup = view()->window();
        if (!popup || !popup->isVisible())
            return;

        int nameWidth = 0;
        int suffixWidth = 0;
        const QFontMetrics metrics = view()->fontMetrics();
        for (int i = 0; i < count(); ++i) {
            nameWidth = qMax(nameWidth, metrics.horizontalAdvance(itemText(i)));
            suffixWidth = qMax(
                suffixWidth,
                metrics.horizontalAdvance(
                    bestTimeSuffix(itemData(i, kBestTimeRole))));
        }
        const int textWidth = nameWidth + kBestTimeGap + suffixWidth;
        const int padding = view()->verticalScrollBar()->sizeHint().width()
                            + 2 * view()->frameWidth() + 24;
        if (textWidth + padding <= popup->width())
            return;

        QRect geometry = popup->geometry();
        geometry.setWidth(textWidth + padding);
        if (const QScreen *screen = this->screen()) {
            const QRect available = screen->availableGeometry();
            geometry.setWidth(qMin(geometry.width(), available.width()));
            if (geometry.right() > available.right())
                geometry.moveRight(available.right());
            if (geometry.left() < available.left())
                geometry.moveLeft(available.left());
        }
        popup->setGeometry(geometry);
    }
};
} // namespace

Minesweeper::Minesweeper(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    auto *controls = new QHBoxLayout;
    m_difficulty = new DifficultyComboBox(this);
    m_difficulty->addItem(tr("Beginner"), 0);
    m_difficulty->addItem(tr("Intermediate"), 1);
    m_difficulty->addItem(tr("Expert"), 2);
    m_difficulty->addItem(tr("Insane"), 3);
    m_difficulty->addItem(tr("Custom"), kCustomDifficulty);
    m_difficulty->view()->setItemDelegate(
        new BestTimeDelegate(m_difficulty->view()));
    connect(m_difficulty, &QComboBox::currentIndexChanged,
            this, [this](int index) {
                // A user choice emits both signals; the activated handler
                // below only exists to reopen the prompt when the current
                // Custom entry is picked again.
                m_ignoreNextActivation = true;
                setDifficulty(index);
            });
    connect(m_difficulty, &QComboBox::activated, this, [this](int index) {
        if (m_ignoreNextActivation) {
            m_ignoreNextActivation = false;
            return;
        }
        if (index == kCustomDifficulty)
            setDifficulty(index);
    });

    m_flagMode = new QPushButton(tr("Flag: off"), this);
    m_flagMode->setCheckable(true);
    auto *restart = new QPushButton(tr("New"), this);

    controls->addWidget(m_difficulty);
    controls->addStretch(1);
    controls->addWidget(m_flagMode);
    controls->addWidget(restart);
    layout->addLayout(controls);

    // Counters get their own row so the clock and the record stay readable on
    // a phone, where the difficulty and buttons use the full width.
    auto *counters = new QHBoxLayout;
    counters->setSpacing(18);
    m_minesLeft = new QLabel(this);
    m_time = new QLabel(this);
    m_bestTime = new QLabel(this);
    counters->addWidget(m_minesLeft);
    counters->addWidget(m_time);
    counters->addWidget(m_bestTime);
    counters->addStretch(1);
    layout->addLayout(counters);

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
    m_board->setBackgroundRole(QPalette::Base);
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

    QSettings settings;
    m_customRows = qBound(3, settings.value(QStringLiteral("custom/rows"), 20).toInt(),
                          kMaxCustomSize);
    m_customColumns = qBound(
        3, settings.value(QStringLiteral("custom/columns"), 20).toInt(),
        kMaxCustomSize);
    const int maxCustomMines = qMax(0, m_customRows * m_customColumns - 9);
    m_customMines = qBound(0,
                           settings.value(QStringLiteral("custom/mines"), 80).toInt(),
                           maxCustomMines);

    for (int i = 0; i < m_difficulty->count(); ++i) {
        // Custom boards vary, so they never get a best-time record.
        if (i == kCustomDifficulty)
            continue;
        const QString key = bestTimeKey(i);
        if (!settings.contains(key))
            continue;
        const int best = settings.value(key).toInt();
        m_bestTimes.insert(i, best);
        m_difficulty->setItemData(i, formatTime(best), kBestTimeRole);
    }

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

    const int index = m_difficulty->currentIndex();
    if (index == kCustomDifficulty) {
        m_rows = m_customRows;
        m_columns = m_customColumns;
        m_mineCount = m_customMines;
    } else {
        const Difficulty &difficulty =
            kDifficulties[qBound(0, index, kCustomDifficulty - 1)];
        m_rows = difficulty.rows;
        m_columns = difficulty.columns;
        m_mineCount = difficulty.mines;
    }

    buildBoard();
    m_status->setText(tr("Click a tile to start. Right-click, or turn on Flag "
                         "mode on a touch screen, to mark a mine. Click or "
                         "right-click a number with all its mines flagged to "
                         "clear the rest around it."));
    updateCounters();
}

void Minesweeper::buildBoard()
{
    // Rebuilding a visible board makes QGridLayout relayout on every added
    // tile. On the 100x100 Insane board that takes seconds, so keep the board
    // hidden until the new grid is complete.
    m_board->hide();

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
            button->setIconSize(QSize(22, 22));
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
    m_board->show();
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
    if (m_gameOver || index < 0 || index >= m_cells.size())
        return;

    // A click on an opened number clears the tiles around it (a chord) when the
    // flags there match the digit, as in the classic game. Check this before
    // Flag mode: the flag toggle only applies to unopened tiles, so tapping a
    // number should chord without having to turn Flag mode off first.
    const Cell &cell = m_cells.at(index);
    if (cell.revealed && !cell.mine && cell.adjacent > 0) {
        revealNeighbours(index);
        return;
    }

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

void Minesweeper::revealNeighbours(int index)
{
    if (m_gameOver || index < 0 || index >= m_cells.size())
        return;

    const Cell &cell = m_cells.at(index);
    if (!cell.revealed || cell.mine || cell.adjacent == 0)
        return;

    // Classic chord: if the number of surrounding flags matches the digit,
    // open every remaining neighbour. A wrongly placed flag therefore costs
    // the game, exactly as in the original.
    const int row = index / m_columns;
    const int column = index % m_columns;
    int flags = 0;
    QVector<int> targets;
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
            if (next.flagged) {
                ++flags;
            } else if (!next.revealed) {
                targets.append(neighbour);
            }
        }
    }

    if (flags != cell.adjacent)
        return;

    for (int neighbour : targets) {
        if (m_gameOver)
            break;
        reveal(neighbour);
    }
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

        const int difficulty = m_difficulty->currentIndex();
        if (difficulty == kCustomDifficulty) {
            m_status->setText(
                tr("You cleared the field in %1.").arg(formatTime(m_seconds)));
        } else {
            const bool newBest =
                !m_bestTimes.contains(difficulty)
                || m_seconds < m_bestTimes.value(difficulty);
            if (newBest) {
                m_bestTimes[difficulty] = m_seconds;
                QSettings settings;
                settings.setValue(bestTimeKey(difficulty), m_seconds);
                m_difficulty->setItemData(difficulty, formatTime(m_seconds),
                                          kBestTimeRole);
                m_status->setText(
                    tr("You cleared the field in %1 - a new best time!")
                        .arg(formatTime(m_seconds)));
            } else {
                m_status->setText(tr("You cleared the field in %1. Best: %2.")
                                      .arg(formatTime(m_seconds),
                                           formatTime(m_bestTimes.value(difficulty))));
            }
        }
        // Refresh the counters so the new record shows up next to the clock.
        updateCounters();
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

    const bool dark = isDarkTheme();
    const Cell &cell = m_cells.at(index);
    if (cell.wrong) {
        button->setText(QStringLiteral("\u00d7"));
        button->setIcon(QIcon());
        button->setEnabled(false);
        button->setStyleSheet(revealedTileStyle(warningColor().name(),
                                                QStringLiteral("rgba(245, 124, 0, 60)"),
                                                warningColor().name()));
        return;
    }

    if (!cell.revealed) {
        button->setText(QString());
        button->setIcon(cell.flagged ? AppIcons::flag(dark) : QIcon());
        button->setEnabled(!m_gameOver || cell.flagged);
        button->setStyleSheet(hiddenTileStyle(cell.flagged));
        return;
    }

    if (cell.mine) {
        button->setEnabled(false);
        button->setText(QString());
        button->setIcon(AppIcons::mine(dark));
        button->setStyleSheet(revealedTileStyle(numberColor(3).name(),
                                                QStringLiteral("rgba(211, 47, 47, 70)"),
                                                numberColor(3).name()));
    } else if (cell.adjacent > 0) {
        // Opened numbers stay enabled: Qt does not deliver clicks to disabled
        // widgets, and a click on the digit has to reach us to chord.
        button->setEnabled(true);
        button->setText(QString::number(cell.adjacent));
        button->setIcon(QIcon());
        button->setStyleSheet(revealedTileStyle(numberColor(cell.adjacent).name()));
    } else {
        button->setEnabled(true);
        button->setText(QString());
        button->setIcon(QIcon());
        button->setStyleSheet(revealedTileStyle(tilePalette().text.name()));
    }
}

void Minesweeper::updateCounters()
{
    m_minesLeft->setText(tr("Mines: %1").arg(m_mineCount - m_flagCount));
    m_time->setText(tr("Time: %1").arg(formatTime(m_seconds)));

    if (m_difficulty->currentIndex() == kCustomDifficulty) {
        // Custom boards differ in size, so a time is not comparable.
        m_bestTime->setText(tr("Best: -"));
        return;
    }

    const auto best = m_bestTimes.constFind(m_difficulty->currentIndex());
    m_bestTime->setText(best == m_bestTimes.cend()
                            ? tr("Best: -")
                            : tr("Best: %1").arg(formatTime(*best)));
}

void Minesweeper::tick()
{
    ++m_seconds;
    updateCounters();
}

void Minesweeper::setDifficulty(int index)
{
    // A custom game needs its parameters from the user first. Cancelling the
    // dialog puts the previous difficulty back without starting a new game.
    if (index == kCustomDifficulty && !askCustomGame()) {
        const QSignalBlocker blocker(m_difficulty);
        m_difficulty->setCurrentIndex(m_previousDifficulty);
        return;
    }

    m_previousDifficulty = index;
    newGame();
}

bool Minesweeper::askCustomGame()
{
    // Keep the prompt inside the game window. A separate top-level dialog
    // creates another Android surface, and closing it leaves Qt's
    // accessibility surface count at zero, so the app's accessible tree
    // disappears until it is restarted. A child widget avoids that entirely.
    QWidget backdrop(this);
    backdrop.setObjectName(QStringLiteral("customBackdrop"));
    backdrop.setStyleSheet(
        QStringLiteral("#customBackdrop { background-color: rgba(0, 0, 0, 120); }"));
    backdrop.setGeometry(rect());
    backdrop.show();

    QDialog dialog(&backdrop);
    // QDialog always turns itself into a window; clear that so it stays a
    // plain child widget inside the existing Android surface.
    dialog.setWindowFlags(Qt::Widget);
    dialog.setAutoFillBackground(true);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background-color: palette(window); border: 1px solid palette(mid); }"));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 12);
    layout->setSpacing(10);

    auto *title = new QLabel(tr("Custom game"), &dialog);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *form = new QFormLayout;
    auto *rows = new QSpinBox(&dialog);
    rows->setRange(3, kMaxCustomSize);
    rows->setValue(m_customRows);

    auto *columns = new QSpinBox(&dialog);
    columns->setRange(3, kMaxCustomSize);
    columns->setValue(m_customColumns);

    auto *mines = new QSpinBox(&dialog);
    const auto updateMineLimit = [rows, columns, mines] {
        // The first click always opens the eight tiles around it, so at least
        // nine tiles stay free of mines.
        const int maxMines = qMax(0, rows->value() * columns->value() - 9);
        mines->setMaximum(maxMines);
        mines->setValue(qMin(mines->value(), maxMines));
    };
    updateMineLimit();
    mines->setValue(qMin(m_customMines, mines->maximum()));
    connect(rows, &QSpinBox::valueChanged, &dialog,
            [updateMineLimit] { updateMineLimit(); });
    connect(columns, &QSpinBox::valueChanged, &dialog,
            [updateMineLimit] { updateMineLimit(); });

    form->addRow(tr("Rows:"), rows);
    form->addRow(tr("Columns:"), columns);
    form->addRow(tr("Mines:"), mines);
    layout->addLayout(form);

    auto *hint = new QLabel(
        tr("Rows and columns can be up to %1. Mines are limited so the first "
           "click always has a free area.")
            .arg(kMaxCustomSize),
        &dialog);
    hint->setWordWrap(true);
    hint->setMinimumWidth(260);
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (QPushButton *ok = buttons->button(QDialogButtonBox::Ok))
        ok->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.adjustSize();
    dialog.move((backdrop.width() - dialog.width()) / 2,
                (backdrop.height() - dialog.height()) / 2);

    bool accepted = false;
    QEventLoop loop;
    connect(&dialog, &QDialog::accepted, &loop, [&accepted, &loop] {
        accepted = true;
        loop.quit();
    });
    connect(&dialog, &QDialog::rejected, &loop, &QEventLoop::quit);
    dialog.show();
    dialog.raise();
#ifndef Q_OS_ANDROID
    // Focusing a spin box on Android would pop up the on-screen keyboard.
    rows->setFocus();
#endif
    loop.exec();

    if (!accepted)
        return false;

    m_customRows = rows->value();
    m_customColumns = columns->value();
    m_customMines = mines->value();

    QSettings settings;
    settings.setValue(QStringLiteral("custom/rows"), m_customRows);
    settings.setValue(QStringLiteral("custom/columns"), m_customColumns);
    settings.setValue(QStringLiteral("custom/mines"), m_customMines);
    return true;
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
                    const int cellIndex = index.toInt();
                    if (cellIndex < 0 || cellIndex >= m_cells.size())
                        return false;
                    const Cell &cell = m_cells.at(cellIndex);
                    // On an opened number the right button chords, just like a
                    // regular click. On an unopened tile it still flags.
                    if (cell.revealed && !cell.mine && cell.adjacent > 0)
                        revealNeighbours(cellIndex);
                    else
                        toggleFlag(cellIndex);
                    return true;
                }
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}
