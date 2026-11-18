#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class CellWidget;
class Minesweeper;
class QShortcut;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void openMiniApp(int number);
    void goBack();
    void showMenu();

private:
    void populateGrid();
    void layoutCells();
    void setupNavigation();
    void showAppWidget(QWidget *widget);

    Ui::MainWindow *ui;
    QList<CellWidget *> m_cells;
    QShortcut *m_backShortcut = nullptr;
    Minesweeper *m_minesweeper = nullptr;
    QWidget *m_activeApp = nullptr;
    int m_activeNumber = 0;
    int m_columns = 0;
};
#endif // MAINWINDOW_H
