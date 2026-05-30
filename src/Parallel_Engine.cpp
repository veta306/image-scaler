#include "Parallel_Engine.hpp"
#include <omp.h>
#include <algorithm>

/**
 * @brief Розраховує прямокутну сітку блоків для масштабування.
 */
std::vector<cv::Rect> ParallelEngine::GenerateGrid(int outWidth, int outHeight, int blockW, int blockH) {
    std::vector<cv::Rect> blocks;
    if (blockW <= 0 || blockH <= 0 || outWidth <= 0 || outHeight <= 0) {
        return blocks;
    }

    for (int y = 0; y < outHeight; y += blockH) {
        for (int x = 0; x < outWidth; x += blockW) {
            int w = std::min(blockW, outWidth - x);
            int h = std::min(blockH, outHeight - y);
            blocks.push_back(cv::Rect(x, y, w, h));
        }
    }
    return blocks;
}

/**
 * @brief Виконує паралельне або послідовне масштабування сітки блоків.
 */
void ParallelEngine::ScaleImage(const cv::Mat& input, cv::Mat& output, const std::vector<cv::Rect>& blocks,
                               IScaler& scaler, double scaleX, double scaleY, int numThreads) {
    if (input.empty() || output.empty() || blocks.empty()) {
        return;
    }

    #pragma omp parallel for num_threads(numThreads) schedule(dynamic)
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        scaler.ScaleBlock(input, output, blocks[i], scaleX, scaleY);
    }
}
