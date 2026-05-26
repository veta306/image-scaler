#ifndef PARALLEL_ENGINE_HPP
#define PARALLEL_ENGINE_HPP

#include <opencv2/opencv.hpp>
#include <vector>
#include "IScaler.hpp"

class ParallelEngine {
public:
    /**
     * @brief Розраховує сітку: ділить прямокутник вихідного зображення на блоки.
     */
    static std::vector<cv::Rect> GenerateGrid(int outWidth, int outHeight, int blockW, int blockH);

    /**
     * @brief Виконує паралельне (або послідовне) масштабування зображення.
     * @param input Вхідна матриця пікселів (оригінал).
     * @param output Вихідна матриця пікселів (результат, виділена заздалегідь).
     * @param blocks Сітка блоків у системі координат вихідного зображення.
     * @param scaler Об'єкт конкретного алгоритму масштабування.
     * @param scaleX Масштаб по X.
     * @param scaleY Масштаб по Y.
     * @param numThreads Кількість потоків для OpenMP (1 - один потік).
     */
    static void ScaleImage(const cv::Mat& input, cv::Mat& output, const std::vector<cv::Rect>& blocks,
                           IScaler& scaler, double scaleX, double scaleY, int numThreads);
};

#endif // PARALLEL_ENGINE_HPP
