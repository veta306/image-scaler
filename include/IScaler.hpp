#ifndef ISCALER_HPP
#define ISCALER_HPP

#include <opencv2/opencv.hpp>

class IScaler {
public:
    virtual ~IScaler() = default;

    /**
     * @brief Масштабує заданий блок (прямокутник) зображення.
     * @param input Вхідне оригінальне зображення.
     * @param output Вихідне результуюче зображення (пам'ять під яке вже виділено).
     * @param blockRect Координати блоку у ВИХІДНОМУ зображенні, який потрібно обробити.
     * @param scaleX Коефіцієнт масштабування по осі X.
     * @param scaleY Коефіцієнт масштабування по осі Y.
     */
    virtual void ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) = 0;
};

#endif // ISCALER_HPP
