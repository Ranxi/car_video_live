#include "livewindow.h"
#include <QApplication>
#include "encoder.h"
#include <QMutex>
#include <QQueue>

QMutex mutex;
QList<JYFrame* > listImage;
bool listOver;
//QList<IplImage*> listImage;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    LiveWindow w;
    w.show();
    Encoder abc;

    listImage.clear();
    listOver = false;
    w.set_encoder(&abc);

    return a.exec();
}
