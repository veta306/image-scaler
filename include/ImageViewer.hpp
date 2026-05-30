#ifndef IMAGEVIEWER_HPP
#define IMAGEVIEWER_HPP

#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QString>
#include <QPoint>
#include <opencv2/opencv.hpp>

class ImageViewer : public QFrame {
    Q_OBJECT

public:
    explicit ImageViewer(const QString& defaultText, QWidget* parent = nullptr);
    ~ImageViewer() = default;

    void setImage(const cv::Mat& mat);
    const cv::Mat& image() const;
    
    void setZoom(double zoom);
    double zoom() const;

    void updateViewer();
    void setPixmap(const QPixmap& pixmap);
    void setStatusText(const QString& text);
    void clear();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QScrollArea* scrollArea;
    QLabel* lblImage;
    QLabel* lblStatus;

    cv::Mat currentMat;
    double m_zoom;
    QString m_defaultText;

    bool isPanning;
    QPoint panStartPos;
    int startScrollX;
    int startScrollY;
};

#endif // IMAGEVIEWER_HPP
