#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <opencv2/opencv.hpp>
#include "BilinearScaler.hpp"
#include "BicubicScaler.hpp"
#include "LanczosScaler.hpp"
#include "AdaptiveScaler.hpp"
#include "Parallel_Engine.hpp"

int main() {
    std::cout << "========================================================\n";
    std::cout << "   Full Core SIMD (AVX2 + FMA) Verification & Benchmark \n";
    std::cout << "========================================================\n";

    const int srcW = 1920;
    const int srcH = 1080;
    cv::Mat inputImage(srcH, srcW, CV_8UC3);
    for (int y = 0; y < srcH; ++y) {
        cv::Vec3b* ptr = inputImage.ptr<cv::Vec3b>(y);
        for (int x = 0; x < srcW; ++x) {
            uchar b = static_cast<uchar>((x * 255) / srcW);
            uchar g = static_cast<uchar>((y * 255) / srcH);
            uchar r = static_cast<uchar>((std::sin(x * 0.05) * std::cos(y * 0.05) + 1.0) * 127.5);
            ptr[x] = cv::Vec3b(b, g, r);
        }
    }

    const double scale = 2.0;
    const int dstW = static_cast<int>(srcW * scale);
    const int dstH = static_cast<int>(srcH * scale);
    const int blockW = 256;
    const int blockH = 256;
    auto blocks = ParallelEngine::GenerateGrid(dstW, dstH, blockW, blockH);

    for (int numThreads : {1, 8}) {
        std::cout << "\n========================================================";
        std::cout << "\n   TEST RUN WITH " << numThreads << " THREAD(S) (Grid blocks: " << blocks.size() << ")\n";
        std::cout << "========================================================\n";

        // --- ТЕСТ 1: BilinearScaler ---
        {
            std::cout << "--- [1] Bilinear Scaler ---\n";
            BilinearScaler scalerScalar;
            scalerScalar.setEnableSIMD(false);
            cv::Mat outScalar = cv::Mat::zeros(dstH, dstW, CV_8UC3);

            ParallelEngine::ScaleImage(inputImage, outScalar, blocks, scalerScalar, scale, scale, numThreads);

            auto t0 = std::chrono::high_resolution_clock::now();
            const int iterations = (numThreads == 1) ? 2 : 4;
            for (int i = 0; i < iterations; ++i) {
                ParallelEngine::ScaleImage(inputImage, outScalar, blocks, scalerScalar, scale, scale, numThreads);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            double msScalar = std::chrono::duration<double, std::milli>(t1 - t0).count() / iterations;

            BilinearScaler scalerAVX2;
            scalerAVX2.setEnableSIMD(true);
            cv::Mat outAVX2 = cv::Mat::zeros(dstH, dstW, CV_8UC3);

            ParallelEngine::ScaleImage(inputImage, outAVX2, blocks, scalerAVX2, scale, scale, numThreads);

            t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                ParallelEngine::ScaleImage(inputImage, outAVX2, blocks, scalerAVX2, scale, scale, numThreads);
            }
            t1 = std::chrono::high_resolution_clock::now();
            double msAVX2 = std::chrono::duration<double, std::milli>(t1 - t0).count() / iterations;

            cv::Mat diff;
            cv::absdiff(outScalar, outAVX2, diff);
            double minVal, maxVal;
            cv::minMaxLoc(diff.reshape(1), &minVal, &maxVal);

            std::cout << "  Scalar Time:  " << msScalar << " ms\n";
            std::cout << "  AVX2 Time:    " << msAVX2 << " ms\n";
            std::cout << "  SIMD Speedup: " << (msScalar / msAVX2) << "x\n";
            std::cout << "  Max Pixel Diff: " << maxVal << " (threshold <= 1)\n";
            std::cout << "  Status: " << (maxVal <= 1.0 ? "PASS (Accurate)" : "FAIL (Discrepancy)") << "\n\n";
        }

        // --- ТЕСТ 2: BicubicScaler ---
        {
            std::cout << "--- [2] Bicubic Scaler ---\n";
            BicubicScaler scalerScalar;
            scalerScalar.setEnableSIMD(false);
            cv::Mat outScalar = cv::Mat::zeros(dstH, dstW, CV_8UC3);

            ParallelEngine::ScaleImage(inputImage, outScalar, blocks, scalerScalar, scale, scale, numThreads);

            auto t0 = std::chrono::high_resolution_clock::now();
            const int iterations = (numThreads == 1) ? 2 : 3;
            for (int i = 0; i < iterations; ++i) {
                ParallelEngine::ScaleImage(inputImage, outScalar, blocks, scalerScalar, scale, scale, numThreads);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            double msScalar = std::chrono::duration<double, std::milli>(t1 - t0).count() / iterations;

            BicubicScaler scalerAVX2;
            scalerAVX2.setEnableSIMD(true);
            cv::Mat outAVX2 = cv::Mat::zeros(dstH, dstW, CV_8UC3);

            ParallelEngine::ScaleImage(inputImage, outAVX2, blocks, scalerAVX2, scale, scale, numThreads);

            t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                ParallelEngine::ScaleImage(inputImage, outAVX2, blocks, scalerAVX2, scale, scale, numThreads);
            }
            t1 = std::chrono::high_resolution_clock::now();
            double msAVX2 = std::chrono::duration<double, std::milli>(t1 - t0).count() / iterations;

            cv::Mat diff;
            cv::absdiff(outScalar, outAVX2, diff);
            double minVal, maxVal;
            cv::minMaxLoc(diff.reshape(1), &minVal, &maxVal);

            std::cout << "  Scalar Time:  " << msScalar << " ms\n";
            std::cout << "  AVX2 Time:    " << msAVX2 << " ms\n";
            std::cout << "  SIMD Speedup: " << (msScalar / msAVX2) << "x\n";
            std::cout << "  Max Pixel Diff: " << maxVal << " (threshold <= 1)\n";
            std::cout << "  Status: " << (maxVal <= 1.0 ? "PASS (Accurate)" : "FAIL (Discrepancy)") << "\n\n";
        }

        // --- ТЕСТ 3: LanczosScaler ---
        {
            std::cout << "--- [3] Lanczos-3 Scaler ---\n";
            LanczosScaler scalerScalar;
            scalerScalar.setEnableSIMD(false);
            cv::Mat outScalar = cv::Mat::zeros(dstH, dstW, CV_8UC3);

            ParallelEngine::ScaleImage(inputImage, outScalar, blocks, scalerScalar, scale, scale, numThreads);

            auto t0 = std::chrono::high_resolution_clock::now();
            const int iterations = (numThreads == 1) ? 1 : 2;
            for (int i = 0; i < iterations; ++i) {
                ParallelEngine::ScaleImage(inputImage, outScalar, blocks, scalerScalar, scale, scale, numThreads);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            double msScalar = std::chrono::duration<double, std::milli>(t1 - t0).count() / iterations;

            LanczosScaler scalerAVX2;
            scalerAVX2.setEnableSIMD(true);
            cv::Mat outAVX2 = cv::Mat::zeros(dstH, dstW, CV_8UC3);

            ParallelEngine::ScaleImage(inputImage, outAVX2, blocks, scalerAVX2, scale, scale, numThreads);

            t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                ParallelEngine::ScaleImage(inputImage, outAVX2, blocks, scalerAVX2, scale, scale, numThreads);
            }
            t1 = std::chrono::high_resolution_clock::now();
            double msAVX2 = std::chrono::duration<double, std::milli>(t1 - t0).count() / iterations;

            cv::Mat diff;
            cv::absdiff(outScalar, outAVX2, diff);
            double minVal, maxVal;
            cv::minMaxLoc(diff.reshape(1), &minVal, &maxVal);

            std::cout << "  Scalar Time:  " << msScalar << " ms\n";
            std::cout << "  AVX2 Time:    " << msAVX2 << " ms\n";
            std::cout << "  SIMD Speedup: " << (msScalar / msAVX2) << "x\n";
            std::cout << "  Max Pixel Diff: " << maxVal << " (threshold <= 1)\n";
            std::cout << "  Status: " << (maxVal <= 1.0 ? "PASS (Accurate)" : "FAIL (Discrepancy)") << "\n\n";
        }

        // --- ТЕСТ 4: AdaptiveScaler ---
        {
            std::cout << "--- [4] Adaptive Sobel Scaler ---\n";
            AdaptiveScaler scalerScalar;
            scalerScalar.setEnableSIMD(false);
            cv::Mat outScalar = cv::Mat::zeros(dstH, dstW, CV_8UC3);

            ParallelEngine::ScaleImage(inputImage, outScalar, blocks, scalerScalar, scale, scale, numThreads);

            auto t0 = std::chrono::high_resolution_clock::now();
            const int iterations = (numThreads == 1) ? 1 : 2;
            for (int i = 0; i < iterations; ++i) {
                ParallelEngine::ScaleImage(inputImage, outScalar, blocks, scalerScalar, scale, scale, numThreads);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            double msScalar = std::chrono::duration<double, std::milli>(t1 - t0).count() / iterations;

            AdaptiveScaler scalerAVX2;
            scalerAVX2.setEnableSIMD(true);
            cv::Mat outAVX2 = cv::Mat::zeros(dstH, dstW, CV_8UC3);

            ParallelEngine::ScaleImage(inputImage, outAVX2, blocks, scalerAVX2, scale, scale, numThreads);

            t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < iterations; ++i) {
                ParallelEngine::ScaleImage(inputImage, outAVX2, blocks, scalerAVX2, scale, scale, numThreads);
            }
            t1 = std::chrono::high_resolution_clock::now();
            double msAVX2 = std::chrono::duration<double, std::milli>(t1 - t0).count() / iterations;

            cv::Mat diff;
            cv::absdiff(outScalar, outAVX2, diff);
            double minVal, maxVal;
            cv::minMaxLoc(diff.reshape(1), &minVal, &maxVal);

            std::cout << "  Scalar Time:  " << msScalar << " ms\n";
            std::cout << "  AVX2 Time:    " << msAVX2 << " ms\n";
            std::cout << "  SIMD Speedup: " << (msScalar / msAVX2) << "x\n";
            std::cout << "  Max Pixel Diff: " << maxVal << " (threshold <= 1)\n";
            std::cout << "  Status: " << (maxVal <= 1.0 ? "PASS (Accurate)" : "FAIL (Discrepancy)") << "\n\n";
        }
    }

    std::cout << "========================================================\n";
    std::cout << "   Full Core SIMD AVX2 Verification Completed!          \n";
    std::cout << "========================================================\n";
    return 0;
}
