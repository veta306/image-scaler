#include "BilinearScaler.hpp"
#include <algorithm>
#include <cmath>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

/**
 * @brief Виконує масштабування окремого блоку зображення з підтримкою SIMD AVX2, фільтра різкості та оверлапу.
 */
void BilinearScaler::ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) {
    if (input.empty() || output.empty()) {
        return;
    }

    const int inCols = input.cols;
    const int inRows = input.rows;
    const int channels = input.channels();

    const int pad = m_enableOverlap ? 1 : 0;

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
        double y_in = (y_global + 0.5) / scaleY - 0.5;

        if (y_in < 0.0) y_in = 0.0;
        if (y_in >= inRows - 1) y_in = inRows - 1;

        const int y0 = static_cast<int>(y_in);
        const int y1 = (y0 < inRows - 1) ? y0 + 1 : y0;
        const double dy = y_in - y0;
        const double wy0 = 1.0 - dy;
        const double wy1 = dy;

        int x_local = 0;

#if defined(__AVX2__)
        if (m_enableSIMD) {
            const __m256 v_x_idx = _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f);
            const __m256 v_inv_scaleX = _mm256_set1_ps(static_cast<float>(1.0 / scaleX));
            const __m256 v_half = _mm256_set1_ps(0.5f);
            const __m256 v_zero = _mm256_setzero_ps();
            const __m256 v_max_x = _mm256_set1_ps(static_cast<float>(inCols - 1));
            const __m256 v_wy0 = _mm256_set1_ps(static_cast<float>(wy0));
            const __m256 v_wy1 = _mm256_set1_ps(static_cast<float>(wy1));
            const __m256 v_one = _mm256_set1_ps(1.0f);
            const __m256 v_255 = _mm256_set1_ps(255.0f);

            if (channels == 3) {
                const cv::Vec3b* row0 = input.ptr<cv::Vec3b>(y0);
                const cv::Vec3b* row1 = input.ptr<cv::Vec3b>(y1);
                cv::Vec3b* outRow = tempBlock.ptr<cv::Vec3b>(y_local);

                for (; x_local <= tempW - 8; x_local += 8) {
                    const int x_global = x_start + x_local;
                    __m256 v_x_global = _mm256_add_ps(_mm256_set1_ps(static_cast<float>(x_global)), v_x_idx);
                    __m256 v_x_in = _mm256_sub_ps(_mm256_mul_ps(_mm256_add_ps(v_x_global, v_half), v_inv_scaleX), v_half);
                    v_x_in = _mm256_max_ps(v_zero, _mm256_min_ps(v_max_x, v_x_in));

                    __m256 v_x0_f = _mm256_floor_ps(v_x_in);
                    __m256 v_dx = _mm256_sub_ps(v_x_in, v_x0_f);
                    __m256 v_one_minus_dx = _mm256_sub_ps(v_one, v_dx);

                    __m256 v_w00 = _mm256_mul_ps(v_one_minus_dx, v_wy0);
                    __m256 v_w10 = _mm256_mul_ps(v_dx, v_wy0);
                    __m256 v_w01 = _mm256_mul_ps(v_one_minus_dx, v_wy1);
                    __m256 v_w11 = _mm256_mul_ps(v_dx, v_wy1);

                    alignas(32) float x0_floats[8];
                    _mm256_store_ps(x0_floats, v_x0_f);

                    alignas(32) float b00[8], b10[8], b01[8], b11[8];
                    alignas(32) float g00[8], g10[8], g01[8], g11[8];
                    alignas(32) float r00[8], r10[8], r01[8], r11[8];

                    for (int k = 0; k < 8; ++k) {
                        const int ix0 = static_cast<int>(x0_floats[k]);
                        const int ix1 = (ix0 < inCols - 1) ? ix0 + 1 : ix0;

                        const cv::Vec3b& p00 = row0[ix0];
                        const cv::Vec3b& p10 = row0[ix1];
                        const cv::Vec3b& p01 = row1[ix0];
                        const cv::Vec3b& p11 = row1[ix1];

                        b00[k] = p00[0]; b10[k] = p10[0]; b01[k] = p01[0]; b11[k] = p11[0];
                        g00[k] = p00[1]; g10[k] = p10[1]; g01[k] = p01[1]; g11[k] = p11[1];
                        r00[k] = p00[2]; r10[k] = p10[2]; r01[k] = p01[2]; r11[k] = p11[2];
                    }

                    __m256 vb00 = _mm256_load_ps(b00), vb10 = _mm256_load_ps(b10);
                    __m256 vb01 = _mm256_load_ps(b01), vb11 = _mm256_load_ps(b11);

                    __m256 vg00 = _mm256_load_ps(g00), vg10 = _mm256_load_ps(g10);
                    __m256 vg01 = _mm256_load_ps(g01), vg11 = _mm256_load_ps(g11);

                    __m256 vr00 = _mm256_load_ps(r00), vr10 = _mm256_load_ps(r10);
                    __m256 vr01 = _mm256_load_ps(r01), vr11 = _mm256_load_ps(r11);

                    __m256 v_b = _mm256_fmadd_ps(v_w11, vb11, _mm256_fmadd_ps(v_w01, vb01, _mm256_fmadd_ps(v_w10, vb10, _mm256_mul_ps(v_w00, vb00))));
                    __m256 v_g = _mm256_fmadd_ps(v_w11, vg11, _mm256_fmadd_ps(v_w01, vg01, _mm256_fmadd_ps(v_w10, vg10, _mm256_mul_ps(v_w00, vg00))));
                    __m256 v_r = _mm256_fmadd_ps(v_w11, vr11, _mm256_fmadd_ps(v_w01, vr01, _mm256_fmadd_ps(v_w10, vr10, _mm256_mul_ps(v_w00, vr00))));

                    v_b = _mm256_max_ps(v_zero, _mm256_min_ps(v_255, v_b));
                    v_g = _mm256_max_ps(v_zero, _mm256_min_ps(v_255, v_g));
                    v_r = _mm256_max_ps(v_zero, _mm256_min_ps(v_255, v_r));

                    alignas(32) float outB[8], outG[8], outR[8];
                    _mm256_store_ps(outB, v_b);
                    _mm256_store_ps(outG, v_g);
                    _mm256_store_ps(outR, v_r);

                    for (int k = 0; k < 8; ++k) {
                        outRow[x_local + k] = cv::Vec3b(
                            static_cast<uchar>(outB[k] + 0.5f),
                            static_cast<uchar>(outG[k] + 0.5f),
                            static_cast<uchar>(outR[k] + 0.5f)
                        );
                    }
                }
            } else if (channels == 1) {
                const uchar* row0 = input.ptr<uchar>(y0);
                const uchar* row1 = input.ptr<uchar>(y1);
                uchar* outRow = tempBlock.ptr<uchar>(y_local);

                for (; x_local <= tempW - 8; x_local += 8) {
                    const int x_global = x_start + x_local;
                    __m256 v_x_global = _mm256_add_ps(_mm256_set1_ps(static_cast<float>(x_global)), v_x_idx);
                    __m256 v_x_in = _mm256_sub_ps(_mm256_mul_ps(_mm256_add_ps(v_x_global, v_half), v_inv_scaleX), v_half);
                    v_x_in = _mm256_max_ps(v_zero, _mm256_min_ps(v_max_x, v_x_in));

                    __m256 v_x0_f = _mm256_floor_ps(v_x_in);
                    __m256 v_dx = _mm256_sub_ps(v_x_in, v_x0_f);
                    __m256 v_one_minus_dx = _mm256_sub_ps(v_one, v_dx);

                    __m256 v_w00 = _mm256_mul_ps(v_one_minus_dx, v_wy0);
                    __m256 v_w10 = _mm256_mul_ps(v_dx, v_wy0);
                    __m256 v_w01 = _mm256_mul_ps(v_one_minus_dx, v_wy1);
                    __m256 v_w11 = _mm256_mul_ps(v_dx, v_wy1);

                    alignas(32) float x0_floats[8];
                    _mm256_store_ps(x0_floats, v_x0_f);

                    alignas(32) float p00_arr[8], p10_arr[8], p01_arr[8], p11_arr[8];
                    for (int k = 0; k < 8; ++k) {
                        const int ix0 = static_cast<int>(x0_floats[k]);
                        const int ix1 = (ix0 < inCols - 1) ? ix0 + 1 : ix0;
                        p00_arr[k] = row0[ix0];
                        p10_arr[k] = row0[ix1];
                        p01_arr[k] = row1[ix0];
                        p11_arr[k] = row1[ix1];
                    }

                    __m256 vp00 = _mm256_load_ps(p00_arr), vp10 = _mm256_load_ps(p10_arr);
                    __m256 vp01 = _mm256_load_ps(p01_arr), vp11 = _mm256_load_ps(p11_arr);

                    __m256 v_val = _mm256_fmadd_ps(v_w11, vp11, _mm256_fmadd_ps(v_w01, vp01, _mm256_fmadd_ps(v_w10, vp10, _mm256_mul_ps(v_w00, vp00))));
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

        // Скалярна обробка залишку рядка або коли SIMD вимкнено
        for (; x_local < tempW; ++x_local) {
            const int x_global = x_start + x_local;
            double x_in = (x_global + 0.5) / scaleX - 0.5;

            if (x_in < 0.0) x_in = 0.0;
            if (x_in >= inCols - 1) x_in = inCols - 1;

            const int x0 = static_cast<int>(x_in);
            const int x1 = (x0 < inCols - 1) ? x0 + 1 : x0;
            const double dx = x_in - x0;

            const double w00 = (1.0 - dx) * wy0;
            const double w10 = dx * wy0;
            const double w01 = (1.0 - dx) * wy1;
            const double w11 = dx * wy1;

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
                const uchar p00 = input.at<uchar>(y0, x0);
                const uchar p10 = input.at<uchar>(y0, x1);
                const uchar p01 = input.at<uchar>(y1, x0);
                const uchar p11 = input.at<uchar>(y1, x1);

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

    const int localX = blockRect.x - x_start;
    const int localY = blockRect.y - y_start;
    const cv::Rect localRect(localX, localY, blockRect.width, blockRect.height);

    tempBlock(localRect).copyTo(output(blockRect));
}
