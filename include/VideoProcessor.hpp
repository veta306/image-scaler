#ifndef VIDEOPROCESSOR_HPP
#define VIDEOPROCESSOR_HPP

#include <QThread>
#include <QImage>
#include <QString>
#include <QMutex>
#include <atomic>
#include <opencv2/opencv.hpp>
#include "IScaler.hpp"

/**
 * @brief Клас VideoProcessor забезпечує асинхронне покадрове масштабування відеопотоку
 * (із файлу або вебкамери) в окремому робочому потоці QThread.
 */
class VideoProcessor : public QThread {
    Q_OBJECT

public:
    explicit VideoProcessor(QObject* parent = nullptr);
    ~VideoProcessor() override;

    bool openVideo(const QString& filePath);
    bool openCamera(int cameraIndex = 0);
    void closeVideo();

    void play();
    void pause();
    void stop();
    void seek(int frameIndex);

    void setScaler(IScaler* scaler);
    void setScale(double scaleX, double scaleY);
    void setTargetResolution(int width, int height);
    void setThreads(int threads);
    void setBlockSize(int blockSize);
    void setRealtimeMode(bool enabled);
    void setUseGpu(bool enabled);
    void setAlgorithmIndex(int index);
    void setEnableSharpen(bool enabled);
    bool setSaveOutput(bool enable, const QString& outputPath = "");

    bool isPlaying() const { return m_running && !m_paused; }
    bool isPaused() const { return m_paused; }
    bool isOpen() const { return m_capture.isOpened(); }
    int sourceWidth() const { return m_sourceWidth; }
    int sourceHeight() const { return m_sourceHeight; }
    double sourceFps() const { return m_sourceFps; }
    int totalFrames() const { return m_totalFrames; }
    bool isCamera() const { return m_isCamera; }

    const QString& currentFilePath() const { return m_filePath; }

signals:
    void frameProcessed(const QImage& frame, double frameMs, double fps, int currentFrame, int totalFrames);
    void videoLoaded(int width, int height, double fps, int totalFrames);
    void playbackFinished();
    void statusChanged(const QString& status);
    void errorOccurred(const QString& error);

protected:
    void run() override;

private:
    cv::VideoCapture m_capture;
    cv::VideoWriter m_writer;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_seekRequested{false};
    std::atomic<int> m_seekTargetFrame{0};
    std::atomic<bool> m_realtimeMode{true};
    std::atomic<bool> m_useGpu{false};
    std::atomic<int> m_algoIndex{0};
    std::atomic<bool> m_enableSharpen{false};
    std::atomic<bool> m_saveOutput{false};

    IScaler* m_scaler{nullptr};
    double m_scaleX{1.0};
    double m_scaleY{1.0};
    int m_targetWidth{0};
    int m_targetHeight{0};
    int m_threads{4};
    int m_blockSize{64};

    int m_sourceWidth{0};
    int m_sourceHeight{0};
    double m_sourceFps{30.0};
    int m_totalFrames{0};
    bool m_isCamera{false};

    QString m_filePath;
    QString m_outputPath;
    mutable QMutex m_mutex;
};

/**
 * @brief Клас VideoExporter виконує фоновий пакетний експорт масштабованого відеофайлу
 * з максимальним задіянням ресурсів CPU/GPU без обмежень реального часу та з оновленням прогресу.
 */
class VideoExporter : public QThread {
    Q_OBJECT

public:
    explicit VideoExporter(QObject* parent = nullptr);
    ~VideoExporter() override;

    void setParams(const QString& inputPath,
                   const QString& outputPath,
                   int outWidth, int outHeight,
                   double scaleX, double scaleY,
                   IScaler* scaler,
                   int threads,
                   int blockSize,
                   bool useGpu,
                   int algoIndex,
                   bool enableSharpen);

    void cancel();

signals:
    void progressChanged(int currentFrame, int totalFrames, double fps, double elapsedSec, double remainingSec);
    void finishedSuccess(int totalFrames, double totalTimeSec, double avgFps, int outW, int outH);
    void finishedError(const QString& error);
    void exportCanceled();

protected:
    void run() override;

private:
    QString m_inputPath;
    QString m_outputPath;
    int m_outWidth{0};
    int m_outHeight{0};
    double m_scaleX{1.0};
    double m_scaleY{1.0};
    IScaler* m_scaler{nullptr};
    int m_threads{4};
    int m_blockSize{64};
    bool m_useGpu{false};
    int m_algoIndex{0};
    bool m_enableSharpen{false};
    std::atomic<bool> m_cancelRequested{false};
};

#endif // VIDEOPROCESSOR_HPP
