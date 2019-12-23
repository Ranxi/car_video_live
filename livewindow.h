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
#include "encoder.h"
#include "radialstitcher.h"


namespace Ui {
class LiveWindow;
enum VIDEOTYPE {CAMERA, NETWORK, LOCALFILE, NONE};
enum SERVERSTAT {IDLE, CONNTECTING, PUSHING};
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
    Encoder * pusher;
    QUdpSocket *rcver;
    Ui::VIDEOTYPE vtype;
    Ui::SERVERSTAT m_stat;
    std::vector<cv::VideoCapture> cam_captures;
    bool whetherToPush;
private slots:
    void on_startBtn_pressed();
    void updateFrame();
    void updateFrameFromStitcher();
    void processPendingDatagram();
public:
    void set_encoder(Encoder *ecder);
};

#endif // LIVEWINDOW_H
