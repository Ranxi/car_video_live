#include "livewindow.h"
#include "ui_livewindow.h"
#include <QMutex>
#include <QTime>

extern QMutex mutex;
extern QList<JYFrame* > listImage;
extern bool decoded_listOver;
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
    m_stat = Ui::SERVERSTAT::IDLE;
    vtype = Ui::VIDEOTYPE::NONE;
    ui->videoEdit->setText("/media/teeshark/Work/LYQ/autonomous_car/video/360s.Relying.mp4");
}

LiveWindow::~LiveWindow()
{
    delete ui;
}

void LiveWindow::set_encoder(Encoder *ecder){
    pusher = ecder;
}


void LiveWindow::on_startBtn_pressed(){
    // dealing with interaction when it is during playing
    if(video.isOpened() || (pusher!=NULL && pusher->isRunning()) || rcver!=NULL){
        video.release();
        disconnect(&timer, &QTimer::timeout, this, 0);
        if(vtype==Ui::VIDEOTYPE::NETWORK){
            rcver->close();
            delete rcver;
            rcver = NULL;
        }
        else{
            if (vtype==Ui::CAMERA){
                for (auto cap : cam_captures)
                    cap.release();
                cam_captures.clear();
            }
            assert(m_stat==Ui::SERVERSTAT::PUSHING);
            pusher->requestInterruption();
            pusher->quit();
            pusher->wait();
            mutex.lock();
            for (auto x : listImage)
                x->frame.release();
            listImage.clear();
            mutex.unlock();
        }
        vtype = Ui::VIDEOTYPE::NONE;
        m_stat = Ui::SERVERSTAT::IDLE;
        ui->startBtn->setText("Start");
        decoded_listOver = true;
        return;
    }
    else if (pusher!=NULL && pusher->isFinished() && (Ui::SERVERSTAT::IDLE!=m_stat)){
        disconnect(&timer, &QTimer::timeout, this, 0);
        mutex.lock();
        for (auto x : listImage)
            x->frame.release();
        listImage.clear();
        mutex.unlock();
        m_stat = Ui::SERVERSTAT::IDLE;
        ui->startBtn->setText("Start");
        decoded_listOver = true;
        return;
    }
    // dealing with interaction when it is inactive
    else{
        bool isCamera;
        int cameraIndex = ui->videoEdit->text().toInt(&isCamera);
        int v_width, v_height;
        if(isCamera){
            if(cameraIndex < 100 && !video.open(cameraIndex)){
                QMessageBox::critical(this, "amera Error", "Make sure you entered a correct camera index, or that the camera is not being accessed by another program!");
                ui->startBtn->setText("Start");
                return;
            }
            const std::string cam_idx[4] = {"/media/teeshark/Work/LYQ/autonomous_car/video/whole.Relying.mp4",
                                            "/media/teeshark/Work/LYQ/autonomous_car/video/whole.Relying.mp4",
                                            "/media/teeshark/Work/LYQ/autonomous_car/video/whole.Relying.mp4",
                                            "/media/teeshark/Work/LYQ/autonomous_car/video/whole.Relying.mp4"};
            this->cam_captures.clear();
            for (int i=0; i < 4; i++){
                cv::VideoCapture capture(cam_idx[i]);
                this->cam_captures.push_back(capture);
            }
            connect(&timer, &QTimer::timeout, this, &LiveWindow::updateFrameFromStitcher);
            vtype = Ui::VIDEOTYPE::CAMERA;
            v_width = 640 << 2;
            v_height = 520;
        }
        else if(ui->videoEdit->text().trimmed().contains("http://") || ui->videoEdit->text().trimmed().contains("https://")){
            QString addr = ui->videoEdit->text().trimmed();
            QStringList addr_list = addr.split(":");
            int port = addr_list.at(addr_list.size()-1).toInt();
            rcver = new QUdpSocket(this);
            rcver->bind(QHostAddress::Any, port);
            connect(rcver, &QUdpSocket::readyRead, this, &LiveWindow::processPendingDatagram);
            vtype = Ui::VIDEOTYPE::NETWORK;
            // ************** RETRANSMIT FROM HTTP TO ANOTHER ADDRESS NOT IMPLEMENTED *************
        }
        else{
            //qDebug() <<  ui->videoEdit->text();
            if(!video.open(ui->videoEdit->text().trimmed().toStdString())){
                    QMessageBox::critical(this,
                                              "Video Error",
                                              "Make sure you entered a valid video file path!");
                    return;
            }
            connect(&timer, &QTimer::timeout, this, &LiveWindow::updateFrame);
            vtype = Ui::VIDEOTYPE::LOCALFILE;
            v_width = 1920; //1280;  //1920;
            v_height = 1080; //720;  //1080;
        }
        if (whetherToPush && Ui::VIDEOTYPE::NETWORK!=vtype){
            pusher->set_filename_Run(v_width, v_height);
            decoded_listOver = false;
        }
        ui->startBtn->setText("Stop");
        m_stat = Ui::SERVERSTAT::PUSHING;
        timer.start(40);
    }
}

