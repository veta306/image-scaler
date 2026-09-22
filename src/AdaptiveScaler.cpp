#include "AdaptiveScaler.hpp"
#include <algorithm>
#include <cmath>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace {
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
 * @brief Виконує адаптивне масштабування прямокутного блоку зображення з підтримкою SIMD AVX2/FMA.
 * Динамічно обирає між Lanczos-3 (на контурах) та Bicubic (на гладких поверхнях) на базі оператора Собеля.
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
        const double dy = y_in - y0;

        // Попередній розрахунок вертикальних коефіцієнтів Lanczos-3 (один раз на рядок)
        double wy_lanczos[6];
        double sumWy_lanczos = 0.0;
        int clampedY_lanczos[6];
        for (int k = 0; k < 6; ++k) {
            const int sy = y0 - 2 + k;
            wy_lanczos[k] = g_lanczosLUT.get(sy - y_in);
            sumWy_lanczos += wy_lanczos[k];
            clampedY_lanczos[k] = std::max(0, std::min(inRows - 1, sy));
        }
        if (std::abs(sumWy_lanczos) > 1e-6) {
            const double inv = 1.0 / sumWy_lanczos;
            for (int k = 0; k < 6; ++k) wy_lanczos[k] *= inv;
        }

        // Попередній розрахунок вертикальних коефіцієнтів Bicubic (один раз на рядок)
        double wy_cubic[4];
        wy_cubic[0] = cubicWeight(1.0 + dy);
        wy_cubic[1] = cubicWeight(dy);
        wy_cubic[2] = cubicWeight(1.0 - dy);
        wy_cubic[3] = cubicWeight(2.0 - dy);
        double sumWy_cubic = wy_cubic[0] + wy_cubic[1] + wy_cubic[2] + wy_cubic[3];
        if (std::abs(sumWy_cubic) > 1e-6) {
            const double inv = 1.0 / sumWy_cubic;
            wy_cubic[0] *= inv; wy_cubic[1] *= inv; wy_cubic[2] *= inv; wy_cubic[3] *= inv;
        }
        int clampedY_cubic[4];
        for (int k = 0; k < 4; ++k) {
            clampedY_cubic[k] = std::max(0, std::min(inRows - 1, y0 - 1 + k));
        }

        int x_local = 0;

