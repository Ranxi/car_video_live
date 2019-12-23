#include <encoder.h>
#include <cinttypes>
#include <QMutex>
#include <QTime>
#include <ext/hash_map>
#include <algorithm>
#include <cstring>


extern QMutex mutex;
extern QList<JYFrame* > listImage;
extern bool listOver;
//extern QList<IplImage*> listImage;
std::string RDIP = "172.16.20.116";
//std::string RDIP = "127.0.0.1";
int RDPORT = 56789;
__gnu_cxx::hash_map<int, int64_t> buff_latency;
std::vector<int> laten_list;

Encoder::Encoder(QObject *parent):QThread(parent)
{
//    qDebug("%s", avcodec_configuration());
//    qDebug("version: %d", avcodec_version());
}

Encoder::~Encoder(void){

}

void Encoder::run()
{
    encode_and_push();
}

void Encoder::set_filename_Run(int w, int h){
    char push_addr[256];
    sprintf(push_addr, "rtp://%s:%d/live", RDIP.c_str(), RDPORT);
//    sprintf(push_addr, "udp://%s:%d", RDIP.c_str(), RDPORT);
    this->push_addr = std::string(push_addr);
    this->video_w = w;
    this->video_h = h;
    this->start();
}



static void encode(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
                   AVFormatContext *fmtctx)
{
    int ret, ret2;
    /* send the frame to the encoder */
    if (frame)
        qDebug("[Encoder] ----> Send frame %3""lld""\n", frame->pts);
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
            return;
        }
        else if (ret < 0) {
            printf("Error during encoding\n");
            exit(1);
        }
//        printf("Write packet %3""ld"" (size=%5d)\n", pkt->pts, pkt->size);
//        fwrite(pkt->data, 1, pkt->size, outfile);
        int64_t * time_zone;
//        bool embeded = false;
//        int bi_zero = 0;
//        if(pkt->flags){
//            for(int hkp=0; (hkp<<1) < pkt->size; hkp++){
//                time_zone = (int32_t *)(pkt->data+(hkp<<1));
//                if( *time_zone == 0 ){
//    //                *time_zone = pkt->pts;
//                    embeded = true;
//                    bi_zero++;
//                }
//            }
//            qDebug("[Encoder] ----> Receive packet %3""lld"", 0x%llx, sofar latency: %d, encode latency: %d, embeded %d, last bytes: 0x%llx\n",
//                   pkt->pts, pkt->pts, av_gettime()-pkt->pts, av_gettime()-buff_latency[pkt->pts],bi_zero, *time_zone);
//        }
        time_zone = (int64_t *)(pkt->data+pkt->size-128);
//        if(!pkt->flags){
//            *time_zone = pkt->pts;
//        }
//        else
        qDebug("[Encoder] ----> Receive packet %3""lld"", 0x%llx, sofar latency: %d, encode latency: %d, last bytes: 0x%llx\n",
               pkt->pts, pkt->pts, av_gettime()-pkt->pts, av_gettime()-buff_latency[pkt->pts],*time_zone);
//        *hack2 ^= pkt->pts;
//        qDebug("[Encoder] ----> Hack pkt.data %x %x %x %x %x %x %x %x",
//               pkt->data[pkt->size-8],
//               pkt->data[pkt->size-7],
//               pkt->data[pkt->size-6],
//               pkt->data[pkt->size-5],
//               pkt->data[pkt->size-4],
//               pkt->data[pkt->size-3],
//               pkt->data[pkt->size-2],
//               pkt->data[pkt->size-1]);
        laten_list.push_back(av_gettime()-pkt->pts);
        buff_latency.erase(pkt->pts);
        pkt->convergence_duration = pkt->pts;
        ret2 = av_interleaved_write_frame(fmtctx, pkt);
        if (ret2 < 0)
            printf("Error muxing packet\n");
        av_packet_unref(pkt);
        //av_free_packet(pkt);
    }
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
    // qDebug("matframe cols: %d, rows: %d", width, height);
    if ((matframe.cols <= 0 || matframe.cols > 2000) || matframe.rows <= 0 || matframe.rows > 2000)
        qDebug("something wrong with matframe");
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

