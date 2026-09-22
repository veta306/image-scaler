#include "VideoProcessor.hpp"
#include "Parallel_Engine.hpp"
#include "IO_Manager.hpp"
#include <chrono>
#include <cmath>

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

    auto lastFpsCalcTime = std::chrono::high_resolution_clock::now();
    int framesSinceFpsCalc = 0;
    double currentFps = 0.0;

    int outW = 0;
    int outH = 0;
    double sX = 1.0;
    double sY = 1.0;
    int blockSize = 64;
    int threads = 4;
    IScaler* scaler = nullptr;

    while (m_running && !m_stopRequested) {
        if (m_paused) {
            msleep(25);
            continue;
        }

        if (m_seekRequested && !m_isCamera) {
            int target = m_seekTargetFrame.load();
            m_capture.set(cv::CAP_PROP_POS_FRAMES, target);
            m_seekRequested = false;
        }

        auto frameStartTime = std::chrono::high_resolution_clock::now();

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

        // Ініціалізація VideoWriter при першому кадрі, якщо увімкнено збереження
        if (m_saveOutput && !m_writer.isOpened() && !m_outputPath.isEmpty()) {
            int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
            m_writer.open(m_outputPath.toStdString(), fourcc, m_sourceFps, cv::Size(outW, outH));
            if (!m_writer.isOpened()) {
                // Спроба альтернативного кодеку
                fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
                m_writer.open(m_outputPath.toStdString(), fourcc, m_sourceFps, cv::Size(outW, outH));
            }
        }

        cv::Mat scaledFrame = cv::Mat::zeros(outH, outW, frame.type());
        std::vector<cv::Rect> blocks = ParallelEngine::GenerateGrid(outW, outH, blockSize, blockSize);

        auto scaleStartTime = std::chrono::high_resolution_clock::now();
        if (scaler != nullptr) {
            ParallelEngine::ScaleImage(frame, scaledFrame, blocks, *scaler, sX, sY, threads);
        } else {
            cv::resize(frame, scaledFrame, cv::Size(outW, outH));
        }
        auto scaleEndTime = std::chrono::high_resolution_clock::now();
        double scaleDurationMs = std::chrono::duration<double, std::milli>(scaleEndTime - scaleStartTime).count();

        if (m_saveOutput && m_writer.isOpened()) {
            m_writer.write(scaledFrame);
        }

        QImage qimg = IO_Manager::MatToQImage(scaledFrame);

        framesSinceFpsCalc++;
        auto now = std::chrono::high_resolution_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - lastFpsCalcTime).count();
        if (elapsedSec >= 0.4) {
            currentFps = framesSinceFpsCalc / elapsedSec;
            framesSinceFpsCalc = 0;
            lastFpsCalcTime = now;
        }

        emit frameProcessed(qimg, scaleDurationMs, currentFps, curFrameIdx, m_totalFrames);

        if (m_realtimeMode && m_sourceFps > 0.0) {
            double targetFrameTimeMs = 1000.0 / m_sourceFps;
            auto frameEndTime = std::chrono::high_resolution_clock::now();
            double totalFrameTimeMs = std::chrono::duration<double, std::milli>(frameEndTime - frameStartTime).count();
            double sleepTimeMs = targetFrameTimeMs - totalFrameTimeMs;
            if (sleepTimeMs > 1.0) {
                msleep(static_cast<unsigned long>(sleepTimeMs));
            }
        }
    }

    if (m_writer.isOpened()) {
        m_writer.release();
    }

    m_running = false;
    emit playbackFinished();
    emit statusChanged("Відтворення / обробку завершено.");
}
