#include "mainwindow.h"
#include "cellwidget.h"
#include "minesweeper.h"
#include "./ui_mainwindow.h"

#include <QEvent>
#include <QShortcut>
#include <QStyle>

#ifdef Q_OS_ANDROID
#include <QScroller>
#endif

namespace {
// Maximum number of tiles per row. Narrower windows use fewer columns so the
// grid still fits, e.g. two on a phone instead of a sideways scrollbar.
constexpr int MaxGridColumns = 3;

// Keep in sync with the actual grid contents.
constexpr int CellCount = 10;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

#ifdef Q_OS_ANDROID
    // Phones use the system Back gesture/key instead; an on-screen Menu
    // button would be a second, unnecessary way to leave the mini app.
    ui->backButton->hide();
#else
    ui->backButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
#endif

    populateGrid();
    setupNavigation();

    ui->stackedWidget->setCurrentWidget(ui->gridPage);
    ui->scrollArea->viewport()->installEventFilter(this);

#ifdef Q_OS_ANDROID
    // A finger has no hover and no wheel: let drags scroll the grid, and let
    // taps fall through to the tiles.
    QScroller::grabGesture(ui->scrollArea->viewport(),
                           QScroller::LeftMouseButtonGesture);
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::populateGrid()
{
    for (int i = 0; i < CellCount; ++i) {
        auto *cell = new CellWidget(i + 1, ui->scrollAreaWidgetContents);
        connect(cell, &CellWidget::clicked, this, &MainWindow::openMiniApp);
        m_cells.append(cell);
    }
}

void MainWindow::layoutCells()
{
    if (m_cells.isEmpty())
        return;

    // Fit as many columns as the current viewport allows, never more than
    // MaxGridColumns. On a desktop window this is MaxGridColumns.
    const int minCellWidth = m_cells.first()->minimumWidth();
    const QMargins margins = ui->gridLayout->contentsMargins();
    const int spacing = ui->gridLayout->horizontalSpacing();
    const int available = ui->scrollArea->viewport()->width()
                          - margins.left() - margins.right();
    const int columns = qBound(1, (available + spacing) / (minCellWidth + spacing),
                               MaxGridColumns);
    if (columns == m_columns)
        return;
    m_columns = columns;

    // Clear the stretches of the previous arrangement first.
    for (int column = 0; column < MaxGridColumns; ++column)
        ui->gridLayout->setColumnStretch(column, 0);
    for (int row = 0; row < m_cells.size(); ++row)
        ui->gridLayout->setRowStretch(row, 0);

    for (int i = 0; i < m_cells.size(); ++i)
        ui->gridLayout->addWidget(m_cells.at(i), i / columns, i % columns);

    // Let the tiles share the whole viewport, so the grid only scrolls
    // once the window becomes smaller than the tiles' minimum sizes.
    for (int column = 0; column < columns; ++column)
        ui->gridLayout->setColumnStretch(column, 1);

    const int rows = (m_cells.size() + columns - 1) / columns;
    for (int row = 0; row < rows; ++row)
        ui->gridLayout->setRowStretch(row, 1);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->scrollArea->viewport() && event->type() == QEvent::Resize)
        layoutCells();

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupNavigation()
{
#ifndef Q_OS_ANDROID
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::showMenu);
#endif

    // Esc returns to the grid from anywhere in the window (desktop keyboards).
    // Window-level context matters on touch devices: a tap does not
    // necessarily leave any widget focused, and the widget that was tapped
    // may already be hidden after switching pages.
    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WindowShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, &MainWindow::showMenu);

    // Android's back gesture/key is a navigation request rather than a close
    // request (Qt 6.12 sends the app to the background if nobody handles it).
    // It is only enabled while a mini app is open: on the grid page it stays
    // unhandled, so a back press leaves the app the way Android users expect.
    // As above, use the window context so this works without keyboard focus,
    // which is the normal state of a touch-driven interface.
    m_backShortcut = new QShortcut(QKeySequence(Qt::Key_Back), this);
    m_backShortcut->setContext(Qt::WindowShortcut);
    m_backShortcut->setEnabled(false);
    connect(m_backShortcut, &QShortcut::activated, this, &MainWindow::showMenu);
}

void MainWindow::showAppWidget(QWidget *widget)
{
    if (m_activeApp == widget)
        return;

    if (m_activeApp) {
        ui->appContentLayout->removeWidget(m_activeApp);
        m_activeApp->hide();
    }

    m_activeApp = widget;
    if (m_activeApp) {
        ui->appLabel->hide();
        m_activeApp->setParent(ui->appContent);
        ui->appContentLayout->addWidget(m_activeApp);
        m_activeApp->show();
    } else {
        ui->appLabel->show();
    }
}

void MainWindow::openMiniApp(int number)
{
    if (number < 1 || number > m_cells.size())
        return;

    // Keep the number up to date even when a real app covers it, because
    // showMenu() uses it to restore focus to the tile that was opened.
    ui->appLabel->setText(QString::number(number));

    switch (number) {
    case 1:
        if (!m_minesweeper)
            m_minesweeper = new Minesweeper(ui->appContent);
        showAppWidget(m_minesweeper);
        ui->appTitleLabel->setText(tr("Minesweeper"));
        break;
    default:
        // Placeholder for the mini apps that are not written yet.
        showAppWidget(nullptr);
        ui->appTitleLabel->setText(tr("Mini app %1").arg(number));
        break;
    }

    ui->stackedWidget->setCurrentWidget(ui->appPage);
    m_backShortcut->setEnabled(true);
    setWindowTitle(tr("Emerald Master - %1").arg(number));
}

void MainWindow::showMenu()
{
    if (ui->stackedWidget->currentWidget() == ui->gridPage)
        return;

    ui->stackedWidget->setCurrentWidget(ui->gridPage);
    m_backShortcut->setEnabled(false);
    setWindowTitle(tr("Emerald Master"));

#ifndef Q_OS_ANDROID
    // On desktop, give focus back to the tile that was opened so keyboard
    // users can keep navigating from where they left off. Touch devices have
    // no keyboard, and focusing tiles leaves another tile highlighted when
    // the stacked widget shows the grid again.
    const int index = ui->appLabel->text().toInt() - 1;
    if (CellWidget *cell = m_cells.value(index))
        cell->setFocus(Qt::OtherFocusReason);
#endif
}
