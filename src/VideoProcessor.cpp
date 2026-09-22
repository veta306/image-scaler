#include "VideoProcessor.hpp"
#include "Parallel_Engine.hpp"
#include "IO_Manager.hpp"
#include "GpuScaler.hpp"
#include <chrono>
#include <cmath>
#include <QFile>

#ifdef _WIN32
#include <windows.h>
#include <timeapi.h>
#endif

/**
 * @brief Конструктор класу VideoProcessor.
 */
VideoProcessor::VideoProcessor(QObject* parent)
    : QThread(parent) {
}

/**
 * @brief Деструктор класу VideoProcessor: коректно зупиняє фоновий потік перед знищенням.
 */
VideoProcessor::~VideoProcessor() {
    stop();
    closeVideo();
}

/**
 * @brief Відкриває відеофайл за вказаним шляхом та зчитує метадані потоку.
 */
bool VideoProcessor::openVideo(const QString& filePath) {
    stop();
    closeVideo();

    QMutexLocker locker(&m_mutex);
    if (!m_capture.open(filePath.toStdString())) {
        emit errorOccurred("Не вдалося відкрити відеофайл: " + filePath);
        return false;
    }

    m_isCamera = false;
    m_filePath = filePath;
    m_sourceWidth = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_WIDTH));
    m_sourceHeight = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    m_sourceFps = m_capture.get(cv::CAP_PROP_FPS);
    if (m_sourceFps <= 0.0 || std::isnan(m_sourceFps)) {
        m_sourceFps = 30.0;
    }
    m_totalFrames = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_COUNT));

    emit videoLoaded(m_sourceWidth, m_sourceHeight, m_sourceFps, m_totalFrames);
    emit statusChanged(QString("Завантажено відео: %1x%2, %3 FPS, всього кадрів: %4")
        .arg(m_sourceWidth).arg(m_sourceHeight).arg(m_sourceFps, 0, 'f', 1).arg(m_totalFrames));

    return true;
}

/**
 * @brief Відкриває підключену вебкамеру за системним індексом.
 */
bool VideoProcessor::openCamera(int cameraIndex) {
    stop();
    closeVideo();

    QMutexLocker locker(&m_mutex);
    if (!m_capture.open(cameraIndex)) {
        emit errorOccurred(QString("Не вдалося підключитися до камери з індексом %1!").arg(cameraIndex));
        return false;
    }

    m_isCamera = true;
    m_filePath.clear();
    m_sourceWidth = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_WIDTH));
    m_sourceHeight = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    m_sourceFps = 30.0;
    m_totalFrames = 0;

    emit videoLoaded(m_sourceWidth, m_sourceHeight, m_sourceFps, m_totalFrames);
    emit statusChanged(QString("Підключено вебкамеру #%1: %2x%3 @ 30 FPS")
        .arg(cameraIndex).arg(m_sourceWidth).arg(m_sourceHeight));

    return true;
}

/**
 * @brief Закриває активний відеопотік та рендерер запису.
 */
void VideoProcessor::closeVideo() {
    QMutexLocker locker(&m_mutex);
    if (m_capture.isOpened()) {
        m_capture.release();
    }
    if (m_writer.isOpened()) {
        m_writer.release();
    }
    m_filePath.clear();
    m_sourceWidth = 0;
    m_sourceHeight = 0;
    m_totalFrames = 0;
}

/**
 * @brief Запускає або відновлює обробку відеопотоку.
 */
void VideoProcessor::play() {
    if (!m_capture.isOpened()) {
        return;
    }

    if (isRunning()) {
        m_paused = false;
        emit statusChanged("Відтворення продовжено.");
    } else {
        m_stopRequested = false;
        m_paused = false;
        m_running = true;
        start();
    }
}

/**
 * @brief Призупиняє обробку відеопотоку.
 */
void VideoProcessor::pause() {
    m_paused = true;
    emit statusChanged("Обробку призупинено.");
}

/**
 * @brief Повністю зупиняє відтворення та очікує завершення робочого потоку.
 */
void VideoProcessor::stop() {
    m_stopRequested = true;
    m_paused = false;
    if (isRunning()) {
        wait();
    }
    m_running = false;
    m_stopRequested = false;

    if (m_capture.isOpened() && !m_isCamera) {
        m_capture.set(cv::CAP_PROP_POS_FRAMES, 0);
    }

    emit statusChanged("Обробку зупинено.");
}

/**
 * @brief Здійснює перехід до конкретного кадру у відеофайлі.
 */
