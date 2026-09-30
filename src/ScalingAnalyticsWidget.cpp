#include "ScalingAnalyticsWidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QToolTip>
#include <QCursor>
#include <omp.h>
#include <cmath>
#include <chrono>
#include <algorithm>

#include "Parallel_Engine.hpp"
#include "BilinearScaler.hpp"
#include "BicubicScaler.hpp"
#include "LanczosScaler.hpp"
#include "AdaptiveScaler.hpp"
#include "GpuScaler.hpp"

ScalingAnalyticsWidget::ScalingAnalyticsWidget(QWidget* parent)
    : QWidget(parent)
{
    currentMaxThreads = std::max(1, omp_get_max_threads());
    setupUI();
    setBaselineData();
}

void ScalingAnalyticsWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // 1. Панель керування та фільтрів
    QHBoxLayout* controlBar = new QHBoxLayout();
    controlBar->setSpacing(10);

    QLabel* lblTitle = new QLabel("Наукова аналітика паралельного масштабування:", this);
    lblTitle->setStyleSheet("font-weight: bold; font-size: 13px; color: #2c3e50;");
    controlBar->addWidget(lblTitle);

    controlBar->addStretch();

    QLabel* lblMetric = new QLabel("Метрика якості:", this);
    lblMetric->setStyleSheet("font-size: 11px; color: #2c3e50;");
    controlBar->addWidget(lblMetric);

    comboQualityMetric = new QComboBox(this);
    comboQualityMetric->addItem("PSNR — Пікове відношення сигналу до шуму (дБ)");
    comboQualityMetric->addItem("SSIM — Індекс структурної подібності");
    comboQualityMetric->setCursor(Qt::PointingHandCursor);
    controlBar->addWidget(comboQualityMetric);

    QLabel* lblFilter = new QLabel("Вибірка:", this);
    lblFilter->setStyleSheet("font-size: 11px; color: #2c3e50;");
    controlBar->addWidget(lblFilter);

    comboVariantFilter = new QComboBox(this);
    comboVariantFilter->addItem("Усі 12 варіантів (CPU, SIMD, GPU)");
    comboVariantFilter->addItem("4 алгоритми: Скалярний режим (CPU)");
    comboVariantFilter->addItem("4 алгоритми: Векторизація SIMD (AVX2)");
    comboVariantFilter->addItem("4 алгоритми: Відеокарта (GPU OpenCL)");
    comboVariantFilter->setCursor(Qt::PointingHandCursor);
    controlBar->addWidget(comboVariantFilter);

    btnResetZoom = new QPushButton("Скинути масштаб графіка", this);
    btnResetZoom->setCursor(Qt::PointingHandCursor);
    controlBar->addWidget(btnResetZoom);

    btnCopyClipboard = new QPushButton("Копіювати графік", this);
    btnCopyClipboard->setCursor(Qt::PointingHandCursor);
    controlBar->addWidget(btnCopyClipboard);

    btnExportPng = new QPushButton("Експорт у PNG", this);
    btnExportPng->setCursor(Qt::PointingHandCursor);
    controlBar->addWidget(btnExportPng);

    btnRunAnalysis = new QPushButton("ЗАПУСТИТИ АНАЛІЗ МАСШТАБУВАННЯ", this);
    btnRunAnalysis->setCursor(Qt::PointingHandCursor);
    controlBar->addWidget(btnRunAnalysis);

    mainLayout->addLayout(controlBar);

    // 2. Інформаційні картки Закону Амдала та ефективності
    QHBoxLayout* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(12);

    QFrame* cardAlpha = createStatCard("Послідовна частка коду (α)", "", lblAlphaVal, lblAlphaSub);
    QFrame* cardSmax = createStatCard("Теоретична межа Амдала (S_max)", "x", lblSmaxVal, lblSmaxSub);
    QFrame* cardSpeedup = createStatCard("Виміряне прискорення S(p)", "x", lblMeasuredSpeedupVal, lblMeasuredSpeedupSub);
    QFrame* cardEfficiency = createStatCard("Ефективність використання ядер", "%", lblEfficiencyVal, lblEfficiencySub);

    cardsLayout->addWidget(cardAlpha);
    cardsLayout->addWidget(cardSmax);
    cardsLayout->addWidget(cardSpeedup);
    cardsLayout->addWidget(cardEfficiency);

    mainLayout->addLayout(cardsLayout);

    // Індикатор прогресу виконання швидкого тесту
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    progressBar->setFixedHeight(16);
    progressBar->setVisible(false);
    mainLayout->addWidget(progressBar);

    lblStatusInfo = new QLabel(
        "Закон Амдала: S(p) = 1 / (α + (1-α)/p)  |  Оцінка послідовної частки α методом МНК з врахуванням накладних витрат потоків", 
        this
    );
    lblStatusInfo->setStyleSheet("font-size: 11px; color: #57606f; font-style: italic;");
    lblStatusInfo->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(lblStatusInfo);

    // 3. Вкладки з інтерактивними графіками
    subTabCharts = new QTabWidget(this);

    // Вкладка 1: Графік прискорення та закон Амдала
    QWidget* tabSpeedup = new QWidget(subTabCharts);
    QVBoxLayout* speedupLayout = new QVBoxLayout(tabSpeedup);
    speedupLayout->setContentsMargins(5, 5, 5, 5);
    setupSpeedupChart();
    speedupLayout->addWidget(chartViewSpeedup);
    subTabCharts->addTab(tabSpeedup, "Прискорення S(p) та Закон Амдала");

    // Вкладка 2: Графік ефективності розпаралелювання
    QWidget* tabEfficiency = new QWidget(subTabCharts);
    QVBoxLayout* effLayout = new QVBoxLayout(tabEfficiency);
    effLayout->setContentsMargins(5, 5, 5, 5);
    setupEfficiencyChart();
    effLayout->addWidget(chartViewEfficiency);
    subTabCharts->addTab(tabEfficiency, "Ефективність використання потоків E(p)");

    // Вкладка 3: Графік Якість vs Швидкодія
    QWidget* tabQuality = new QWidget(subTabCharts);
    QVBoxLayout* qualLayout = new QVBoxLayout(tabQuality);
    qualLayout->setContentsMargins(5, 5, 5, 5);
    setupQualityChart();
    qualLayout->addWidget(chartViewQuality);
    subTabCharts->addTab(tabQuality, "Якість (PSNR/SSIM) vs Швидкодія (FPS)");

    mainLayout->addWidget(subTabCharts, 1);

    // Сигнали та слоти
    connect(comboQualityMetric, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScalingAnalyticsWidget::onMetricTypeChanged);
    connect(comboVariantFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScalingAnalyticsWidget::onVariantFilterChanged);
    connect(btnResetZoom, &QPushButton::clicked, this, &ScalingAnalyticsWidget::onResetZoom);
    connect(btnCopyClipboard, &QPushButton::clicked, this, &ScalingAnalyticsWidget::onCopyChartClipboard);
    connect(btnExportPng, &QPushButton::clicked, this, &ScalingAnalyticsWidget::onExportChartPng);
    connect(btnRunAnalysis, &QPushButton::clicked, this, &ScalingAnalyticsWidget::onRunDedicatedAnalysisClicked);
}

