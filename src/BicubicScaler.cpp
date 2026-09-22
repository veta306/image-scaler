#include "BicubicScaler.hpp"
#include <algorithm>
#include <cmath>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

/**
 * @brief Виконує бікубічне масштабування прямокутного блоку зображення з підтримкою SIMD AVX2, фільтра різкості та оверлапу.
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
            const double inv = 1.0 / sumWy;
            wy[0] *= inv; wy[1] *= inv; wy[2] *= inv; wy[3] *= inv;
        }

        int clampedY[4];
        for (int k = 0; k < 4; ++k) {
            clampedY[k] = std::max(0, std::min(inRows - 1, y0 - 1 + k));
        }

        int x_local = 0;

#if defined(__AVX2__)
        if (m_enableSIMD) {
            const __m256 v_x_idx = _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f);
            const __m256 v_inv_scaleX = _mm256_set1_ps(static_cast<float>(1.0 / scaleX));
            const __m256 v_half = _mm256_set1_ps(0.5f);
            const __m256 v_zero = _mm256_setzero_ps();
            const __m256 v_255 = _mm256_set1_ps(255.0f);

            if (channels == 3) {
                const cv::Vec3b* rPtr[4] = {
                    input.ptr<cv::Vec3b>(clampedY[0]),
                    input.ptr<cv::Vec3b>(clampedY[1]),
                    input.ptr<cv::Vec3b>(clampedY[2]),
                    input.ptr<cv::Vec3b>(clampedY[3])
                };
                cv::Vec3b* outRow = tempBlock.ptr<cv::Vec3b>(y_local);

                for (; x_local <= tempW - 8; x_local += 8) {
                    const int x_global = x_start + x_local;
                    __m256 v_x_global = _mm256_add_ps(_mm256_set1_ps(static_cast<float>(x_global)), v_x_idx);
                    __m256 v_x_in = _mm256_sub_ps(_mm256_mul_ps(_mm256_add_ps(v_x_global, v_half), v_inv_scaleX), v_half);

                    __m256 v_x0_f = _mm256_floor_ps(v_x_in);
                    __m256 v_dx = _mm256_sub_ps(v_x_in, v_x0_f);
                    __m256 v_dx2 = _mm256_mul_ps(v_dx, v_dx);
                    __m256 v_dx3 = _mm256_mul_ps(v_dx2, v_dx);

                    __m256 v_wx0 = _mm256_sub_ps(v_dx2, _mm256_mul_ps(_mm256_set1_ps(0.5f), _mm256_add_ps(v_dx, v_dx3)));
                    __m256 v_wx1 = _mm256_add_ps(_mm256_set1_ps(1.0f), _mm256_sub_ps(_mm256_mul_ps(_mm256_set1_ps(1.5f), v_dx3), _mm256_mul_ps(_mm256_set1_ps(2.5f), v_dx2)));
                    __m256 v_wx2 = _mm256_sub_ps(_mm256_fmadd_ps(_mm256_set1_ps(2.0f), v_dx2, _mm256_mul_ps(_mm256_set1_ps(0.5f), v_dx)), _mm256_mul_ps(_mm256_set1_ps(1.5f), v_dx3));
                    __m256 v_wx3 = _mm256_mul_ps(_mm256_set1_ps(0.5f), _mm256_sub_ps(v_dx3, v_dx2));

                    alignas(32) float x0_arr[8];
                    _mm256_store_ps(x0_arr, v_x0_f);

                    alignas(32) float c0_b[8], c1_b[8], c2_b[8], c3_b[8];
                    alignas(32) float c0_g[8], c1_g[8], c2_g[8], c3_g[8];
                    alignas(32) float c0_r[8], c1_r[8], c2_r[8], c3_r[8];

                    for (int k = 0; k < 8; ++k) {
                        const int x0 = static_cast<int>(x0_arr[k]);
                        const int cx[4] = {
                            std::max(0, std::min(inCols - 1, x0 - 1)),
                            std::max(0, std::min(inCols - 1, x0)),
                            std::max(0, std::min(inCols - 1, x0 + 1)),
                            std::max(0, std::min(inCols - 1, x0 + 2))
                        };

                        for (int i = 0; i < 4; ++i) {
                            const cv::Vec3b& p0 = rPtr[0][cx[i]];
                            const cv::Vec3b& p1 = rPtr[1][cx[i]];
                            const cv::Vec3b& p2 = rPtr[2][cx[i]];
                            const cv::Vec3b& p3 = rPtr[3][cx[i]];

                            float b = static_cast<float>(wy[0] * p0[0] + wy[1] * p1[0] + wy[2] * p2[0] + wy[3] * p3[0]);
                            float g = static_cast<float>(wy[0] * p0[1] + wy[1] * p1[1] + wy[2] * p2[1] + wy[3] * p3[1]);
                            float r = static_cast<float>(wy[0] * p0[2] + wy[1] * p1[2] + wy[2] * p2[2] + wy[3] * p3[2]);

                            if (i == 0) { c0_b[k] = b; c0_g[k] = g; c0_r[k] = r; }
                            else if (i == 1) { c1_b[k] = b; c1_g[k] = g; c1_r[k] = r; }
                            else if (i == 2) { c2_b[k] = b; c2_g[k] = g; c2_r[k] = r; }
                            else { c3_b[k] = b; c3_g[k] = g; c3_r[k] = r; }
                        }
                    }

                    __m256 vb = _mm256_fmadd_ps(v_wx3, _mm256_load_ps(c3_b), _mm256_fmadd_ps(v_wx2, _mm256_load_ps(c2_b), _mm256_fmadd_ps(v_wx1, _mm256_load_ps(c1_b), _mm256_mul_ps(v_wx0, _mm256_load_ps(c0_b)))));
                    __m256 vg = _mm256_fmadd_ps(v_wx3, _mm256_load_ps(c3_g), _mm256_fmadd_ps(v_wx2, _mm256_load_ps(c2_g), _mm256_fmadd_ps(v_wx1, _mm256_load_ps(c1_g), _mm256_mul_ps(v_wx0, _mm256_load_ps(c0_g)))));
                    __m256 vr = _mm256_fmadd_ps(v_wx3, _mm256_load_ps(c3_r), _mm256_fmadd_ps(v_wx2, _mm256_load_ps(c2_r), _mm256_fmadd_ps(v_wx1, _mm256_load_ps(c1_r), _mm256_mul_ps(v_wx0, _mm256_load_ps(c0_r)))));

                    vb = _mm256_max_ps(v_zero, _mm256_min_ps(v_255, vb));
                    vg = _mm256_max_ps(v_zero, _mm256_min_ps(v_255, vg));
                    vr = _mm256_max_ps(v_zero, _mm256_min_ps(v_255, vr));

                    alignas(32) float outB[8], outG[8], outR[8];
                    _mm256_store_ps(outB, vb);
                    _mm256_store_ps(outG, vg);
                    _mm256_store_ps(outR, vr);

                    for (int k = 0; k < 8; ++k) {
                        outRow[x_local + k] = cv::Vec3b(
                            static_cast<uchar>(outB[k] + 0.5f),
                            static_cast<uchar>(outG[k] + 0.5f),
                            static_cast<uchar>(outR[k] + 0.5f)
                        );
                    }
                }
            } else if (channels == 1) {
                const uchar* rPtr[4] = {
                    input.ptr<uchar>(clampedY[0]),
                    input.ptr<uchar>(clampedY[1]),
                    input.ptr<uchar>(clampedY[2]),
                    input.ptr<uchar>(clampedY[3])
                };
                uchar* outRow = tempBlock.ptr<uchar>(y_local);

                for (; x_local <= tempW - 8; x_local += 8) {
                    const int x_global = x_start + x_local;
                    __m256 v_x_global = _mm256_add_ps(_mm256_set1_ps(static_cast<float>(x_global)), v_x_idx);
                    __m256 v_x_in = _mm256_sub_ps(_mm256_mul_ps(_mm256_add_ps(v_x_global, v_half), v_inv_scaleX), v_half);

                    __m256 v_x0_f = _mm256_floor_ps(v_x_in);
                    __m256 v_dx = _mm256_sub_ps(v_x_in, v_x0_f);
                    __m256 v_dx2 = _mm256_mul_ps(v_dx, v_dx);
                    __m256 v_dx3 = _mm256_mul_ps(v_dx2, v_dx);

                    __m256 v_wx0 = _mm256_sub_ps(v_dx2, _mm256_mul_ps(_mm256_set1_ps(0.5f), _mm256_add_ps(v_dx, v_dx3)));
                    __m256 v_wx1 = _mm256_add_ps(_mm256_set1_ps(1.0f), _mm256_sub_ps(_mm256_mul_ps(_mm256_set1_ps(1.5f), v_dx3), _mm256_mul_ps(_mm256_set1_ps(2.5f), v_dx2)));
                    __m256 v_wx2 = _mm256_sub_ps(_mm256_fmadd_ps(_mm256_set1_ps(2.0f), v_dx2, _mm256_mul_ps(_mm256_set1_ps(0.5f), v_dx)), _mm256_mul_ps(_mm256_set1_ps(1.5f), v_dx3));
                    __m256 v_wx3 = _mm256_mul_ps(_mm256_set1_ps(0.5f), _mm256_sub_ps(v_dx3, v_dx2));

                    alignas(32) float x0_arr[8];
                    _mm256_store_ps(x0_arr, v_x0_f);

                    alignas(32) float c0[8], c1[8], c2[8], c3[8];

                    for (int k = 0; k < 8; ++k) {
                        const int x0 = static_cast<int>(x0_arr[k]);
                        const int cx[4] = {
                            std::max(0, std::min(inCols - 1, x0 - 1)),
                            std::max(0, std::min(inCols - 1, x0)),
                            std::max(0, std::min(inCols - 1, x0 + 1)),
                            std::max(0, std::min(inCols - 1, x0 + 2))
                        };

                        for (int i = 0; i < 4; ++i) {
                            float val = static_cast<float>(wy[0] * rPtr[0][cx[i]] + wy[1] * rPtr[1][cx[i]] + wy[2] * rPtr[2][cx[i]] + wy[3] * rPtr[3][cx[i]]);
                            if (i == 0) c0[k] = val;
                            else if (i == 1) c1[k] = val;
                            else if (i == 2) c2[k] = val;
                            else c3[k] = val;
                        }
                    }

                    __m256 v_val = _mm256_fmadd_ps(v_wx3, _mm256_load_ps(c3), _mm256_fmadd_ps(v_wx2, _mm256_load_ps(c2), _mm256_fmadd_ps(v_wx1, _mm256_load_ps(c1), _mm256_mul_ps(v_wx0, _mm256_load_ps(c0)))));
                    v_val = _mm256_max_ps(v_zero, _mm256_min_ps(v_255, v_val));

                    alignas(32) float outVal[8];
                    _mm256_store_ps(outVal, v_val);

                    for (int k = 0; k < 8; ++k) {
                        outRow[x_local + k] = static_cast<uchar>(outVal[k] + 0.5f);
                    }
                }
            }
        }
#endif

        // Скалярна обробка залишку або коли SIMD вимкнено
        for (; x_local < tempW; ++x_local) {
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
