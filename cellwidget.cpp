#include "cellwidget.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QVBoxLayout>

CellWidget::CellWidget(int number, QWidget *parent)
    : QFrame(parent)
    , m_number(number)
{
    setCursor(Qt::PointingHandCursor);
#ifdef Q_OS_ANDROID
    // No keyboard on a phone: keeping tiles out of the focus chain avoids a
    // stray highlight on a different tile when the grid page is shown again.
    setFocusPolicy(Qt::NoFocus);
#else
    setFocusPolicy(Qt::StrongFocus);
#endif
    // 150 px is the baseline used to size the grid. Desktop keeps tiles at
    // this size; on phones they are widened to fill their column, but that
    // width is derived from the full grid so search never resizes a tile.
    // Keep this floor for the initial layout, where two tiles still fit side
    // by side on a phone screen.
    setMinimumSize(150, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QString style = QStringLiteral(
        "CellWidget {"
        "  background-color: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "}");
#ifndef Q_OS_ANDROID
    // Touch screens have no real hover; synthesizing it can leave a phantom
    // highlight behind after a tap.
    style += QStringLiteral(
        "CellWidget:hover {"
        "  border: 2px solid palette(highlight);"
        "}"
        "CellWidget:focus {"
        "  border: 2px solid palette(highlight);"
        "}");
#endif
    setStyleSheet(style);

    m_numberLabel = new QLabel(QString::number(m_number), this);
    QFont numberFont = m_numberLabel->font();
    numberFont.setPointSize(32);
    numberFont.setBold(true);
    m_numberLabel->setFont(numberFont);
    m_numberLabel->setAlignment(Qt::AlignCenter);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_titleLabel->setWordWrap(true);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(11);
    m_titleLabel->setFont(titleFont);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->addWidget(m_numberLabel, 1);
    m_layout->addWidget(m_titleLabel);
}

void CellWidget::setIcon(const QIcon &icon)
{
    const qreal ratio = devicePixelRatioF();
    m_numberLabel->setPixmap(icon.pixmap(QSize(88, 88), ratio));
    m_numberLabel->setText(QString());
}

void CellWidget::setTitle(const QString &title)
{
    m_name = title;
    m_titleLabel->setText(title);
    setToolTip(title);
}

void CellWidget::setContentWidget(QWidget *widget)
{
    if (m_content == widget)
        return;

    if (m_content) {
        m_layout->removeWidget(m_content);
        delete m_content;
        m_content = nullptr;
    }

    if (!widget)
        return;

    m_numberLabel->hide();
    m_titleLabel->hide();

    m_content = widget;
    m_content->setParent(this);
    m_layout->insertWidget(0, m_content, 1);
}

void CellWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressPosition = event->position().toPoint();
        if (focusPolicy() != Qt::NoFocus)
            setFocus(Qt::MouseFocusReason);
    }

    // Ignored on purpose: the parent scroll area (and its touch scroller on
    // Android) only sees the event if it propagates, which lets a finger drag
    // start scrolling instead of opening the tile.
    QFrame::mousePressEvent(event);
}

void CellWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // Acting on release rather than press means a drag that scrolls the grid
    // never counts as a click. The distance check matters because a short
    // drag can end before the scroller takes over, and the release then still
    // lands inside the same tile.
    const bool stayedPut =
        (event->position().toPoint() - m_pressPosition).manhattanLength()
        <= QApplication::startDragDistance();
    if (event->button() == Qt::LeftButton && stayedPut
        && rect().contains(event->position().toPoint())) {
        emit clicked(m_number);
    }

    QFrame::mouseReleaseEvent(event);
}

void CellWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        emit clicked(m_number);
        return;
    default:
        QFrame::keyPressEvent(event);
    }
}
