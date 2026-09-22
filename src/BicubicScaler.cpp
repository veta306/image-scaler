#include "BicubicScaler.hpp"
#include <algorithm>
#include <cmath>

/**
 * @brief Виконує бікубічне масштабування прямокутного блоку зображення з підтримкою фільтра різкості та оверлапу.
 */
void BicubicScaler::ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) {
    if (input.empty() || output.empty()) {
        return;
    }

    const int inCols = input.cols;
    const int inRows = input.rows;
    const int channels = input.channels();

    const int pad = m_enableOverlap ? 2 : 0;

    const int x_start = std::max(0, blockRect.x - pad);
    const int y_start = std::max(0, blockRect.y - pad);
    const int x_end = std::min(output.cols, blockRect.x + blockRect.width + pad);
    const int y_end = std::min(output.rows, blockRect.y + blockRect.height + pad);

    const int tempW = x_end - x_start;
    const int tempH = y_end - y_start;

    if (tempW <= 0 || tempH <= 0) {
        return;
    }

    cv::Mat tempBlock = cv::Mat::zeros(tempH, tempW, output.type());

    for (int y_local = 0; y_local < tempH; ++y_local) {
        const int y_global = y_start + y_local;
        const double y_in = (y_global + 0.5) / scaleY - 0.5;
        const int y0 = static_cast<int>(std::floor(y_in));
        const double dy = y_in - y0;

        double wy[4];
        wy[0] = cubicWeight(1.0 + dy);
        wy[1] = cubicWeight(dy);
        wy[2] = cubicWeight(1.0 - dy);
        wy[3] = cubicWeight(2.0 - dy);

        double sumWy = wy[0] + wy[1] + wy[2] + wy[3];
        if (std::abs(sumWy) > 1e-6) {
            wy[0] /= sumWy; wy[1] /= sumWy; wy[2] /= sumWy; wy[3] /= sumWy;
        }

        int clampedY[4];
        for (int k = 0; k < 4; ++k) {
            clampedY[k] = std::max(0, std::min(inRows - 1, y0 - 1 + k));
        }

        for (int x_local = 0; x_local < tempW; ++x_local) {
            const int x_global = x_start + x_local;
            const double x_in = (x_global + 0.5) / scaleX - 0.5;
            const int x0 = static_cast<int>(std::floor(x_in));
            const double dx = x_in - x0;

            double wx[4];
            wx[0] = cubicWeight(1.0 + dx);
            wx[1] = cubicWeight(dx);
            wx[2] = cubicWeight(1.0 - dx);
            wx[3] = cubicWeight(2.0 - dx);

            double sumWx = wx[0] + wx[1] + wx[2] + wx[3];
            if (std::abs(sumWx) > 1e-6) {
                wx[0] /= sumWx; wx[1] /= sumWx; wx[2] /= sumWx; wx[3] /= sumWx;
            }

            int clampedX[4];
            for (int k = 0; k < 4; ++k) {
                clampedX[k] = std::max(0, std::min(inCols - 1, x0 - 1 + k));
            }

            if (channels == 3) {
                double accumB = 0.0;
                double accumG = 0.0;
                double accumR = 0.0;

                for (int j = 0; j < 4; ++j) {
                    const cv::Vec3b* rowPtr = input.ptr<cv::Vec3b>(clampedY[j]);
                    double rowB = 0.0, rowG = 0.0, rowR = 0.0;
                    for (int i = 0; i < 4; ++i) {
                        const cv::Vec3b& pix = rowPtr[clampedX[i]];
                        rowB += wx[i] * pix[0];
                        rowG += wx[i] * pix[1];
                        rowR += wx[i] * pix[2];
                    }
                    accumB += wy[j] * rowB;
                    accumG += wy[j] * rowG;
                    accumR += wy[j] * rowR;
                }

                cv::Vec3b& outPix = tempBlock.at<cv::Vec3b>(y_local, x_local);
                outPix[0] = static_cast<uchar>(std::max(0.0, std::min(255.0, accumB)));
                outPix[1] = static_cast<uchar>(std::max(0.0, std::min(255.0, accumG)));
                outPix[2] = static_cast<uchar>(std::max(0.0, std::min(255.0, accumR)));
            } else if (channels == 1) {
                double accum = 0.0;
                for (int j = 0; j < 4; ++j) {
                    const uchar* rowPtr = input.ptr<uchar>(clampedY[j]);
                    double rowVal = 0.0;
                    for (int i = 0; i < 4; ++i) {
                        rowVal += wx[i] * rowPtr[clampedX[i]];
                    }
                    accum += wy[j] * rowVal;
                }
                tempBlock.at<uchar>(y_local, x_local) = static_cast<uchar>(std::max(0.0, std::min(255.0, accum)));
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

    const int localX = blockRect.x - x_start;
    const int localY = blockRect.y - y_start;
    const cv::Rect localRect(localX, localY, blockRect.width, blockRect.height);

    tempBlock(localRect).copyTo(output(blockRect));
}
