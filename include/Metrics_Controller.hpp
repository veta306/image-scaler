#ifndef METRICS_CONTROLLER_HPP
#define METRICS_CONTROLLER_HPP

#include <string>
#include <vector>

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
};

class MetricsController {
private:
    std::vector<std::string> logs;

public:
    void AddLog(const std::string& message);
    const std::vector<std::string>& GetLogs() const;
    void ClearLogs();

    static ScalingMetrics CalculateMetrics(double singleTimeMs, double multiTimeMs, int threads);
};

#endif // METRICS_CONTROLLER_HPP
