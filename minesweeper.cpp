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
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTouchEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
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

// Zoom levels. The default is a middle level, so there is room to go both
// smaller (down to a true whole-board overview) and larger (for small boards
// on a high-density screen). Fit and pinch zoom can select sizes between the
// listed steps as well.
constexpr int kDefaultTileSize = 32;
constexpr int kMinTileSize = 2;
constexpr int kMaxTileSize = 64;
constexpr int kTileSizes[] = {2, 3, 4, 5, 6, 8, 10, 12,
                              16, 20, 24, 32, 40, 48, 64};
constexpr int kTileSizeCount = int(sizeof(kTileSizes) / sizeof(kTileSizes[0]));

bool isDarkTheme()
{
    return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
}

QColor numberColor(int number, bool dark)
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
    return QColor::fromRgb(dark ? darkColors[index] : lightColors[index]);
}

QColor warningColor(bool dark)
{
    return dark ? QColor(QStringLiteral("#ffb74d"))
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

TilePalette tilePalette(bool dark)
{
    if (dark) {
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

// The flag and mine markers are drawn straight into the board with the same
// shapes the old QIcon-based tiles used, so the look stays familiar at every
// zoom level without creating a pixmap for each cell.
void paintFlagGlyph(QPainter &painter, const QRect &rect, bool dark)
{
    const QColor cloth = dark ? QColor(QStringLiteral("#ff8a80"))
                              : QColor(QStringLiteral("#c62828"));
    const QColor pole = dark ? QColor(QStringLiteral("#efebe9"))
                             : QColor(QStringLiteral("#4e342e"));
    const qreal size = rect.width() * 0.72;
    if (size < 5.0) {
        const qreal side = qMax<qreal>(1.0, size * 0.55);
        painter.fillRect(QRectF(rect.center().x() - side / 2,
                                rect.center().y() - side / 2, side, side),
                         cloth);
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(rect.center());
    painter.scale(size / 24.0, size / 24.0);
    painter.translate(-12.0, -12.0);

    QPen polePen(pole, 1.8, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(polePen);
    painter.drawLine(QPointF(8.0, 3.6), QPointF(8.0, 20.4));
    painter.drawLine(QPointF(4.9, 20.4), QPointF(11.1, 20.4));

    QPainterPath pennant;
    pennant.moveTo(9.0, 4.1);
    pennant.lineTo(20.2, 8.2);
    pennant.lineTo(9.0, 12.3);
    pennant.closeSubpath();

    painter.setPen(Qt::NoPen);
    painter.setBrush(cloth);
    painter.drawPath(pennant);
    painter.restore();
}

void paintMineGlyph(QPainter &painter, const QRect &rect, bool dark)
{
    const QColor body = dark ? QColor(QStringLiteral("#eceff1"))
                             : QColor(QStringLiteral("#212121"));
    const QColor glint = dark ? QColor(QStringLiteral("#242424"))
                              : QColor(QStringLiteral("#f7f7f7"));
    const qreal size = rect.width() * 0.75;
    if (size < 5.0) {
        const qreal radius = qMax<qreal>(1.0, size * 0.28);
        painter.setPen(Qt::NoPen);
        painter.setBrush(body);
        painter.drawEllipse(QPointF(rect.center()), radius, radius);
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(rect.center());
    painter.scale(size / 24.0, size / 24.0);
    painter.translate(-12.0, -12.0);

    const QRectF box(3.0, 3.0, 18.0, 18.0);
    const QPointF center = box.center();
    const qreal bodyRadius = box.width() * 0.25;
    const qreal spikeInner = bodyRadius * 1.08;
    const qreal spikeOuter = bodyRadius * 1.82;
    constexpr qreal kPi = 3.14159265358979323846;

    QPen spikePen(body, box.width() * 0.09, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(spikePen);
    for (int i = 0; i < 8; ++i) {
        const qreal angle = i * kPi / 4.0;
        const QPointF direction(std::cos(angle), std::sin(angle));
        painter.drawLine(center + direction * spikeInner,
                         center + direction * spikeOuter);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(body);
    painter.drawEllipse(center, bodyRadius, bodyRadius);

    painter.setBrush(glint);
    painter.drawEllipse(center - QPointF(bodyRadius * 0.34, bodyRadius * 0.34),
                        bodyRadius * 0.26, bodyRadius * 0.26);
    painter.restore();
}

void paintWrongGlyph(QPainter &painter, const QRect &rect)
{
    const QPointF center = rect.center();
    const qreal radius = rect.width() * 0.22;
    if (radius < 2.0) {
        painter.fillRect(QRectF(center.x() - 1, center.y() - 1, 2, 2),
                         warningColor(isDarkTheme()));
        return;
    }

    QPen pen(warningColor(isDarkTheme()),
             qMax<qreal>(1.0, rect.width() * 0.09), Qt::SolidLine,
             Qt::RoundCap);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(pen);
    painter.drawLine(center + QPointF(-radius, -radius),
                     center + QPointF(radius, radius));
    painter.drawLine(center + QPointF(radius, -radius),
                     center + QPointF(-radius, radius));
    painter.restore();
}

// One tile, painted in the same visual language as the old QPushButton tiles:
// unopened cells are raised grey rounded squares with a one-pixel border, while
// opened cells are flat and blend into the board.
void paintTile(QPainter &painter, const QRect &rect,
               const MinesweeperCell &cell, bool hovered, bool pressed,
               const TilePalette &palette, bool dark)
{
    if (!cell.revealed && !cell.wrong) {
        QColor background = palette.hidden;
        QColor border = palette.hiddenBorder;
        if (cell.flagged) {
            background = palette.flagged;
            border = warningColor(dark);
        } else if (pressed) {
            background = palette.hiddenPressed;
        } else if (hovered) {
            background = palette.hiddenHover;
        }

        if (rect.width() >= 6) {
            const qreal radius =
                qMin<qreal>(3.0, qMax<qreal>(1.0, rect.width() / 5.0));
            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(border, 1.0));
            painter.setBrush(background);
            painter.drawRoundedRect(
                QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
            painter.restore();
        } else {
            painter.fillRect(rect, background);
        }

        if (cell.flagged)
            paintFlagGlyph(painter, rect, dark);
        return;
    }

    QColor background = palette.open;
    QColor border = palette.openBorder;
    if (cell.wrong) {
        background = QColor(245, 124, 0, 60);
        border = warningColor(dark);
    } else if (cell.mine) {
        background = QColor(211, 47, 47, 70);
        border = numberColor(3, dark);
    }

    painter.fillRect(rect, palette.open);
    if (background != palette.open)
        painter.fillRect(rect, background);

    if (rect.width() >= 4) {
        painter.setPen(QPen(border, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
    }

    if (cell.wrong) {
        paintWrongGlyph(painter, rect);
        return;
    }
    if (cell.mine) {
        paintMineGlyph(painter, rect, dark);
        return;
    }
    if (cell.adjacent > 0 && rect.width() >= 9) {
        painter.setPen(numberColor(cell.adjacent, dark));
        painter.drawText(rect, Qt::AlignCenter, QString::number(cell.adjacent));
    }
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

// Qt 5 and Qt 6 spell a few event accessors differently.
QPointF touchPointPosition(const QTouchEvent::TouchPoint &point)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return point.position();
#else
    return point.pos();
#endif
}

QPointF touchPointGlobalPosition(const QTouchEvent::TouchPoint &point)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return point.globalPosition();
#else
    return point.screenPos();
#endif
}

QList<QTouchEvent::TouchPoint> touchPoints(const QTouchEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->points();
#else
    return event->touchPoints();
#endif
}

bool touchPointIsActive(const QTouchEvent::TouchPoint &point)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QEventPoint::State state = point.state();
    return state == QEventPoint::Pressed || state == QEventPoint::Updated
           || state == QEventPoint::Stationary;
#else
    const Qt::TouchPointState state = point.state();
    return state == Qt::TouchPointPressed || state == Qt::TouchPointMoved
           || state == Qt::TouchPointStationary;
#endif
}

QPoint mousePosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

QPoint mouseGlobalPosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

QPoint wheelPosition(const QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

QPoint nativeGesturePosition(const QNativeGestureEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

qreal distanceBetween(const QPointF &a, const QPointF &b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}
} // namespace

MinesweeperBoard::MinesweeperBoard(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

int MinesweeperBoard::spacingFor(int tileSize)
{
    // Tiny overview tiles have no gap: at that scale a gap is as wide as a
    // cell and would hide most of the board.
    if (tileSize < 12)
        return 0;
    return qMax(1, tileSize / 16);
}

int MinesweeperBoard::marginFor(int tileSize)
{
    if (tileSize < 8)
        return 1;
    return qMax(2, tileSize / 8);
}

QSize MinesweeperBoard::documentSize(int rows, int columns, int tileSize)
{
    const int spacing = spacingFor(tileSize);
    const int margin = marginFor(tileSize);

    int width = columns > 0 ? columns * tileSize + (columns - 1) * spacing : 0;
    int height = rows > 0 ? rows * tileSize + (rows - 1) * spacing : 0;
    width += 2 * margin;
    height += 2 * margin;
    return QSize(qMax(1, width), qMax(1, height));
}

void MinesweeperBoard::setGrid(int rows, int columns, int tileSize)
{
    m_rows = qMax(0, rows);
    m_columns = qMax(0, columns);
    m_tileSize = qBound(kMinTileSize, tileSize, kMaxTileSize);
    m_hoverIndex = -1;
    m_pressedIndex = -1;
    m_dragging = false;
    resetTouch();
    setFixedSize(documentSize(m_rows, m_columns, m_tileSize));
    update();
}

void MinesweeperBoard::setTileSize(int tileSize)
{
    const int bounded = qBound(kMinTileSize, tileSize, kMaxTileSize);
    if (bounded == m_tileSize && size() == documentSize(m_rows, m_columns, bounded))
        return;

    m_tileSize = bounded;
    setFixedSize(documentSize(m_rows, m_columns, m_tileSize));
    update();
}

int MinesweeperBoard::cellAt(const QPoint &position) const
{
    return cellAtPoint(position);
}

int MinesweeperBoard::cellAtPoint(const QPointF &position) const
{
    if (!m_cells || m_rows <= 0 || m_columns <= 0)
        return -1;

    const int step = m_tileSize + spacingFor(m_tileSize);
    const int margin = marginFor(m_tileSize);
    const qreal x = position.x() - margin;
    const qreal y = position.y() - margin;
    if (x < 0 || y < 0)
        return -1;

    const int column = int(x) / step;
    const int row = int(y) / step;
    if (column >= m_columns || row >= m_rows)
        return -1;
    if (int(x) % step >= m_tileSize || int(y) % step >= m_tileSize)
        return -1;
    return row * m_columns + column;
}

QRect MinesweeperBoard::cellRect(int index) const
{
    if (index < 0 || index >= m_rows * m_columns)
        return QRect();

    const int step = m_tileSize + spacingFor(m_tileSize);
    const int margin = marginFor(m_tileSize);
    const int row = index / m_columns;
    const int column = index % m_columns;
    return QRect(margin + column * step, margin + row * step, m_tileSize,
                 m_tileSize);
}

void MinesweeperBoard::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    const bool dark = isDarkTheme();
    const TilePalette palette = tilePalette(dark);

    // Opened cells and the outer margin read as one flat surface.
    painter.fillRect(event->rect(), palette.open);

    if (!m_cells || m_rows <= 0 || m_columns <= 0)
        return;

    QFont tileFont = font();
    tileFont.setBold(true);
    tileFont.setPixelSize(qMax(7, qRound(m_tileSize * 0.52)));
    painter.setFont(tileFont);

    const int spacing = spacingFor(m_tileSize);
    const int margin = marginFor(m_tileSize);
    const int step = m_tileSize + spacing;
    const QRect clip = event->rect();

    // Only walk the cells that intersect the exposed region. On the Insane
    // board this is a few hundred cells even while panning.
    const int firstRow = qMax(0, (clip.top() - margin) / step);
    const int lastRow = qMin(m_rows - 1, (clip.bottom() - margin) / step);
    const int firstColumn = qMax(0, (clip.left() - margin) / step);
    const int lastColumn =
        qMin(m_columns - 1, (clip.right() - margin) / step);

    for (int row = firstRow; row <= lastRow; ++row) {
        const int y = margin + row * step;
        for (int column = firstColumn; column <= lastColumn; ++column) {
            const QRect rect(margin + column * step, y, m_tileSize, m_tileSize);
            if (!rect.intersects(clip))
                continue;

            const int index = row * m_columns + column;
            paintTile(painter, rect, m_cells->at(index),
                      index == m_hoverIndex, index == m_pressedIndex, palette,
                      dark);
        }
    }
}

void MinesweeperBoard::mousePressEvent(QMouseEvent *event)
{
    // Touches are handled directly so that pinch zoom and direct panning do
    // not fight the synthesized mouse events.
    if (event->source() == Qt::MouseEventSynthesizedByQt) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::RightButton) {
        const int index = cellAtPoint(mousePosition(event));
        if (index >= 0)
            emit cellRightClicked(index);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_pressedIndex = cellAtPoint(mousePosition(event));
        m_hoverIndex = -1;
        m_pressPosition = mousePosition(event);
        m_lastDragPosition = mouseGlobalPosition(event);
        m_dragging = false;
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void MinesweeperBoard::mouseMoveEvent(QMouseEvent *event)
{
    if (event->source() == Qt::MouseEventSynthesizedByQt) {
        event->ignore();
        return;
    }

    const QPoint position = mousePosition(event);

    if (!(event->buttons() & Qt::LeftButton)) {
        const int index = cellAtPoint(position);
        if (index != m_hoverIndex) {
            if (m_hoverIndex >= 0)
                update(cellRect(m_hoverIndex));
            m_hoverIndex = index;
            if (m_hoverIndex >= 0)
                update(cellRect(m_hoverIndex));
        }
        event->accept();
        return;
    }

    const QPoint global = mouseGlobalPosition(event);
    if (!m_dragging
        && (position - m_pressPosition).manhattanLength()
               >= QApplication::startDragDistance()) {
        m_dragging = true;
        const int oldPressed = m_pressedIndex;
        m_pressedIndex = -1;
        if (oldPressed >= 0)
            update(cellRect(oldPressed));
        // Keep m_lastDragPosition at the press point so the first pan applies
        // the full movement instead of losing the drag threshold as an offset.
    }

    if (m_dragging) {
        const QPoint delta = global - m_lastDragPosition;
        m_lastDragPosition = global;
        if (!delta.isNull())
            emit panRequested(delta);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void MinesweeperBoard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->source() == Qt::MouseEventSynthesizedByQt) {
        event->ignore();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const int pressed = m_pressedIndex;
    const bool dragged = m_dragging;
    m_pressedIndex = -1;
    m_dragging = false;
    m_hoverIndex = cellAtPoint(mousePosition(event));
    update();

    if (!dragged && pressed >= 0)
        emit cellClicked(pressed);
    event->accept();
}

void MinesweeperBoard::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::AltModifier) {
        // Alt + wheel pans horizontally. QScrollArea only reacts to vertical
        // wheel deltas itself, so feed the delta through the drag-to-pan path.
        const QPoint delta = event->angleDelta();
        const int horizontal = delta.x() != 0 ? delta.x() : delta.y();
        if (horizontal != 0) {
            emit panRequested(QPoint(horizontal, 0));
            event->accept();
            return;
        }
    }

    if (!(event->modifiers() & Qt::ControlModifier)) {
        // Let the scroll area handle ordinary wheel scrolling.
        event->ignore();
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    const qreal factor = delta > 0 ? 1.25 : 0.8;
    const int size = qBound(kMinTileSize, qRound(m_tileSize * factor),
                            kMaxTileSize);
    if (size != m_tileSize)
        emit zoomRequested(size, wheelPosition(event));
    event->accept();
}

void MinesweeperBoard::leaveEvent(QEvent *event)
{
    if (m_hoverIndex >= 0) {
        const int oldHover = m_hoverIndex;
        m_hoverIndex = -1;
        update(cellRect(oldHover));
    }
    QWidget::leaveEvent(event);
}

bool MinesweeperBoard::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        handleTouch(static_cast<QTouchEvent *>(event));
        return true;
    case QEvent::NativeGesture: {
        // A trackpad pinch arrives as a native gesture on the desktop. On a
        // touchscreen the same pinch is handled above through QTouchEvent.
        if (m_touchPinching)
            return true;
        auto *gesture = static_cast<QNativeGestureEvent *>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            const int size = qBound(
                kMinTileSize, qRound(m_tileSize * (1.0 + gesture->value())),
                kMaxTileSize);
            if (size != m_tileSize)
                emit zoomRequested(size, nativeGesturePosition(gesture));
            return true;
        }
        if (gesture->gestureType() == Qt::EndNativeGesture) {
            emit zoomGestureFinished();
            return true;
        }
        break;
    }
    default:
        break;
    }

    return QWidget::event(event);
}

void MinesweeperBoard::handleTouch(QTouchEvent *event)
{
    if (event->type() == QEvent::TouchCancel) {
        const bool wasPinching = m_touchPinching;
        resetTouch();
        if (m_pressedIndex >= 0) {
            const int oldPressed = m_pressedIndex;
            m_pressedIndex = -1;
            update(cellRect(oldPressed));
        }
        if (wasPinching)
            emit zoomGestureFinished();
        return;
    }

    const auto points = touchPoints(event);
    for (const auto &point : points) {
        const int id = point.id();
        if (touchPointIsActive(point)) {
            m_touchPositions.insert(id, touchPointPosition(point));
            m_touchGlobals.insert(id, touchPointGlobalPosition(point));
        } else {
            m_touchPositions.remove(id);
            m_touchGlobals.remove(id);
        }
    }

    if (event->type() == QEvent::TouchBegin) {
        m_touchMoved = false;
        m_touchPinching = false;
        m_touchCell = -1;
        if (!m_touchGlobals.isEmpty()) {
            m_touchStart = *m_touchGlobals.constBegin();
            m_touchLastGlobal = m_touchStart;
            m_touchCell = cellAtPoint(*m_touchPositions.constBegin());
        }

        if (m_touchGlobals.size() >= 2) {
            m_touchPinching = true;
            m_touchCell = -1;
            auto it = m_touchGlobals.constBegin();
            const QPointF first = *it;
            ++it;
            const QPointF second = *it;
            m_pinchStartDistance =
                qMax<qreal>(1.0, distanceBetween(first, second));
            m_pinchStartTileSize = m_tileSize;
            if (m_pressedIndex >= 0) {
                const int oldPressed = m_pressedIndex;
                m_pressedIndex = -1;
                update(cellRect(oldPressed));
            }
        } else if (m_touchCell >= 0) {
            m_pressedIndex = m_touchCell;
            m_hoverIndex = -1;
            update(cellRect(m_pressedIndex));
        }
        return;
    }

    if (event->type() == QEvent::TouchEnd) {
        if (m_touchGlobals.isEmpty()) {
            const bool wasPinching = m_touchPinching;
            const bool moved = m_touchMoved;
            const int cell = m_touchCell;
            resetTouch();
            if (m_pressedIndex >= 0) {
                const int oldPressed = m_pressedIndex;
                m_pressedIndex = -1;
                update(cellRect(oldPressed));
            }
            if (wasPinching)
                emit zoomGestureFinished();
            else if (!moved && cell >= 0)
                emit cellClicked(cell);
        }
        return;
    }

    // TouchUpdate.
    if (m_touchGlobals.size() >= 2) {
        if (!m_touchPinching) {
            m_touchPinching = true;
            m_touchMoved = true;
            m_touchCell = -1;
            auto it = m_touchGlobals.constBegin();
            const QPointF first = *it;
            ++it;
            const QPointF second = *it;
            m_pinchStartDistance =
                qMax<qreal>(1.0, distanceBetween(first, second));
            m_pinchStartTileSize = m_tileSize;
            if (m_pressedIndex >= 0) {
                const int oldPressed = m_pressedIndex;
                m_pressedIndex = -1;
                update(cellRect(oldPressed));
            }
        }

        auto it = m_touchGlobals.constBegin();
        const QPointF first = *it;
        ++it;
        const QPointF second = *it;
        const qreal distance = distanceBetween(first, second);
        const qreal factor = distance / m_pinchStartDistance;
        const int size = qBound(kMinTileSize,
                                qRound(m_pinchStartTileSize * factor),
                                kMaxTileSize);
        if (size != m_tileSize) {
            QPointF sum;
            int count = 0;
            for (auto position = m_touchPositions.constBegin();
                 position != m_touchPositions.constEnd(); ++position) {
                sum += position.value();
                ++count;
            }
            const QPoint anchor =
                (sum / qMax(1, count)).toPoint();
            emit zoomRequested(size, anchor);
        }
        return;
    }

    if (m_touchGlobals.size() == 1 && !m_touchPinching) {
        const QPointF global = *m_touchGlobals.constBegin();
        if (!m_touchMoved) {
            if (distanceBetween(global, m_touchStart)
                < qMax<qreal>(8.0, QApplication::startDragDistance())) {
                return;
            }
            m_touchMoved = true;
            m_touchCell = -1;
            if (m_pressedIndex >= 0) {
                const int oldPressed = m_pressedIndex;
                m_pressedIndex = -1;
                update(cellRect(oldPressed));
            }
            // m_touchLastGlobal still holds the first touch point, so the
            // board jumps to the finger rather than trailing it by the
            // gesture threshold.
        }

        const QPointF delta = global - m_touchLastGlobal;
        m_touchLastGlobal = global;
        if (delta.x() != 0.0 || delta.y() != 0.0)
            emit panRequested(delta.toPoint());
    }
}

void MinesweeperBoard::resetTouch()
{
    m_touchPositions.clear();
    m_touchGlobals.clear();
    m_touchStart = QPointF();
    m_touchLastGlobal = QPointF();
    m_touchMoved = false;
    m_touchPinching = false;
    m_pinchStartDistance = 1.0;
    m_pinchStartTileSize = m_tileSize;
    m_touchCell = -1;
}

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

    // The whole minefield is a single widget. It paints only the visible cells,
    // so a 100x100 board pans smoothly and can shrink to a true overview.
    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(false);
    m_scroll->setAlignment(Qt::AlignCenter);
#ifdef Q_OS_ANDROID
    // Touch panning already moves the board. Visible bars only invite a stray
    // tap that jumps to a random part of the field, so hide them while leaving
    // the scroll bars themselves active for the pan/zoom code.
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#endif
    m_boardView = new MinesweeperBoard(m_scroll);
    m_boardView->setCells(&m_cells);
    m_scroll->setWidget(m_boardView);
    layout->addWidget(m_scroll, 1);

    connect(m_boardView, &MinesweeperBoard::cellClicked,
            this, &Minesweeper::onButtonClicked);
    connect(m_boardView, &MinesweeperBoard::cellRightClicked,
            this, &Minesweeper::onCellRightClicked);
    connect(m_boardView, &MinesweeperBoard::panRequested,
            this, &Minesweeper::panBoard);
    connect(m_boardView, &MinesweeperBoard::zoomRequested,
            this, &Minesweeper::zoomBoardAt);
    connect(m_boardView, &MinesweeperBoard::zoomGestureFinished,
            this, &Minesweeper::saveZoom);

    // Explicit controls stay alongside the gestures: a tap is easier for a
    // single step, Fit gives the whole board in one go, and pinch works for
    // people who expect it. The tooltips explain the buttons on desktop.
    auto *zoomRow = new QHBoxLayout;
    zoomRow->setSpacing(6);
    m_zoomOut = new QPushButton(QStringLiteral("\u2212"), this);
    m_zoomReset = new QPushButton(this);
    m_zoomIn = new QPushButton(QStringLiteral("+"), this);
    m_fit = new QPushButton(tr("Fit"), this);
    m_zoomOut->setFixedWidth(34);
    m_zoomIn->setFixedWidth(34);
    m_zoomReset->setMinimumWidth(58);
    for (QPushButton *button : {m_zoomOut, m_zoomReset, m_zoomIn, m_fit})
        button->setFixedHeight(28);
    m_zoomOut->setToolTip(tr("Zoom out"));
    m_zoomIn->setToolTip(tr("Zoom in"));
    m_zoomReset->setToolTip(tr("Reset zoom to 100%"));
    m_fit->setToolTip(tr("Fit the whole board on screen"));
    m_zoomOut->setAutoRepeat(true);
    m_zoomIn->setAutoRepeat(true);
    connect(m_zoomOut, &QPushButton::clicked, this, &Minesweeper::zoomOut);
    connect(m_zoomIn, &QPushButton::clicked, this, &Minesweeper::zoomIn);
    connect(m_zoomReset, &QPushButton::clicked, this, &Minesweeper::resetZoom);
    connect(m_fit, &QPushButton::clicked, this, &Minesweeper::fitBoard);
    zoomRow->addWidget(m_zoomOut);
    zoomRow->addWidget(m_zoomReset);
    zoomRow->addWidget(m_zoomIn);
    zoomRow->addWidget(m_fit);
    zoomRow->addStretch(1);
    layout->addLayout(zoomRow);

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

    // Any tile size between the limits is valid now, so a Fit or pinch value
    // restores exactly instead of snapping to a button step.
    m_tileSize = qBound(
        kMinTileSize,
        settings.value(QStringLiteral("view/tileSize"), kDefaultTileSize).toInt(),
        kMaxTileSize);
    m_boardView->setTileSize(m_tileSize);
    updateZoomLabel();

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

bool Minesweeper::handleBack()
{
    if (!m_activeDialog)
        return false;

    // The prompt runs a nested loop; rejecting it lets that loop finish so the
    // rest of askCustomGame() unwinds normally.
    m_activeDialog->reject();
    return true;
}

void Minesweeper::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (m_resumeTimer && !m_gameOver) {
        m_timer->start();
    }
    m_resumeTimer = false;
}

void Minesweeper::hideEvent(QHideEvent *event)
{
    // Leaving the app for the launcher must not keep the clock running, and a
    // game that has not started yet still counts as paused when it resumes.
    if (m_timer->isActive()) {
        m_timer->stop();
        m_resumeTimer = true;
    }

    QWidget::hideEvent(event);
}

void Minesweeper::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (!m_fitMode || m_fitPending)
        return;

    // The scroll area's viewport may not have taken its new size yet, so fit
    // once the resize has settled instead of measuring stale geometry.
    m_fitPending = true;
    QTimer::singleShot(0, this, [this] {
        m_fitPending = false;
        if (m_fitMode)
            fitBoard();
    });
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
    setStatus(QString());
    m_scroll->horizontalScrollBar()->setValue(0);
    m_scroll->verticalScrollBar()->setValue(0);
    updateCounters();
}

void Minesweeper::buildBoard()
{
    m_cells.clear();
    m_cells.resize(m_rows * m_columns);
    m_boardView->setCells(&m_cells);
    m_boardView->setGrid(m_rows, m_columns, m_tileSize);
    updateZoomLabel();
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

void Minesweeper::onCellRightClicked(int index)
{
    if (m_gameOver || index < 0 || index >= m_cells.size())
        return;

    const Cell &cell = m_cells.at(index);
    // On an opened number the right button chords, just like a regular click.
    // On an unopened tile it still flags.
    if (cell.revealed && !cell.mine && cell.adjacent > 0)
        revealNeighbours(index);
    else
        toggleFlag(index);
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

    refreshBoard();
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
    refreshBoard();
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
            }
        }

        const int difficulty = m_difficulty->currentIndex();
        if (difficulty == kCustomDifficulty) {
            setStatus(
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
                setStatus(
                    tr("You cleared the field in %1 - a new best time!")
                        .arg(formatTime(m_seconds)));
            } else {
                setStatus(tr("You cleared the field in %1. Best: %2.")
                              .arg(formatTime(m_seconds),
                                   formatTime(m_bestTimes.value(difficulty))));
            }
        }
    } else {
        for (int i = 0; i < m_cells.size(); ++i) {
            Cell &cell = m_cells[i];
            if (cell.mine && !cell.flagged)
                cell.revealed = true;
            else if (!cell.mine && cell.flagged)
                cell.wrong = true;
        }
        setStatus(tr("Boom! Game over. Press New to try again."));
    }

    refreshBoard();
    updateCounters();
}

void Minesweeper::refreshBoard()
{
    m_boardView->update();
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

void Minesweeper::setStatus(const QString &text)
{
    // The instructions used to live here and took a permanent row on a phone.
    // Now the row only appears when there is a result to report.
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
}

void Minesweeper::setTileSize(int size)
{
    const int bounded = qBound(kMinTileSize, size, kMaxTileSize);
    if (bounded == m_tileSize && m_boardView->tileSize() == bounded) {
        updateZoomLabel();
        return;
    }

    m_tileSize = bounded;
    m_boardView->setTileSize(bounded);
    updateZoomLabel();
}

void Minesweeper::zoomToAnchor(int size, const QPoint &anchor, bool persist)
{
    const int target = qBound(kMinTileSize, size, kMaxTileSize);
    const QSize oldDocument = m_boardView->size();
    const QPoint oldScroll(m_scroll->horizontalScrollBar()->value(),
                           m_scroll->verticalScrollBar()->value());
    const QPoint viewportPoint = anchor - oldScroll;

    setTileSize(target);

    // Map the anchor through the size change so the spot under the fingers (or
    // under the viewport centre for the buttons) stays put.
    const QSize newDocument = m_boardView->size();
    const qreal xRatio = oldDocument.width() > 0
                             ? qreal(anchor.x()) / oldDocument.width()
                             : 0.5;
    const qreal yRatio = oldDocument.height() > 0
                             ? qreal(anchor.y()) / oldDocument.height()
                             : 0.5;
    const QPoint newAnchor(qRound(xRatio * newDocument.width()),
                           qRound(yRatio * newDocument.height()));

    m_scroll->horizontalScrollBar()->setValue(newAnchor.x() - viewportPoint.x());
    m_scroll->verticalScrollBar()->setValue(newAnchor.y() - viewportPoint.y());

    if (persist)
        saveZoom();
}

void Minesweeper::zoomBoardAt(int size, const QPoint &anchor)
{
    m_fitMode = false;
    zoomToAnchor(size, anchor, false);
}

void Minesweeper::zoomIn()
{
    for (int i = 0; i < kTileSizeCount; ++i) {
        if (kTileSizes[i] > m_tileSize) {
            m_fitMode = false;
            zoomToAnchor(kTileSizes[i], viewportCenterInBoard(), true);
            return;
        }
    }
}

void Minesweeper::zoomOut()
{
    for (int i = kTileSizeCount - 1; i >= 0; --i) {
        if (kTileSizes[i] < m_tileSize) {
            m_fitMode = false;
            zoomToAnchor(kTileSizes[i], viewportCenterInBoard(), true);
            return;
        }
    }
}

void Minesweeper::resetZoom()
{
    m_fitMode = false;
    zoomToAnchor(kDefaultTileSize, viewportCenterInBoard(), true);
}

void Minesweeper::fitBoard()
{
    const QSize available = m_scroll->viewport()->size();
    if (available.width() <= 0 || available.height() <= 0)
        return;

    // Search downwards from the largest tile so Fit is a true whole-board
    // view, even on the 100x100 Insane board.
    int chosen = kMinTileSize;
    for (int size = kMaxTileSize; size >= kMinTileSize; --size) {
        const QSize document =
            MinesweeperBoard::documentSize(m_rows, m_columns, size);
        if (document.width() <= available.width()
            && document.height() <= available.height()) {
            chosen = size;
            break;
        }
    }

    // Keep re-fitting when the window is resized or the phone is rotated,
    // until the player zooms by hand again.
    m_fitMode = true;
    zoomToAnchor(chosen, viewportCenterInBoard(), true);
}

void Minesweeper::panBoard(const QPoint &delta)
{
    // Dragging right moves the board right, which means scrolling left.
    m_scroll->horizontalScrollBar()->setValue(
        m_scroll->horizontalScrollBar()->value() - delta.x());
    m_scroll->verticalScrollBar()->setValue(
        m_scroll->verticalScrollBar()->value() - delta.y());
}

void Minesweeper::saveZoom()
{
    QSettings settings;
    settings.setValue(QStringLiteral("view/tileSize"), m_tileSize);
}

void Minesweeper::updateZoomLabel()
{
    m_zoomReset->setText(
        tr("%1%").arg(qRound(100.0 * m_tileSize / kDefaultTileSize)));
}

QPoint Minesweeper::viewportCenterInBoard() const
{
    return QPoint(m_scroll->horizontalScrollBar()->value()
                      + m_scroll->viewport()->width() / 2,
                  m_scroll->verticalScrollBar()->value()
                      + m_scroll->viewport()->height() / 2);
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
    m_activeDialog = &dialog;
    dialog.show();
    dialog.raise();
#ifndef Q_OS_ANDROID
    // Focusing a spin box on Android would pop up the on-screen keyboard.
    rows->setFocus();
#endif
    loop.exec();
    m_activeDialog = nullptr;

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
