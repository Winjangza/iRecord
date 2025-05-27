#include <QCoreApplication>
#include "mainwindows.h"
//#define QT_NO_DEBUG
int main(int argc, char *argv[])
{
//#ifdef QT_NO_DEBUG
//         qputenv("QT_LOGGING_RULES", "qml=false; *.debug=false");
//#endif
    QCoreApplication a(argc, argv);

    mainwindows iRecord;
    return a.exec();
}
