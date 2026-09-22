#ifndef BICUBIC_SCALER_HPP
#define BICUBIC_SCALER_HPP

#include "IScaler.hpp"

/**
 * @brief Клас BicubicScaler реалізує бікубічне масштабування зображення на базі кубічної згортки 4x4 (Catmull-Rom)
 * із підтримкою апаратної векторизації SIMD AVX2.
 */
class BicubicScaler : public IScaler {
private:
    bool m_enableSharpen = false;
    bool m_enableOverlap = false;
    bool m_enableSIMD = true;

    static inline double cubicWeight(double x) {
        x = std::abs(x);
        constexpr double a = -0.5; // Catmull-Rom / Keys parameter
        if (x <= 1.0) {
            return (a + 2.0) * x * x * x - (a + 3.0) * x * x + 1.0;
        } else if (x < 2.0) {
            return a * x * x * x - 5.0 * a * x * x + 8.0 * a * x - 4.0 * a;
        }
        return 0.0;
    }

public:
    /**
     * @brief Масштабує прямокутний блок зображення за допомогою бікубічної інтерполяції.
     */
    void ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) override;

    /**
     * @brief Вмикає або вимикає фільтр підвищення різкості (Unsharp Masking).
     */
    void setEnableSharpen(bool enable) { m_enableSharpen = enable; }

    /**
     * @brief Вмикає або вимикає усунення межових швів (Overlap Padding).
     */
    void setEnableOverlap(bool enable) { m_enableOverlap = enable; }

    /**
     * @brief Вмикає або вимикає векторне прискорення SIMD AVX2.
     */
    void setEnableSIMD(bool enable) { m_enableSIMD = enable; }

    /**
     * @brief Повертає стан прапорця підвищення різкості.
     */
    bool isSharpenEnabled() const { return m_enableSharpen; }

    /**
     * @brief Повертає стан прапорця усунення межових швів.
     */
    bool isOverlapEnabled() const { return m_enableOverlap; }

    /**
     * @brief Повертає стан прапорця векторизації SIMD AVX2.
     */
    bool isSIMDEnabled() const { return m_enableSIMD; }
};

#endif // BICUBIC_SCALER_HPP
