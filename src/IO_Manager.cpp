#include "IO_Manager.hpp"
#include <iostream>

/**
 * @brief Зчитує зображення з диска у матрицю cv::Mat.
 */
cv::Mat IO_Manager::LoadImage(const std::string& filePath) {
    return cv::imread(filePath, cv::IMREAD_COLOR);
}

/**
 * @brief Записує матрицю cv::Mat як зображення на диск.
 */
bool IO_Manager::SaveImage(const std::string& filePath, const cv::Mat& image) {
    if (image.empty()) {
        return false;
    }
    return cv::imwrite(filePath, image);
}

/**
 * @brief Конвертує матрицю cv::Mat в об'єкт QImage для Qt GUI.
 */
QImage IO_Manager::MatToQImage(const cv::Mat& mat) {
    if (mat.empty()) {
        return QImage();
    }

    if (mat.type() == CV_8UC3) {
        const uchar* qImageBuffer = (const uchar*)mat.data;
        QImage img(qImageBuffer, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return img.rgbSwapped().copy();
    } else if (mat.type() == CV_8UC4) {
        const uchar* qImageBuffer = (const uchar*)mat.data;
        QImage img(qImageBuffer, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return img.rgbSwapped().copy();
    } else if (mat.type() == CV_8UC1) {
        const uchar* qImageBuffer = (const uchar*)mat.data;
        QImage img(qImageBuffer, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return img.copy();
    }

    std::cerr << "IO_Manager: Unsupported cv::Mat type for QImage conversion!" << std::endl;
    return QImage();
}
