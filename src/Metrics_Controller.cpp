#include "Metrics_Controller.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

/**
 * @brief Додає повідомлення до списку журналів роботи.
 */
void MetricsController::AddLog(const std::string& message) {
    logs.push_back(message);
}

/**
 * @brief Повертає список повідомлень журналів роботи.
 */
const std::vector<std::string>& MetricsController::GetLogs() const {
    return logs;
}

/**
 * @brief Очищує весь журнал роботи програми.
 */
void MetricsController::ClearLogs() {
    logs.clear();
}

/**
 * @brief Розраховує прискорення та ефективність паралельної роботи ядер процесора.
 */
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

/**
 * @brief Обчислює показник PSNR (пікове відношення сигналу до шуму).
 */
double MetricsController::CalculatePSNR(const cv::Mat& img1, const cv::Mat& img2) {
    if (img1.empty() || img2.empty() || img1.size() != img2.size()) {
        return 0.0;
    }
    return cv::PSNR(img1, img2);
}

/**
 * @brief Обчислює індекс структурної схожості SSIM для оцінки якості зображень.
 */
double MetricsController::CalculateSSIM(const cv::Mat& img1, const cv::Mat& img2) {
    if (img1.empty() || img2.empty() || img1.size() != img2.size()) {
        return 0.0;
    }

    const double C1 = 6.5025, C2 = 58.5225;
    int d = CV_32F;

    cv::Mat I1, I2;
    img1.convertTo(I1, d);
    img2.convertTo(I2, d);

    cv::Mat I1_2   = I1.mul(I1);
    cv::Mat I2_2   = I2.mul(I2);
    cv::Mat I1_I2  = I1.mul(I2);

    cv::Mat mu1, mu2;
    cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

    cv::Mat mu1_2   = mu1.mul(mu1);
    cv::Mat mu2_2   = mu2.mul(mu2);
    cv::Mat mu1_mu2 = mu1.mul(mu2);

    cv::Mat sigma1_2, sigma2_2, sigma12;

    cv::GaussianBlur(I1_2, sigma1_2, cv::Size(11, 11), 1.5);
    sigma1_2 -= mu1_2;

    cv::GaussianBlur(I2_2, sigma2_2, cv::Size(11, 11), 1.5);
    sigma2_2 -= mu2_2;

    cv::GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5);
    sigma12 -= mu1_mu2;

    cv::Mat t1, t2, t3;

    t1 = 2 * mu1_mu2 + C1;
    t2 = 2 * sigma12 + C2;
    t3 = t1.mul(t2);

    t1 = mu1_2 + mu2_2 + C1;
    t2 = sigma1_2 + sigma2_2 + C2;
    t1 = t1.mul(t2);

    cv::Mat ssim_map;
    cv::divide(t3, t1, ssim_map);

    cv::Scalar mssim = cv::mean(ssim_map);

    if (img1.channels() == 1) {
        return mssim[0];
    } else if (img1.channels() == 3) {
        return (mssim[0] + mssim[1] + mssim[2]) / 3.0;
    } else if (img1.channels() == 4) {
        return (mssim[0] + mssim[1] + mssim[2] + mssim[3]) / 4.0;
    }
    return mssim[0];
}