void Encoder::encode_and_push(){
//    QList<IplImage*> listImage;
    avcodec_register_all();

    const AVCodec *codec;
    AVCodecContext *c= NULL;
    AVFormatContext *fmtctx;
    AVStream *video_st=NULL;
    int i, ret, x, y;
    int videoindex = 0;
    //FILE *f;
    AVFrame *frame; // *tmpframe;
    AVPacket *pkt;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    AVCodecID codec_id = AV_CODEC_ID_H264;
    /* find the mpeg1video encoder */
//    codec = avcodec_find_encoder((AVCodecID)codec_id);
    codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) {
        printf( "Codec '%d' not found\n", codec_id);
        exit(1);
    }
    // |>>>>>>>>>>>>>>> init for RTSP >>>>>>>>>>>>>
    avformat_network_init();
    if ( this->push_addr.compare(0, 4, "rtsp")==0)
        avformat_alloc_output_context2(&fmtctx, NULL, "rtsp", this->push_addr.c_str());
    else if(this->push_addr.compare(0, 3, "rtp")==0)
        avformat_alloc_output_context2(&fmtctx, NULL, "rtp", this->push_addr.c_str());
    else if(this->push_addr.compare(0,3,"udp")==0)
        avformat_alloc_output_context2(&fmtctx, NULL, "mpegts", this->push_addr.c_str());
    else{
        fprintf(stderr, "Do not support push address: %s\n", this->push_addr.c_str());
        exit(1);
    }
//    fmtctx = avformat_alloc_context();
//    fmtctx->oformat = av_guess_format("rtsp", NULL, NULL);
//    sprintf(fmtctx->filename,"rtsp://%s:%d/dddd", RDIP.c_str(), RDPORT);
//    avio_open(&fmtctx->pb, fmtctx->filename, AVIO_FLAG_WRITE);

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>|
    video_st = avformat_new_stream(fmtctx, codec);
    c = avcodec_alloc_context3(codec);
    //avcodec_get_context_defaults3(c, codec);
    c->codec_id = codec_id;
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    /* put sample parameters */
    c->bit_rate = 2495000;
    /* resolution must be a multiple of two */
    c->width = this->video_w;
    c->height = this->video_h;
    /* frames per second */
    AVRational avtmp = {1,25};
    c->time_base = avtmp;
    video_st->time_base = avtmp;
