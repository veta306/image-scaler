#include "BilinearScaler.hpp"
#include <algorithm>
#include <cmath>

void BilinearScaler::ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) {
    if (input.empty() || output.empty()) {
        return;
    }

    int inCols = input.cols;
    int inRows = input.rows;
    int channels = input.channels();

    // Iterate over each pixel in the target block coordinates of the output image
    for (int y_out = blockRect.y; y_out < blockRect.y + blockRect.height; ++y_out) {
        // Clamp to prevent out-of-bounds in output image
        if (y_out >= output.rows) continue;

        for (int x_out = blockRect.x; x_out < blockRect.x + blockRect.width; ++x_out) {
            if (x_out >= output.cols) continue;

            // Pixel-center alignment mapping to the original image
            double x_in = (x_out + 0.5) / scaleX - 0.5;
            double y_in = (y_out + 0.5) / scaleY - 0.5;

            // Clamp input coordinates to edge to prevent out-of-bounds
            if (x_in < 0) { x_in = 0; }
            if (y_in < 0) { y_in = 0; }
            if (x_in >= inCols - 1) { x_in = inCols - 1; }
            if (y_in >= inRows - 1) { y_in = inRows - 1; }

            int x0 = static_cast<int>(x_in);
            int y0 = static_cast<int>(y_in);
            
            int x1 = (x0 < inCols - 1) ? x0 + 1 : x0;
            int y1 = (y0 < inRows - 1) ? y0 + 1 : y0;

            double dx = x_in - x0;
            double dy = y_in - y0;

            double w00 = (1.0 - dx) * (1.0 - dy);
            double w10 = dx * (1.0 - dy);
            double w01 = (1.0 - dx) * dy;
            double w11 = dx * dy;

            if (channels == 3) {
                // Get pixels from the 4 nearest neighbors (BGR)
                const cv::Vec3b& p00 = input.at<cv::Vec3b>(y0, x0);
                const cv::Vec3b& p10 = input.at<cv::Vec3b>(y0, x1);
                const cv::Vec3b& p01 = input.at<cv::Vec3b>(y1, x0);
                const cv::Vec3b& p11 = input.at<cv::Vec3b>(y1, x1);

                cv::Vec3b& outPixel = output.at<cv::Vec3b>(y_out, x_out);

                for (int c = 0; c < 3; ++c) {
                    double val = w00 * p00[c] + w10 * p10[c] + w01 * p01[c] + w11 * p11[c];
                    outPixel[c] = static_cast<uchar>(std::max(0.0, std::min(val, 255.0)));
                }
            } else if (channels == 1) {
                // 1-channel Grayscale
                uchar p00 = input.at<uchar>(y0, x0);
                uchar p10 = input.at<uchar>(y0, x1);
                uchar p01 = input.at<uchar>(y1, x0);
                uchar p11 = input.at<uchar>(y1, x1);

                double val = w00 * p00 + w10 * p10 + w01 * p01 + w11 * p11;
                output.at<uchar>(y_out, x_out) = static_cast<uchar>(std::max(0.0, std::min(val, 255.0)));
            }
        }
    }
}