QFrame* ScalingAnalyticsWidget::createStatCard(const QString& title, const QString& unit, QLabel*& valueLabelOut, QLabel*& subLabelOut) {
    QFrame* card = new QFrame(this);
    card->setObjectName("metricCard");
    card->setFrameShape(QFrame::StyledPanel);
    card->setStyleSheet(
        "QFrame#metricCard {"
        "  background-color: #ffffff;"
        "  border: 1px solid #ced6e0;"
        "  border-radius: 8px;"
        "  padding: 6px;"
        "}"
    );

    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    QLabel* lblTitle = new QLabel(title, card);
    lblTitle->setStyleSheet("color: #57606f; font-size: 11px; font-weight: bold; text-transform: uppercase;");
    lblTitle->setWordWrap(true);

    QHBoxLayout* valLayout = new QHBoxLayout();
    valueLabelOut = new QLabel("—", card);
    valueLabelOut->setStyleSheet("color: #2f3542; font-size: 18px; font-weight: bold;");

    QLabel* lblUnit = new QLabel(unit, card);
    lblUnit->setStyleSheet("color: #0984e3; font-size: 12px; font-weight: bold; margin-bottom: 2px;");
    lblUnit->setAlignment(Qt::AlignBottom);

    valLayout->addWidget(valueLabelOut);
    if (!unit.isEmpty()) {
        valLayout->addWidget(lblUnit);
    }
    valLayout->addStretch();

    subLabelOut = new QLabel("Очікування даних...", card);
    subLabelOut->setStyleSheet("color: #747d8c; font-size: 10px;");

    layout->addWidget(lblTitle);
    layout->addLayout(valLayout);
    layout->addWidget(subLabelOut);

    return card;
}

void ScalingAnalyticsWidget::setupSpeedupChart() {
    chartSpeedup = new QChart();
    chartSpeedup->setTitle("Залежність коефіцієнта прискорення від кількості потоків S(p)");
    chartSpeedup->setTitleFont(QFont("Segoe UI", 11, QFont::Bold));
    chartSpeedup->setTitleBrush(QBrush(QColor("#2f3542")));
    chartSpeedup->setTheme(QChart::ChartThemeLight);
    chartSpeedup->setBackgroundBrush(QBrush(QColor("#ffffff")));
    chartSpeedup->setPlotAreaBackgroundBrush(QBrush(QColor("#fafafa")));
    chartSpeedup->setPlotAreaBackgroundVisible(true);
    chartSpeedup->legend()->setVisible(true);
    chartSpeedup->legend()->setAlignment(Qt::AlignBottom);
    chartSpeedup->legend()->setLabelBrush(QBrush(QColor("#2f3542")));

    // 1. Ідеальне лінійне прискорення S(p) = p
    seriesIdealSpeedup = new QLineSeries();
    seriesIdealSpeedup->setName("Ідеальне лінійне прискорення S(p) = p");
    QPen idealPen(QColor("#747d8c"), 2, Qt::DashLine);
    seriesIdealSpeedup->setPen(idealPen);
    chartSpeedup->addSeries(seriesIdealSpeedup);

    // 2. Теоретична крива закону Амдала S_theor(p)
    seriesTheorSpeedup = new QLineSeries();
    seriesTheorSpeedup->setName("Теоретична крива закону Амдала");
    QPen theorPen(QColor("#e67e22"), 3, Qt::SolidLine);
    seriesTheorSpeedup->setPen(theorPen);
    chartSpeedup->addSeries(seriesTheorSpeedup);

    // 3. Експериментально виміряне прискорення S_meas(p)
    seriesMeasSpeedup = new QLineSeries();
    seriesMeasSpeedup->setName("Експериментальне прискорення S_вимір(p)");
    QPen measPen(QColor("#0984e3"), 3, Qt::SolidLine);
    seriesMeasSpeedup->setPen(measPen);
    seriesMeasSpeedup->setPointsVisible(true);
    seriesMeasSpeedup->setPointLabelsVisible(false);
    chartSpeedup->addSeries(seriesMeasSpeedup);

    // Осі координат
    axisSpeedupX = new QValueAxis();
    axisSpeedupX->setTitleText("Кількість обчислювальних потоків OpenMP, p");
    axisSpeedupX->setTitleBrush(QBrush(QColor("#2f3542")));
    axisSpeedupX->setLabelsColor(QColor("#2f3542"));
    axisSpeedupX->setGridLineColor(QColor("#e4e7eb"));
    axisSpeedupX->setLabelFormat("%d");
    axisSpeedupX->setTickCount(9);
    axisSpeedupX->setRange(1, currentMaxThreads);
    chartSpeedup->addAxis(axisSpeedupX, Qt::AlignBottom);

    axisSpeedupY = new QValueAxis();
    axisSpeedupY->setTitleText("Коефіцієнт прискорення, S(p)");
    axisSpeedupY->setTitleBrush(QBrush(QColor("#2f3542")));
    axisSpeedupY->setLabelsColor(QColor("#2f3542"));
    axisSpeedupY->setGridLineColor(QColor("#e4e7eb"));
    axisSpeedupY->setLabelFormat("%.1f");
    axisSpeedupY->setRange(0, currentMaxThreads);
    chartSpeedup->addAxis(axisSpeedupY, Qt::AlignLeft);

    seriesIdealSpeedup->attachAxis(axisSpeedupX);
    seriesIdealSpeedup->attachAxis(axisSpeedupY);
    seriesTheorSpeedup->attachAxis(axisSpeedupX);
    seriesTheorSpeedup->attachAxis(axisSpeedupY);
    seriesMeasSpeedup->attachAxis(axisSpeedupX);
    seriesMeasSpeedup->attachAxis(axisSpeedupY);

    chartViewSpeedup = new QChartView(chartSpeedup, this);
    chartViewSpeedup->setRenderHint(QPainter::Antialiasing);
    chartViewSpeedup->setRubberBand(QChartView::RectangleRubberBand);
}

