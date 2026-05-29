#ifndef BILINEAR_SCALER_HPP
#define BILINEAR_SCALER_HPP

#include "IScaler.hpp"

class BilinearScaler : public IScaler {
private:
    bool m_enableSharpen = false;
    bool m_enableOverlap = false;

public:
    void ScaleBlock(const cv::Mat& input, cv::Mat& output, const cv::Rect& blockRect, double scaleX, double scaleY) override;

    void setEnableSharpen(bool enable) { m_enableSharpen = enable; }
    void setEnableOverlap(bool enable) { m_enableOverlap = enable; }

    bool isSharpenEnabled() const { return m_enableSharpen; }
    bool isOverlapEnabled() const { return m_enableOverlap; }
};

#endif // BILINEAR_SCALER_HPP
