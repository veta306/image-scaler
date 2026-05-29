#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollArea>
#include <QTabWidget>
#include <QString>
#include <opencv2/opencv.hpp>

#include <QCheckBox>
#include <QTableWidget>

#include "Metrics_Controller.hpp"
#include "BilinearScaler.hpp"
#include "IO_Manager.hpp"
#include "Parallel_Engine.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() = default;

private slots:
    void onLoadImageClicked();
    void onProcessClicked();
    void onSaveImageClicked();
    void onThreadSliderChanged(int value);
    void onThreadSpinChanged(int value);
    void onScalingModeChanged(int index);
    void onTargetWidthChanged(int value);
    void onTargetHeightChanged(int value);
    void onRunBenchmarkClicked();

private:
    void setupUI();
    void applyStyleSheet();
    void updateLogsDisplay();
    void updateMetricsDisplay(const ScalingMetrics& metrics);
    QFrame* createMetricCard(const QString& title, const QString& unit, QLabel*& valueLabelOut);
    QFrame* createTimeMetricCard(const QString& title, const QString& unit, QLabel*& valueLabelOut, QLabel*& fpsLabelOut);
    void updateTargetResolutionFromScale();
    bool eventFilter(QObject* obj, QEvent* event) override;
    void updateOrigViewer();
    void updateScaledViewer();

    // Business state
    cv::Mat originalImage;
    cv::Mat scaledImage;
    QString currentFilePath;
    MetricsController metricsController;
    BilinearScaler bilinearScaler;
    
    // Zoom & Pan state
    double displayZoomOrig;
    double displayZoomScaled;
    QScrollArea* activePanArea;
    QPoint panStartPos;
    int startScrollX;
    int startScrollY;
    QScrollArea* scrollOrig;
    QScrollArea* scrollScaled;

    // UI Elements
    QLabel* lblOriginalImage;
    QLabel* lblScaledImage;
    QLabel* lblImageInfo;
    QLabel* lblLeftStatus;
    QLabel* lblRightStatus;

    // Input elements
    QComboBox* comboScalingMode;
    QComboBox* comboScale;
    QComboBox* comboAlgorithm;
    QDoubleSpinBox* spinCustomScale;
    QSpinBox* spinTargetWidth;
    QSpinBox* spinTargetHeight;
    QCheckBox* chkKeepAspectRatio;
    QCheckBox* chkEnableSharpen;
    QCheckBox* chkEnableOverlap;
    QCheckBox* chkEnableDemo;
    QComboBox* comboBlockSize;
    QSlider* sliderThreads;
    QSpinBox* spinThreads;

    QPushButton* btnLoad;
    QPushButton* btnProcess;
    QPushButton* btnSave;

    // Metrics Card Value labels
    QLabel* lblSingleTimeVal;
    QLabel* lblMultiTimeVal;
    QLabel* lblSpeedupVal;
    QLabel* lblEfficiencyVal;
    QLabel* lblSingleFps;
    QLabel* lblMultiFps;
    QLabel* lblPsnrVal;
    QLabel* lblSsimVal;

    // Performance Bars
    QProgressBar* barSingleTime;
    QProgressBar* barMultiTime;

    // Benchmark tab widgets
    QPushButton* btnRunBenchmark;
    QTableWidget* tableBenchmark;
    QTableWidget* tablePivotBenchmark;
};

#endif // MAINWINDOW_HPP
