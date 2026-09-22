#ifndef BILINEAR_SCALER_HPP
#define BILINEAR_SCALER_HPP

#include "IScaler.hpp"

/**
 * @brief Клас BilinearScaler реалізує білінійне масштабування окремого блоку зображення
 * із підтримкою апаратної векторизації SIMD (AVX2 + FMA).
 */
class BilinearScaler : public IScaler {
private:
    bool m_enableSharpen = false;
    bool m_enableOverlap = false;
    bool m_enableSIMD = true;

public:
    /**
     * @brief Масштабує окремий прямокутний блок зображення blockRect за допомогою білінійної інтерполяції.
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
     * @brief Повертає значення прапорця підвищення різкості.
     */
    bool isSharpenEnabled() const { return m_enableSharpen; }

    /**
     * @brief Повертає значення прапорця усунення межових швів.
     */
    bool isOverlapEnabled() const { return m_enableOverlap; }

    /**
     * @brief Повертає стан прапорця векторизації SIMD AVX2.
     */
    bool isSIMDEnabled() const { return m_enableSIMD; }
};

#endif // BILINEAR_SCALER_HPP
