#include "appicons.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <cmath>
#include <functional>

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

} // namespace

QIcon AppIcons::minesweeper()
{
    static const QIcon icon = makeMinesweeperIcon();
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
