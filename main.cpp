#include "livewindow.h"
#include <QApplication>
#include "encoder.h"
#include <QMutex>
#include <QQueue>

QMutex mutex;           // mutex used for listImage;
QList<JYFrame* > listImage;     // image queue used between encoder
bool decoded_listOver;          // better call it : encoded_over;
//QList<IplImage*> listImage;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    LiveWindow w;
    w.show();
    Encoder abc;

    listImage.clear();
    decoded_listOver = false;
    w.set_encoder(&abc);

    return a.exec();
}