#if defined(__AVX2__)
        if (m_enableSIMD) {
            const __m256 v_zero = _mm256_setzero_ps();
            const __m256 v_255 = _mm256_set1_ps(255.0f);
            const __m256 v_thresh = _mm256_set1_ps(static_cast<float>(m_gradientThreshold));

            if (channels == 3) {
                const cv::Vec3b* rLanczos[6] = {
                    input.ptr<cv::Vec3b>(clampedY_lanczos[0]), input.ptr<cv::Vec3b>(clampedY_lanczos[1]),
                    input.ptr<cv::Vec3b>(clampedY_lanczos[2]), input.ptr<cv::Vec3b>(clampedY_lanczos[3]),
                    input.ptr<cv::Vec3b>(clampedY_lanczos[4]), input.ptr<cv::Vec3b>(clampedY_lanczos[5])
                };
                const cv::Vec3b* rCubic[4] = {
                    input.ptr<cv::Vec3b>(clampedY_cubic[0]), input.ptr<cv::Vec3b>(clampedY_cubic[1]),
                    input.ptr<cv::Vec3b>(clampedY_cubic[2]), input.ptr<cv::Vec3b>(clampedY_cubic[3])
                };

                cv::Vec3b* outRow = tempBlock.ptr<cv::Vec3b>(y_local);

                for (; x_local <= tempW - 8; x_local += 8) {
                    alignas(32) float gradArr[8];

                    // Розрахунок градієнтів Собеля для 8 пікселів
                    for (int k = 0; k < 8; ++k) {
                        const int x_global = x_start + x_local + k;
                        const double x_in = (x_global + 0.5) / scaleX - 0.5;
                        const int x0 = static_cast<int>(std::floor(x_in));

                        double lum[3][3];
                        for (int sdy = -1; sdy <= 1; ++sdy) {
                            const int cy = std::max(0, std::min(inRows - 1, y0 + sdy));
                            const cv::Vec3b* sRow = input.ptr<cv::Vec3b>(cy);
                            for (int sdx = -1; sdx <= 1; ++sdx) {
                                const int cx = std::max(0, std::min(inCols - 1, x0 + sdx));
                                const cv::Vec3b& p = sRow[cx];
                                lum[sdy + 1][sdx + 1] = 0.299 * p[2] + 0.587 * p[1] + 0.114 * p[0];
                            }
                        }

                        const double gx = (lum[0][2] + 2.0 * lum[1][2] + lum[2][2]) - (lum[0][0] + 2.0 * lum[1][0] + lum[2][0]);
                        const double gy = (lum[2][0] + 2.0 * lum[2][1] + lum[2][2]) - (lum[0][0] + 2.0 * lum[0][1] + lum[0][2]);
                        gradArr[k] = static_cast<float>(std::sqrt(gx * gx + gy * gy));
                    }

                    __m256 v_grad = _mm256_load_ps(gradArr);
                    __m256 v_mask = _mm256_cmp_ps(v_grad, v_thresh, _CMP_GE_OQ);
                    int maskBits = _mm256_movemask_ps(v_mask);

                    alignas(32) float outB[8], outG[8], outR[8];

                    if (maskBits == 0) {
                        // Всі 8 пікселів гладкі -> Чистий швидкий AVX2 Bicubic
                        alignas(32) float c_b[4][8], c_g[4][8], c_r[4][8], w_arr[4][8];
                        for (int k = 0; k < 8; ++k) {
                            const int x_global = x_start + x_local + k;
                            const double x_in = (x_global + 0.5) / scaleX - 0.5;
                            const int x0 = static_cast<int>(std::floor(x_in));
                            const double cdx = x_in - x0;

                            double wx[4] = {
                                cubicWeight(1.0 + cdx), cubicWeight(cdx),
                                cubicWeight(1.0 - cdx), cubicWeight(2.0 - cdx)
                            };
                            double sum = wx[0] + wx[1] + wx[2] + wx[3];
                            if (std::abs(sum) > 1e-6) {
                                double inv = 1.0 / sum;
                                wx[0] *= inv; wx[1] *= inv; wx[2] *= inv; wx[3] *= inv;
                            }

                            for (int i = 0; i < 4; ++i) {
                                w_arr[i][k] = static_cast<float>(wx[i]);
                                const int cx = std::max(0, std::min(inCols - 1, x0 - 1 + i));
                                double b = wy_cubic[0] * rCubic[0][cx][0] + wy_cubic[1] * rCubic[1][cx][0]
                                         + wy_cubic[2] * rCubic[2][cx][0] + wy_cubic[3] * rCubic[3][cx][0];
                                double g = wy_cubic[0] * rCubic[0][cx][1] + wy_cubic[1] * rCubic[1][cx][1]
                                         + wy_cubic[2] * rCubic[2][cx][1] + wy_cubic[3] * rCubic[3][cx][1];
                                double r = wy_cubic[0] * rCubic[0][cx][2] + wy_cubic[1] * rCubic[1][cx][2]
                                         + wy_cubic[2] * rCubic[2][cx][2] + wy_cubic[3] * rCubic[3][cx][2];
                                c_b[i][k] = static_cast<float>(b);
                                c_g[i][k] = static_cast<float>(g);
                                c_r[i][k] = static_cast<float>(r);
                            }
                        }

                        __m256 vb = _mm256_mul_ps(_mm256_load_ps(w_arr[0]), _mm256_load_ps(c_b[0]));
                        __m256 vg = _mm256_mul_ps(_mm256_load_ps(w_arr[0]), _mm256_load_ps(c_g[0]));
                        __m256 vr = _mm256_mul_ps(_mm256_load_ps(w_arr[0]), _mm256_load_ps(c_r[0]));
                        for (int i = 1; i < 4; ++i) {
                            __m256 wi = _mm256_load_ps(w_arr[i]);
                            vb = _mm256_fmadd_ps(wi, _mm256_load_ps(c_b[i]), vb);
                            vg = _mm256_fmadd_ps(wi, _mm256_load_ps(c_g[i]), vg);
                            vr = _mm256_fmadd_ps(wi, _mm256_load_ps(c_r[i]), vr);
                        }
                        _mm256_store_ps(outB, _mm256_max_ps(v_zero, _mm256_min_ps(v_255, vb)));
                        _mm256_store_ps(outG, _mm256_max_ps(v_zero, _mm256_min_ps(v_255, vg)));
                        _mm256_store_ps(outR, _mm256_max_ps(v_zero, _mm256_min_ps(v_255, vr)));
                    } else if (maskBits == 0xFF) {
                        // Всі 8 пікселів на контурах -> Чистий швидкий AVX2 Lanczos-3
                        alignas(32) float c_b[6][8], c_g[6][8], c_r[6][8], w_arr[6][8];
                        for (int k = 0; k < 8; ++k) {
                            const int x_global = x_start + x_local + k;
                            const double x_in = (x_global + 0.5) / scaleX - 0.5;
                            const int x0 = static_cast<int>(std::floor(x_in));

                            double wx[6];
                            double sum = 0.0;
                            for (int i = 0; i < 6; ++i) {
                                const int sx = x0 - 2 + i;
                                wx[i] = g_lanczosLUT.get(sx - x_in);
                                sum += wx[i];
                            }
                            if (std::abs(sum) > 1e-6) {
                                double inv = 1.0 / sum;
                                for (int i = 0; i < 6; ++i) wx[i] *= inv;
                            }

                            for (int i = 0; i < 6; ++i) {
                                w_arr[i][k] = static_cast<float>(wx[i]);
                                const int cx = std::max(0, std::min(inCols - 1, x0 - 2 + i));
                                double b = wy_lanczos[0] * rLanczos[0][cx][0] + wy_lanczos[1] * rLanczos[1][cx][0] + wy_lanczos[2] * rLanczos[2][cx][0]
                                         + wy_lanczos[3] * rLanczos[3][cx][0] + wy_lanczos[4] * rLanczos[4][cx][0] + wy_lanczos[5] * rLanczos[5][cx][0];
                                double g = wy_lanczos[0] * rLanczos[0][cx][1] + wy_lanczos[1] * rLanczos[1][cx][1] + wy_lanczos[2] * rLanczos[2][cx][1]
                                         + wy_lanczos[3] * rLanczos[3][cx][1] + wy_lanczos[4] * rLanczos[4][cx][1] + wy_lanczos[5] * rLanczos[5][cx][1];
                                double r = wy_lanczos[0] * rLanczos[0][cx][2] + wy_lanczos[1] * rLanczos[1][cx][2] + wy_lanczos[2] * rLanczos[2][cx][2]
                                         + wy_lanczos[3] * rLanczos[3][cx][2] + wy_lanczos[4] * rLanczos[4][cx][2] + wy_lanczos[5] * rLanczos[5][cx][2];
                                c_b[i][k] = static_cast<float>(b);
                                c_g[i][k] = static_cast<float>(g);
                                c_r[i][k] = static_cast<float>(r);
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
                        _mm256_store_ps(outB, _mm256_max_ps(v_zero, _mm256_min_ps(v_255, vb)));
                        _mm256_store_ps(outG, _mm256_max_ps(v_zero, _mm256_min_ps(v_255, vg)));
                        _mm256_store_ps(outR, _mm256_max_ps(v_zero, _mm256_min_ps(v_255, vr)));
                    } else {
                        // Змішана ділянка: для кожного пікселя обираємо гілку
                        for (int k = 0; k < 8; ++k) {
                            const int x_global = x_start + x_local + k;
                            const double x_in = (x_global + 0.5) / scaleX - 0.5;
                            const int x0 = static_cast<int>(std::floor(x_in));

                            if ((maskBits >> k) & 1) {
                                // Lanczos-3
                                double wx[6];
                                double sum = 0.0;
                                for (int i = 0; i < 6; ++i) {
                                    wx[i] = g_lanczosLUT.get(x0 - 2 + i - x_in);
                                    sum += wx[i];
                                }
                                if (std::abs(sum) > 1e-6) {
                                    double inv = 1.0 / sum;
                                    for (int i = 0; i < 6; ++i) wx[i] *= inv;
                                }
                                double b = 0, g = 0, r = 0;
                                for (int i = 0; i < 6; ++i) {
                                    const int cx = std::max(0, std::min(inCols - 1, x0 - 2 + i));
                                    double colB = wy_lanczos[0] * rLanczos[0][cx][0] + wy_lanczos[1] * rLanczos[1][cx][0] + wy_lanczos[2] * rLanczos[2][cx][0]
                                                + wy_lanczos[3] * rLanczos[3][cx][0] + wy_lanczos[4] * rLanczos[4][cx][0] + wy_lanczos[5] * rLanczos[5][cx][0];
                                    double colG = wy_lanczos[0] * rLanczos[0][cx][1] + wy_lanczos[1] * rLanczos[1][cx][1] + wy_lanczos[2] * rLanczos[2][cx][1]
                                                + wy_lanczos[3] * rLanczos[3][cx][1] + wy_lanczos[4] * rLanczos[4][cx][1] + wy_lanczos[5] * rLanczos[5][cx][1];
                                    double colR = wy_lanczos[0] * rLanczos[0][cx][2] + wy_lanczos[1] * rLanczos[1][cx][2] + wy_lanczos[2] * rLanczos[2][cx][2]
                                                + wy_lanczos[3] * rLanczos[3][cx][2] + wy_lanczos[4] * rLanczos[4][cx][2] + wy_lanczos[5] * rLanczos[5][cx][2];
                                    b += wx[i] * colB;
                                    g += wx[i] * colG;
                                    r += wx[i] * colR;
                                }
                                outB[k] = static_cast<float>(b);
                                outG[k] = static_cast<float>(g);
                                outR[k] = static_cast<float>(r);
                            } else {
                                // Bicubic
                                const double cdx = x_in - x0;
                                double wx[4] = {
                                    cubicWeight(1.0 + cdx), cubicWeight(cdx),
                                    cubicWeight(1.0 - cdx), cubicWeight(2.0 - cdx)
                                };
                                double sum = wx[0] + wx[1] + wx[2] + wx[3];
                                if (std::abs(sum) > 1e-6) {
                                    double inv = 1.0 / sum;
                                    wx[0] *= inv; wx[1] *= inv; wx[2] *= inv; wx[3] *= inv;
                                }
                                double b = 0, g = 0, r = 0;
                                for (int i = 0; i < 4; ++i) {
                                    const int cx = std::max(0, std::min(inCols - 1, x0 - 1 + i));
                                    double colB = wy_cubic[0] * rCubic[0][cx][0] + wy_cubic[1] * rCubic[1][cx][0]
                                                + wy_cubic[2] * rCubic[2][cx][0] + wy_cubic[3] * rCubic[3][cx][0];
                                    double colG = wy_cubic[0] * rCubic[0][cx][1] + wy_cubic[1] * rCubic[1][cx][1]
                                                + wy_cubic[2] * rCubic[2][cx][1] + wy_cubic[3] * rCubic[3][cx][1];
                                    double colR = wy_cubic[0] * rCubic[0][cx][2] + wy_cubic[1] * rCubic[1][cx][2]
                                                + wy_cubic[2] * rCubic[2][cx][2] + wy_cubic[3] * rCubic[3][cx][2];
                                    b += wx[i] * colB;
                                    g += wx[i] * colG;
                                    r += wx[i] * colR;
                                }
                                outB[k] = static_cast<float>(b);
                                outG[k] = static_cast<float>(g);
                                outR[k] = static_cast<float>(r);
                            }
                        }
                    }

                    for (int k = 0; k < 8; ++k) {
                        outRow[x_local + k] = cv::Vec3b(
                            static_cast<uchar>(std::max(0.0f, std::min(255.0f, outB[k] + 0.5f))),
                            static_cast<uchar>(std::max(0.0f, std::min(255.0f, outG[k] + 0.5f))),
                            static_cast<uchar>(std::max(0.0f, std::min(255.0f, outR[k] + 0.5f)))
                        );
                    }
                }
            } else if (channels == 1) {
                const uchar* rLanczos[6] = {
                    input.ptr<uchar>(clampedY_lanczos[0]), input.ptr<uchar>(clampedY_lanczos[1]),
                    input.ptr<uchar>(clampedY_lanczos[2]), input.ptr<uchar>(clampedY_lanczos[3]),
                    input.ptr<uchar>(clampedY_lanczos[4]), input.ptr<uchar>(clampedY_lanczos[5])
                };
                const uchar* rCubic[4] = {
                    input.ptr<uchar>(clampedY_cubic[0]), input.ptr<uchar>(clampedY_cubic[1]),
                    input.ptr<uchar>(clampedY_cubic[2]), input.ptr<uchar>(clampedY_cubic[3])
                };
                uchar* outRow = tempBlock.ptr<uchar>(y_local);

                for (; x_local <= tempW - 8; x_local += 8) {
                    alignas(32) float outVal[8];

                    for (int k = 0; k < 8; ++k) {
                        const int x_global = x_start + x_local + k;
                        const double x_in = (x_global + 0.5) / scaleX - 0.5;
                        const int x0 = static_cast<int>(std::floor(x_in));

                        double lum[3][3];
                        for (int sdy = -1; sdy <= 1; ++sdy) {
                            const int cy = std::max(0, std::min(inRows - 1, y0 + sdy));
                            const uchar* sRow = input.ptr<uchar>(cy);
                            for (int sdx = -1; sdx <= 1; ++sdx) {
                                const int cx = std::max(0, std::min(inCols - 1, x0 + sdx));
                                lum[sdy + 1][sdx + 1] = static_cast<double>(sRow[cx]);
                            }
                        }

                        const double gx = (lum[0][2] + 2.0 * lum[1][2] + lum[2][2]) - (lum[0][0] + 2.0 * lum[1][0] + lum[2][0]);
                        const double gy = (lum[2][0] + 2.0 * lum[2][1] + lum[2][2]) - (lum[0][0] + 2.0 * lum[0][1] + lum[0][2]);
                        const double gradMagnitude = std::sqrt(gx * gx + gy * gy);

                        if (gradMagnitude >= m_gradientThreshold) {
                            double wx[6];
                            double sum = 0.0;
                            for (int i = 0; i < 6; ++i) {
                                wx[i] = g_lanczosLUT.get(x0 - 2 + i - x_in);
                                sum += wx[i];
                            }
                            if (std::abs(sum) > 1e-6) {
                                double inv = 1.0 / sum;
                                for (int i = 0; i < 6; ++i) wx[i] *= inv;
                            }
                            double val = 0.0;
                            for (int i = 0; i < 6; ++i) {
                                const int cx = std::max(0, std::min(inCols - 1, x0 - 2 + i));
                                double colVal = wy_lanczos[0] * rLanczos[0][cx] + wy_lanczos[1] * rLanczos[1][cx] + wy_lanczos[2] * rLanczos[2][cx]
                                              + wy_lanczos[3] * rLanczos[3][cx] + wy_lanczos[4] * rLanczos[4][cx] + wy_lanczos[5] * rLanczos[5][cx];
                                val += wx[i] * colVal;
                            }
                            outVal[k] = static_cast<float>(val);
                        } else {
                            const double cdx = x_in - x0;
                            double wx[4] = {
                                cubicWeight(1.0 + cdx), cubicWeight(cdx),
                                cubicWeight(1.0 - cdx), cubicWeight(2.0 - cdx)
                            };
                            double sum = wx[0] + wx[1] + wx[2] + wx[3];
                            if (std::abs(sum) > 1e-6) {
                                double inv = 1.0 / sum;
                                wx[0] *= inv; wx[1] *= inv; wx[2] *= inv; wx[3] *= inv;
                            }
                            double val = 0.0;
                            for (int i = 0; i < 4; ++i) {
                                const int cx = std::max(0, std::min(inCols - 1, x0 - 1 + i));
                                double colVal = wy_cubic[0] * rCubic[0][cx] + wy_cubic[1] * rCubic[1][cx]
                                              + wy_cubic[2] * rCubic[2][cx] + wy_cubic[3] * rCubic[3][cx];
                                val += wx[i] * colVal;
                            }
                            outVal[k] = static_cast<float>(val);
                        }
                    }

                    for (int k = 0; k < 8; ++k) {
                        outRow[x_local + k] = static_cast<uchar>(std::max(0.0f, std::min(255.0f, outVal[k] + 0.5f)));
                    }
                }
            }
        }
#endif

        // Скалярна обробка залишку рядка або коли SIMD вимкнено
        for (; x_local < tempW; ++x_local) {
            const int x_global = x_start + x_local;
            const double x_in = (x_global + 0.5) / scaleX - 0.5;
            const int x0 = static_cast<int>(std::floor(x_in));

            // Обчислення локального градієнта Собеля 3x3
            double lum[3][3];
            for (int sdy = -1; sdy <= 1; ++sdy) {
                const int cy = std::max(0, std::min(inRows - 1, y0 + sdy));
                for (int sdx = -1; sdx <= 1; ++sdx) {
                    const int cx = std::max(0, std::min(inCols - 1, x0 + sdx));
                    if (channels == 3) {
                        const cv::Vec3b& p = input.at<cv::Vec3b>(cy, cx);
                        lum[sdy + 1][sdx + 1] = 0.299 * p[2] + 0.587 * p[1] + 0.114 * p[0];
                    } else {
                        lum[sdy + 1][sdx + 1] = static_cast<double>(input.at<uchar>(cy, cx));
                    }
                }
            }

            const double gx = (lum[0][2] + 2.0 * lum[1][2] + lum[2][2]) - (lum[0][0] + 2.0 * lum[1][0] + lum[2][0]);
            const double gy = (lum[2][0] + 2.0 * lum[2][1] + lum[2][2]) - (lum[0][0] + 2.0 * lum[0][1] + lum[0][2]);
            const double gradMagnitude = std::sqrt(gx * gx + gy * gy);

            if (gradMagnitude >= m_gradientThreshold) {
                double wx[6];
                double sumWx = 0.0;
                int clampedX[6];
                for (int k = 0; k < 6; ++k) {
                    const int sx = x0 - 2 + k;
                    wx[k] = g_lanczosLUT.get(sx - x_in);
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
                        const cv::Vec3b* rPtr = input.ptr<cv::Vec3b>(clampedY_lanczos[j]);
                        double rb = 0.0, rg = 0.0, rr = 0.0;
                        for (int i = 0; i < 6; ++i) {
                            const cv::Vec3b& px = rPtr[clampedX[i]];
                            rb += wx[i] * px[0];
                            rg += wx[i] * px[1];
                            rr += wx[i] * px[2];
                        }
                        b += wy_lanczos[j] * rb;
                        g += wy_lanczos[j] * rg;
                        r += wy_lanczos[j] * rr;
                    }
                    cv::Vec3b& outPix = tempBlock.at<cv::Vec3b>(y_local, x_local);
                    outPix[0] = static_cast<uchar>(std::max(0.0, std::min(255.0, b)));
                    outPix[1] = static_cast<uchar>(std::max(0.0, std::min(255.0, g)));
                    outPix[2] = static_cast<uchar>(std::max(0.0, std::min(255.0, r)));
                } else {
                    double val = 0.0;
                    for (int j = 0; j < 6; ++j) {
                        const uchar* rPtr = input.ptr<uchar>(clampedY_lanczos[j]);
                        double rowVal = 0.0;
                        for (int i = 0; i < 6; ++i) {
                            rowVal += wx[i] * rPtr[clampedX[i]];
                        }
                        val += wy_lanczos[j] * rowVal;
                    }
                    tempBlock.at<uchar>(y_local, x_local) = static_cast<uchar>(std::max(0.0, std::min(255.0, val)));
                }
            } else {
                const double cdx = x_in - x0;
                double wx[4] = {
                    cubicWeight(1.0 + cdx), cubicWeight(cdx),
                    cubicWeight(1.0 - cdx), cubicWeight(2.0 - cdx)
                };
                double sumWx = wx[0] + wx[1] + wx[2] + wx[3];
                if (std::abs(sumWx) > 1e-6) {
                    const double inv = 1.0 / sumWx;
                    wx[0] *= inv; wx[1] *= inv; wx[2] *= inv; wx[3] *= inv;
                }

                int clampedX[4];
                for (int i = 0; i < 4; ++i) {
                    clampedX[i] = std::max(0, std::min(inCols - 1, x0 - 1 + i));
                }

                if (channels == 3) {
                    double b = 0.0, g = 0.0, r = 0.0;
                    for (int j = 0; j < 4; ++j) {
                        const cv::Vec3b* rPtr = input.ptr<cv::Vec3b>(clampedY_cubic[j]);
                        double rb = 0.0, rg = 0.0, rr = 0.0;
                        for (int i = 0; i < 4; ++i) {
                            const cv::Vec3b& px = rPtr[clampedX[i]];
                            rb += wx[i] * px[0];
                            rg += wx[i] * px[1];
                            rr += wx[i] * px[2];
                        }
                        b += wy_cubic[j] * rb;
                        g += wy_cubic[j] * rg;
                        r += wy_cubic[j] * rr;
                    }
                    cv::Vec3b& outPix = tempBlock.at<cv::Vec3b>(y_local, x_local);
                    outPix[0] = static_cast<uchar>(std::max(0.0, std::min(255.0, b)));
                    outPix[1] = static_cast<uchar>(std::max(0.0, std::min(255.0, g)));
                    outPix[2] = static_cast<uchar>(std::max(0.0, std::min(255.0, r)));
                } else {
                    double val = 0.0;
                    for (int j = 0; j < 4; ++j) {
                        const uchar* rPtr = input.ptr<uchar>(clampedY_cubic[j]);
                        double rowVal = 0.0;
                        for (int i = 0; i < 4; ++i) {
                            rowVal += wx[i] * rPtr[clampedX[i]];
                        }
                        val += wy_cubic[j] * rowVal;
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
