#include "IO_Manager.hpp"
#include <iostream>

cv::Mat IO_Manager::LoadImage(const std::string& filePath) {
    // Read in BGR format
    return cv::imread(filePath, cv::IMREAD_COLOR);
}

bool IO_Manager::SaveImage(const std::string& filePath, const cv::Mat& image) {
    if (image.empty()) {
        return false;
    }
    return cv::imwrite(filePath, image);
}

QImage IO_Manager::MatToQImage(const cv::Mat& mat) {
    if (mat.empty()) {
        return QImage();
    }

    if (mat.type() == CV_8UC3) {
        // OpenCV BGR -> QImage RGB888 -> swap to standard RGB
        const uchar* qImageBuffer = (const uchar*)mat.data;
        QImage img(qImageBuffer, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return img.rgbSwapped().copy(); // copy is essential to avoid dangling pointers
    } else if (mat.type() == CV_8UC4) {
        // OpenCV BGRA -> QImage ARGB32
        const uchar* qImageBuffer = (const uchar*)mat.data;
        QImage img(qImageBuffer, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return img.rgbSwapped().copy();
    } else if (mat.type() == CV_8UC1) {
        // Grayscale
        const uchar* qImageBuffer = (const uchar*)mat.data;
        QImage img(qImageBuffer, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return img.copy();
    }

    std::cerr << "IO_Manager: Unsupported cv::Mat type for QImage conversion!" << std::endl;
    return QImage();
}
