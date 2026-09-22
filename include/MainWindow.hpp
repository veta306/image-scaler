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
#include <QCheckBox>
#include <QTableWidget>
#include <vector>
#include <opencv2/opencv.hpp>

#include "Metrics_Controller.hpp"
#include "BilinearScaler.hpp"
#include "BicubicScaler.hpp"
#include "LanczosScaler.hpp"
#include "AdaptiveScaler.hpp"
#include "VideoProcessor.hpp"
#include "IO_Manager.hpp"
#include "Parallel_Engine.hpp"
#include "ImageViewer.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

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
    void onExportCsvClicked();
    void onExportJsonClicked();
    void onToggleSidebarClicked();

    // Video slots
    void onLoadVideoClicked();
    void onWebcamClicked();
    void onVideoPlayClicked();
    void onVideoPauseClicked();
    void onVideoStopClicked();
    void onVideoRecordClicked();
    void onVideoSeekSliderMoved(int position);
    void onVideoFrameProcessed(const QImage& frame, double frameMs, double fps, int currentFrame, int totalFrames);
    void onVideoLoaded(int width, int height, double fps, int totalFrames);
    void onVideoPlaybackFinished();
    void onVideoStatusChanged(const QString& status);
    void onVideoErrorOccurred(const QString& error);

private:
    void setupUI();
    void applyStyleSheet();
    void updateLogsDisplay();
    void updateMetricsDisplay(const ScalingMetrics& metrics);
    QFrame* createMetricCard(const QString& title, const QString& unit, QLabel*& valueLabelOut);
    QFrame* createTimeMetricCard(const QString& title, const QString& unit, QLabel*& valueLabelOut, QLabel*& fpsLabelOut);
    void updateTargetResolutionFromScale();
    IScaler& getActiveScaler();
    QString getActiveScalerName() const;
    void syncVideoProcessorParams();

    cv::Mat originalImage;
    cv::Mat scaledImage;
    QString currentFilePath;
    MetricsController metricsController;

    BilinearScaler bilinearScaler;
    BicubicScaler bicubicScaler;
    LanczosScaler lanczosScaler;
    AdaptiveScaler adaptiveScaler;

    std::vector<BenchmarkRecord> currentBenchmarkRecords;

    ImageViewer* viewerOrig;
    ImageViewer* viewerScaled;
    QLabel* lblImageInfo;

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
    QCheckBox* chkEnableSIMD;
    QComboBox* comboBlockSize;
    QSlider* sliderThreads;
    QSpinBox* spinThreads;

    QPushButton* btnLoad;
    QPushButton* btnProcess;
    QPushButton* btnSave;
    QFrame* sidebarFrame{nullptr};
    QPushButton* btnToggleSidebar{nullptr};

    // Metrics Card Value labels
    QLabel* lblSingleTimeVal;
    QLabel* lblMultiTimeVal;
    QLabel* lblSpeedupVal;
    QLabel* lblEfficiencyVal;
    QLabel* lblSingleFps;
    QLabel* lblMultiFps;
    QLabel* lblMseVal;
    QLabel* lblPsnrVal;
    QLabel* lblSsimVal;
    QLabel* lblActiveAlgorithmVal;

    // Performance Bars
    QProgressBar* barSingleTime;
    QProgressBar* barMultiTime;

    // Benchmark tab widgets
    QPushButton* btnRunBenchmark;
    QPushButton* btnExportCsv;
    QPushButton* btnExportJson;
    QTableWidget* tableBenchmark;
    QTableWidget* tablePivotBenchmark;

    // Video processor & widgets
    VideoProcessor* videoProcessor{nullptr};
    QLabel* lblVideoDisplay;
    QLabel* lblVideoInfo;
    QLabel* lblVideoStats;
    QPushButton* btnLoadVideo;
    QPushButton* btnWebcam;
    QPushButton* btnVideoPlay;
    QPushButton* btnVideoPause;
    QPushButton* btnVideoStop;
    QPushButton* btnVideoRecord;
    QSlider* sliderVideoProgress;
    QCheckBox* chkRealtimeMode;
    QLabel* lblVideoTime;
    QString outputVideoPath;
    bool isRecordingVideo = false;
};

#endif // MAINWINDOW_HPP
