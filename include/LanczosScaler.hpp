#ifndef LANCZOS_SCALER_HPP
#define LANCZOS_SCALER_HPP

#include "IScaler.hpp"

/**
 * @brief Клас LanczosScaler реалізує ресемплінг Ланцоша (Lanczos-3) з вікном 6x6.
 */
class LanczosScaler : public IScaler {
private:
    bool m_enableSharpen = false;
    bool m_enableOverlap = false;

    static inline double lanczosWeight(double x) {
        x = std::abs(x);
        if (x < 1e-7) {
            return 1.0;
        }
        if (x >= 3.0) {
            return 0.0;
        }
        constexpr double pi = 3.14159265358979323846;
        const double pix = pi * x;
        return (3.0 * std::sin(pix) * std::sin(pix / 3.0)) / (pix * pix);
    }

public:
    /**
     * @brief Масштабує прямокутний блок зображення за допомогою Lanczos-3 інтерполяції.
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
     * @brief Повертає стан прапорця підвищення різкості.
     */
    bool isSharpenEnabled() const { return m_enableSharpen; }

    /**
     * @brief Повертає стан прапорця усунення межових швів.
     */
    bool isOverlapEnabled() const { return m_enableOverlap; }
};

#endif // LANCZOS_SCALER_HPP
