#ifndef TEXTURE_ANALYZER_HPP
#define TEXTURE_ANALYZER_HPP

#include <opencv2/opencv.hpp>

/**
 * @brief Клас TextureAnalyzer виконує просторово-частотний аналіз зображень:
 * розраховує карти градієнтів, локальної дисперсії текстур та високочастотних контурів
 * із генерацією теплових карт (Heatmap) та накладанням на оригінал.
 */
class TextureAnalyzer {
public:
    enum class MapType {
        SobelGradient = 0,    // Енергія градієнтів Собеля (контури та границі)
        LocalVariance = 1,    // Локальна дисперсія текстур (шорсткість)
        LaplacianHighFreq = 2 // Високочастотні контури Лапласа
    };

    enum class ColorMap {
        Turbo = 0,   // Перцептивно точна сучасна палітра Google Turbo
        Jet = 1,     // Класична спектральна палітра Jet
        Inferno = 2, // Палітра Inferno
        Hot = 3,     // Теплова палітра Hot
        Plasma = 4   // Палітра Plasma
    };

    struct TextureStats {
        double smoothPercent{0.0};    // % однорідних / гладких ділянок
        double texturedPercent{0.0};  // % помірно текстурованих ділянок
        double edgePercent{0.0};      // % різких контурів та граней
        double meanEnergy{0.0};       // Середня енергія текстури (0..255)
    };

    /**
     * @brief Розраховує теплову карту або напівпрозоре накладання на вхідне зображення.
     */
    static cv::Mat computeHeatmap(const cv::Mat& input,
                                  MapType mapType,
                                  ColorMap colorMap,
                                  double overlayAlpha = 1.0,
                                  TextureStats* statsOut = nullptr);

    /**
     * @brief Перетворює внутрішній тип палітри на ідентифікатор OpenCV.
     */
    static int getCvColorMap(ColorMap colorMap);
};

#endif // TEXTURE_ANALYZER_HPP