void VideoProcessor::seek(int frameIndex) {
    if (m_isCamera || !m_capture.isOpened()) {
        return;
    }
    m_seekTargetFrame = frameIndex;
    m_seekRequested = true;
}

/**
 * @brief Встановлює активний обчислювальний скалер (Bilinear, Bicubic, Lanczos, Adaptive).
 */
void VideoProcessor::setScaler(IScaler* scaler) {
    QMutexLocker locker(&m_mutex);
    m_scaler = scaler;
}

/**
 * @brief Встановлює коефіцієнти масштабування по осях X та Y.
 */
void VideoProcessor::setScale(double scaleX, double scaleY) {
    QMutexLocker locker(&m_mutex);
    m_scaleX = scaleX;
    m_scaleY = scaleY;
    m_targetWidth = 0;
    m_targetHeight = 0;
}

/**
 * @brief Встановлює фіксовану цільову роздільну здатність кадру.
 */
void VideoProcessor::setTargetResolution(int width, int height) {
    QMutexLocker locker(&m_mutex);
    m_targetWidth = width;
    m_targetHeight = height;
}

/**
 * @brief Встановлює кількість робочих потоків OpenMP для масштабування кожного кадру.
 */
void VideoProcessor::setThreads(int threads) {
    QMutexLocker locker(&m_mutex);
    m_threads = std::max(1, threads);
}

/**
 * @brief Встановлює розмір блоків просторової декомпозиції кадру.
 */
void VideoProcessor::setBlockSize(int blockSize) {
    QMutexLocker locker(&m_mutex);
    m_blockSize = std::max(16, blockSize);
}

/**
 * @brief Вмикає або вимикає режим реального часу (обмеження FPS до частоти відео).
 */
void VideoProcessor::setRealtimeMode(bool enabled) {
    m_realtimeMode = enabled;
}

/**
 * @brief Вмикає або вимикає апаратне прискорення масштабування на відеокарті.
 */
void VideoProcessor::setUseGpu(bool enabled) {
    m_useGpu = enabled;
}

/**
 * @brief Встановлює числовий індекс активного алгоритму.
 */
void VideoProcessor::setAlgorithmIndex(int index) {
    m_algoIndex = index;
}

/**
 * @brief Вмикає або вимикає фільтр підвищення різкості.
 */
void VideoProcessor::setEnableSharpen(bool enabled) {
    m_enableSharpen = enabled;
}

/**
 * @brief Вмикає або вимикає збереження результату обробки у відеофайл.
 */
bool VideoProcessor::setSaveOutput(bool enable, const QString& outputPath) {
    QMutexLocker locker(&m_mutex);
    m_saveOutput = enable;
    m_outputPath = outputPath;

    if (!enable && m_writer.isOpened()) {
        m_writer.release();
    }
    return true;
}

/**
 * @brief Головний робочий цикл фонового потоку QThread для покадрового масштабування.
 */
