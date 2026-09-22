#include "LanczosScaler.hpp"
#include <algorithm>
#include <cmath>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace {
/**
 * @brief Високоточна таблиця пошуку (Lookup Table, LUT) значень функції Ланцоша Lanczos-3
 * із субпіксельною лінійною інтерполяцією для усунення важких викликів тригонометричних функцій sin().
 */
struct LanczosLUT {
    static constexpr int TABLE_SIZE = 2048;
    static constexpr double MAX_X = 3.0;
    float table[TABLE_SIZE + 2];

    LanczosLUT() {
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i <= TABLE_SIZE; ++i) {
            double x = (static_cast<double>(i) / TABLE_SIZE) * MAX_X;
            if (x < 1e-7) {
                table[i] = 1.0f;
            } else if (x >= 3.0) {
                table[i] = 0.0f;
            } else {
                double pix = pi * x;
                table[i] = static_cast<float>((3.0 * std::sin(pix) * std::sin(pix / 3.0)) / (pix * pix));
            }
        }
        table[TABLE_SIZE + 1] = 0.0f;
    }

    inline float get(double x) const {
        x = std::abs(x);
        if (x >= MAX_X) return 0.0f;
        double pos = x * (TABLE_SIZE / MAX_X);
        int idx = static_cast<int>(pos);
        float frac = static_cast<float>(pos - idx);
        return table[idx] * (1.0f - frac) + table[idx + 1] * frac;
    }
};

static const LanczosLUT g_lanczosLUT;
}

/**
 * @brief Виконує масштабування прямокутного блоку зображення методом Lanczos-3 із підтримкою SIMD AVX2 та FMA.
 */
