#ifndef BILINEAR_SCALER_HPP
#define BILINEAR_SCALER_HPP

#include "IScaler.hpp"

class BilinearScaler : public IScaler {
public:
    void ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) override;
};

#endif // BILINEAR_SCALER_HPP
