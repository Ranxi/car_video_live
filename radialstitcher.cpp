#include "radialstitcher.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <ctime>

// OpenCV 3.1.0
#include <opencv2/opencv.hpp>

// Radial Stitcher
#include "radialstitcher.h"


////////////////////////////////////////////////////////////////////////////////
// Initialize stitcher parameters and prewarp images

RadialStitcher::RadialStitcher(int numImages){
    // Initialize stitcher parameters...
    this->numImages = numImages;
}


RadialStitcher::~RadialStitcher(){}

// project the image Cylindrical and the focal is define by
// the width of the image.
std::vector<cv::Mat> RadialStitcher::cylinder_projection_map(double width, double  height, double focal) {

    cv::Mat_<float > map_x = cv::Mat::zeros(height, width, CV_16SC1 );
    cv::Mat_<float> map_y = cv::Mat::zeros(height, width, CV_16SC1 );
    ////////////////////////////////////////////////////
    for (int c_y = -int(height / 2); c_y < int(height / 2); c_y++){
        for (int c_x = -int(width / 2); c_x < int(width / 2); c_x++) {
            long double x = focal * tan(c_x / focal);
            long double y = c_y * sqrt(pow(x, 2) + pow(focal, 2)) / focal + height / 2.0;
            x += width / 2.0;
            if (x >= 0 && x < width && y >= 0 && y < height) {
                int location_x = c_y + int(height / 2);
                int location_y =  c_x + int(width / 2);
                map_x.at<float>(location_x, location_y) = x;
                map_y.at<float>(location_x, location_y) = y;
            }
        }
    }
    std::vector<cv::Mat> maps;
    maps.push_back(map_x);
    maps.push_back(map_y);
    return maps;
}


int RadialStitcher::optimizeSeam(cv::Mat &img1, cv::Mat &img2, cv::Mat &dst){

    int start = MIN(corners.left_top.x, corners.left_bottom.x);//开始位置，即重叠区域的左边界
    double processWidth = img1.cols - start;//重叠区域的宽度
    int rows = dst.rows;
    int cols = img1.cols; //注意，是列数*通道数
    double alpha = 1;//img1中像素的权重
    for (int i = 0; i < rows; i++)
    {
        uchar* p = img1.ptr<uchar>(i);  //获取第i行的首地址
        uchar* t = img2.ptr<uchar>(i);
        uchar* d = dst.ptr<uchar>(i);
        for (int j = start; j < cols; j++)
        {
            //如果遇到图像trans中无像素的黑点，则完全拷贝img1中的数据
            if (t[j * 3] == 0 && t[j * 3 + 1] == 0 && t[j * 3 + 2] == 0)
            {
                alpha = 1;
            }
            else
            {
                //img1中像素的权重，与当前处理点距重叠区域左边界的距离成正比，实验证明，这种方法确实好
                alpha = (processWidth - (j - start)) / processWidth;
            }

            d[j * 3] = p[j * 3] * alpha + t[j * 3] * (1 - alpha);
            d[j * 3 + 1] = p[j * 3 + 1] * alpha + t[j * 3 + 1] * (1 - alpha);
            d[j * 3 + 2] = p[j * 3 + 2] * alpha + t[j * 3 + 2] * (1 - alpha);

        }
    }
    return 0;
}

int RadialStitcher::calcCorners(cv::Mat& H,  cv::Mat& src){
    double v2[] = { 0, 0, 1 };//左上角
    double v1[3];//变换后的坐标值
    cv::Mat V2 =cv::Mat(3, 1, CV_64FC1, v2);  //列向量
    cv::Mat V1 =cv::Mat(3, 1, CV_64FC1, v1);  //列向量

    V1 = H * V2;
    //左上角(0,0,1)
//    cout << "V2: " << V2 << endl;
//    cout << "V1: " << V1 << endl;
    corners.left_top.x = v1[0] / v1[2];
    corners.left_top.y = v1[1] / v1[2];

//    左下角(0,src.rows,1)
    v2[0] = 0;
    v2[1] = src.rows;
    v2[2] = 1;
    V2 = cv::Mat(3, 1, CV_64FC1, v2);  //列向量
    V1 = cv::Mat(3, 1, CV_64FC1, v1);  //列向量
    V1 = H * V2;
    corners.left_bottom.x = v1[0] / v1[2];
    corners.left_bottom.y = v1[1] / v1[2];

    //右上角(src.cols,0,1)
    v2[0] = src.cols;
    v2[1] = 0;
    v2[2] = 1;
    V2 = cv::Mat(3, 1, CV_64FC1, v2);  //列向量
    V1 = cv::Mat(3, 1, CV_64FC1, v1);  //列向量
    V1 = H * V2;
    corners.right_top.x = v1[0] / v1[2];
    corners.right_top.y = v1[1] / v1[2];

    //右下角(src.cols,src.rows,1)
    v2[0] = src.cols;
    v2[1] = src.rows;
    v2[2] = 1;
    V2 = cv::Mat(3, 1, CV_64FC1, v2);  //列向量
    V1 = cv::Mat(3, 1, CV_64FC1, v1);  //列向量
    V1 = H * V2;
    corners.right_bottom.x = v1[0] / v1[2];
    corners.right_bottom.y = v1[1] / v1[2];

    return  0;
}