void VideoProcessor::run() {
    if (!m_capture.isOpened()) {
        emit errorOccurred("Відеопотік не відкрито!");
        return;
    }

    emit statusChanged("Обробка відеопотоку запущена...");

#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    auto lastFpsCalcTime = std::chrono::steady_clock::now();
    int framesSinceFpsCalc = 0;
    double currentFps = 0.0;

    int outW = 0;
    int outH = 0;
    double sX = 1.0;
    double sY = 1.0;
    int blockSize = 64;
    int threads = 4;
    IScaler* scaler = nullptr;

    auto playbackStartTime = std::chrono::steady_clock::now();
    int playedFramesCount = 0;

    while (m_running && !m_stopRequested) {
        if (m_paused) {
            msleep(20);
            double frameIntervalSec = (m_sourceFps > 0.0) ? (1.0 / m_sourceFps) : (1.0 / 30.0);
            playbackStartTime = std::chrono::steady_clock::now() - 
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(playedFramesCount * frameIntervalSec)
                );
            continue;
        }

        if (m_seekRequested && !m_isCamera) {
            int target = m_seekTargetFrame.load();
            m_capture.set(cv::CAP_PROP_POS_FRAMES, target);
            m_seekRequested = false;
            playedFramesCount = 0;
            playbackStartTime = std::chrono::steady_clock::now();
        }

        cv::Mat frame;
        if (!m_capture.read(frame) || frame.empty()) {
            if (m_isCamera) {
                msleep(10);
                continue;
            } else {
                break;
            }
        }

        int curFrameIdx = static_cast<int>(m_capture.get(cv::CAP_PROP_POS_FRAMES));

        {
            QMutexLocker locker(&m_mutex);
            sX = m_scaleX;
            sY = m_scaleY;
            outW = m_targetWidth;
            outH = m_targetHeight;
            blockSize = m_blockSize;
            threads = m_threads;
            scaler = m_scaler;
        }

        if (outW <= 0 || outH <= 0) {
            outW = static_cast<int>(std::round(frame.cols * sX));
            outH = static_cast<int>(std::round(frame.rows * sY));
        }

        // Кодеки MPEG4/H264 вимагають парних розмірів
        if (outW % 2 != 0) outW++;
        if (outH % 2 != 0) outH++;

        // Ініціалізація VideoWriter при першому кадрі, якщо увімкнено збереження
        if (m_saveOutput && !m_writer.isOpened() && !m_outputPath.isEmpty()) {
            int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
            if (m_outputPath.endsWith(".avi", Qt::CaseInsensitive)) {
                fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
            }
            m_writer.open(m_outputPath.toStdString(), fourcc, m_sourceFps, cv::Size(outW, outH));
            if (!m_writer.isOpened()) {
                // Спроба альтернативного кодеку
                fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
                m_writer.open(m_outputPath.toStdString(), fourcc, m_sourceFps, cv::Size(outW, outH));
            }
        }

        cv::Mat scaledFrame = cv::Mat::zeros(outH, outW, frame.type());
        std::vector<cv::Rect> blocks = ParallelEngine::GenerateGrid(outW, outH, blockSize, blockSize);

        auto scaleStartTime = std::chrono::steady_clock::now();
        if (m_useGpu && GpuScaler::isAvailable()) {
            GpuScaler::Algorithm algo = static_cast<GpuScaler::Algorithm>(m_algoIndex.load());
            GpuScaler::scaleFrame(frame, scaledFrame, outW, outH, algo, m_enableSharpen.load());
        } else if (scaler != nullptr) {
            ParallelEngine::ScaleImage(frame, scaledFrame, blocks, *scaler, sX, sY, threads);
        } else {
            cv::resize(frame, scaledFrame, cv::Size(outW, outH));
        }
        auto scaleEndTime = std::chrono::steady_clock::now();
        double scaleDurationMs = std::chrono::duration<double, std::milli>(scaleEndTime - scaleStartTime).count();

        if (m_saveOutput && m_writer.isOpened()) {
            m_writer.write(scaledFrame);
        }

        QImage qimg = IO_Manager::MatToQImage(scaledFrame);

        emit frameProcessed(qimg, scaleDurationMs, currentFps, curFrameIdx, m_totalFrames);

        playedFramesCount++;

        // Високоточна синхронізація з частотою джерела
        if (m_realtimeMode && m_sourceFps > 0.0) {
            double frameIntervalSec = 1.0 / m_sourceFps;
            auto targetDeadline = playbackStartTime + 
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(playedFramesCount * frameIntervalSec)
                );
            auto now = std::chrono::steady_clock::now();
            double waitMs = std::chrono::duration<double, std::milli>(targetDeadline - now).count();

            if (waitMs > 1.5) {
                msleep(static_cast<unsigned long>(waitMs - 1.0));
            }
            while (std::chrono::steady_clock::now() < targetDeadline) {
                QThread::yieldCurrentThread();
            }

            // Якщо відставання перевищує 3 кадри (> 100 мс), пересинхронізуємо таймлайн без накопичення
            if (waitMs < -100.0) {
                playbackStartTime = std::chrono::steady_clock::now() - 
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(playedFramesCount * frameIntervalSec)
                    );
            }
        }

        // Розрахунок фактичного темпу відтворення
        framesSinceFpsCalc++;
        auto nowFps = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(nowFps - lastFpsCalcTime).count();
        if (elapsedSec >= 0.4) {
            currentFps = framesSinceFpsCalc / elapsedSec;
            framesSinceFpsCalc = 0;
            lastFpsCalcTime = nowFps;
        }
    }

    if (m_writer.isOpened()) {
        m_writer.release();
    }

#ifdef _WIN32
    timeEndPeriod(1);
#endif

    m_running = false;
    emit playbackFinished();
    emit statusChanged("Відтворення / обробку завершено.");
}

// ============================================================================
// Реалізація класу VideoExporter
// ============================================================================

VideoExporter::VideoExporter(QObject* parent)
    : QThread(parent) {
}

