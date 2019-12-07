#ifndef LIVEWINDOW_H
#define LIVEWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QPixmap>
#include <QCloseEvent>
#include <QMessageBox>
#include <QTimer>
#include <QBuffer>
#include <QUdpSocket>
#include <QImageReader>
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc_c.h"




namespace Ui {
class LiveWindow;
enum VIDEOTYPE {CAMERA, NETWORK, LOCALFILE, NONE};
}

//enum VIDEOTYPE{
//    CAMERA,
//    NETWORK,
//    LOCALFILE,
//    NONE,
//};


class LiveWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LiveWindow(QWidget *parent = 0);
    ~LiveWindow();

private:
    void closeEvent(QCloseEvent *event);
    Ui::LiveWindow *ui;
    QGraphicsPixmapItem pixmap;
    cv::VideoCapture video;
    cv::Mat frame;
    QImage qimg;
    QTimer timer;
    QUdpSocket *rcver;
    Ui::VIDEOTYPE vtype;
private slots:
    void on_startBtn_pressed();
    void updateFrame();
    void processPendingDatagram();
};

#endif // LIVEWINDOW_H
