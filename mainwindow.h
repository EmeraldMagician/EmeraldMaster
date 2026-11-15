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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void openMiniApp(int number);
    void showMenu();

private:
    void populateGrid();
    void setupNavigation();

    Ui::MainWindow *ui;
    QList<CellWidget *> m_cells;
};
#endif // MAINWINDOW_H
