#include "quvcview.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    quvcview w;
    w.show();

    return a.exec();
}
