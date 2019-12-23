#ifndef ENCODER_H
#define ENCODER_H

#include <math.h>
#include <QObject>
#include <QThread>
#include <QDebug>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavformat/version.h>
#include <libavutil/time.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
}
#include "opencv2/imgproc/imgproc_c.h"

////#include <opencv2/cv.h>
////#include <opencv2/cxcore.h>
////#include <opencv2/highgui.h>

class Encoder:public QThread{
    Q_OBJECT

public:
    Encoder(QObject *parent=0);
    ~Encoder(void);
    void encode_and_push();
    void run();
    void set_filename_Run(int w, int h);
private:
    std::string push_addr;
    int video_w;
    int video_h;
};

struct JYFrame{
//    int seq;
    int64_t launch_time;
    cv::Mat frame;
    JYFrame(int64_t t, cv::Mat mt): launch_time(t){
        frame = std::move(mt);
    }
};

#endif // ENCODER_H
