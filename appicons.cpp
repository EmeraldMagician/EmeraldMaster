#include "appicons.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>

#include <cmath>
#include <functional>
#include <initializer_list>

namespace {

// The glyphs are drawn on a 24x24 canvas and scaled to each pixmap size, so
// they stay crisp on every screen density.
constexpr qreal kGlyphCanvas = 24.0;
constexpr qreal kPi = 3.14159265358979323846;

constexpr int kIconSizes[] = {16, 24, 32, 48, 64, 96, 128};

void addPixmap(QIcon &icon, const QPixmap &pixmap)
{
    // Disabled buttons would otherwise let the style grey the icon out, which
    // makes revealed mines hard to see on the opened tile.
    icon.addPixmap(pixmap, QIcon::Normal);
    icon.addPixmap(pixmap, QIcon::Disabled);
}

void buildIcon(QIcon &icon, qreal canvas,
               const std::function<void(QPainter &)> &paint)
{
    for (int size : kIconSizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.scale(size / canvas, size / canvas);
        paint(painter);
        painter.end();

        addPixmap(icon, pixmap);
    }
}

// A naval mine: eight spikes around a round body, with a small glint.
void drawMine(QPainter &painter, const QRectF &box, const QColor &body,
              const QColor &glint)
{
    const QPointF center = box.center();
    const qreal bodyRadius = box.width() * 0.25;
    const qreal spikeInner = bodyRadius * 1.08;
    const qreal spikeOuter = bodyRadius * 1.82;

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
}

// A pennant on a pole, with a small foot so it still reads at 16 px.
void drawFlag(QPainter &painter, const QColor &cloth, const QColor &pole)
{
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
}

QIcon makeFlag(bool dark)
{
    const QColor cloth = dark ? QColor(QStringLiteral("#ff8a80"))
                              : QColor(QStringLiteral("#c62828"));
    const QColor pole = dark ? QColor(QStringLiteral("#efebe9"))
                             : QColor(QStringLiteral("#4e342e"));

    QIcon icon;
    buildIcon(icon, kGlyphCanvas,
              [cloth, pole](QPainter &painter) { drawFlag(painter, cloth, pole); });
    return icon;
}

QIcon makeMine(bool dark)
{
    const QColor body = dark ? QColor(QStringLiteral("#eceff1"))
                             : QColor(QStringLiteral("#212121"));
    const QColor glint = dark ? QColor(QStringLiteral("#242424"))
                              : QColor(QStringLiteral("#f7f7f7"));
    const QRectF glyph(1.2, 1.2, 21.6, 21.6);

    QIcon icon;
    buildIcon(icon, kGlyphCanvas, [body, glint, glyph](QPainter &painter) {
        drawMine(painter, glyph, body, glint);
    });
    return icon;
}

QIcon makeMinesweeperIcon()
{
    QIcon icon;
    buildIcon(icon, 64.0, [](QPainter &painter) {
        QLinearGradient background(0, 0, 0, 64);
        background.setColorAt(0.0, QColor(QStringLiteral("#4caf50")));
        background.setColorAt(1.0, QColor(QStringLiteral("#1b5e20")));
        painter.setPen(QPen(QColor(QStringLiteral("#14421a")), 2.0));
        painter.setBrush(background);
        painter.drawRoundedRect(QRectF(1, 1, 62, 62), 13, 13);

        drawMine(painter, QRectF(15, 15, 34, 34), Qt::white, QColor(QStringLiteral("#2e7d32")));
    });
    return icon;
}

// The gradient and silhouette are shared by every platform icon. Android uses
// the round cut so its adaptive mask does not have to crop a rounded square.
void drawLauncherBackground(QPainter &painter, bool round)
{
    QLinearGradient background(0, 0, 0, 64);
    background.setColorAt(0.0, QColor(QStringLiteral("#2e7d32")));
    background.setColorAt(1.0, QColor(QStringLiteral("#0b3510")));
    painter.setPen(QPen(QColor(QStringLiteral("#052009")), 2.0));
    painter.setBrush(background);
    const QRectF shape(1, 1, 62, 62);
    if (round)
        painter.drawEllipse(shape);
    else
        painter.drawRoundedRect(shape, 13, 13);
}

void drawLauncherGem(QPainter &painter)
{
    // Emerald vertices exactly as supplied, on a 7x5 design grid. The grid
    // is scaled into the 64 px canvas with y pointing up, as in maths.
    const qreal scale = 8.0;
    const QPointF grid[] = {
        {3, 0}, {0, 3}, {2, 3}, {4, 3},
        {6, 3}, {1, 4}, {3, 4}, {5, 4}
    };
    const auto at = [&](int i) {
        return QPointF(8.0 + grid[i].x() * scale, 48.0 - grid[i].y() * scale);
    };
    const QPointF culet = at(0);
    const QPointF girdleLeft = at(1);
    const QPointF girdleInnerLeft = at(2);
    const QPointF girdleInnerRight = at(3);
    const QPointF girdleRight = at(4);
    const QPointF tableLeft = at(5);
    const QPointF tableMid = at(6);
    const QPointF tableRight = at(7);

    painter.setPen(Qt::NoPen);

    // Fill the silhouette first; antialiasing seams between the flat facets
    // then reveal gem green instead of the dark background.
    QPolygonF outline;
    outline << culet << girdleRight << tableRight << tableLeft << girdleLeft;
    painter.setBrush(QColor(QStringLiteral("#a6f2a0")));
    painter.drawPolygon(outline);

    const auto facet = [&painter](const QColor &color,
                                  std::initializer_list<QPointF> points) {
        QPolygonF polygon;
        for (const QPointF &point : points)
            polygon << point;
        painter.setBrush(color);
        painter.drawPolygon(polygon);
    };

    // Table strip: five facets from the flat top down to the girdle.
    facet(QColor(QStringLiteral("#c9ffc1")),
          {girdleLeft, girdleInnerLeft, tableLeft});
    facet(QColor(QStringLiteral("#8fe88a")),
          {girdleInnerLeft, tableMid, tableLeft});
    facet(QColor(QStringLiteral("#a6f2a0")),
          {girdleInnerLeft, girdleInnerRight, tableMid});
    facet(QColor(QStringLiteral("#6fd66a")),
          {girdleInnerRight, tableRight, tableMid});
    facet(QColor(QStringLiteral("#4cbf4c")),
          {girdleInnerRight, girdleRight, tableRight});

    // Pavilion: three facets converging on the bottom point.
    facet(QColor(QStringLiteral("#7fe07a")),
          {culet, girdleLeft, girdleInnerLeft});
    facet(QColor(QStringLiteral("#3aa63a")),
          {culet, girdleInnerLeft, girdleInnerRight});
    facet(QColor(QStringLiteral("#2c8a30")),
          {culet, girdleInnerRight, girdleRight});
}

void drawLauncher(QPainter &painter)
{
    drawLauncherBackground(painter, false);
    drawLauncherGem(painter);
}

QIcon makeLauncherIcon()
{
    QIcon icon;
    buildIcon(icon, 64.0, drawLauncher);
    return icon;
}

} // namespace

QIcon AppIcons::minesweeper()
{
    static const QIcon icon = makeMinesweeperIcon();
    return icon;
}

QIcon AppIcons::launcher()
{
    static const QIcon icon = makeLauncherIcon();
    return icon;
}

QIcon AppIcons::flag(bool dark)
{
    static const QIcon lightIcon = makeFlag(false);
    static const QIcon darkIcon = makeFlag(true);
    return dark ? darkIcon : lightIcon;
}

QIcon AppIcons::mine(bool dark)
{
    static const QIcon lightIcon = makeMine(false);
    static const QIcon darkIcon = makeMine(true);
    return dark ? darkIcon : lightIcon;
}
