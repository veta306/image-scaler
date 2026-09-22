#ifndef METRICS_CONTROLLER_HPP
#define METRICS_CONTROLLER_HPP

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

/**
 * @brief Структура ScalingMetrics містить показники швидкодії та якості масштабування.
 */
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
    double mse = 0.0;
    double psnr = 0.0;
    double ssim = 1.0;
};

/**
 * @brief Запис результату одного експерименту бенчмарку для експорту в CSV та JSON.
 */
struct BenchmarkRecord {
    std::string method;
    std::string blockSize;
    int threads = 1;
    double durationMs = 0.0;
    double fps = 0.0;
    double mse = 0.0;
    double psnr = 0.0;
    double ssim = 0.0;
};

/**
 * @brief Клас MetricsController управляє записом журналів роботи програми та обчислює метрики.
 */
class MetricsController {
private:
    std::vector<std::string> logs;

public:
    /**
     * @brief Додає новий рядок повідомлення до журналу логів.
     */
    void AddLog(const std::string& message);

    /**
     * @brief Повертає список усіх записаних повідомлень журналу.
     */
    const std::vector<std::string>& GetLogs() const;

    /**
     * @brief Очищує журнал повідомлень.
     */
    void ClearLogs();

    /**
     * @brief Обчислює показники швидкодії, прискорення та ефективності.
     */
    static ScalingMetrics CalculateMetrics(double singleTimeMs, double multiTimeMs, int threads);

    /**
     * @brief Обчислює середньоквадратичну помилку (MSE) між двома зображеннями.
     */
    static double CalculateMSE(const cv::Mat& img1, const cv::Mat& img2);

    /**
     * @brief Обчислює метрику якості PSNR між двома зображеннями.
     */
    static double CalculatePSNR(const cv::Mat& img1, const cv::Mat& img2);

    /**
     * @brief Обчислює індекс структурної подібності SSIM між двома зображеннями.
     */
    static double CalculateSSIM(const cv::Mat& img1, const cv::Mat& img2);

    /**
     * @brief Експортує дані бенчмарку у файл формату CSV.
     */
    static bool ExportBenchmarkToCSV(const std::string& filePath, const std::vector<BenchmarkRecord>& records);

    /**
     * @brief Експортує дані бенчмарку у файл формату JSON.
     */
    static bool ExportBenchmarkToJSON(const std::string& filePath, const std::vector<BenchmarkRecord>& records);
};

#endif // METRICS_CONTROLLER_HPP