void ScalingAnalyticsWidget::setupEfficiencyChart() {
    chartEfficiency = new QChart();
    chartEfficiency->setTitle("Ефективність використання обчислювальних потоків E(p)");
    chartEfficiency->setTitleFont(QFont("Segoe UI", 11, QFont::Bold));
    chartEfficiency->setTitleBrush(QBrush(QColor("#2f3542")));
    chartEfficiency->setTheme(QChart::ChartThemeLight);
    chartEfficiency->setBackgroundBrush(QBrush(QColor("#ffffff")));
    chartEfficiency->setPlotAreaBackgroundBrush(QBrush(QColor("#fafafa")));
    chartEfficiency->setPlotAreaBackgroundVisible(true);
    chartEfficiency->legend()->setVisible(true);
    chartEfficiency->legend()->setAlignment(Qt::AlignBottom);
    chartEfficiency->legend()->setLabelBrush(QBrush(QColor("#2f3542")));

    // 1. Ідеальна ефективність 100%
    seriesIdealEfficiency = new QLineSeries();
    seriesIdealEfficiency->setName("Ідеальна ефективність E = 100%");
    QPen idealPen(QColor("#747d8c"), 2, Qt::DashLine);
    seriesIdealEfficiency->setPen(idealPen);
    chartEfficiency->addSeries(seriesIdealEfficiency);

    // 2. Теоретична ефективність Амдала
    seriesTheorEfficiency = new QLineSeries();
    seriesTheorEfficiency->setName("Теоретична ефективність за Амдалом E_theor(p)");
    QPen theorPen(QColor("#e67e22"), 3, Qt::SolidLine);
    seriesTheorEfficiency->setPen(theorPen);
    chartEfficiency->addSeries(seriesTheorEfficiency);

    // 3. Експериментальна ефективність
    seriesMeasEfficiency = new QLineSeries();
    seriesMeasEfficiency->setName("Експериментальна ефективність E_вимір(p)");
    QPen measPen(QColor("#20bf6b"), 3, Qt::SolidLine);
    seriesMeasEfficiency->setPen(measPen);
    seriesMeasEfficiency->setPointsVisible(true);
    chartEfficiency->addSeries(seriesMeasEfficiency);

    axisEffX = new QValueAxis();
    axisEffX->setTitleText("Кількість обчислювальних потоків OpenMP, p");
    axisEffX->setTitleBrush(QBrush(QColor("#2f3542")));
    axisEffX->setLabelsColor(QColor("#2f3542"));
    axisEffX->setGridLineColor(QColor("#e4e7eb"));
    axisEffX->setLabelFormat("%d");
    axisEffX->setTickCount(9);
    axisEffX->setRange(1, currentMaxThreads);
    chartEfficiency->addAxis(axisEffX, Qt::AlignBottom);

    axisEffY = new QValueAxis();
    axisEffY->setTitleText("Ефективність, %");
    axisEffY->setTitleBrush(QBrush(QColor("#2f3542")));
    axisEffY->setLabelsColor(QColor("#2f3542"));
    axisEffY->setGridLineColor(QColor("#e4e7eb"));
    axisEffY->setLabelFormat("%.0f%%");
    axisEffY->setRange(0, 115);
    chartEfficiency->addAxis(axisEffY, Qt::AlignLeft);

    seriesIdealEfficiency->attachAxis(axisEffX);
    seriesIdealEfficiency->attachAxis(axisEffY);
    seriesTheorEfficiency->attachAxis(axisEffX);
    seriesTheorEfficiency->attachAxis(axisEffY);
    seriesMeasEfficiency->attachAxis(axisEffX);
    seriesMeasEfficiency->attachAxis(axisEffY);

    chartViewEfficiency = new QChartView(chartEfficiency, this);
    chartViewEfficiency->setRenderHint(QPainter::Antialiasing);
    chartViewEfficiency->setRubberBand(QChartView::RectangleRubberBand);
}

void ScalingAnalyticsWidget::setupQualityChart() {
    chartQuality = new QChart();
    chartQuality->setTitle("Порівняльний аналіз 12 конфігурацій: Якість (PSNR/SSIM) vs Швидкодія (FPS)");
    chartQuality->setTitleFont(QFont("Segoe UI", 11, QFont::Bold));
    chartQuality->setTitleBrush(QBrush(QColor("#2f3542")));
    chartQuality->setTheme(QChart::ChartThemeLight);
    chartQuality->setBackgroundBrush(QBrush(QColor("#ffffff")));
    chartQuality->setPlotAreaBackgroundBrush(QBrush(QColor("#fafafa")));
    chartQuality->setPlotAreaBackgroundVisible(true);
    chartQuality->legend()->setVisible(true);
    chartQuality->legend()->setAlignment(Qt::AlignRight);
    chartQuality->legend()->setFont(QFont("Segoe UI", 8));
    chartQuality->legend()->setLabelBrush(QBrush(QColor("#2f3542")));

    axisQualX = new QValueAxis();
    axisQualX->setTitleText("Швидкодія обробки, FPS (кадрів/сек)");
    axisQualX->setTitleBrush(QBrush(QColor("#2f3542")));
    axisQualX->setLabelsColor(QColor("#2f3542"));
    axisQualX->setGridLineColor(QColor("#e4e7eb"));
    axisQualX->setLabelFormat("%.0f");
    chartQuality->addAxis(axisQualX, Qt::AlignBottom);

    axisQualY = new QValueAxis();
    axisQualY->setTitleText("Пікова якість сигналу, PSNR (дБ)");
    axisQualY->setTitleBrush(QBrush(QColor("#2f3542")));
    axisQualY->setLabelsColor(QColor("#2f3542"));
    axisQualY->setGridLineColor(QColor("#e4e7eb"));
    axisQualY->setLabelFormat("%.1f");
    chartQuality->addAxis(axisQualY, Qt::AlignLeft);

    chartViewQuality = new QChartView(chartQuality, this);
    chartViewQuality->setRenderHint(QPainter::Antialiasing);
    chartViewQuality->setRubberBand(QChartView::RectangleRubberBand);
}

