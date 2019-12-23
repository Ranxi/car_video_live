#include "livewindow.h"
#include "ui_livewindow.h"
#include <QMutex>
#include <QTime>

extern QMutex mutex;
extern QList<JYFrame* > listImage;
extern bool listOver;
//extern QList<IplImage*> listImage;

LiveWindow::LiveWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LiveWindow)
{
    ui->setupUi(this);
    ui->graphicsView->setScene(new QGraphicsScene());
    ui->graphicsView->scene()->addItem(&pixmap);
    timer.start(1000);
    rcver = NULL;
    pusher = NULL;
    whetherToPush = true;
    m_stat = Ui::IDLE;
    ui->videoEdit->setText("/media/teeshark/Work/LYQ/autonomous_car/video/360s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mp4");
}

LiveWindow::~LiveWindow()
{
    delete ui;
}

void LiveWindow::set_encoder(Encoder *ecder){
    pusher = ecder;
}


void LiveWindow::on_startBtn_pressed(){
    if(video.isOpened() || (pusher!=NULL && pusher->isRunning()) || rcver!=NULL){
        video.release();
        disconnect(&timer, &QTimer::timeout, this, 0);
        if(vtype==Ui::NETWORK){
            rcver->close();
            delete rcver;
            rcver = NULL;
        }
        else{
            assert(m_stat==Ui::PUSHING);
            pusher->requestInterruption();
            pusher->quit();
            pusher->wait();
            mutex.lock();
            for (auto x : listImage)
                x->frame.release();
            listImage.clear();
            mutex.unlock();
        }
        m_stat = Ui::IDLE;
        ui->startBtn->setText("Start");
        listOver = true;
        return;
    }
    else if (pusher!=NULL && pusher->isFinished() && (Ui::IDLE!=m_stat)){
        disconnect(&timer, &QTimer::timeout, this, 0);
        mutex.lock();
        for (auto x : listImage)
            x->frame.release();
        listImage.clear();
        mutex.unlock();
        m_stat = Ui::IDLE;
        ui->startBtn->setText("Start");
        listOver = true;
        return;
    }
    else{
        bool isCamera;
        int cameraIndex = ui->videoEdit->text().toInt(&isCamera);
        if(isCamera){
            if(!video.open(cameraIndex)){
                QMessageBox::critical(this, "amera Error", "Make sure you entered a correct camera index, or that the camera is not being accessed by another program!");
                ui->startBtn->setText("Start");
                return;
            }
            connect(&timer, &QTimer::timeout, this, &LiveWindow::updateFrame);
            vtype = Ui::CAMERA;
        }
        else if(ui->videoEdit->text().trimmed().contains("http://") || ui->videoEdit->text().trimmed().contains("https://")){
            QString addr = ui->videoEdit->text().trimmed();
            QStringList addr_list = addr.split(":");
            int port = addr_list.at(addr_list.size()-1).toInt();
            rcver = new QUdpSocket(this);
            rcver->bind(QHostAddress::Any, port);
            connect(rcver, &QUdpSocket::readyRead, this, &LiveWindow::processPendingDatagram);
            vtype = Ui::NETWORK;
            // ************** RETRANSMIT FROM HTTP TO ANOTHER ADDRESS NOT IMPLEMENTED *************
        }
        else{
            //qDebug() <<  ui->videoEdit->text();
            if(!video.open(ui->videoEdit->text().trimmed().toStdString())){
                    QMessageBox::critical(this,
                                              "Video Error",
                                              "Make sure you entered a correct and supported video file path,"
                                              "<br>or a correct RTSP feed URL!");
                    return;
            }
            connect(&timer, &QTimer::timeout, this, &LiveWindow::updateFrame);
            vtype = Ui::LOCALFILE;
        }
        if (whetherToPush && Ui::NETWORK!=vtype){
            pusher->set_filename_Run();
            listOver = false;
        }
        ui->startBtn->setText("Stop");
        m_stat = Ui::PUSHING;
        timer.start(40);
    }
}

void LiveWindow::updateFrame(){

    if (vtype!=Ui::NETWORK)
        video >> frame;
    if(!frame.empty()){
//        IplImage *pimage = &IplImage(frame);
//        IplImage *image = cvCloneImage(pimage);
        mutex.lock();
        JYFrame * curframe = new JYFrame(av_gettime(), frame.clone());
        listImage.append(curframe);
//        listImage.append(image);
        qDebug("[Player] listImage count : %d\n", listImage.size());
        mutex.unlock();

        QImage qimg((uchar*)(frame.data),
                    frame.cols,
                    frame.rows,
                    frame.step,
                    QImage::Format_RGB888);
        // qimg.scaled(ui->graphicsView->width(), ui->graphicsView->height());
        // If your QImage is not the same size as the label and setPixmap does the scaling,
        // you could perhaps use a faster scaling routine
        QTime cur_time;
        cur_time.start();
        QPixmap tmpPixMap = QPixmap::fromImage(qimg.rgbSwapped());
        pixmap.setPixmap(tmpPixMap);
        qDebug("[Player] update pixmap: %d\n", cur_time.elapsed());
//        frame.release();
        //qApp->processEvents();
//        curframe->frame.release();
//        free(curframe);
    }
    else{
        listOver = true;
        on_startBtn_pressed();
    }

}
// http://172.16.20.252:23323
// C:\Users\admin\Videos\cam\hhhh.mp4
// C:\Users\Public\Videos\Sample Videos\Wildlife.wmv
// C:\Users\admin\Videos\Prepar3D\Prepar3D 04.13.2018 - 16.53.52.01.mp4
// C:\Users\admin\Desktop\shifeiJ20\VID_20180513_203441.mp4_20180526_155356.mkv
// D:\LYQ\entertainment\18s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mpeg
// D:\LYQ\entertainment\49s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mpeg



