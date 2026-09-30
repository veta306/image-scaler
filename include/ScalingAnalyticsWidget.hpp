#ifndef SCALINGANALYTICSWIDGET_HPP
#define SCALINGANALYTICSWIDGET_HPP

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTabWidget>
#include <QFrame>
#include <QProgressBar>
#include <QString>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>

#include "Metrics_Controller.hpp"

/**
 * @brief Віджет наукової аналітики та побудови графіків масштабування, 
 * закону Амдала та співвідношення «Якість vs Швидкодія».
 */
class ScalingAnalyticsWidget : public QWidget {
    Q_OBJECT

public:
    struct ThreadScalingPoint {
        int threads = 1;
        double timeMs = 0.0;
        double speedup = 1.0;
        double efficiency = 100.0;
    };

    struct AlgorithmMetricPoint {
        QString algorithmName;
        double fps = 0.0;
        double psnr = 0.0;
        double ssim = 1.0;
        double mse = 0.0;
        bool isGpu = false;
    };

    explicit ScalingAnalyticsWidget(QWidget* parent = nullptr);
    ~ScalingAnalyticsWidget() override = default;

    void updateFromBenchmarkRecords(const std::vector<BenchmarkRecord>& records);
    void setBaselineData();
    void runQuickScalingBenchmark(const cv::Mat& sourceImage, double scaleFactor = 2.0);
    void setSourceImage(const cv::Mat& img, double scale = 2.0);

signals:
    void requestRunFullBenchmark();

private slots:
    void onMetricTypeChanged(int index);
    void onVariantFilterChanged(int index);
    void onExportChartPng();
    void onCopyChartClipboard();
    void onResetZoom();
    void onRunDedicatedAnalysisClicked();

private:
    void setupUI();
    QFrame* createStatCard(const QString& title, const QString& unit, QLabel*& valueLabelOut, QLabel*& subLabelOut);
    void setupSpeedupChart();
    void setupEfficiencyChart();
    void setupQualityChart();
    void recalculateAmdahlModel();
    void updateSpeedupChartData();
    void updateEfficiencyChartData();
    void updateQualityChartData();
    QChart* currentActiveChart() const;

    // Charts & Views
    QTabWidget* subTabCharts{nullptr};
    
    QChartView* chartViewSpeedup{nullptr};
    QChart* chartSpeedup{nullptr};
    QLineSeries* seriesIdealSpeedup{nullptr};
    QLineSeries* seriesTheorSpeedup{nullptr};
    QLineSeries* seriesMeasSpeedup{nullptr};
    QValueAxis* axisSpeedupX{nullptr};
    QValueAxis* axisSpeedupY{nullptr};

    QChartView* chartViewEfficiency{nullptr};
    QChart* chartEfficiency{nullptr};
    QLineSeries* seriesIdealEfficiency{nullptr};
    QLineSeries* seriesTheorEfficiency{nullptr};
    QLineSeries* seriesMeasEfficiency{nullptr};
    QValueAxis* axisEffX{nullptr};
    QValueAxis* axisEffY{nullptr};

    QChartView* chartViewQuality{nullptr};
    QChart* chartQuality{nullptr};
    QValueAxis* axisQualX{nullptr};
    QValueAxis* axisQualY{nullptr};

    // UI elements
    QComboBox* comboQualityMetric{nullptr};
    QComboBox* comboVariantFilter{nullptr};
    QPushButton* btnRunAnalysis{nullptr};
    QPushButton* btnExportPng{nullptr};
    QPushButton* btnCopyClipboard{nullptr};
    QPushButton* btnResetZoom{nullptr};
    QProgressBar* progressBar{nullptr};
    QLabel* lblStatusInfo{nullptr};

    // Statistics Cards
    QLabel* lblAlphaVal{nullptr};
    QLabel* lblAlphaSub{nullptr};
    QLabel* lblSmaxVal{nullptr};
    QLabel* lblSmaxSub{nullptr};
    QLabel* lblMeasuredSpeedupVal{nullptr};
    QLabel* lblMeasuredSpeedupSub{nullptr};
    QLabel* lblEfficiencyVal{nullptr};
    QLabel* lblEfficiencySub{nullptr};

    // Data structures
    std::vector<ThreadScalingPoint> threadPoints;
    std::vector<AlgorithmMetricPoint> algorithmPoints;
    double estimatedAlpha{0.05};
    double maxTheoreticalSpeedup{20.0};
    int currentMaxThreads{8};
    bool showPsnrMetric{true};
    cv::Mat currentSourceImage;
    double currentScaleFactor{2.0};
};

#endif // SCALINGANALYTICSWIDGET_HPP