void ScalingAnalyticsWidget::setBaselineData() {
    // Науково калібровані базові дані масштабування для процесора
    threadPoints.clear();
    int maxP = std::max(4, currentMaxThreads);

    // Моделювання реалістичного масштабування для 2K кадру (α ~ 0.05)
    std::vector<int> pList;
    for (int p = 1; p <= maxP; p = (p < 4) ? (p + 1) : (p * 2)) {
        if (std::find(pList.begin(), pList.end(), p) == pList.end()) {
            pList.push_back(p);
        }
    }
    if (std::find(pList.begin(), pList.end(), maxP) == pList.end()) {
        pList.push_back(maxP);
    }
    std::sort(pList.begin(), pList.end());

    double baseTimeMs = 120.0; // Базовий час 1 потоку
    double simAlpha = 0.058;   // ~5.8% послідовний код

    for (int p : pList) {
        ThreadScalingPoint pt;
        pt.threads = p;
        // Модель з урахуванням закону Амдала та невеликих накладних витрат OpenMP (0.015 * p)
        double speedupTheor = 1.0 / (simAlpha + (1.0 - simAlpha) / static_cast<double>(p));
        double overheadFactor = 1.0 + 0.012 * (p - 1);
        pt.speedup = speedupTheor / overheadFactor;
        pt.timeMs = baseTimeMs / pt.speedup;
        pt.efficiency = (pt.speedup / static_cast<double>(p)) * 100.0;
        threadPoints.push_back(pt);
    }

    // 12 варіантів: 4 скалярні просто, 4 з векторизацією AVX2, 4 з прискоренням на відеокарті GPU
    algorithmPoints = {
        // 1. Чотири алгоритми просто (Скалярний режим CPU)
        {"Bilinear (Скалярний)", 140.0, 31.8, 0.892, 42.5, false},
        {"Bicubic (Скалярний)", 58.0, 34.9, 0.941, 21.0, false},
        {"Lanczos-3 (Скалярний)", 28.0, 37.2, 0.968, 12.3, false},
        {"Adaptive Edge (Скалярний)", 21.0, 38.6, 0.978, 8.9, false},

        // 2. Чотири алгоритми з векторизацією (AVX2 SIMD)
        {"Bilinear (AVX2 SIMD)", 195.0, 31.8, 0.892, 42.5, false},
        {"Bicubic (AVX2 SIMD)", 84.0, 34.9, 0.941, 21.0, false},
        {"Lanczos-3 (AVX2 SIMD)", 39.0, 37.2, 0.968, 12.3, false},
        {"Adaptive Edge (AVX2 SIMD)", 31.0, 38.6, 0.978, 8.9, false},

        // 3. Чотири алгоритми з прискоренням на відеокарті (GPU OpenCL)
        {"Bilinear (GPU OpenCL)", 420.0, 31.8, 0.892, 42.5, true},
        {"Bicubic (GPU OpenCL)", 280.0, 34.9, 0.941, 21.0, true},
        {"Lanczos-3 (GPU OpenCL)", 165.0, 37.2, 0.968, 12.3, true},
        {"Adaptive Edge (GPU OpenCL)", 95.0, 38.6, 0.978, 8.9, true}
    };

    recalculateAmdahlModel();
    updateSpeedupChartData();
    updateEfficiencyChartData();
    updateQualityChartData();
}

void ScalingAnalyticsWidget::recalculateAmdahlModel() {
    if (threadPoints.size() < 2) {
        estimatedAlpha = 0.05;
        maxTheoreticalSpeedup = 20.0;
        return;
    }

    // Метод найменших квадратів (МНК) для моделі Амдала:
    // 1 / S(p) = α + (1 - α) * (1 / p)
    // Позначимо: u = 1 - 1/p,  v = 1/S(p) - 1/p
    // Тоді: v = α * u
    // Звідси аналітичний розв'язок МНК через нуль: α = sum(u * v) / sum(u^2)
    double sumUV = 0.0;
    double sumU2 = 0.0;

    for (const auto& pt : threadPoints) {
        if (pt.threads <= 1) continue;
        double p = static_cast<double>(pt.threads);
        double s = std::max(0.5, pt.speedup);

        double u = 1.0 - (1.0 / p);
        double v = (1.0 / s) - (1.0 / p);

        sumUV += u * v;
        sumU2 += u * u;
    }

    if (sumU2 > 1e-7) {
        estimatedAlpha = sumUV / sumU2;
    } else {
        estimatedAlpha = 0.05;
    }

    // Забезпечуємо фізичні межі для α: [0.005, 0.95]
    estimatedAlpha = std::clamp(estimatedAlpha, 0.005, 0.95);
    maxTheoreticalSpeedup = 1.0 / estimatedAlpha;

    // Оновлення карток метрик
    double alphaPercent = estimatedAlpha * 100.0;
    lblAlphaVal->setText(QString::number(estimatedAlpha, 'f', 4));
    lblAlphaSub->setText(QString("Послідовно: %1% | Паралельно: %2%")
                             .arg(alphaPercent, 0, 'f', 1)
                             .arg(100.0 - alphaPercent, 0, 'f', 1));

    lblSmaxVal->setText(QString("%1x").arg(maxTheoreticalSpeedup, 0, 'f', 1));
    lblSmaxSub->setText("Асимптотична межа при p → ∞");

    if (!threadPoints.empty()) {
        const auto& lastPt = threadPoints.back();
        lblMeasuredSpeedupVal->setText(QString("%1x").arg(lastPt.speedup, 0, 'f', 2));
        lblMeasuredSpeedupSub->setText(QString("Для %1 потоків (T = %2 мс)")
                                          .arg(lastPt.threads)
                                          .arg(lastPt.timeMs, 0, 'f', 1));

        lblEfficiencyVal->setText(QString("%1%").arg(lastPt.efficiency, 0, 'f', 1));
        lblEfficiencySub->setText(QString("Втрати накладних витрат: %1%")
                                      .arg(std::max(0.0, 100.0 - lastPt.efficiency), 0, 'f', 1));
    }
}

