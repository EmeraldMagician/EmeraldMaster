#include "mainwindow.h"
#include "cellwidget.h"
#include "./ui_mainwindow.h"

#include <QShortcut>
#include <QStyle>

namespace {
// You can keep these two numbers in sync with the actual grid contents.
constexpr int CellCount = 10;
constexpr int GridColumns = 3;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->backButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));

    populateGrid();
    setupNavigation();

    ui->stackedWidget->setCurrentWidget(ui->gridPage);
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
        ui->gridLayout->addWidget(cell, i / GridColumns, i % GridColumns);
        m_cells.append(cell);
    }

    // Let the tiles share the whole viewport, so the grid only scrolls
    // once the window becomes smaller than the tiles' minimum sizes.
    for (int column = 0; column < GridColumns; ++column)
        ui->gridLayout->setColumnStretch(column, 1);

    const int rows = (CellCount + GridColumns - 1) / GridColumns;
    for (int row = 0; row < rows; ++row)
        ui->gridLayout->setRowStretch(row, 1);
}

void MainWindow::setupNavigation()
{
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::showMenu);

    // Esc or Backspace also returns to the grid from anywhere in the window.
    auto *shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, &MainWindow::showMenu);
}

void MainWindow::openMiniApp(int number)
{
    if (number < 1 || number > m_cells.size())
        return;

    // This is where a real mini app would take over the page: replace
    // appLabel with the app's own widget once it exists.
    ui->appLabel->setText(QString::number(number));
    ui->appTitleLabel->setText(tr("Mini app %1").arg(number));

    ui->stackedWidget->setCurrentWidget(ui->appPage);
    setWindowTitle(tr("Emerald Master - %1").arg(number));
    ui->backButton->setFocus(Qt::OtherFocusReason);
    statusBar()->showMessage(tr("Press Esc or the Menu button to go back"));
}

void MainWindow::showMenu()
{
    if (ui->stackedWidget->currentWidget() == ui->gridPage)
        return;

    ui->stackedWidget->setCurrentWidget(ui->gridPage);
    setWindowTitle(tr("Emerald Master"));
    statusBar()->clearMessage();

    // Give focus back to the tile that was opened, so keyboard users
    // can keep navigating from where they left off.
    const int index = ui->appLabel->text().toInt() - 1;
    if (CellWidget *cell = m_cells.value(index))
        cell->setFocus(Qt::OtherFocusReason);
}
