#include "mainwindow.h"
#include <QLabel>

#include <QApplication>
#include <QtWidgets>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/appIcon.ico"));

    MainWindow *w = new MainWindow;
    w->show();
    return a.exec();
}