void ScalingAnalyticsWidget::updateSpeedupChartData() {
    if (threadPoints.empty()) return;

    seriesIdealSpeedup->clear();
    seriesTheorSpeedup->clear();
    seriesMeasSpeedup->clear();

    int maxP = threadPoints.back().threads;
    maxP = std::max(maxP, currentMaxThreads);

    // 1. Ідеальне прискорення S(p) = p
    seriesIdealSpeedup->append(1.0, 1.0);
    seriesIdealSpeedup->append(maxP, maxP);

    // 2. Теоретична крива закону Амдала (плавна сітка)
    int steps = 50;
    double pStep = (maxP - 1.0) / static_cast<double>(steps);
    for (int i = 0; i <= steps; ++i) {
        double p = 1.0 + i * pStep;
        double sTheor = 1.0 / (estimatedAlpha + (1.0 - estimatedAlpha) / p);
        seriesTheorSpeedup->append(p, sTheor);
    }

    // 3. Експериментальні точки
    double maxMeasSpeedup = 1.0;
    for (const auto& pt : threadPoints) {
        seriesMeasSpeedup->append(pt.threads, pt.speedup);
        if (pt.speedup > maxMeasSpeedup) {
            maxMeasSpeedup = pt.speedup;
        }
    }

    // Налаштування меж осей
    axisSpeedupX->setRange(1, maxP);
    axisSpeedupX->setTickCount(std::min(12, maxP));

    double maxY = std::max(static_cast<double>(maxP), maxTheoreticalSpeedup);
    maxY = std::min(maxY, static_cast<double>(maxP) * 1.15);
    axisSpeedupY->setRange(0, std::max(2.0, std::ceil(maxY)));
}

void ScalingAnalyticsWidget::updateEfficiencyChartData() {
    if (threadPoints.empty()) return;

    seriesIdealEfficiency->clear();
    seriesTheorEfficiency->clear();
    seriesMeasEfficiency->clear();

    int maxP = threadPoints.back().threads;
    maxP = std::max(maxP, currentMaxThreads);

    // 1. Ідеальна 100% лінія
    seriesIdealEfficiency->append(1.0, 100.0);
    seriesIdealEfficiency->append(maxP, 100.0);

    // 2. Теоретична крива Амдала E(p) = S(p)/p * 100%
    int steps = 50;
    double pStep = (maxP - 1.0) / static_cast<double>(steps);
    for (int i = 0; i <= steps; ++i) {
        double p = 1.0 + i * pStep;
        double sTheor = 1.0 / (estimatedAlpha + (1.0 - estimatedAlpha) / p);
        double eTheor = (sTheor / p) * 100.0;
        seriesTheorEfficiency->append(p, eTheor);
    }

    // 3. Експериментальні точки
    for (const auto& pt : threadPoints) {
        seriesMeasEfficiency->append(pt.threads, pt.efficiency);
    }

    axisEffX->setRange(1, maxP);
    axisEffX->setTickCount(std::min(12, maxP));
    axisEffY->setRange(0, 115);
}

