#ifndef METRICS_CONTROLLER_HPP
#define METRICS_CONTROLLER_HPP

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

struct ScalingMetrics {
    double singleThreadedTimeMs = 0.0;
    double multiThreadedTimeMs = 0.0;
    double speedup = 1.0;
    double efficiency = 100.0;
    int threadCount = 1;
    int blockSizeX = 64;
    int blockSizeY = 64;
    int imageWidth = 0;
    int imageHeight = 0;
    double psnr = 0.0;
    double ssim = 1.0;
};

class MetricsController {
private:
    std::vector<std::string> logs;

public:
    void AddLog(const std::string& message);
    const std::vector<std::string>& GetLogs() const;
    void ClearLogs();

    static ScalingMetrics CalculateMetrics(double singleTimeMs, double multiTimeMs, int threads);
    static double CalculatePSNR(const cv::Mat& img1, const cv::Mat& img2);
    static double CalculateSSIM(const cv::Mat& img1, const cv::Mat& img2);
};

#endif // METRICS_CONTROLLER_HPP