//    AVRational avtmp2 = {25, 1};
//    c->framerate = avtmp2;
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 5;
    c->max_b_frames = 0;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->codec_tag = 0;
    if(fmtctx->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags|=  AV_CODEC_FLAG_GLOBAL_HEADER;
    if (strcmp(c->codec->name, "libx264")==0){
        av_opt_set(c->priv_data, "preset", "veryfast", 0);
        av_opt_set(c->priv_data, "tune","stillimage,zerolatency",0);
    }
    else if (strcmp(c->codec->name, "h264_nvenc")==0){
        av_opt_set(c->priv_data, "preset", "llhq", 0);
        av_opt_set(c->priv_data, "delay", "0", 0);
        av_opt_set(c->priv_data, "zerolatency", "1", 0);
        av_opt_set(c->priv_data, "gpu", "0", 0);
    }
//    av_opt_set(c->priv_data, "x264opts","crf=26:vbv-maxrate=728:vbv-bufsize=364:keyint=25",0);
    /* open it */
    video_st->codec = c;
    videoindex = video_st->id = fmtctx->nb_streams - 1;  //加入到fmt_ctx流
    ret = avcodec_open2(video_st->codec, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        exit(1);
    }


    AVDictionary *format_opts = NULL;
    av_dict_set(&format_opts, "stimeout", std::to_string(2 * 1000000).c_str(), 0);
    av_dict_set(&format_opts, "rtsp_transport", "udp", 0);

    fmtctx->max_interleave_delta = 1000000;
    if (!(fmtctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&fmtctx->pb, fmtctx->filename, AVIO_FLAG_WRITE);
        if (ret < 0){
            fprintf(stderr, "Could Could not open output file '%s': %s", fmtctx->filename, av_err2str(ret));
            exit(1);
        }
    }

    ret = avformat_write_header(fmtctx, &format_opts);
    if (ret < 0){
        fprintf(stderr, "Write header error: %s\n", av_err2str(ret));
        exit(1);
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>|

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
        fprintf(stderr, "Could not allocate the video frame data: %s\n", av_err2str(ret));
        exit(1);
    }
    // alloc packet for receiving encoded img
    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);
    /* encode video */
    QTime cur_time;
    cur_time.start();
    int sum_time = 0, intval;
    int index = 0;

    while (!isInterruptionRequested() && !listOver ) {
        JYFrame* jyf;
        int64_t l_time;
        mutex.lock();
        if(listImage.count() <= 0){
            mutex.unlock();
            continue;
        }
        intval = cur_time.elapsed();
        cur_time.start();
        qDebug("[Encoder] one frame time lock: %d\n", intval);
        if (listImage.count() > 3){
            jyf = listImage.back();
            listImage.pop_back();
            for (auto x : listImage){
                x->frame.release();
                free(x);
            }
            listImage.clear();
        }
        else{
            jyf = listImage[0];
            listImage.pop_front();
        }
        mutex.unlock();
        cv::Mat pFrame = jyf->frame;
        l_time = jyf->launch_time;
        // NECESSARY! Reduce the ref count to cv::Mat.data,
        //     so that pFrame.release() will release memory finally.
        jyf->frame.release();
        free(jyf);
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
        frame->pts = l_time;
        buff_latency[frame->pts] = av_gettime();
        // |>>>>>>>>> Encode the image >>>>>>>>>>
//        av_init_packet(pkt);
        pkt->stream_index = videoindex;
        encode(video_st->codec, frame, pkt, fmtctx);
//        av_init_packet(pkt);
//        pkt->pts = AV_NOPTS_VALUE;        // Presentation timestamp; the time at which the decompressed packet will be presented to the user
//        pkt->dts = AV_NOPTS_VALUE;        // Decompression timestamp; the time at which the packet is decompressed.
//        frame->pts = video_st->codec->frame_number;
//        ret = avcodec_encode_video2(video_st->codec, pkt, frame, &got_output);
//        if (ret < 0) {
//            fprintf(stderr, "Error encoding video frame: %d\n", ret);
//            exit(1);
//        }
//        if (got_output){
//            if (c->coded_frame->key_frame)
//                pkt->flags |= AV_PKT_FLAG_KEY;
//            pkt->stream_index = video_st->index;
//            if (pkt->pts != AV_NOPTS_VALUE)
//                pkt->pts = av_rescale_q(pkt->pts, video_st->codec->time_base, video_st->time_base);
//            if (pkt->dts != AV_NOPTS_VALUE)
//                pkt->dts = av_rescale_q(pkt->dts, video_st->codec->time_base, video_st->time_base);
//            ret = av_interleaved_write_frame(fmtctx, pkt);
//        }
//        else{
//            ret = 0;
//        }
        // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>|
        intval = cur_time.elapsed();
        cur_time.start();
        qDebug("[Encoder] one frame time encode: %d\n", intval);
        sum_time += intval;
    }
    /* flush the encoder */
    qDebug("i is: %d\n", i);
    qDebug("I am over!.");
    encode(video_st->codec, NULL, pkt, fmtctx);

    /* add sequence end code to have a real MPEG file */
    // fwrite(endcode, 1, sizeof(endcode), f);
    //fclose(f);

    av_write_trailer(fmtctx);

    // |>>>>>> print SDP info for VLC >>>>>>>
    char sdp[2048];
    av_sdp_create(&fmtctx,1, sdp, sizeof(sdp));
    qDebug("sdp:\n%s\n", sdp);
    fflush(stdout);
    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>|

    int64_t sum_laten = 0;
    for(auto x : laten_list)
        sum_laten += x;
    qDebug("\navg latency:%f\n", (float)sum_laten*1.0/laten_list.size());

    intval = cur_time.elapsed();
    sum_time += intval;
    qDebug("last interval: %d, total time: %d\n", intval, sum_time);

    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    //avformat_free_context(fmtctx);

}