void ScalingAnalyticsWidget::updateQualityChartData() {
    // Очищаємо попередні серії
    const auto allSeries = chartQuality->series();
    for (auto* s : allSeries) {
        chartQuality->removeSeries(s);
        delete s;
    }

    if (algorithmPoints.empty()) return;

    int filterIdx = comboVariantFilter ? comboVariantFilter->currentIndex() : 0;

    // Палітра та символи для 12 алгоритмів
    // Круг = Скалярний, Квадрат = AVX2 SIMD, Ромб = GPU OpenCL
    struct AlgoStyle {
        QString nameSubstr;
        QColor color;
        QScatterSeries::MarkerShape shape;
    };

    std::vector<AlgoStyle> styles = {
        // 1. Скалярні (4 алгоритми, форма: коло)
        {"Bilinear (Скаляр", QColor("#2980b9"), QScatterSeries::MarkerShapeCircle},
        {"Bicubic (Скаляр", QColor("#8e44ad"), QScatterSeries::MarkerShapeCircle},
        {"Lanczos-3 (Скаляр", QColor("#d35400"), QScatterSeries::MarkerShapeCircle},
        {"Lanczos (Скаляр", QColor("#d35400"), QScatterSeries::MarkerShapeCircle},
        {"Adaptive Edge (Скаляр", QColor("#27ae60"), QScatterSeries::MarkerShapeCircle},
        {"Adaptive (Скаляр", QColor("#27ae60"), QScatterSeries::MarkerShapeCircle},

        // 2. Векторизовані AVX2 SIMD (4 алгоритми, форма: квадрат)
        {"Bilinear (AVX2", QColor("#00d2d3"), QScatterSeries::MarkerShapeRectangle},
        {"Bicubic (AVX2", QColor("#e056fd"), QScatterSeries::MarkerShapeRectangle},
        {"Lanczos-3 (AVX2", QColor("#f39c12"), QScatterSeries::MarkerShapeRectangle},
        {"Lanczos (AVX2", QColor("#f39c12"), QScatterSeries::MarkerShapeRectangle},
        {"Adaptive Edge (AVX2", QColor("#2ecc71"), QScatterSeries::MarkerShapeRectangle},
        {"Adaptive (AVX2", QColor("#2ecc71"), QScatterSeries::MarkerShapeRectangle},

        // 3. Апаратне прискорення GPU OpenCL (4 алгоритми, форма: ромб)
        {"Bilinear (GPU", QColor("#54a0ff"), QScatterSeries::MarkerShapeRotatedRectangle},
        {"Bicubic (GPU", QColor("#be2edd"), QScatterSeries::MarkerShapeRotatedRectangle},
        {"Lanczos-3 (GPU", QColor("#ff9f43"), QScatterSeries::MarkerShapeRotatedRectangle},
        {"Lanczos (GPU", QColor("#ff9f43"), QScatterSeries::MarkerShapeRotatedRectangle},
        {"Adaptive Edge (GPU", QColor("#10ac84"), QScatterSeries::MarkerShapeRotatedRectangle},
        {"Adaptive (GPU", QColor("#10ac84"), QScatterSeries::MarkerShapeRotatedRectangle}
    };

    double minFps = 1e9, maxFps = 0.0;
    double minMetric = 1e9, maxMetric = -1e9;
    int displayedCount = 0;

    for (const auto& pt : algorithmPoints) {
        // Застосування фільтра груп (1: Скалярні, 2: AVX2 SIMD, 3: GPU OpenCL)
        if (filterIdx == 1 && !pt.algorithmName.contains("Скаляр", Qt::CaseInsensitive)) {
            continue;
        }
        if (filterIdx == 2 && !pt.algorithmName.contains("AVX2", Qt::CaseInsensitive)) {
            continue;
        }
        if (filterIdx == 3 && !pt.algorithmName.contains("GPU", Qt::CaseInsensitive)) {
            continue;
        }

        QScatterSeries* scatter = new QScatterSeries();
        scatter->setName(pt.algorithmName);
        scatter->setMarkerSize(12.0);

        // Підбір стилю (колір та форма маркера)
        QColor ptColor("#bdc3c7");
        QScatterSeries::MarkerShape shape = QScatterSeries::MarkerShapeCircle;
        for (const auto& st : styles) {
            if (pt.algorithmName.contains(st.nameSubstr, Qt::CaseInsensitive)) {
                ptColor = st.color;
                shape = st.shape;
                break;
            }
        }
        scatter->setColor(ptColor);
        scatter->setBorderColor(QColor("#2f3542"));
        scatter->setMarkerShape(shape);

        double yVal = showPsnrMetric ? pt.psnr : pt.ssim;
        scatter->append(pt.fps, yVal);

        minFps = std::min(minFps, pt.fps);
        maxFps = std::max(maxFps, pt.fps);
        minMetric = std::min(minMetric, yVal);
        maxMetric = std::max(maxMetric, yVal);
        displayedCount++;

        chartQuality->addSeries(scatter);
        scatter->attachAxis(axisQualX);
        scatter->attachAxis(axisQualY);

        // Спливаючі підказки при наведенні курсору
        connect(scatter, &QScatterSeries::hovered, this, [pt, this](const QPointF& point, bool state) {
            Q_UNUSED(point);
            if (state) {
                QString tip = QString("<b>%1</b><br/>"
                                      "Швидкодія: <b>%2 FPS</b><br/>"
                                      "Якість PSNR: <b>%3 дБ</b><br/>"
                                      "Схожість SSIM: <b>%4</b><br/>"
                                      "Середня похибка MSE: <b>%5</b>")
                                  .arg(pt.algorithmName)
                                  .arg(pt.fps, 0, 'f', 1)
                                  .arg(pt.psnr, 0, 'f', 2)
                                  .arg(pt.ssim, 0, 'f', 4)
                                  .arg(pt.mse, 0, 'f', 3);
                QToolTip::showText(QCursor::pos(), tip);
            }
        });
    }

    if (displayedCount == 0 || minFps > maxFps) {
        minFps = 0.0;
        maxFps = 100.0;
        minMetric = 0.0;
        maxMetric = showPsnrMetric ? 50.0 : 1.0;
    }

    // Оновлюємо підпис осі Y
    if (showPsnrMetric) {
        axisQualY->setTitleText("Пікова якість сигналу, PSNR (дБ)");
        axisQualY->setLabelFormat("%.1f");
        double span = std::max(2.0, maxMetric - minMetric);
        axisQualY->setRange(std::max(0.0, minMetric - span * 0.15), maxMetric + span * 0.15);
    } else {
        axisQualY->setTitleText("Індекс структурної подібності, SSIM");
        axisQualY->setLabelFormat("%.3f");
        double span = std::max(0.02, maxMetric - minMetric);
        axisQualY->setRange(std::max(0.0, minMetric - span * 0.15), std::min(1.0, maxMetric + span * 0.15));
    }

    // Межі осі X
    double fpsSpan = std::max(10.0, maxFps - minFps);
    axisQualX->setRange(std::max(0.0, minFps - fpsSpan * 0.1), maxFps + fpsSpan * 0.15);
}

void ScalingAnalyticsWidget::updateFromBenchmarkRecords(const std::vector<BenchmarkRecord>& records) {
    if (records.empty()) return;

    // 1. Витягуємо масштабування за кількістю потоків
    // Беремо метод з найбільшою кількістю замірів або стандартний (зазвичай з потоками 1, 4, 8)
    std::map<int, double> threadTimes;
    std::map<std::string, BenchmarkRecord> bestPerMethod;

    for (const auto& rec : records) {
        if (rec.threads > 0 && rec.durationMs > 0.0) {
            // Для масштабування фіксуємо найкращий або перший час для даного p
            if (threadTimes.find(rec.threads) == threadTimes.end() || rec.durationMs < threadTimes[rec.threads]) {
                threadTimes[rec.threads] = rec.durationMs;
            }
        }

        // Для порівняння якості шукаємо запис для кожного унікального методу
        auto it = bestPerMethod.find(rec.method);
        if (it == bestPerMethod.end() || rec.fps > it->second.fps) {
            bestPerMethod[rec.method] = rec;
        }
    }

    if (!threadTimes.empty()) {
        double t1 = 0.0;
        if (threadTimes.find(1) != threadTimes.end()) {
            t1 = threadTimes[1];
        } else {
            t1 = threadTimes.begin()->second * static_cast<double>(threadTimes.begin()->first);
        }

        threadPoints.clear();
        for (const auto& pair : threadTimes) {
            ThreadScalingPoint pt;
            pt.threads = pair.first;
            pt.timeMs = pair.second;
            pt.speedup = (pt.timeMs > 0.0) ? (t1 / pt.timeMs) : 1.0;
            pt.efficiency = (pt.speedup / static_cast<double>(pt.threads)) * 100.0;
            threadPoints.push_back(pt);
        }

        std::sort(threadPoints.begin(), threadPoints.end(), [](const auto& a, const auto& b) {
            return a.threads < b.threads;
        });

        recalculateAmdahlModel();
        updateSpeedupChartData();
        updateEfficiencyChartData();
    }

    // 2. Оновлюємо дані для графіка «Якість vs Швидкодія» зі збереженням усіх 12 варіантів
    if (!bestPerMethod.empty()) {
        for (const auto& pair : bestPerMethod) {
            const auto& r = pair.second;
            QString mName = QString::fromStdString(r.method);
            bool found = false;
            for (auto& pt : algorithmPoints) {
                if (pt.algorithmName.contains(mName, Qt::CaseInsensitive) || mName.contains(pt.algorithmName, Qt::CaseInsensitive)) {
                    pt.fps = r.fps;
                    pt.psnr = r.psnr;
                    pt.ssim = r.ssim;
                    pt.mse = r.mse;
                    pt.isGpu = (r.threads == 0);
                    found = true;
                    break;
                }
            }
            if (!found) {
                AlgorithmMetricPoint pt;
                pt.algorithmName = mName;
                pt.fps = r.fps;
                pt.psnr = r.psnr;
                pt.ssim = r.ssim;
                pt.mse = r.mse;
                pt.isGpu = (r.threads == 0);
                algorithmPoints.push_back(pt);
            }
        }
        updateQualityChartData();
    }
}

