#include <QCoreApplication>
#include "mainwindows.h"
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    mainwindows iRecord;
    return a.exec();
}