//void LiveWindow::on_startBtn_pressed(){
//    //using namespace cv;
//    if(video.isOpened()){
//        ui->startBtn->setText("Start");
//        video.release();
//        return;
//    }
//    bool isCamera;
//    int cameraIndex = ui->videoEdit->text().toInt(&isCamera);
//    if(isCamera){
//        if(!video.open(cameraIndex)){
//            QMessageBox::critical(this, "amera Error", "Make sure you entered a correct camera index, or that the camera is not being accessed by another program!");
//            return;
//        }
//    }
//    else{
//        if(!video.open(ui->videoEdit->text().trimmed().toStdString())){
//                QMessageBox::critical(this,
//                                          "Video Error",
//                                          "Make sure you entered a correct and supported video file path,"
//                                          "<br>or a correct RTSP feed URL!");
//        }
//    }

//    ui->startBtn->setText("Stop");

//    cv::Mat frame;
//    QTime cur_time;
//    while(video.isOpened()){
//        cur_time.start();
//        video >> frame;
//        printf("video >> frame,  time : %d\n", cur_time.elapsed());
//        cur_time.start();
//        if (!frame.empty()){
////            cv::copyMakeBorder(frame,
////                               frame,
////                               frame.rows/2,
////                               frame.rows/2,
////                               frame.cols/2,
////                               frame.cols/2,
////                               cv::BORDER_ISOLATED);

//            QImage qimg((uchar*)(frame.data),
//                        frame.cols,
//                        frame.rows,
//                        frame.step,
//                        QImage::Format_RGB888);
//            //qimg.scaled(ui->graphicsView->width(), ui->graphicsView->height());
//            printf("qimg,  time : %d\n", cur_time.elapsed());
//            cur_time.start();
//            // If your QImage is not the same size as the label and setPixmap does the scaling,
//            // you could perhaps use a faster scaling routine
//            pixmap.setPixmap(QPixmap::fromImage(qimg.rgbSwapped()));
//            //ui->graphicsView->fitInView(&pixmap, Qt::KeepAspectRatio);
//        }
//        printf("setPixmap time : %d\n", cur_time.elapsed());
//        cur_time.start();
//        qApp->processEvents();
//        printf("qApp time : %d\n", cur_time.elapsed());
//    }
//    ui->startBtn->setText("Start");
//    printf("ui height %d and width %d\n",ui->graphicsView->width(), ui->graphicsView->height());
//}
// http://172.16.20.116:23323
// /media/teeshark/OS/Users/Public/Videos/Sample Videos/animal.wmv
// /media/teeshark/OS/Users/admin/Videos/Prepar3D/Prepar3D 04.13.2018 - 16.53.52.01.mp4
// /media/teeshark/OS/Users/admin/Desktop/shifeiJ20/VID_20180513_203441.mp4_20180526_155356.mkv
// /media/teeshark/Work/LYQ/entertainment/Relying.on.Heaven.to.Slaughter.Dragons.E49.1080P.WEB-DL.AAC.H264-DanNi.mp4
// /media/teeshark/Work/LYQ/entertainment/18s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mpeg
// /media/teeshark/Work/LYQ/entertainment/49s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mpeg
//  ====== WITH TIME COUNTER ======
// /media/teeshark/Work/LYQ/autonomous_car/video/timecounter.mp4
// /media/teeshark/Work/LYQ/autonomous_car/video/360s.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mp4
// /media/teeshark/Work/LYQ/autonomous_car/video/whole.Relying.on.Heaven.to.Slaughter.Dragons.2019.E43.1080p.WEB-DL.H264.mp4



void LiveWindow::processPendingDatagram(){
    QByteArray datagram;
    datagram.resize(rcver->pendingDatagramSize());
    rcver->readDatagram(datagram.data(), datagram.size());
    QByteArray decryptedByte;
    decryptedByte = QByteArray::fromBase64(datagram.data());
    QByteArray uncompressByte=qUncompress(decryptedByte);
    QImage net_image;
    net_image.loadFromData(uncompressByte);
    pixmap.setPixmap(QPixmap::fromImage(net_image));
}

void LiveWindow::closeEvent(QCloseEvent *event){
    if(video.isOpened()){
        QMessageBox::warning(this, "Warning",
                             "Stop the video before closing the application!");
        //event->ignore();
        listOver = true;
        on_startBtn_pressed();
        event->accept();
    }
    else{
        event->accept();
    }
}