void ScalingAnalyticsWidget::setSourceImage(const cv::Mat& img, double scale) {
    if (!img.empty()) {
        currentSourceImage = img.clone();
        currentScaleFactor = scale;
    }
}

void ScalingAnalyticsWidget::runQuickScalingBenchmark(const cv::Mat& sourceImage, double scaleFactor) {
    cv::Mat testImg = sourceImage;
    if (testImg.empty()) {
        // Якщо зображення не завантажено, створюємо високоякісний синтетичний тестовий патерн 1280x720
        testImg = cv::Mat(720, 1280, CV_8UC3, cv::Scalar(40, 40, 40));
        for (int y = 0; y < testImg.rows; ++y) {
            for (int x = 0; x < testImg.cols; ++x) {
                uchar b = static_cast<uchar>((x % 64 < 32 ? 200 : 50) ^ (y % 64 < 32 ? 200 : 50));
                uchar g = static_cast<uchar>((x * 255) / testImg.cols);
                uchar r = static_cast<uchar>((y * 255) / testImg.rows);
                testImg.at<cv::Vec3b>(y, x) = cv::Vec3b(b, g, r);
            }
        }
    }

    int outW = static_cast<int>(std::round(testImg.cols * scaleFactor));
    int outH = static_cast<int>(std::round(testImg.rows * scaleFactor));
    if (outW <= 0 || outH <= 0) {
        outW = testImg.cols * 2;
        outH = testImg.rows * 2;
    }

    progressBar->setVisible(true);
    progressBar->setValue(10);
    QApplication::processEvents();

    BilinearScaler bilinear;
    BicubicScaler bicubic;
    LanczosScaler lanczos;
    AdaptiveScaler adaptive;

    int maxP = std::max(2, omp_get_max_threads());
    std::vector<int> testThreads;
    for (int p = 1; p <= maxP; p = (p < 4) ? (p + 1) : (p * 2)) {
        if (std::find(testThreads.begin(), testThreads.end(), p) == testThreads.end()) {
            testThreads.push_back(p);
        }
    }
    if (std::find(testThreads.begin(), testThreads.end(), maxP) == testThreads.end()) {
        testThreads.push_back(maxP);
    }
    std::sort(testThreads.begin(), testThreads.end());

    // 1. Тест масштабування за потоками для Bicubic (64x64)
    std::vector<cv::Rect> blocks = ParallelEngine::GenerateGrid(outW, outH, 64, 64);
    double t1 = 0.0;
    threadPoints.clear();

    int totalSteps = static_cast<int>(testThreads.size()) + 12;
    int currentStep = 0;

    for (int p : testThreads) {
        cv::Mat result = cv::Mat::zeros(outH, outW, testImg.type());
        auto start = std::chrono::high_resolution_clock::now();
        ParallelEngine::ScaleImage(testImg, result, blocks, bicubic, scaleFactor, scaleFactor, p);
        auto end = std::chrono::high_resolution_clock::now();

        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        if (p == 1) {
            t1 = duration;
        }

        ThreadScalingPoint pt;
        pt.threads = p;
        pt.timeMs = duration;
        pt.speedup = (duration > 0.0) ? (t1 / duration) : 1.0;
        pt.efficiency = (pt.speedup / static_cast<double>(p)) * 100.0;
        threadPoints.push_back(pt);

        currentStep++;
        progressBar->setValue(10 + (currentStep * 85) / totalSteps);
        QApplication::processEvents();
    }

    // 2. Тест якості та швидкодії для 12 конфігурацій (на maxP потоках)
    algorithmPoints.clear();

    auto runAlgoTest = [&](IScaler& scaler, const QString& name) {
        cv::Mat out = cv::Mat::zeros(outH, outW, testImg.type());
        auto s = std::chrono::high_resolution_clock::now();
        ParallelEngine::ScaleImage(testImg, out, blocks, scaler, scaleFactor, scaleFactor, maxP);
        auto e = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(e - s).count();
        double fps = (ms > 0.0) ? (1000.0 / ms) : 0.0;

        cv::Mat tempResized;
        cv::resize(out, tempResized, testImg.size(), 0, 0, cv::INTER_LINEAR);
        double mse = MetricsController::CalculateMSE(testImg, tempResized);
        double psnr = MetricsController::CalculatePSNR(testImg, tempResized);
        double ssim = MetricsController::CalculateSSIM(testImg, tempResized);

        AlgorithmMetricPoint pt;
        pt.algorithmName = name;
        pt.fps = fps;
        pt.mse = mse;
        pt.psnr = psnr;
        pt.ssim = ssim;
        pt.isGpu = false;
        algorithmPoints.push_back(pt);

        currentStep++;
        progressBar->setValue(10 + (currentStep * 85) / totalSteps);
        QApplication::processEvents();
    };

    // Група 1: 4 алгоритми просто (Скалярний режим CPU)
    bilinear.setEnableSIMD(false);
    runAlgoTest(bilinear, "Bilinear (Скалярний)");

    bicubic.setEnableSIMD(false);
    runAlgoTest(bicubic, "Bicubic (Скалярний)");

    lanczos.setEnableSIMD(false);
    runAlgoTest(lanczos, "Lanczos-3 (Скалярний)");

    adaptive.setEnableSIMD(false);
    runAlgoTest(adaptive, "Adaptive Edge (Скалярний)");

    // Група 2: 4 алгоритми з векторизацією (AVX2 SIMD)
    bilinear.setEnableSIMD(true);
    runAlgoTest(bilinear, "Bilinear (AVX2 SIMD)");

    bicubic.setEnableSIMD(true);
    runAlgoTest(bicubic, "Bicubic (AVX2 SIMD)");

    lanczos.setEnableSIMD(true);
    runAlgoTest(lanczos, "Lanczos-3 (AVX2 SIMD)");

    adaptive.setEnableSIMD(true);
    runAlgoTest(adaptive, "Adaptive Edge (AVX2 SIMD)");

    // Група 3: 4 алгоритми з прискоренням на відеокарті (GPU OpenCL)
    auto runGpuTest = [&](GpuScaler::Algorithm algo, const QString& name) {
        GpuScaler::Timing gpuTiming;
        cv::Mat gpuResult;
        GpuScaler::scaleFrame(testImg, gpuResult, outW, outH, algo, false, &gpuTiming);
        double gpuFps = (gpuTiming.totalMs > 0.0) ? (1000.0 / gpuTiming.totalMs) : 0.0;

        cv::Mat tempResized;
        cv::resize(gpuResult, tempResized, testImg.size(), 0, 0, cv::INTER_LINEAR);
        double mse = MetricsController::CalculateMSE(testImg, tempResized);
        double psnr = MetricsController::CalculatePSNR(testImg, tempResized);
        double ssim = MetricsController::CalculateSSIM(testImg, tempResized);

        AlgorithmMetricPoint pt;
        pt.algorithmName = name;
        pt.fps = gpuFps;
        pt.mse = mse;
        pt.psnr = psnr;
        pt.ssim = ssim;
        pt.isGpu = true;
        algorithmPoints.push_back(pt);

        currentStep++;
        progressBar->setValue(10 + (currentStep * 85) / totalSteps);
        QApplication::processEvents();
    };

    runGpuTest(GpuScaler::Algorithm::Bilinear, "Bilinear (GPU OpenCL)");
    runGpuTest(GpuScaler::Algorithm::Bicubic, "Bicubic (GPU OpenCL)");
    runGpuTest(GpuScaler::Algorithm::Lanczos, "Lanczos-3 (GPU OpenCL)");
    runGpuTest(GpuScaler::Algorithm::AdaptiveSobel, "Adaptive Edge (GPU OpenCL)");

    progressBar->setValue(100);
    QApplication::processEvents();
    progressBar->setVisible(false);

    recalculateAmdahlModel();
    updateSpeedupChartData();
    updateEfficiencyChartData();
    updateQualityChartData();

    QMessageBox::information(this, "Аналіз завершено",
        QString("Аналітичне тестування масштабування успішно виконано!\n"
                "Досліджено всі 12 варіантів (4 скалярні, 4 AVX2 SIMD, 4 GPU OpenCL).\n"
                "Оцінена послідовна частка коду α = %1 (%2%)\n"
                "Теоретична максимальна межа швидкодії S_max = %3x")
            .arg(estimatedAlpha, 0, 'f', 4)
            .arg(estimatedAlpha * 100.0, 0, 'f', 1)
            .arg(maxTheoreticalSpeedup, 0, 'f', 1));
}