cv::Mat RadialStitcher::merge_image(cv::Mat img1, cv::Mat  img2, cv::Point shift){
    cv::Mat H1 = (cv::Mat_<double>(3,3) << 1, 0, shift.x, 0, 1, shift.y, 0, 0, 1); //偏移量Ｈ
    calcCorners(H1, img2); // 也就是根据Ｈ和img2计算经过偏移后的边界值

    if (shift.y < 0){
        cv::Mat image_transform = cv::Mat(cv::Size(MAX(corners.right_top.x, corners.right_bottom.x), img2.rows), CV_8UC3, cv::Scalar(255, 255, 255)); //创建一个用于移动img2的Mat
        img2.copyTo(image_transform(cv::Rect(shift.x, 0,img2.cols, img2.rows))); //移动img2到image:Transforamtion
        cv::Mat image_transform_img1 = cv::Mat(cv::Size(img1.cols, img1.rows+abs(shift.y)), CV_8UC3, cv::Scalar(255, 255, 255)); //创建一个用于移动img2的Mat
        img1.copyTo(image_transform_img1(cv::Rect(0, abs(shift.y),img1.cols, img1.rows)));
        /////////////////////////////////////////////////////////////////////////////////////////////
        int dst_width =  image_transform.cols;//获得有扩展图的右边界
        int dst_height = image_transform.rows+abs(shift.y);//行数
        cv::Mat dst(dst_height, dst_width, CV_8UC3); //创建一个mat用于存放拼接图像
        dst.setTo(0); //把Mat设为０
        image_transform.copyTo(dst(cv::Rect(0, 0, image_transform.cols, image_transform.rows))); //放入第一张图像
        image_transform_img1.copyTo(dst(cv::Rect(0, 0, image_transform_img1.cols, image_transform_img1.rows))); //放入第二章图像
//        optimizeSeam(image_transform_img1, image_transform, dst); //缝合线优化
        return dst;
    }else{
        cv::Mat imageTransform = cv::Mat(cv::Size(MAX(corners.right_top.x, corners.right_bottom.x), img2.rows+shift.y), CV_8UC3, cv::Scalar(255, 255, 255)); //创建一个用于移动img2的Mat
        img2.copyTo(imageTransform(cv::Rect(shift.x, shift.y,img2.cols, img2.rows))); //移动img2到image:Transforamtion
        /////////////////////////////////////////////////////////////////////////////////////////////
        int dst_width =  imageTransform.cols;//获得有扩展图的右边界
        int dst_height = imageTransform.rows;//行数
        cv::Mat dst(dst_height, dst_width, CV_8UC3); //创建一个mat用于存放拼接图像
        dst.setTo(0); //把Mat设为０
        imageTransform.copyTo(dst(cv::Rect(0, 0, imageTransform.cols, imageTransform.rows))); //放入第一张图像
        img1.copyTo(dst(cv::Rect(0, 0, img1.cols, img1.rows))); //放入第二章图像
//        optimizeSeam(img1, imageTransform, dst); //缝合线优化
        return dst;
    }
}

// Main stitching process
cv::Mat RadialStitcher::Stitch(std::vector<cv::Mat>& imgs){
    std::vector<cv::Point> corners_exposure;
    //计时开始
    clock_t startTime,endTime;
    startTime = clock();
    cv::Mat first, second, third, fourth;
    imgs[0].copyTo(first);
    imgs[1].copyTo(second);
    imgs[2].copyTo(third);
    imgs[3].copyTo(fourth);
    cv::Point shift3_4  = cv::Point(425,10);
    cv::Point shift2_34 = cv::Point(425,-20);
    cv::Point shift1_234 = cv::Point(425,-8);
    cv::Mat dst = merge_image(third, fourth, shift3_4);
            dst = merge_image(second, dst, shift2_34);
            dst = merge_image(first, dst,shift1_234);
    endTime = clock();//计时结束
//    std::cout << "The run time is: " <<(double)(endTime - startTime) / CLOCKS_PER_SEC << "s" << std::endl;
    return dst;
}


//void Exposure_compensator(){
    ////////////////////////////////////////////////////创建曝光补偿器，应用增益补偿方法//////////////////////////////////////////////////////////
//    cv::UMat first_u, second_u, third_u, fourth_u;
//    cv::Mat first, second, third, fourth;
//    imgs[0].copyTo(first_u);
//    imgs[1].copyTo(second_u);
//    imgs[2].copyTo(third_u);
//    imgs[3].copyTo(fourth_u);
//    std::vector<cv::UMat>  compensator_u;
//    compensator_u.push_back(first_u);
//    compensator_u.push_back(second_u);
//    for (int i = 0; i < 2; ++i) {
//        cv::Point point(640, 480);
//        corners_exposure.push_back(point);
//    }
//    cv::Ptr<cv::detail::ExposureCompensator> compensator =cv::detail::ExposureCompensator::createDefault(cv::detail::ExposureCompensator::GAIN);
//    compensator->feed(corners_exposure, compensator_u, compensator_u);    //得到曝光补偿器
//    for(int i=0;i<2;++i)    //应用曝光补偿器，对图像进行曝光补偿
//    {
//        compensator->apply(i, corners_exposure[i], compensator_u[i], compensator_u[i]);
//    }
//    first_u.copyTo(first);
//    second_u.copyTo(second);
    ////////////////////////////////////////////////////曝光补偿完成///////////////////////////////////////////////////////////////////////////
//};

