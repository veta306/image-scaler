#include "BilinearScaler.hpp"
#include <algorithm>
#include <cmath>

/**
 * @brief Виконує масштабування окремого блоку зображення з підтримкою фільтра різкості та оверлапу.
 */
void BilinearScaler::ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) {
    if (input.empty() || output.empty()) {
        return;
    }

    int inCols = input.cols;
    int inRows = input.rows;
    int channels = input.channels();

    int pad = m_enableOverlap ? 1 : 0;

    int x_start = std::max(0, blockRect.x - pad);
    int y_start = std::max(0, blockRect.y - pad);
    int x_end = std::min(output.cols, blockRect.x + blockRect.width + pad);
    int y_end = std::min(output.rows, blockRect.y + blockRect.height + pad);

    int tempW = x_end - x_start;
    int tempH = y_end - y_start;

    if (tempW <= 0 || tempH <= 0) {
        return;
    }

    cv::Mat tempBlock = cv::Mat::zeros(tempH, tempW, output.type());

    for (int y_local = 0; y_local < tempH; ++y_local) {
        int y_global = y_start + y_local;
        for (int x_local = 0; x_local < tempW; ++x_local) {
            int x_global = x_start + x_local;

            double x_in = (x_global + 0.5) / scaleX - 0.5;
            double y_in = (y_global + 0.5) / scaleY - 0.5;

            if (x_in < 0) { x_in = 0; }
            if (y_in < 0) { y_in = 0; }
            if (x_in >= inCols - 1) { x_in = inCols - 1; }
            if (y_in >= inRows - 1) { y_in = inRows - 1; }

            int x0 = static_cast<int>(x_in);
            int y0 = static_cast<int>(y_in);
            
            int x1 = (x0 < inCols - 1) ? x0 + 1 : x0;
            int y1 = (y0 < inRows - 1) ? y0 + 1 : y0;

            double dx = x_in - x0;
            double dy = y_in - y0;

            double w00 = (1.0 - dx) * (1.0 - dy);
            double w10 = dx * (1.0 - dy);
            double w01 = (1.0 - dx) * dy;
            double w11 = dx * dy;

            if (channels == 3) {
                const cv::Vec3b& p00 = input.at<cv::Vec3b>(y0, x0);
                const cv::Vec3b& p10 = input.at<cv::Vec3b>(y0, x1);
                const cv::Vec3b& p01 = input.at<cv::Vec3b>(y1, x0);
                const cv::Vec3b& p11 = input.at<cv::Vec3b>(y1, x1);

                cv::Vec3b& outPixel = tempBlock.at<cv::Vec3b>(y_local, x_local);

                for (int c = 0; c < 3; ++c) {
                    double val = w00 * p00[c] + w10 * p10[c] + w01 * p01[c] + w11 * p11[c];
                    outPixel[c] = static_cast<uchar>(std::max(0.0, std::min(val, 255.0)));
                }
            } else if (channels == 1) {
                uchar p00 = input.at<uchar>(y0, x0);
                uchar p10 = input.at<uchar>(y0, x1);
                uchar p01 = input.at<uchar>(y1, x0);
                uchar p11 = input.at<uchar>(y1, x1);

                double val = w00 * p00 + w10 * p10 + w01 * p01 + w11 * p11;
                tempBlock.at<uchar>(y_local, x_local) = static_cast<uchar>(std::max(0.0, std::min(val, 255.0)));
            }
        }
    }

    if (m_enableSharpen) {
        cv::Mat sharpened;
        cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
             0, -1,  0,
            -1,  5, -1,
             0, -1,  0
        );
        cv::filter2D(tempBlock, sharpened, tempBlock.depth(), kernel);
        tempBlock = sharpened;
    }

    int localX = blockRect.x - x_start;
    int localY = blockRect.y - y_start;
    cv::Rect localRect(localX, localY, blockRect.width, blockRect.height);

    tempBlock(localRect).copyTo(output(blockRect));
}
