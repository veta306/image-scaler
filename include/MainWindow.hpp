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

private:
    void setupUI();
    void applyStyleSheet();
    void updateLogsDisplay();
    void updateMetricsDisplay(const ScalingMetrics& metrics);
    QFrame* createMetricCard(const QString& title, const QString& unit, QLabel*& valueLabelOut);

    // Business state
    cv::Mat originalImage;
    cv::Mat scaledImage;
    QString currentFilePath;
    MetricsController metricsController;
    BilinearScaler bilinearScaler;

    // UI Elements
    QLabel* lblOriginalImage;
    QLabel* lblScaledImage;
    QLabel* lblImageInfo;

    // Input elements
    QComboBox* comboScale;
    QComboBox* comboAlgorithm;
    QDoubleSpinBox* spinCustomScale;
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

    // Performance Bars
    QProgressBar* barSingleTime;
    QProgressBar* barMultiTime;

    // Log Console
    QPlainTextEdit* logConsole;
};

#endif // MAINWINDOW_HPP
