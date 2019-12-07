#include <encoder.h>
#include <cinttypes>
#include <QMutex>
#include <QTime>

extern QMutex mutex;
extern QList<cv::Mat > listImage;
extern bool listOver;
//extern QList<IplImage*> listImage;
std::string RDIP = "localhost";
int RDPORT = 56789;

Encoder::Encoder(QObject *parent):QThread(parent)
{
//    qDebug("%s", avcodec_configuration());
//    qDebug("version: %d", avcodec_version());
}

Encoder::~Encoder(void){

}

void Encoder::run()
{
    encode_iplimage("./nothing.h264");
}


static bool encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
                   FILE *outfile)
{
    int ret;
    /* send the frame to the encoder */
    if (frame)
        qDebug("[Encoder] ----> Send frame %3""ld""\n", frame->pts);
    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            // 在解码或编码开始时，编解码器可能会接收多个输入帧/数据包而不返回帧，
            // 直到其内部缓冲区被填充为止。
            if (ret==AVERROR_EOF)
                qDebug("[Encoder] avcodec_receive_packet END!");
            return (ret==AVERROR_EOF);
        }
        else if (ret < 0) {
            printf("Error during encoding\n");
            exit(1);
        }
        qDebug("[Encoder] ---> Write packet %3""ld"" (size=%5d)\n", pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
    return (ret==AVERROR_EOF);
}

static cv::Mat avframeToCvmat(const AVFrame * frame){
    int width = frame->width;
    int height = frame->height;
    cv::Mat image(height, width, CV_8UC3);
    int cvLinesizes[1];
    cvLinesizes[0] = image.step1();
    SwsContext* conversion = sws_getContext(width, height, (AVPixelFormat) frame->format,
                                            width, height, AVPixelFormat::AV_PIX_FMT_BGR24,
                                            SWS_FAST_BILINEAR, NULL, NULL, NULL);
    sws_scale(conversion, frame->data, frame->linesize, 0, height, &image.data, cvLinesizes);
    sws_freeContext(conversion);
    return image;
}

static void cvmat_to_avframe(cv::Mat &matframe, AVFrame *frame){
//    AVFrame dst;
////    cvtColor( matframe , matframe , CV_BGR2RGB ) ;
//    cv::Size frameSize = matframe->size();

//    for(int h=0; h < frameSize.height; h++){
//        memcpy(&(frame->data[0][h*frame->linesize[0]]), &(matframe->data[h*matframe->step]), frameSize.width*3);
//    }
//    return ;
    // |>>>>>>>>> code from dalao, refer to: https://zhuanlan.zhihu.com/p/80350418 >>>>>>>>
    int width = matframe.cols;
    int height = matframe.rows;
//    if ((matframe.cols <= 0 || matframe.cols > 2000) || matframe.rows <= 0 || matframe.rows > 2000)
//        qDebug("something wrong with matframe");
    int cvLinesizes[1];
    cvLinesizes[0] = matframe.step1();
    if(frame==NULL){
        frame = av_frame_alloc();
        av_image_alloc(frame->data, frame->linesize, width, height, AVPixelFormat::AV_PIX_FMT_YUV420P, 1);
    }
    SwsContext * conversion = sws_getContext(width, height, AVPixelFormat::AV_PIX_FMT_BGR24,
                                             width, height, (AVPixelFormat)frame->format,
                                             SWS_FAST_BILINEAR,NULL,NULL,NULL);
    sws_scale(conversion, &matframe.data, cvLinesizes, 0, height, frame->data, frame->linesize);
    sws_freeContext(conversion);
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>|
}

void Encoder::encode_iplimage(const char* filenm){
//    this->filename = filenm;
//    QList<IplImage*> listImage;
    avcodec_register_all();

    const char *filename=filenm;
    const AVCodec *codec;
    AVCodecContext *c= NULL;
    int i, ret, x, y;
    FILE *f;
    AVFrame *frame; // *tmpframe;
    AVPacket *pkt;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    AVCodecID codec_id = AV_CODEC_ID_H264;
    /* find the mpeg1video encoder */
    codec = avcodec_find_encoder((AVCodecID)codec_id);
    if (!codec) {
        printf( "Codec '%d' not found\n", codec_id);
        exit(1);
    }
    c = avcodec_alloc_context3(codec);
    if (!c) {
        printf( "Could not allocate video codec context\n");
        exit(1);
    }
    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);
    /* put sample parameters */
    c->bit_rate = 2495000;
    /* resolution must be a multiple of two */
    c->width = 1920;
    c->height = 1080;
    /* frames per second */
    AVRational avtmp = {1,25};
    c->time_base = avtmp;
//    AVRational avtmp2 = {25, 1};
//    c->framerate = avtmp2;
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 10;
    c->max_b_frames = 0;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    if (codec->id == AV_CODEC_ID_H264){
        av_opt_set(c->priv_data, "preset", "veryfast", 0);
        //av_opt_set(c->priv_data, "tune","stillimage,fastdecode,zerolatency",0);
        //av_opt_set(c->priv_data, "tune","zerolatency",0);
        //av_opt_set(c->priv_data, "x264opts","crf=26:vbv-maxrate=728:vbv-bufsize=364:keyint=25",0);
    }
    /* open it */
    ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
//        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        fprintf(stderr, "Could not open codec: %d\n", ret);
        exit(1);
    }
    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame->format = c->pix_fmt;
    frame->width  = c->width;
    frame->height = c->height;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }
    /* encode 1 second of video */
    QTime cur_time;
    cur_time.start();
    int sum_time = 0, intval;
    int index = 0;
    while (true) {
        mutex.lock();
        if(listImage.size() <= 0){
            mutex.unlock();
            if (listOver)
                break;
            else
                continue;
        }
        intval = cur_time.elapsed();
        cur_time.start();
        qDebug("[Encoder] one frame time mutex: %d\n", intval);
        cv::Mat pFrame = listImage[0];
//        IplImage *pFrame = listImage[0];
        listImage.pop_front();
        mutex.unlock();
        fflush(stdout);
        /* make sure the frame data is writable */
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            exit(1);

        intval = cur_time.elapsed();
        cur_time.start();
        qDebug("[Encoder] one frame time deque: %d\n", intval);
        cvmat_to_avframe(pFrame, frame);
        pFrame.release();

        intval = cur_time.elapsed();
        cur_time.start();
        qDebug("[Encoder] one frame time convert: %d\n", intval);
        frame->pts = index++; //av_gettime();       // use the time as pts will boom up the file size (it seems the quality improves as well.)
        /* encode the image */
        encode(c, frame, pkt, f);
        intval = cur_time.elapsed();
        cur_time.start();
        qDebug("[Encoder] one frame time encode: %d\n", intval);
        sum_time += intval;
    }
    /* flush the encoder */
    qDebug("i is: %d\n", i);
    qDebug("I am over!.");
    encode(c, NULL, pkt, f);

    /* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof(endcode), f);
    fclose(f);
    intval = cur_time.elapsed();
    sum_time += intval;
    qDebug("last interval: %d, total time: %d\n", intval, sum_time);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);

}
