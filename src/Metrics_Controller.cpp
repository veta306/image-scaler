#include "Metrics_Controller.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

void MetricsController::AddLog(const std::string& message) {
    logs.push_back(message);
}

const std::vector<std::string>& MetricsController::GetLogs() const {
    return logs;
}

void MetricsController::ClearLogs() {
    logs.clear();
}

ScalingMetrics MetricsController::CalculateMetrics(double singleTimeMs, double multiTimeMs, int threads) {
    ScalingMetrics metrics;
    metrics.singleThreadedTimeMs = singleTimeMs;
    metrics.multiThreadedTimeMs = multiTimeMs;
    metrics.threadCount = threads;

    if (multiTimeMs > 0.0) {
        metrics.speedup = singleTimeMs / multiTimeMs;
    } else {
        metrics.speedup = 1.0;
    }

    if (threads > 0) {
        metrics.efficiency = (metrics.speedup / threads) * 100.0;
    } else {
        metrics.efficiency = 100.0;
    }

    return metrics;
}
