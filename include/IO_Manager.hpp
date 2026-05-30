#ifndef IO_MANAGER_HPP
#define IO_MANAGER_HPP

#include <string>
#include <opencv2/opencv.hpp>
#include <QImage>

/**
 * @brief Клас IO_Manager забезпечує функції введення-виведення для роботи із зображеннями.
 */
class IO_Manager {
public:
    /**
     * @brief Зчитує зображення з диска у матрицю cv::Mat.
     */
    static cv::Mat LoadImage(const std::string& filePath);

    /**
     * @brief Записує матрицю cv::Mat як зображення на диск.
     */
    static bool SaveImage(const std::string& filePath, const cv::Mat& image);

    /**
     * @brief Конвертує матрицю cv::Mat в об'єкт QImage для Qt GUI.
     */
    static QImage MatToQImage(const cv::Mat& mat);
};

#endif // IO_MANAGER_HPP
