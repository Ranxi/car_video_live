# Car Video Live (2020.05.26)
## Server 

该工程是车载摄像头进行直播的发送端。为了可视化，也将摄像头采集到的页面在窗口中展示。

### Structure
* main.cpp
    - 主程序入口，定义 Livewindow 、 Encoder 、 listImage 。
* livewindow.cpp
    - 该程序继承自QWidget，负责展示摄像头采集到的画面(当前还是从文件中读取frame)，并将读取到的frame 压入 listImage 供 encoder 编码。
    - 其对应有一个.ui文件(UI界面的定义文件），界面内主要有一个 graphicsView 控件，一个输入框和一个按钮。View是展示frame的控件，输入框填入video源，可以是摄像头编号、http网址或文件地址，程序将自动检测输入的是哪一类。点击按钮则开始或结束推流。
        - 摄像头编号： 使用opencv库打开摄像头，并将 `Timer::timeout` 绑定 `LiveWindow::updateFrameFromStitcher`，每隔固定时间读取frame，将其push进listImage，并展示。注：现在已整合stich代码，请用 `>100` 的摄像头编号来触发从文件读取frame的stich测试。
        - http网址：从网络流中获取frame，然后展示。之所以在这有这一类别是因为本工程代码是拷贝自接收端，保留在此处以备一些测试使用。
        - 文件地址：使用opencv库打开文件，并将 `Timer::timeout` 绑定 `LiveWindow::updateFrame`，每隔固定时间读取frame，将其push进listImage，并展示。
* encoder.cpp
    * 该程序继承自QThread，负责取出 listImage 中的frame进行编码，然后推流到远端。
    * 有两个编译开关，`USE_NVENC`为是否使用NVIDIA显卡的nvenc编码加速（确实效果明显），`USE_AV_INTERLEAVE`为是否使用ffmpeg自带的packet传输方式（即是否不使用udp双端口传输方式）。
    * 成员函数 set_filename_Run() 设置推流地址和frame宽高，然后开始运行；
        * 推流地址支持两种协议，一种rtp，另一种是udp，虽然rtp底层也是udp实现的，但是直接使用udp能够降低延迟。这里的协议切换目前还只能在代码上修改。如果使用纯udp推流，则文件头部的编译开关`USE_AV_INTERLEAVE`应该关闭 (你也可以打开试试-:)，并且使用两个端口分别发送packet控制信息和packet像素信息。主要原因是ffmpeg的`av_interleaved_write_frame`比较复杂，内部有很多发送给接收端的控制信息我还没弄清楚，所以现在用adhoc的办法也就是udp双端口方式传输：使用`av_interleaved_write_frame`来让接收端获得正确的ffmpeg流参数，然后用 `udp_send_packet` 来传输纯packet数据。
    * 成员函数 encode_and_push() 为 run() 的主体函数，负责编码和推流。
        * 主体为一个while循环，while循环之前的代码为ffmpeg推流所需的初始化工作（这一部分比较恶心，带有AV开头类型的变量如果没有十足把握请不要改动）。while循环中首先是取frame（为了降低延迟，listImage中堆积的frame将被跳过）。然后是调用encode函数进行编码和推流，encode内部调用ffmpeg的avcodec_send_frame将frame送入ffmpeg内部的编码缓冲区，然后通过avcodec_receive_packet取出ffmpeg编码好的pkt（frame编码之后变成packet）。拿到pkt之后通过udp或者rtp推流。这里面如果使用udp则会间歇在端口1使用`av_interleaved_write_frame` 让ffmepg发送带有控制信息的packet，在端口2使用`udp_send_packet` 发送纯packet数据。
        * 在livewindow中decoded_over（实际上把名字改成encoded_over更准确，也是因为代码拷贝的历史原因保留了这个名字）被置为true之后或被按下交互按钮之后，while循环将结束，然后调用encode发送NULL frame使其编码完所有frame并吐出、发送对应的packet。调用`av_write_trailer`让ffmpeg发送结束语，这一句可能在纯udp的时候也需要被执行，所以这里的 #ifdef 可能需要删除。后面则是释放缓冲区。
    * 静态函数 udp_send_packet() 使用udp双端口方式时发送packet的函数
        * 首先申请数组head_field，写入packet原本就有的头部信息，然后和packet的data字段（即像素信息字段）拼接为QByteArray，通过QUdpSocket::writeDatagram发送出去。在头部信息的pts字段我们藏了该frame从文件中取出来的时间戳，这个时间戳在client中被用来计算整个流程的延时，而如果使用ffmpeg自带的`av_interleaved_write_frame`，client那边使用`av_read_frame`读出来的packet的pts其实会被overwrite，丢失了时间信息。
    * 整个代码中存在为了评估延迟所写的时间统计代码，比较好理解就不啰嗦了。
* radialstitcher.cpp
    * 该程序是由新疆大学的萨尼同学写的，github地址为：https://github.com/Eksanf/video_stitching ，出于对内存泄漏的恐惧，改了几个地方。
    * cv::Mat实际上是一个智能指针，可阅读博文做一个简单了解：https://fzheng.me/2016/01/14/opencv-basic-structure/

## Client

该工程是车载摄像头进行直播的接收端。负责从网络地址获取packet，解码并显示到窗口中。

### Structure

- main.cpp
  - 主程序入口，定义 Livewindow 、 Decoder 、 listImage 。
- liveclientwindow.cpp
  - 该程序继承自QWidget，负责从解码好的 listImage 中取出frame，并显示在窗口中。
  - 其对应有一个.ui文件(UI界面的定义文件），界面内主要有一个 graphicsView 控件，一个输入框和一个按钮。View是展示frame的控件，输入框填入video源，可以是udp或者rtsp/rtp，程序将自动检测是哪一类。点击按钮则开始或结束推流。
    - UDP地址：即使用纯udp进行直播。该方式将初始化一个QUdpSocket实例rcver，绑定的端口为输入的端口+1，并将`QUdpSocket::readyRead`与 `LiveClientWindow::processPendingDatagram`绑定使得每接收到一个udp报文就使用后者进行拆包，然后追加到 待解码链表 pktl末尾。
    - rtp/rtsp地址：使用rtp或rtsp进行直播。
    - 需要说明的上面这两者都将由decoder在给定的端口打开一个ffmpeg的输入流。针对udp则是利用该流获得控制信息，针对rtp则是利用该流同时获得控制信息和数据信息。之后将 `Timer::timeout` 绑定 `LiveClientWindow::updateFrame`，使得本窗口定时从 listImage 中取出frame，显示在窗口中。
  - 成员函数processPendingDatagram() ，负责利用QUdpSocket获得packet，拆包，追加到待解码图像的链表 pktl 末尾。
- decoder.cpp
  - 该程序继承自 QThread，负责取出待解码图像链表中的packet，解码获得frame放入listImage。
  - 有两个编译开关，`USE_H264_CUVID`为是否使用NVIDIA显卡的cuvid进行解码加速（实测好像反倒更慢），`USE_AV_READ_FRAME`为是否使用rtp/rtsp传输图像信息（即是否不使用udp双端口方式传输）。
  - 成员函数 set_filename_Run() 设置拉流地址，然后开始 run()，run的主体函数是decode_iplimage()
  - 成员函数 decode_iplimage()，负责解码
    - 初始化ffmpeg输入流。如果使用的是rtp，则rtp的地址应该指向一个本机的sdp文件，sdp文件包含了这个流的大部分信息（sdp如何生成可见Server的encoder.cpp末尾，生成后拷贝到client机器）。如果是udp则比较简单，使用`avformat_open_input`直接打开即可。然后`avformat_find_stream_info`将读取来自server的第一帧packet，获得其中更详细的流信息（因此Server中udp方式`av_interleaved_write_frame` 发送的帧只有第一个会用到，其他的都丢弃了）
    - 使用while循环从链表头部取packet（udp方式）或使用`av_read_frame`（其他方式）获得packet，调用decode_packet解码，放入 listImage 队列等待显示。
    - while循环被liveclientwindow中的按钮结束或者遇到输入流结尾时退出，然后再循环调用decode_packet 榨干 ffmpeg 内部的解码缓冲区。
  - 静态函数 decode() 和 decode_packet()，前者是输入流中只有视频频道的解码方式，后者是输入流中既有视频频道又有音频频道的解码方式。音频传输现在还不支持（可能会比较麻烦）。
  - 整个代码中存在为了评估延迟所写的时间统计代码，比较好理解就不啰嗦了。