void LanczosScaler::ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) {
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

        float wy[6];
        float sumWy = 0.0f;
        int clampedY[6];

        for (int k = 0; k < 6; ++k) {
            const int sampleY = y0 - 2 + k;
            wy[k] = g_lanczosLUT.get(sampleY - y_in);
            sumWy += wy[k];
            clampedY[k] = std::max(0, std::min(inRows - 1, sampleY));
        }

        if (std::abs(sumWy) > 1e-6f) {
            const float invSum = 1.0f / sumWy;
            for (int k = 0; k < 6; ++k) {
                wy[k] *= invSum;
            }
        }

        int x_local = 0;

#if defined(__AVX2__)
        if (m_enableSIMD) {
            const __m256 v_zero = _mm256_setzero_ps();
            const __m256 v_255 = _mm256_set1_ps(255.0f);

            if (channels == 3) {
                const cv::Vec3b* rPtr[6] = {
                    input.ptr<cv::Vec3b>(clampedY[0]),
                    input.ptr<cv::Vec3b>(clampedY[1]),
                    input.ptr<cv::Vec3b>(clampedY[2]),
                    input.ptr<cv::Vec3b>(clampedY[3]),
                    input.ptr<cv::Vec3b>(clampedY[4]),
                    input.ptr<cv::Vec3b>(clampedY[5])
                };
                cv::Vec3b* outRow = tempBlock.ptr<cv::Vec3b>(y_local);

                for (; x_local <= tempW - 8; x_local += 8) {
                    alignas(32) float c_b[6][8];
                    alignas(32) float c_g[6][8];
                    alignas(32) float c_r[6][8];
                    alignas(32) float w_arr[6][8];

                    for (int k = 0; k < 8; ++k) {
                        const int x_global = x_start + x_local + k;
                        const double x_in = (x_global + 0.5) / scaleX - 0.5;
                        const int x0 = static_cast<int>(std::floor(x_in));

                        float wx[6];
                        float sumWx = 0.0f;
                        int clampedX[6];

                        for (int i = 0; i < 6; ++i) {
                            const int sampleX = x0 - 2 + i;
                            wx[i] = g_lanczosLUT.get(sampleX - x_in);
                            sumWx += wx[i];
                            clampedX[i] = std::max(0, std::min(inCols - 1, sampleX));
                        }

                        if (std::abs(sumWx) > 1e-6f) {
                            const float inv = 1.0f / sumWx;
                            for (int i = 0; i < 6; ++i) wx[i] *= inv;
                        }

                        for (int i = 0; i < 6; ++i) {
                            w_arr[i][k] = wx[i];
                            const int cx = clampedX[i];
                            float colB = wy[0] * rPtr[0][cx][0] + wy[1] * rPtr[1][cx][0] + wy[2] * rPtr[2][cx][0]
                                       + wy[3] * rPtr[3][cx][0] + wy[4] * rPtr[4][cx][0] + wy[5] * rPtr[5][cx][0];
                            float colG = wy[0] * rPtr[0][cx][1] + wy[1] * rPtr[1][cx][1] + wy[2] * rPtr[2][cx][1]
                                       + wy[3] * rPtr[3][cx][1] + wy[4] * rPtr[4][cx][1] + wy[5] * rPtr[5][cx][1];
                            float colR = wy[0] * rPtr[0][cx][2] + wy[1] * rPtr[1][cx][2] + wy[2] * rPtr[2][cx][2]
                                       + wy[3] * rPtr[3][cx][2] + wy[4] * rPtr[4][cx][2] + wy[5] * rPtr[5][cx][2];
                            c_b[i][k] = colB;
                            c_g[i][k] = colG;
                            c_r[i][k] = colR;
                        }
                    }

                    __m256 vb = _mm256_mul_ps(_mm256_load_ps(w_arr[0]), _mm256_load_ps(c_b[0]));
                    __m256 vg = _mm256_mul_ps(_mm256_load_ps(w_arr[0]), _mm256_load_ps(c_g[0]));
                    __m256 vr = _mm256_mul_ps(_mm256_load_ps(w_arr[0]), _mm256_load_ps(c_r[0]));

                    for (int i = 1; i < 6; ++i) {
                        __m256 wi = _mm256_load_ps(w_arr[i]);
                        vb = _mm256_fmadd_ps(wi, _mm256_load_ps(c_b[i]), vb);
                        vg = _mm256_fmadd_ps(wi, _mm256_load_ps(c_g[i]), vg);
                        vr = _mm256_fmadd_ps(wi, _mm256_load_ps(c_r[i]), vr);
                    }

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
                const uchar* rPtr[6] = {
                    input.ptr<uchar>(clampedY[0]),
                    input.ptr<uchar>(clampedY[1]),
                    input.ptr<uchar>(clampedY[2]),
                    input.ptr<uchar>(clampedY[3]),
                    input.ptr<uchar>(clampedY[4]),
                    input.ptr<uchar>(clampedY[5])
                };
                uchar* outRow = tempBlock.ptr<uchar>(y_local);

                for (; x_local <= tempW - 8; x_local += 8) {
                    alignas(32) float c_val[6][8];
                    alignas(32) float w_arr[6][8];

                    for (int k = 0; k < 8; ++k) {
                        const int x_global = x_start + x_local + k;
                        const double x_in = (x_global + 0.5) / scaleX - 0.5;
                        const int x0 = static_cast<int>(std::floor(x_in));

                        float wx[6];
                        float sumWx = 0.0f;
                        int clampedX[6];

                        for (int i = 0; i < 6; ++i) {
                            const int sampleX = x0 - 2 + i;
                            wx[i] = g_lanczosLUT.get(sampleX - x_in);
                            sumWx += wx[i];
                            clampedX[i] = std::max(0, std::min(inCols - 1, sampleX));
                        }

                        if (std::abs(sumWx) > 1e-6f) {
                            const float inv = 1.0f / sumWx;
                            for (int i = 0; i < 6; ++i) wx[i] *= inv;
                        }

                        for (int i = 0; i < 6; ++i) {
                            w_arr[i][k] = wx[i];
                            const int cx = clampedX[i];
                            float colVal = wy[0] * rPtr[0][cx] + wy[1] * rPtr[1][cx] + wy[2] * rPtr[2][cx]
                                         + wy[3] * rPtr[3][cx] + wy[4] * rPtr[4][cx] + wy[5] * rPtr[5][cx];
                            c_val[i][k] = colVal;
                        }
                    }

                    __m256 v_val = _mm256_mul_ps(_mm256_load_ps(w_arr[0]), _mm256_load_ps(c_val[0]));
                    for (int i = 1; i < 6; ++i) {
                        v_val = _mm256_fmadd_ps(_mm256_load_ps(w_arr[i]), _mm256_load_ps(c_val[i]), v_val);
                    }
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

            double wx[6];
            double sumWx = 0.0;
            int clampedX[6];

            for (int k = 0; k < 6; ++k) {
                const int sampleX = x0 - 2 + k;
                wx[k] = lanczosWeight(sampleX - x_in);
                sumWx += wx[k];
                clampedX[k] = std::max(0, std::min(inCols - 1, sampleX));
            }

            if (std::abs(sumWx) > 1e-6) {
                const double invSum = 1.0 / sumWx;
                for (int k = 0; k < 6; ++k) {
                    wx[k] *= invSum;
                }
            }

            if (channels == 3) {
                double accumB = 0.0;
                double accumG = 0.0;
                double accumR = 0.0;

                for (int j = 0; j < 6; ++j) {
                    const cv::Vec3b* rowPtr = input.ptr<cv::Vec3b>(clampedY[j]);
                    double rowB = 0.0, rowG = 0.0, rowR = 0.0;
                    for (int i = 0; i < 6; ++i) {
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
                for (int j = 0; j < 6; ++j) {
                    const uchar* rowPtr = input.ptr<uchar>(clampedY[j]);
                    double rowVal = 0.0;
                    for (int i = 0; i < 6; ++i) {
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