void LiveWindow::updateFrame(){

    if (vtype!=Ui::VIDEOTYPE::NETWORK)
        video >> frame;
    if(!frame.empty()){
//        IplImage *pimage = &IplImage(frame);
//        IplImage *image = cvCloneImage(pimage);
        JYFrame * curframe = new JYFrame(av_gettime(), frame.clone());
        mutex.lock();
        listImage.append(curframe);
//        listImage.append(image);
        mutex.unlock();
        qDebug("[Player] listImage count : %d\n", listImage.size());

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
        decoded_listOver = true;
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




void LiveWindow::updateFrameFromStitcher(){
    bool EMPTY = false;
    std::vector<cv::Mat> img_list;
    RadialStitcher *rs = new RadialStitcher(1);
    cv::Rect rect(19, 0, 602, 480);
    std::vector<cv::Mat> cylinder_maps = rs->cylinder_projection_map(640, 480, 720);  //calculate the mapping of the cylinder

    for (int i = 0; i < 4; ++i) {
        cv::Mat frame; cv::Mat cylinder;
        cam_captures[i] >> frame;
        if(frame.empty()){
            EMPTY = true;
            break;
        }
        cv::remap(frame, cylinder, cylinder_maps[0], cylinder_maps[1],cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
        cylinder = cylinder(rect); //remove the blank edge of the cylinder image
        img_list.push_back(cylinder);
    }
    if(EMPTY){
        decoded_listOver = true;
        on_startBtn_pressed();
        return;
    }
    cv::Mat result = rs->Stitch(img_list);
    for (auto cyl : img_list)
        cyl.release();
    img_list.clear();
//    if ((cv::waitKey(25) & 0XFF) == 27)
//        return;
    JYFrame * curframe = new JYFrame(av_gettime(), result.clone());
    mutex.lock();
    listImage.append(curframe);
    mutex.unlock();
    qDebug("[Player] listImage count : %d\n", listImage.size());
    QImage qimg((uchar*)(result.data),
                result.cols,
                result.rows,
                result.step,
                QImage::Format_RGB888);
    QTime cur_time;
    cur_time.start();
    QPixmap tmpPixMap = QPixmap::fromImage(qimg.rgbSwapped());
    pixmap.setPixmap(tmpPixMap);
    qDebug("[Player] update pixmap: %d\n", cur_time.elapsed());
}

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
// /media/teeshark/Work/LYQ/entertainment/The.Big.Short.2015.720p.BluRay.x264.AAC-iHD.mp4
//  ====== WITH TIME COUNTER ======
// /media/teeshark/Work/LYQ/autonomous_car/video/timecounter.mp4
// /media/teeshark/Work/LYQ/autonomous_car/video/360s.Relying.mp4
// /media/teeshark/Work/LYQ/autonomous_car/video/whole.Relying.mp4
// /media/teeshark/Work/LYQ/entertainment/The.Big.Short.2015.720p.BluRay.x264.AAC-iHD.mp4



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
        decoded_listOver = true;
        on_startBtn_pressed();
        event->accept();
    }
    else{
        event->accept();
    }
}
