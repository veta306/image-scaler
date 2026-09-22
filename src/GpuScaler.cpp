#include "GpuScaler.hpp"
#include <opencv2/core/ocl.hpp>
#include <chrono>
#include <cmath>
#include <algorithm>

GpuScaler::GpuScaler() {
    initGpu();
}

/**
 * @brief Перевіряє наявність сумісного графічного процесора OpenCL на поточній системі.
 */
bool GpuScaler::isAvailable() {
    static bool checked = false;
    static bool available = false;
    if (!checked) {
        cv::ocl::setUseOpenCL(true);
        available = cv::ocl::haveOpenCL() && cv::ocl::useOpenCL();
        checked = true;
    }
    return available;
}

/**
 * @brief Динамічно повертає назву виявленого графічного адаптера без статичного кодування.
 */
std::string GpuScaler::getDeviceNameStd() {
    if (!isAvailable()) {
        return "Графічний прискорювач недоступний";
    }
    cv::ocl::Device dev = cv::ocl::Device::getDefault();
    std::string name = dev.name();
    if (name.empty()) {
        return "Сумісний графічний адаптер OpenCL";
    }
    return name;
}

/**
 * @brief Ініціалізує контекст OpenCL для роботи з відеокартою.
 */
bool GpuScaler::initGpu() {
    if (!isAvailable()) return false;
    cv::ocl::setUseOpenCL(true);
    return cv::ocl::useOpenCL();
}

/**
 * @brief Виконує апаратне масштабування всього кадру на відеокарті з роздільним заміром таймінгів.
 */
bool GpuScaler::scaleFrame(const cv::Mat& input, cv::Mat& output, 
                           int targetWidth, int targetHeight, 
                           Algorithm algo, bool enableSharpen, Timing* timingOut) {
    if (input.empty() || targetWidth <= 0 || targetHeight <= 0) {
        return false;
    }

    if (!isAvailable()) {
        int interp = cv::INTER_LINEAR;
        if (algo == Algorithm::Bicubic) interp = cv::INTER_CUBIC;
        else if (algo == Algorithm::Lanczos) interp = cv::INTER_LANCZOS4;
        cv::resize(input, output, cv::Size(targetWidth, targetHeight), 0, 0, interp);
        if (enableSharpen) {
            cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
                 0.0f, -1.0f,  0.0f,
                -1.0f,  5.0f, -1.0f,
                 0.0f, -1.0f,  0.0f);
            cv::filter2D(output, output, -1, kernel);
        }
        return true;
    }

    auto tStart = std::chrono::high_resolution_clock::now();

    // 1. Host-to-Device (Завантаження даних із RAM у VRAM відеокарти)
    auto tUploadStart = std::chrono::high_resolution_clock::now();
    cv::UMat uIn = input.getUMat(cv::ACCESS_READ);
    cv::ocl::finish();
    auto tUploadEnd = std::chrono::high_resolution_clock::now();

    // 2. Kernel Execution (Виконання обчислювального конвеєра на GPU)
    auto tKernelStart = std::chrono::high_resolution_clock::now();
    cv::UMat uOut;

    switch (algo) {
        case Algorithm::Bilinear: {
            cv::resize(uIn, uOut, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_LINEAR);
            break;
        }
        case Algorithm::Bicubic: {
            cv::resize(uIn, uOut, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_CUBIC);
            break;
        }
        case Algorithm::Lanczos: {
            cv::resize(uIn, uOut, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_LANCZOS4);
            break;
        }
        case Algorithm::AdaptiveSobel: {
            // Адаптивне поєднання на GPU: Bicubic на однорідних ділянках, Lanczos-4 на контурах
            cv::UMat uBicubic, uLanczos;
            cv::resize(uIn, uBicubic, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_CUBIC);
            cv::resize(uIn, uLanczos, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_LANCZOS4);

            cv::UMat uGray, uGradX, uGradY, uAbsX, uAbsY, uGrad, uMask;
            cv::cvtColor(uBicubic, uGray, cv::COLOR_BGR2GRAY);
            cv::Sobel(uGray, uGradX, CV_16S, 1, 0, 3);
            cv::Sobel(uGray, uGradY, CV_16S, 0, 1, 3);
            cv::convertScaleAbs(uGradX, uAbsX);
            cv::convertScaleAbs(uGradY, uAbsY);
            cv::addWeighted(uAbsX, 0.5, uAbsY, 0.5, 0, uGrad);
            cv::threshold(uGrad, uMask, 25, 255, cv::THRESH_BINARY);

            uOut = uBicubic.clone();
            uLanczos.copyTo(uOut, uMask);
            break;
        }
    }

    // Застосування фільтра підвищення різкості Лапласа на GPU за вибором користувача
    if (enableSharpen) {
        cv::Mat laplacianKernel = (cv::Mat_<float>(3, 3) <<
             0.0f, -1.0f,  0.0f,
            -1.0f,  5.0f, -1.0f,
             0.0f, -1.0f,  0.0f);
        cv::UMat uSharpened;
        cv::filter2D(uOut, uSharpened, -1, laplacianKernel);
        uOut = uSharpened;
    }

    cv::ocl::finish();
    auto tKernelEnd = std::chrono::high_resolution_clock::now();

    // 3. Device-to-Host (Зчитування результату з VRAM відеокарти назад у RAM)
    auto tDownloadStart = std::chrono::high_resolution_clock::now();
    if (output.empty() || output.cols != targetWidth || output.rows != targetHeight || output.type() != input.type()) {
        output.create(targetHeight, targetWidth, input.type());
    }
    uOut.copyTo(output);
    auto tDownloadEnd = std::chrono::high_resolution_clock::now();

    if (timingOut) {
        timingOut->uploadMs = std::chrono::duration<double, std::milli>(tUploadEnd - tUploadStart).count();
        timingOut->kernelMs = std::chrono::duration<double, std::milli>(tKernelEnd - tKernelStart).count();
        timingOut->downloadMs = std::chrono::duration<double, std::milli>(tDownloadEnd - tDownloadStart).count();
        timingOut->totalMs = std::chrono::duration<double, std::milli>(tDownloadEnd - tStart).count();
    }

    return true;
}

/**
 * @brief Реалізація інтерфейсу IScaler для сумісності з блоковою обробкою.
 */
void GpuScaler::ScaleBlock(const cv::Mat& input, cv::Mat& output, 
                           const cv::Rect& blockRect, double scaleX, double scaleY) {
    if (input.empty() || output.empty() || blockRect.width <= 0 || blockRect.height <= 0) return;

    int srcX = static_cast<int>(std::floor(blockRect.x / scaleX));
    int srcY = static_cast<int>(std::floor(blockRect.y / scaleY));
    int srcW = static_cast<int>(std::ceil(blockRect.width / scaleX)) + 2;
    int srcH = static_cast<int>(std::ceil(blockRect.height / scaleY)) + 2;

    srcX = std::max(0, std::min(srcX, input.cols - 1));
    srcY = std::max(0, std::min(srcY, input.rows - 1));
    srcW = std::min(srcW, input.cols - srcX);
    srcH = std::min(srcH, input.rows - srcY);

    cv::Mat subIn = input(cv::Rect(srcX, srcY, srcW, srcH));
    cv::Mat subOut;
    scaleFrame(subIn, subOut, blockRect.width, blockRect.height, m_algorithm, m_enableSharpen, nullptr);

    if (!subOut.empty() && subOut.cols == blockRect.width && subOut.rows == blockRect.height) {
        subOut.copyTo(output(blockRect));
    }
}