VideoExporter::~VideoExporter() {
    cancel();
    wait();
}

void VideoExporter::setParams(const QString& inputPath,
                             const QString& outputPath,
                             int outWidth, int outHeight,
                             double scaleX, double scaleY,
                             IScaler* scaler,
                             int threads,
                             int blockSize,
                             bool useGpu,
                             int algoIndex,
                             bool enableSharpen) {
    m_inputPath = inputPath;
    m_outputPath = outputPath;
    m_outWidth = outWidth;
    m_outHeight = outHeight;
    m_scaleX = scaleX;
    m_scaleY = scaleY;
    m_scaler = scaler;
    m_threads = std::max(1, threads);
    m_blockSize = std::max(16, blockSize);
    m_useGpu = useGpu;
    m_algoIndex = algoIndex;
    m_enableSharpen = enableSharpen;
}

void VideoExporter::cancel() {
    m_cancelRequested = true;
}

void VideoExporter::run() {
    m_cancelRequested = false;

    cv::VideoCapture cap(m_inputPath.toStdString());
    if (!cap.isOpened()) {
        emit finishedError("Не вдалося відкрити вхідний відеофайл:\n" + m_inputPath);
        return;
    }

    int inW = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int inH = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double inFps = cap.get(cv::CAP_PROP_FPS);
    if (inFps <= 0.0 || std::isnan(inFps)) {
        inFps = 30.0;
    }
    int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));

    int outW = m_outWidth;
    int outH = m_outHeight;
    if (outW <= 0 || outH <= 0) {
        outW = static_cast<int>(std::round(inW * m_scaleX));
        outH = static_cast<int>(std::round(inH * m_scaleY));
    }
    // Кодеки MPEG4/H264 вимагають парних розмірів
    if (outW % 2 != 0) outW++;
    if (outH % 2 != 0) outH++;

    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    if (m_outputPath.endsWith(".avi", Qt::CaseInsensitive)) {
        fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    }

    cv::VideoWriter writer;
    bool opened = writer.open(m_outputPath.toStdString(), fourcc, inFps, cv::Size(outW, outH));
    if (!opened) {
        fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        opened = writer.open(m_outputPath.toStdString(), fourcc, inFps, cv::Size(outW, outH));
    }

    if (!opened) {
        emit finishedError("Не вдалося створити вихідний файл відео. Перевірте шлях до файлу та права доступу:\n" + m_outputPath);
        return;
    }

    std::vector<cv::Rect> blocks = ParallelEngine::GenerateGrid(outW, outH, m_blockSize, m_blockSize);
    GpuScaler::Algorithm gpuAlgo = static_cast<GpuScaler::Algorithm>(m_algoIndex);

    auto startTime = std::chrono::steady_clock::now();
    int processedFrames = 0;
    cv::Mat frame;

    while (!m_cancelRequested) {
        if (!cap.read(frame) || frame.empty()) {
            break;
        }

        cv::Mat scaledFrame = cv::Mat::zeros(outH, outW, frame.type());

        if (m_useGpu && GpuScaler::isAvailable()) {
            GpuScaler::scaleFrame(frame, scaledFrame, outW, outH, gpuAlgo, m_enableSharpen);
        } else if (m_scaler != nullptr) {
            ParallelEngine::ScaleImage(frame, scaledFrame, blocks, *m_scaler, m_scaleX, m_scaleY, m_threads);
        } else {
            cv::resize(frame, scaledFrame, cv::Size(outW, outH));
        }

        writer.write(scaledFrame);
        processedFrames++;

        auto now = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - startTime).count();
        double curFps = (elapsedSec > 0.0) ? (processedFrames / elapsedSec) : 0.0;
        double remainingSec = 0.0;
        if (curFps > 0.0 && totalFrames > processedFrames) {
            remainingSec = (totalFrames - processedFrames) / curFps;
        }

        emit progressChanged(processedFrames, totalFrames, curFps, elapsedSec, remainingSec);
    }

    writer.release();
    cap.release();

    if (m_cancelRequested) {
        QFile::remove(m_outputPath);
        emit exportCanceled();
    } else {
        auto endTime = std::chrono::steady_clock::now();
        double totalTimeSec = std::chrono::duration<double>(endTime - startTime).count();
        double avgFps = (totalTimeSec > 0.0) ? (processedFrames / totalTimeSec) : 0.0;
        emit finishedSuccess(processedFrames, totalTimeSec, avgFps, outW, outH);
    }
}
