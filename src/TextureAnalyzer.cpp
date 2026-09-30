#include "TextureAnalyzer.hpp"
#include <algorithm>
#include <cmath>

int TextureAnalyzer::getCvColorMap(ColorMap colorMap) {
    switch (colorMap) {
        case ColorMap::Turbo:   return cv::COLORMAP_TURBO;
        case ColorMap::Jet:     return cv::COLORMAP_JET;
        case ColorMap::Inferno: return cv::COLORMAP_INFERNO;
        case ColorMap::Hot:     return cv::COLORMAP_HOT;
        case ColorMap::Plasma:  return cv::COLORMAP_PLASMA;
        default:                return cv::COLORMAP_TURBO;
    }
}

cv::Mat TextureAnalyzer::computeHeatmap(const cv::Mat& input,
                                        MapType mapType,
                                        ColorMap colorMap,
                                        double overlayAlpha,
                                        TextureStats* statsOut) {
    if (input.empty()) {
        return cv::Mat();
    }

    // 1. Конвертація в градації сірого для аналізу просторових частот
    cv::Mat gray;
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    } else if (input.channels() == 4) {
        cv::cvtColor(input, gray, cv::COLOR_BGRA2GRAY);
    } else {
        gray = input.clone();
    }

    // 2. Обчислення сирої енергії просторових частот відповідно до типу аналізу
    cv::Mat normMap;

    switch (mapType) {
        case MapType::SobelGradient: {
            cv::Mat gradX, gradY;
            cv::Sobel(gray, gradX, CV_32F, 1, 0, 3);
            cv::Sobel(gray, gradY, CV_32F, 0, 1, 3);
            cv::Mat mag;
            cv::magnitude(gradX, gradY, mag);
            cv::normalize(mag, normMap, 0, 255, cv::NORM_MINMAX, CV_8U);
            break;
        }
        case MapType::LocalVariance: {
            cv::Mat grayF;
            gray.convertTo(grayF, CV_32F);
            cv::Mat mean, sqMean;
            cv::boxFilter(grayF, mean, CV_32F, cv::Size(7, 7));
            cv::boxFilter(grayF.mul(grayF), sqMean, CV_32F, cv::Size(7, 7));
            cv::Mat variance = sqMean - mean.mul(mean);
            cv::max(variance, 0.0, variance);
            cv::Mat stdDev;
            cv::sqrt(variance, stdDev);
            cv::normalize(stdDev, normMap, 0, 255, cv::NORM_MINMAX, CV_8U);
            break;
        }
        case MapType::LaplacianHighFreq: {
            cv::Mat lap;
            cv::Laplacian(gray, lap, CV_32F, 3);
            cv::Mat absLap;
            cv::absdiff(lap, cv::Scalar::all(0), absLap);
            cv::normalize(absLap, normMap, 0, 255, cv::NORM_MINMAX, CV_8U);
            break;
        }
    }

    // 3. Розрахунок текстурних статистичних метрик
    if (statsOut != nullptr && !normMap.empty()) {
        const int totalPixels = normMap.rows * normMap.cols;
        if (totalPixels > 0) {
            int smoothCount = 0;
            int texturedCount = 0;
            int edgeCount = 0;
            double sumEnergy = 0.0;

            for (int r = 0; r < normMap.rows; ++r) {
                const uchar* p = normMap.ptr<uchar>(r);
                for (int c = 0; c < normMap.cols; ++c) {
                    const uchar val = p[c];
                    sumEnergy += val;
                    if (val < 45) {
                        smoothCount++;
                    } else if (val < 135) {
                        texturedCount++;
                    } else {
                        edgeCount++;
                    }
                }
            }

            statsOut->smoothPercent = (smoothCount * 100.0) / totalPixels;
            statsOut->texturedPercent = (texturedCount * 100.0) / totalPixels;
            statsOut->edgePercent = (edgeCount * 100.0) / totalPixels;
            statsOut->meanEnergy = sumEnergy / totalPixels;
        }
    }

    // 4. Застосування псевдоколірної палітри
    const int cvCmap = getCvColorMap(colorMap);
    cv::Mat heatmap;
    cv::applyColorMap(normMap, heatmap, cvCmap);

    // 5. Накладання на оригінальне зображення
    if (overlayAlpha >= 0.999) {
        return heatmap;
    }

    cv::Mat base;
    if (input.channels() == 1) {
        cv::cvtColor(input, base, cv::COLOR_GRAY2BGR);
    } else if (input.channels() == 4) {
        cv::cvtColor(input, base, cv::COLOR_BGRA2BGR);
    } else {
        base = input;
    }

    if (base.size() != heatmap.size()) {
        cv::resize(base, base, heatmap.size());
    }

    const double alpha = std::max(0.0, std::min(1.0, overlayAlpha));
    const double beta = 1.0 - alpha;
    cv::Mat result;
    cv::addWeighted(heatmap, alpha, base, beta, 0.0, result);
    return result;
}
