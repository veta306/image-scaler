#include "AdaptiveScaler.hpp"
#include <algorithm>
#include <cmath>

/**
 * @brief Виконує адаптивне масштабування прямокутного блоку зображення з динамічним вибором Lanczos-3 або Bicubic.
 */
void AdaptiveScaler::ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) {
    if (input.empty() || output.empty()) {
        return;
    }

    const int inCols = input.cols;
    const int inRows = input.rows;
    const int channels = input.channels();

    const int pad = m_enableOverlap ? 3 : 0;

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

        for (int x_local = 0; x_local < tempW; ++x_local) {
            const int x_global = x_start + x_local;
            const double x_in = (x_global + 0.5) / scaleX - 0.5;
            const int x0 = static_cast<int>(std::floor(x_in));

            // Обчислення локального градієнта Собеля 3x3
            double lum[3][3];
            for (int dy = -1; dy <= 1; ++dy) {
                const int cy = std::max(0, std::min(inRows - 1, y0 + dy));
                for (int dx = -1; dx <= 1; ++dx) {
                    const int cx = std::max(0, std::min(inCols - 1, x0 + dx));
                    if (channels == 3) {
                        const cv::Vec3b& p = input.at<cv::Vec3b>(cy, cx);
                        lum[dy + 1][dx + 1] = 0.299 * p[2] + 0.587 * p[1] + 0.114 * p[0];
                    } else {
                        lum[dy + 1][dx + 1] = static_cast<double>(input.at<uchar>(cy, cx));
                    }
                }
            }

            const double gx = (lum[0][2] + 2.0 * lum[1][2] + lum[2][2]) - (lum[0][0] + 2.0 * lum[1][0] + lum[2][0]);
            const double gy = (lum[2][0] + 2.0 * lum[2][1] + lum[2][2]) - (lum[0][0] + 2.0 * lum[0][1] + lum[0][2]);
            const double gradMagnitude = std::sqrt(gx * gx + gy * gy);

            const bool useLanczos = (gradMagnitude >= m_gradientThreshold);

            if (useLanczos) {
                // Lanczos-3 (6x6)
                double wy[6];
                double sumWy = 0.0;
                int clampedY[6];
                for (int k = 0; k < 6; ++k) {
                    const int sy = y0 - 2 + k;
                    wy[k] = lanczosWeight(sy - y_in);
                    sumWy += wy[k];
                    clampedY[k] = std::max(0, std::min(inRows - 1, sy));
                }
                if (std::abs(sumWy) > 1e-6) {
                    const double invSum = 1.0 / sumWy;
                    for (int k = 0; k < 6; ++k) wy[k] *= invSum;
                }

                double wx[6];
                double sumWx = 0.0;
                int clampedX[6];
                for (int k = 0; k < 6; ++k) {
                    const int sx = x0 - 2 + k;
                    wx[k] = lanczosWeight(sx - x_in);
                    sumWx += wx[k];
                    clampedX[k] = std::max(0, std::min(inCols - 1, sx));
                }
                if (std::abs(sumWx) > 1e-6) {
                    const double invSum = 1.0 / sumWx;
                    for (int k = 0; k < 6; ++k) wx[k] *= invSum;
                }

                if (channels == 3) {
                    double b = 0.0, g = 0.0, r = 0.0;
                    for (int j = 0; j < 6; ++j) {
                        const cv::Vec3b* rPtr = input.ptr<cv::Vec3b>(clampedY[j]);
                        double rb = 0.0, rg = 0.0, rr = 0.0;
                        for (int i = 0; i < 6; ++i) {
                            const cv::Vec3b& px = rPtr[clampedX[i]];
                            rb += wx[i] * px[0];
                            rg += wx[i] * px[1];
                            rr += wx[i] * px[2];
                        }
                        b += wy[j] * rb;
                        g += wy[j] * rg;
                        r += wy[j] * rr;
                    }
                    cv::Vec3b& outPix = tempBlock.at<cv::Vec3b>(y_local, x_local);
                    outPix[0] = static_cast<uchar>(std::max(0.0, std::min(255.0, b)));
                    outPix[1] = static_cast<uchar>(std::max(0.0, std::min(255.0, g)));
                    outPix[2] = static_cast<uchar>(std::max(0.0, std::min(255.0, r)));
                } else {
                    double val = 0.0;
                    for (int j = 0; j < 6; ++j) {
                        const uchar* rPtr = input.ptr<uchar>(clampedY[j]);
                        double rowVal = 0.0;
                        for (int i = 0; i < 6; ++i) {
                            rowVal += wx[i] * rPtr[clampedX[i]];
                        }
                        val += wy[j] * rowVal;
                    }
                    tempBlock.at<uchar>(y_local, x_local) = static_cast<uchar>(std::max(0.0, std::min(255.0, val)));
                }
            } else {
                // Bicubic (4x4)
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
                    double b = 0.0, g = 0.0, r = 0.0;
                    for (int j = 0; j < 4; ++j) {
                        const cv::Vec3b* rPtr = input.ptr<cv::Vec3b>(clampedY[j]);
                        double rb = 0.0, rg = 0.0, rr = 0.0;
                        for (int i = 0; i < 4; ++i) {
                            const cv::Vec3b& px = rPtr[clampedX[i]];
                            rb += wx[i] * px[0];
                            rg += wx[i] * px[1];
                            rr += wx[i] * px[2];
                        }
                        b += wy[j] * rb;
                        g += wy[j] * rg;
                        r += wy[j] * rr;
                    }
                    cv::Vec3b& outPix = tempBlock.at<cv::Vec3b>(y_local, x_local);
                    outPix[0] = static_cast<uchar>(std::max(0.0, std::min(255.0, b)));
                    outPix[1] = static_cast<uchar>(std::max(0.0, std::min(255.0, g)));
                    outPix[2] = static_cast<uchar>(std::max(0.0, std::min(255.0, r)));
                } else {
                    double val = 0.0;
                    for (int j = 0; j < 4; ++j) {
                        const uchar* rPtr = input.ptr<uchar>(clampedY[j]);
                        double rowVal = 0.0;
                        for (int i = 0; i < 4; ++i) {
                            rowVal += wx[i] * rPtr[clampedX[i]];
                        }
                        val += wy[j] * rowVal;
                    }
                    tempBlock.at<uchar>(y_local, x_local) = static_cast<uchar>(std::max(0.0, std::min(255.0, val)));
                }
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
