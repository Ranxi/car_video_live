#ifndef RADIALSTITCHER_H
#define RADIALSTITCHER_H

#include <vector>
#include <opencv2/opencv.hpp>

////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    cv::Point2f left_top;
    cv::Point2f left_bottom;
    cv::Point2f right_top;
    cv::Point2f right_bottom;
}four_corners_t;

// Radial Stitcher Class reads in images and stitches them along horizontal
// axis.
// -----------------------------------------------------------------------------
class RadialStitcher {

public:

    RadialStitcher(int numImages);
    ~RadialStitcher();

    // Main stitching process
    cv::Mat Stitch(std::vector<cv::Mat> imgs);
    std::vector<cv::Mat> cylinder_projection_map(double width, double  height, double focal);

private:

    // Stitcher Parameters
    int numImages;
    double focalLength;
    four_corners_t corners;

    // Images and Masks
    std::vector<cv::Mat> src_list; // Stores input images

    // Feature point information, rewritten over course of stitching
    std::vector<cv::KeyPoint> keypoints1; // curr image keypoints
    std::vector<cv::KeyPoint> keypoints2; // neighbor image keypoints
    std::vector<cv::DMatch> matches;      // feature pairs

    // Auxiliary functions
    cv::Mat merge_image(cv::Mat img1, cv::Mat  img2, cv::Point shift);
    int optimizeSeam(cv::Mat &img1, cv::Mat &img2, cv::Mat& dst);
    int calcCorners(cv::Mat& H, cv::Mat& src);

};

#endif // RADIALSTITCHER_H
