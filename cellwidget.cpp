#include "cellwidget.h"

#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

CellWidget::CellWidget(int number, QWidget *parent)
    : QFrame(parent)
    , m_number(number)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setToolTip(tr("Mini app %1").arg(m_number));
    setStyleSheet(QStringLiteral(
        "CellWidget {"
        "  background-color: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "}"
        "CellWidget:hover {"
        "  border: 2px solid palette(highlight);"
        "}"
        "CellWidget:focus {"
        "  border: 2px solid palette(highlight);"
        "}"));

    m_numberLabel = new QLabel(QString::number(m_number), this);
    QFont numberFont = m_numberLabel->font();
    numberFont.setPointSize(32);
    numberFont.setBold(true);
    m_numberLabel->setFont(numberFont);
    m_numberLabel->setAlignment(Qt::AlignCenter);

    m_hintLabel = new QLabel(tr("Click me"), this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setForegroundRole(QPalette::PlaceholderText);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->addWidget(m_numberLabel, 1);
    m_layout->addWidget(m_hintLabel);
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
    m_hintLabel->hide();

    m_content = widget;
    m_content->setParent(this);
    m_layout->insertWidget(0, m_content, 1);
}

void CellWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        emit clicked(m_number);
    }
    QFrame::mousePressEvent(event);
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