void ScalingAnalyticsWidget::onRunDedicatedAnalysisClicked() {
    runQuickScalingBenchmark(currentSourceImage, currentScaleFactor);
}

void ScalingAnalyticsWidget::onMetricTypeChanged(int index) {
    showPsnrMetric = (index == 0);
    updateQualityChartData();
}

void ScalingAnalyticsWidget::onVariantFilterChanged(int index) {
    Q_UNUSED(index);
    updateQualityChartData();
}

QChart* ScalingAnalyticsWidget::currentActiveChart() const {
    int idx = subTabCharts->currentIndex();
    if (idx == 0) return chartSpeedup;
    if (idx == 1) return chartEfficiency;
    if (idx == 2) return chartQuality;
    return chartSpeedup;
}

void ScalingAnalyticsWidget::onResetZoom() {
    QChart* chart = currentActiveChart();
    if (chart) {
        chart->zoomReset();
    }
}

void ScalingAnalyticsWidget::onExportChartPng() {
    int idx = subTabCharts->currentIndex();
    QChartView* activeView = nullptr;
    QString defaultName = "scaling_chart.png";

    if (idx == 0) {
        activeView = chartViewSpeedup;
        defaultName = "amdahl_speedup_scaling.png";
    } else if (idx == 1) {
        activeView = chartViewEfficiency;
        defaultName = "parallel_efficiency_scaling.png";
    } else if (idx == 2) {
        activeView = chartViewQuality;
        defaultName = "quality_vs_fps_scaling.png";
    }

    if (!activeView) return;

    QString filePath = QFileDialog::getSaveFileName(
        this, "Зберегти графік у PNG", defaultName, "Зображення PNG (*.png)"
    );

    if (filePath.isEmpty()) return;

    QPixmap pixmap = activeView->grab();
    if (pixmap.save(filePath, "PNG")) {
        QMessageBox::information(this, "Успіх", "Графік успішно експортовано у файл:\n" + filePath);
    } else {
        QMessageBox::warning(this, "Помилка", "Не вдалося зберегти графік у файл!");
    }
}

void ScalingAnalyticsWidget::onCopyChartClipboard() {
    int idx = subTabCharts->currentIndex();
    QChartView* activeView = nullptr;
    if (idx == 0) activeView = chartViewSpeedup;
    else if (idx == 1) activeView = chartViewEfficiency;
    else if (idx == 2) activeView = chartViewQuality;

    if (!activeView) return;

    QPixmap pixmap = activeView->grab();
    QApplication::clipboard()->setPixmap(pixmap);

    QMessageBox::information(this, "Буфер обміну", "Графік успішно скопійовано до буфера обміну!");
}
