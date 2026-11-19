#include "mainwindow.h"
#include "appicons.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setWindowIcon(AppIcons::launcher());
    // Gives QSettings (used for the Minesweeper best times) a stable home.
    QCoreApplication::setOrganizationName(QStringLiteral("EmeraldMaster"));
    QCoreApplication::setApplicationName(QStringLiteral("EmeraldMaster"));
    MainWindow w;
    w.show();
    return QApplication::exec();
}
