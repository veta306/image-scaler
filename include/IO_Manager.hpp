#ifndef IO_MANAGER_HPP
#define IO_MANAGER_HPP

#include <string>
#include <opencv2/opencv.hpp>
#include <QImage>

class IO_Manager {
public:
    // Read image from disk
    static cv::Mat LoadImage(const std::string& filePath);

    // Save image to disk
    static bool SaveImage(const std::string& filePath, const cv::Mat& image);

    // Convert cv::Mat to QImage safely by cloning pixels
    static QImage MatToQImage(const cv::Mat& mat);
};

#endif // IO_MANAGER_HPP
