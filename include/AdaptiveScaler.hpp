#ifndef ADAPTIVE_SCALER_HPP
#define ADAPTIVE_SCALER_HPP

#include "IScaler.hpp"

/**
 * @brief Клас AdaptiveScaler реалізує адаптивне (Edge-Directed) масштабування:
 * динамічно обирає між Lanczos-3 (на краях та деталях) та Bicubic (на гладких ділянках)
 * на основі локального аналізу градієнтів за допомогою оператора Собеля.
 */
class AdaptiveScaler : public IScaler {
private:
    bool m_enableSharpen = false;
    bool m_enableOverlap = false;
    double m_gradientThreshold = 25.0;

    static inline double cubicWeight(double x) {
        x = std::abs(x);
        constexpr double a = -0.5;
        if (x <= 1.0) {
            return (a + 2.0) * x * x * x - (a + 3.0) * x * x + 1.0;
        } else if (x < 2.0) {
            return a * x * x * x - 5.0 * a * x * x + 8.0 * a * x - 4.0 * a;
        }
        return 0.0;
    }

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
     * @brief Масштабує прямокутний блок зображення за допомогою адаптивного методу.
     */
    void ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) override;

    /**
     * @brief Встановлює поріг чутливості виявлення градієнта Собеля.
     */
    void setGradientThreshold(double threshold) { m_gradientThreshold = threshold; }

    /**
     * @brief Повертає поточний поріг градієнта.
     */
    double gradientThreshold() const { return m_gradientThreshold; }

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

#endif // ADAPTIVE_SCALER_HPP
