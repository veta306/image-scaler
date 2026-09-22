#ifndef GPUSCALER_HPP
#define GPUSCALER_HPP

#include <opencv2/opencv.hpp>
#include <string>
#if defined(__has_include)
    #if __has_include(<QString>)
        #include <QString>
    #endif
#endif
#include "IScaler.hpp"

/**
 * @brief Клас GpuScaler забезпечує апаратне масштабування зображень та відеокадрів
 * на графічному процесорі (GPU) з використанням OpenCL та конвеєрів cv::UMat.
 */
class GpuScaler : public IScaler {
public:
    enum class Algorithm {
        Bilinear = 0,
        Bicubic = 1,
        Lanczos = 2,
        AdaptiveSobel = 3
    };

    struct Timing {
        double uploadMs{0.0};
        double kernelMs{0.0};
        double downloadMs{0.0};
        double totalMs{0.0};
    };

    GpuScaler();
    ~GpuScaler() override = default;

    static bool isAvailable();
    static std::string getDeviceNameStd();
#ifdef QT_CORE_LIB
    static QString getDeviceName() {
        return QString::fromStdString(getDeviceNameStd());
    }
#endif
    static bool initGpu();

    void setAlgorithm(Algorithm algo) { m_algorithm = algo; }
    Algorithm algorithm() const { return m_algorithm; }

    void setEnableSharpen(bool enable) { m_enableSharpen = enable; }
    bool enableSharpen() const { return m_enableSharpen; }

    /**
     * @brief Повне апаратне масштабування кадру на графічному процесорі.
     */
    static bool scaleFrame(const cv::Mat& input, cv::Mat& output, 
                           int targetWidth, int targetHeight, 
                           Algorithm algo, bool enableSharpen = false, 
                           Timing* timingOut = nullptr);

    /**
     * @brief Реалізація інтерфейсу IScaler для сумісності з блоковою обробкою.
     */
    void ScaleBlock(const cv::Mat& input, cv::Mat& output, 
                    const cv::Rect& blockRect, double scaleX, double scaleY) override;

private:
    Algorithm m_algorithm{Algorithm::Bilinear};
    bool m_enableSharpen{false};
};

#endif // GPUSCALER_HPP
