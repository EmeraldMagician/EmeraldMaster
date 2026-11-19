#include "mainwindow.h"
#include "appicons.h"
#include "cellwidget.h"
#include "minesweeper.h"
#include "./ui_mainwindow.h"

#include <QEvent>
#include <QLayoutItem>
#include <QLineEdit>
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
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &MainWindow::filterApps);
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
        if (i == 0)
            cell->setIcon(AppIcons::minesweeper());

        // Placeholder tiles keep their bare number as the name for now: it
        // is enough to tell them apart in the grid and in search results.
        cell->setTitle(i == 0 ? tr("Minesweeper") : QString::number(i + 1));
        connect(cell, &CellWidget::clicked, this, &MainWindow::openMiniApp);
        m_cells.append(cell);
    }
}

QList<CellWidget *> MainWindow::visibleCells() const
{
    QList<CellWidget *> result;
    for (CellWidget *cell : m_cells) {
        if (!cell->isHidden())
            result.append(cell);
    }
    return result;
}

void MainWindow::filterApps(const QString &text)
{
    bool anyMatch = false;
    for (CellWidget *cell : m_cells) {
        const bool matches = cell->name().contains(text, Qt::CaseInsensitive);
        cell->setVisible(matches);
        anyMatch |= matches;
    }

    ui->noResultsLabel->setVisible(!anyMatch);

    // The column count alone no longer describes the arrangement: the same
    // number of columns can now hold a different set of tiles, so force a
    // rebuild even when the width has not changed.
    m_columns = -1;
    layoutCells();
}

void MainWindow::layoutCells()
{
    const QList<CellWidget *> visible = visibleCells();
    if (visible.isEmpty()) {
        if (m_columns == 0)
            return;

        // Still tear down the old arrangement when a search matches nothing.
        while (QLayoutItem *item = ui->gridLayout->takeAt(0))
            delete item;
        for (int column = 0; column < MaxGridColumns; ++column)
            ui->gridLayout->setColumnStretch(column, 0);
        for (int row = 0; row < CellCount; ++row)
            ui->gridLayout->setRowStretch(row, 0);
        m_columns = 0;
        return;
    }

    const QSize viewport = ui->scrollArea->viewport()->size();
    const QMargins margins = ui->gridLayout->contentsMargins();
    const int horizontalSpacing = ui->gridLayout->horizontalSpacing();
    const QSize minCell = visible.first()->minimumSize();
    const int count = visible.size();

    // Fit as many columns as the current viewport allows, never more than
    // MaxGridColumns. On a desktop window this is MaxGridColumns.
    const int availableWidth = viewport.width() - margins.left() - margins.right();
    const int columns = qBound(
        1,
        (availableWidth + horizontalSpacing)
            / (minCell.width() + horizontalSpacing),
        MaxGridColumns);

    if (columns == m_columns)
        return;
    m_columns = columns;

    // Re-adding a widget that is still in the grid leaves the old cell behind,
    // so clear the items (not the widgets) before arranging them again.
    while (QLayoutItem *item = ui->gridLayout->takeAt(0))
        delete item;

    // Stretches belong to the column index, not to the item, so an empty
    // column left over from a wider arrangement would keep taking space.
    for (int column = 0; column < MaxGridColumns; ++column)
        ui->gridLayout->setColumnStretch(column, 0);
    for (int row = 0; row < CellCount; ++row)
        ui->gridLayout->setRowStretch(row, 0);

    for (int i = 0; i < count; ++i)
        ui->gridLayout->addWidget(visible.at(i), i / columns, i % columns);

    // Let the tiles share the whole viewport, so the grid only scrolls
    // once the window becomes smaller than the tiles' minimum sizes.
    for (int column = 0; column < columns; ++column)
        ui->gridLayout->setColumnStretch(column, 1);

    const int rowCount = (count + columns - 1) / columns;
    for (int row = 0; row < rowCount; ++row)
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

    // Esc asks the open app to go back, then returns to the grid. Window-level
    // context matters on touch devices: a tap does not necessarily leave any
    // widget focused, and the widget that was tapped may already be hidden
    // after switching pages.
    auto *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WindowShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, &MainWindow::goBack);

#ifdef Q_OS_ANDROID
    // Android's back gesture/key is a navigation request rather than a close
    // request (Qt 6.12 sends the app to the background if nobody handles it).
    // It is only enabled while a mini app is open: on the grid page it stays
    // unhandled, so a back press leaves the app the way Android users expect.
    // As above, use the window context so this works without keyboard focus,
    // which is the normal state of a touch-driven interface.
    m_backShortcut = new QShortcut(QKeySequence(Qt::Key_Back), this);
    m_backShortcut->setContext(Qt::WindowShortcut);
    m_backShortcut->setEnabled(false);
    connect(m_backShortcut, &QShortcut::activated, this, &MainWindow::goBack);
#endif
}

void MainWindow::goBack()
{
    // Give the open mini app the first say: its overlays are child widgets,
    // so nothing else would consume Esc or Back for them. Leaving the page
    // with the custom-game prompt still open would hide it mid-loop and show
    // it again the next time the tile is opened.
    if (auto *minesweeper = qobject_cast<Minesweeper *>(m_activeApp)) {
        if (minesweeper->handleBack())
            return;
    }

    showMenu();
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
    m_activeNumber = number;
    ui->appLabel->setText(QString::number(number));
    ui->appTitleLabel->setText(m_cells.at(number - 1)->name());

    switch (number) {
    case 1:
        if (!m_minesweeper)
            m_minesweeper = new Minesweeper(ui->appContent);
        showAppWidget(m_minesweeper);
        break;
    default:
        // Placeholder for the mini apps that are not written yet.
        showAppWidget(nullptr);
        break;
    }

    ui->stackedWidget->setCurrentWidget(ui->appPage);
    if (m_backShortcut)
        m_backShortcut->setEnabled(true);
    setWindowTitle(tr("Emerald Master - %1").arg(ui->appTitleLabel->text()));
}

void MainWindow::showMenu()
{
    if (ui->stackedWidget->currentWidget() == ui->gridPage)
        return;

    ui->stackedWidget->setCurrentWidget(ui->gridPage);
    if (m_backShortcut)
        m_backShortcut->setEnabled(false);
    setWindowTitle(tr("Emerald Master"));

#ifndef Q_OS_ANDROID
    // On desktop, give focus back to the tile that was opened so keyboard
    // users can keep navigating from where they left off. Touch devices have
    // no keyboard, and focusing tiles leaves another tile highlighted when
    // the stacked widget shows the grid again.
    const int index = m_activeNumber - 1;
    CellWidget *cell = m_cells.value(index);
    if (cell && !cell->isHidden())
        cell->setFocus(Qt::OtherFocusReason);
#endif
}
