#include "ImageViewer.hpp"
#include "IO_Manager.hpp"
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <cmath>
#include <algorithm>

/**
 * @brief Конструктор класу ImageViewer. Ініціалізує дочірні елементи та встановлює фільтри подій.
 */
ImageViewer::ImageViewer(const QString& defaultText, QWidget* parent)
    : QFrame(parent), m_zoom(1.0), m_defaultText(defaultText), isPanning(false), startScrollX(0), startScrollY(0) {
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    lblImage = new QLabel(scrollArea);
    lblImage->setAlignment(Qt::AlignCenter);
    lblImage->setText(m_defaultText);
    lblImage->setStyleSheet("font-size: 14px;");
    lblImage->setCursor(Qt::OpenHandCursor);
    
    scrollArea->setWidget(lblImage);

    lblStatus = new QLabel("---x--- 100%", this);
    lblStatus->setStyleSheet("font-size: 11px; font-weight: bold; color: #6c757d;");
    lblStatus->setAlignment(Qt::AlignRight);

    layout->addWidget(scrollArea);
    layout->addWidget(lblStatus);

    scrollArea->viewport()->installEventFilter(this);
    lblImage->installEventFilter(this);
}

/**
 * @brief Встановлює нове зображення cv::Mat для відображення.
 */
void ImageViewer::setImage(const cv::Mat& mat) {
    currentMat = mat;
    updateViewer();
}

/**
 * @brief Повертає поточне зображення cv::Mat.
 */
const cv::Mat& ImageViewer::image() const {
    return currentMat;
}

/**
 * @brief Встановлює коефіцієнт масштабування (зуму).
 */
void ImageViewer::setZoom(double zoom) {
    m_zoom = std::max(0.1, std::min(10.0, zoom));
    updateViewer();
}

/**
 * @brief Повертає поточний коефіцієнт масштабування (зуму).
 */
double ImageViewer::zoom() const {
    return m_zoom;
}

/**
 * @brief Оновлює відображення зображення з поточним рівнем масштабування.
 */
void ImageViewer::updateViewer() {
    if (!currentMat.empty()) {
        QImage qimg = IO_Manager::MatToQImage(currentMat);
        if (std::abs(m_zoom - 1.0) > 0.001) {
            int w = static_cast<int>(std::round(qimg.width() * m_zoom));
            int h = static_cast<int>(std::round(qimg.height() * m_zoom));
            if (w > 0 && h > 0) {
                qimg = qimg.scaled(w, h, Qt::KeepAspectRatio, Qt::FastTransformation);
            }
        }
        lblImage->setPixmap(QPixmap::fromImage(qimg));
        
        int zoomPct = static_cast<int>(std::round(m_zoom * 100));
        lblStatus->setText(QString("%1x%2 %3%")
            .arg(currentMat.cols)
            .arg(currentMat.rows)
            .arg(zoomPct));
    } else {
        lblImage->clear();
        lblImage->setText(m_defaultText);
        lblStatus->setText("---x--- 100%");
    }
}

/**
 * @brief Встановлює готову піксельну карту для відображення безпосередньо.
 */
void ImageViewer::setPixmap(const QPixmap& pixmap) {
    lblImage->setPixmap(pixmap);
}

/**
 * @brief Оновлює текст статусу в нижньому кутку переглядача.
 */
void ImageViewer::setStatusText(const QString& text) {
    lblStatus->setText(text);
}

/**
 * @brief Очищує поточне зображення та повертає віджет до початкового стану.
 */
void ImageViewer::clear() {
    currentMat = cv::Mat();
    lblImage->clear();
    lblImage->setText(m_defaultText);
    lblStatus->setText("---x--- 100%");
}

/**
 * @brief Перехоплює та обробляє події миші для панування та зуму колесом.
 */
bool ImageViewer::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            double angleDelta = wheelEvent->angleDelta().y();
            double factor = (angleDelta > 0) ? 1.1 : 0.9;
            double newZoom = m_zoom * factor;
            if (newZoom >= 0.1 && newZoom <= 10.0) {
                m_zoom = newZoom;
                updateViewer();
            }
            wheelEvent->accept();
            return true;
        }
    }
    
    bool isViewport = (obj == scrollArea->viewport());
    bool isLabel = (obj == lblImage);
    
    if (isViewport || isLabel) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                isPanning = true;
                panStartPos = mouseEvent->globalPosition().toPoint();
                startScrollX = scrollArea->horizontalScrollBar()->value();
                startScrollY = scrollArea->verticalScrollBar()->value();
                
                if (isLabel) {
                    lblImage->setCursor(Qt::ClosedHandCursor);
                } else {
                    scrollArea->viewport()->setCursor(Qt::ClosedHandCursor);
                }
                
                mouseEvent->accept();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (isPanning) {
                QPoint currentPos = mouseEvent->globalPosition().toPoint();
                QPoint delta = currentPos - panStartPos;
                
                scrollArea->horizontalScrollBar()->setValue(startScrollX - delta.x());
                scrollArea->verticalScrollBar()->setValue(startScrollY - delta.y());
                
                mouseEvent->accept();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && isPanning) {
                isPanning = false;
                
                if (isLabel) {
                    lblImage->setCursor(Qt::OpenHandCursor);
                } else {
                    scrollArea->viewport()->setCursor(Qt::OpenHandCursor);
                }
                mouseEvent->accept();
                return true;
            }
        }
    }
    
    return QFrame::eventFilter(obj, event);
}
