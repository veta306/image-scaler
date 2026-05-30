#include "MainWindow.hpp"
#include <QApplication>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QStyle>
#include <QGuiApplication>
#include <omp.h>
#include <chrono>
#include <thread>
#include <cmath>
#include <qnamespace.h>
#include <sstream>
#include <iomanip>
#include <QPainter>
#include <QHeaderView>

/**
 * @brief Конструктор головного вікна MainWindow.
 */
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Паралельний масштабувальник зображень - Курсова робота");
    resize(1200, 800);
    
    setupUI();
    applyStyleSheet();
    
    metricsController.AddLog("Програму успішно ініціалізовано.");
    metricsController.AddLog("Виберіть вхідне зображення для початку роботи.");
    updateLogsDisplay();
}

/**
 * @brief Створює та компонує графічний інтерфейс користувача (GUI).
 */
void MainWindow::setupUI() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(12);

    QFrame* sidebar = new QFrame(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(310);
    
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(10, 10, 10, 10);
    sidebarLayout->setSpacing(15);

    QGroupBox* fileGroup = new QGroupBox("Вхідні дані", sidebar);
    QVBoxLayout* fileLayout = new QVBoxLayout(fileGroup);
    
    btnLoad = new QPushButton("Завантажити зображення", fileGroup);
    lblImageInfo = new QLabel("Зображення не обрано", fileGroup);
    lblImageInfo->setWordWrap(true);
    lblImageInfo->setStyleSheet("font-size: 11px;");
    
    fileLayout->addWidget(btnLoad);
    fileLayout->addWidget(lblImageInfo);
    sidebarLayout->addWidget(fileGroup);

    QGroupBox* scaleGroup = new QGroupBox("Параметри масштабу", sidebar);
    QFormLayout* scaleForm = new QFormLayout(scaleGroup);
    scaleForm->setLabelAlignment(Qt::AlignLeft);
    
    comboAlgorithm = new QComboBox(scaleGroup);
    comboAlgorithm->addItem("Білінійна інтерполяція", 0);
    
    comboScalingMode = new QComboBox(scaleGroup);
    comboScalingMode->addItem("За коефіцієнтом", 0);
    comboScalingMode->addItem("За роздільною здатністю", 1);
    
    comboScale = new QComboBox(scaleGroup);
    comboScale->addItem("0.5 x", 0.5);
    comboScale->addItem("1.5 x", 1.5);
    comboScale->addItem("2.0 x", 2.0);
    comboScale->addItem("3.0 x", 3.0);
    comboScale->addItem("4.0 x", 4.0);
    comboScale->addItem("8.0 x", 8.0);
    comboScale->addItem("Свій варіант...", -1.0);
    comboScale->setCurrentIndex(2);
    
    spinCustomScale = new QDoubleSpinBox(scaleGroup);
    spinCustomScale->setRange(0.1, 100.0);
    spinCustomScale->setSingleStep(0.1);
    spinCustomScale->setValue(2.0);
    spinCustomScale->setEnabled(false);
    
    spinTargetWidth = new QSpinBox(scaleGroup);
    spinTargetWidth->setRange(1, 16384);
    spinTargetWidth->setValue(640);
    spinTargetWidth->setEnabled(false);
    
    spinTargetHeight = new QSpinBox(scaleGroup);
    spinTargetHeight->setRange(1, 16384);
    spinTargetHeight->setValue(480);
    spinTargetHeight->setEnabled(false);
    
    chkKeepAspectRatio = new QCheckBox("Зберігати пропорції", scaleGroup);
    chkKeepAspectRatio->setChecked(true);
    chkKeepAspectRatio->setEnabled(false);

    scaleForm->addRow("Алгоритм:", comboAlgorithm);
    scaleForm->addRow("Режим задання:", comboScalingMode);
    scaleForm->addRow("Коефіцієнт:", comboScale);
    scaleForm->addRow("Свій масштаб:", spinCustomScale);
    scaleForm->addRow("Цільова ширина:", spinTargetWidth);
    scaleForm->addRow("Цільова висота:", spinTargetHeight);
    scaleForm->addRow("", chkKeepAspectRatio);
    
    sidebarLayout->addWidget(scaleGroup);

    QGroupBox* parallelGroup = new QGroupBox("Паралельність та блоки", sidebar);
    QFormLayout* parallelForm = new QFormLayout(parallelGroup);
    parallelForm->setLabelAlignment(Qt::AlignLeft);
    
    comboBlockSize = new QComboBox(parallelGroup);
    comboBlockSize->addItem("16 x 16 px", 16);
    comboBlockSize->addItem("32 x 32 px", 32);
    comboBlockSize->addItem("64 x 64 px", 64);
    comboBlockSize->addItem("128 x 128 px", 128);
    comboBlockSize->addItem("256 x 256 px", 256);
    comboBlockSize->setCurrentIndex(2);

    int maxThreads = omp_get_max_threads();
    sliderThreads = new QSlider(Qt::Horizontal, parallelGroup);
    sliderThreads->setRange(1, maxThreads);
    sliderThreads->setValue(maxThreads);
    
    spinThreads = new QSpinBox(parallelGroup);
    spinThreads->setRange(1, maxThreads);
    spinThreads->setValue(maxThreads);
    
    QHBoxLayout* threadLayout = new QHBoxLayout();
    threadLayout->addWidget(sliderThreads);
    threadLayout->addWidget(spinThreads);

    parallelForm->addRow("Розмір блоку (px):", comboBlockSize);
    parallelForm->addRow("Кількість потоків:", threadLayout);
    
    sidebarLayout->addWidget(parallelGroup);

    QGroupBox* filterGroup = new QGroupBox("Покращення та візуалізація", sidebar);
    QVBoxLayout* filterLayout = new QVBoxLayout(filterGroup);
    
    chkEnableSharpen = new QCheckBox("Фільтр різкості (Unsharp Mask)", filterGroup);
    chkEnableSharpen->setChecked(false);
    
    chkEnableOverlap = new QCheckBox("Усунення швів (Overlap Padding)", filterGroup);
    chkEnableOverlap->setChecked(true);
    
    chkEnableDemo = new QCheckBox("Демонстраційний режим (візуалізація)", filterGroup);
    chkEnableDemo->setChecked(false);

    filterLayout->addWidget(chkEnableSharpen);
    filterLayout->addWidget(chkEnableOverlap);
    filterLayout->addWidget(chkEnableDemo);
    
    sidebarLayout->addWidget(filterGroup);
    
    sidebarLayout->addStretch();
    
    btnProcess = new QPushButton("ОБРОБИТИ ЗОБРАЖЕННЯ", sidebar);
    btnProcess->setObjectName("btnProcess");
    btnProcess->setEnabled(false);
    btnProcess->setCursor(Qt::PointingHandCursor);
    
    btnSave = new QPushButton("Зберегти результат", sidebar);
    btnSave->setEnabled(false);
    btnSave->setCursor(Qt::PointingHandCursor);
    
    sidebarLayout->addWidget(btnProcess);
    sidebarLayout->addWidget(btnSave);

    mainLayout->addWidget(sidebar);

    QTabWidget* tabWidget = new QTabWidget(this);
    
    QWidget* tabCompare = new QWidget(tabWidget);
    QHBoxLayout* compareLayout = new QHBoxLayout(tabCompare);
    compareLayout->setContentsMargins(5, 5, 5, 5);
    
    QSplitter* splitter = new QSplitter(Qt::Horizontal, tabCompare);
    
    QWidget* leftPanel = new QWidget(splitter);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* lblLeftHeader = new QLabel("ВХІДНЕ ЗОБРАЖЕННЯ (ОРИГІНАЛ)", leftPanel);
    lblLeftHeader->setStyleSheet("font-weight: bold; font-size: 11px;");
    viewerOrig = new ImageViewer("Завантажте фото", leftPanel);
    
    leftLayout->addWidget(lblLeftHeader);
    leftLayout->addWidget(viewerOrig);
    
    QWidget* rightPanel = new QWidget(splitter);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* lblRightHeader = new QLabel("ОБРОБЛЕНЕ ЗОБРАЖЕННЯ (БІЛІНІЙНЕ)", rightPanel);
    lblRightHeader->setStyleSheet("font-weight: bold; font-size: 11px;");
    viewerScaled = new ImageViewer("Масштабуйте зображення", rightPanel);
    
    rightLayout->addWidget(lblRightHeader);
    rightLayout->addWidget(viewerScaled);
    
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    compareLayout->addWidget(splitter);
    
    tabWidget->addTab(tabCompare, "Порівняння зображень");

    QWidget* tabPerformance = new QWidget(tabWidget);
    QVBoxLayout* perfLayout = new QVBoxLayout(tabPerformance);
    perfLayout->setContentsMargins(15, 15, 15, 15);
    perfLayout->setSpacing(15);
    
    QLabel* lblPerfHeader = new QLabel("Аналіз паралельної швидкодії процесора:", tabPerformance);
    lblPerfHeader->setStyleSheet("font-weight: bold; font-size: 11px;");
    perfLayout->addWidget(lblPerfHeader);

    QGridLayout* cardGrid = new QGridLayout();
    cardGrid->setSpacing(10);
    
    QFrame* cardT1 = createTimeMetricCard("Час послідовної обробки (T1)", "мс", lblSingleTimeVal, lblSingleFps);
    QFrame* cardTp = createTimeMetricCard("Час паралельної обробки (Tp)", "мс", lblMultiTimeVal, lblMultiFps);
    QFrame* cardS = createMetricCard("Коефіцієнт прискорення (S)", "x", lblSpeedupVal);
    QFrame* cardE = createMetricCard("Ефективність потоків (E)", "%", lblEfficiencyVal);
    QFrame* cardPsnr = createMetricCard("Якість обробки (PSNR)", "дБ", lblPsnrVal);
    QFrame* cardSsim = createMetricCard("Метрика схожості (SSIM)", "", lblSsimVal);

    cardGrid->addWidget(cardT1, 0, 0);
    cardGrid->addWidget(cardTp, 0, 1);
    cardGrid->addWidget(cardS, 0, 2);
    cardGrid->addWidget(cardE, 1, 0);
    cardGrid->addWidget(cardPsnr, 1, 1);
    cardGrid->addWidget(cardSsim, 1, 2);
    
    perfLayout->addLayout(cardGrid);

    QGroupBox* chartGroup = new QGroupBox("Діаграма порівняння часу виконання (менше = краще)", tabPerformance);
    QVBoxLayout* chartLayout = new QVBoxLayout(chartGroup);
    chartLayout->setContentsMargins(15, 15, 15, 15);
    chartLayout->setSpacing(10);

    QHBoxLayout* bar1Layout = new QHBoxLayout();
    QLabel* lblBar1Title = new QLabel("Послідовний режим (1 потік):", chartGroup);
    lblBar1Title->setFixedWidth(160);
    barSingleTime = new QProgressBar(chartGroup);
    barSingleTime->setTextVisible(true);
    barSingleTime->setFormat("%v мс");
    barSingleTime->setStyleSheet("QProgressBar::chunk { background-color: #e74c3c; }");
    bar1Layout->addWidget(lblBar1Title);
    bar1Layout->addWidget(barSingleTime);
    chartLayout->addLayout(bar1Layout);

    QHBoxLayout* bar2Layout = new QHBoxLayout();
    QLabel* lblBar2Title = new QLabel("Паралельний режим:", chartGroup);
    lblBar2Title->setFixedWidth(160);
    barMultiTime = new QProgressBar(chartGroup);
    barMultiTime->setTextVisible(true);
    barMultiTime->setFormat("%v мс");
    barMultiTime->setStyleSheet("QProgressBar::chunk { background-color: #2ecc71; }");
    bar2Layout->addWidget(lblBar2Title);
    bar2Layout->addWidget(barMultiTime);
    chartLayout->addLayout(bar2Layout);

    perfLayout->addWidget(chartGroup);
    perfLayout->addStretch();
    
    tabWidget->addTab(tabPerformance, "Аналіз швидкодії та Метрики");

    QWidget* tabBenchmark = new QWidget(tabWidget);
    QVBoxLayout* benchLayout = new QVBoxLayout(tabBenchmark);
    benchLayout->setContentsMargins(15, 15, 15, 15);
    benchLayout->setSpacing(15);
    
    QHBoxLayout* benchHeader = new QHBoxLayout();
    QLabel* lblBenchDesc = new QLabel("Порівняння конфігурацій (блоки, потоки) з еталонним інструментом OpenCV:", tabBenchmark);
    lblBenchDesc->setStyleSheet("font-weight: bold; font-size: 11px;");
    
    btnRunBenchmark = new QPushButton("ЗАПУСТИТИ АВТОМАТИЧНИЙ БЕНЧМАРК", tabBenchmark);
    btnRunBenchmark->setCursor(Qt::PointingHandCursor);
    
    benchHeader->addWidget(lblBenchDesc);
    benchHeader->addStretch();
    benchHeader->addWidget(btnRunBenchmark);
    benchLayout->addLayout(benchHeader);
    
    QLabel* lblTable1 = new QLabel("Детальні показники конфігурацій:", tabBenchmark);
    lblTable1->setStyleSheet("font-weight: bold; font-size: 11px;");
    benchLayout->addWidget(lblTable1);

    tableBenchmark = new QTableWidget(tabBenchmark);
    tableBenchmark->setColumnCount(7);
    tableBenchmark->setHorizontalHeaderLabels({
        "Метод масштабування", "Розмір блоку", "Потоки OpenMP", 
        "Час обробки (мс)", "Швидкодія (FPS)", "Якість PSNR (дБ)", "Подібність SSIM"
    });
    tableBenchmark->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableBenchmark->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableBenchmark->setSelectionMode(QAbstractItemView::SingleSelection);
    tableBenchmark->horizontalHeader()->setStretchLastSection(true);
    tableBenchmark->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    benchLayout->addWidget(tableBenchmark);

    QLabel* lblTable2 = new QLabel("Матриця швидкодії паралельної обробки (Час виконання, мс):", tabBenchmark);
    lblTable2->setStyleSheet("font-weight: bold; font-size: 11px; margin-top: 10px;");
    benchLayout->addWidget(lblTable2);

    tablePivotBenchmark = new QTableWidget(tabBenchmark);
    tablePivotBenchmark->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tablePivotBenchmark->setSelectionBehavior(QAbstractItemView::SelectRows);
    tablePivotBenchmark->setSelectionMode(QAbstractItemView::SingleSelection);
    tablePivotBenchmark->horizontalHeader()->setStretchLastSection(true);
    tablePivotBenchmark->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tablePivotBenchmark->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    benchLayout->addWidget(tablePivotBenchmark);

    tabWidget->addTab(tabBenchmark, "Порівняльний Бенчмарк");
    
    mainLayout->addWidget(tabWidget);

    connect(btnLoad, &QPushButton::clicked, this, &MainWindow::onLoadImageClicked);
    connect(btnProcess, &QPushButton::clicked, this, &MainWindow::onProcessClicked);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::onSaveImageClicked);
    connect(btnRunBenchmark, &QPushButton::clicked, this, &MainWindow::onRunBenchmarkClicked);
    connect(sliderThreads, &QSlider::valueChanged, this, &MainWindow::onThreadSliderChanged);
    connect(spinThreads, &QSpinBox::valueChanged, this, &MainWindow::onThreadSpinChanged);
    connect(comboScalingMode, &QComboBox::currentIndexChanged, this, &MainWindow::onScalingModeChanged);
    connect(comboScale, &QComboBox::currentIndexChanged, this, [this](int index) {
        double val = comboScale->itemData(index).toDouble();
        spinCustomScale->setEnabled(val < 0);
        if (val > 0) {
            spinCustomScale->setValue(val);
        }
        updateTargetResolutionFromScale();
    });
    connect(spinCustomScale, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        updateTargetResolutionFromScale();
    });
    connect(spinTargetWidth, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onTargetWidthChanged);
    connect(spinTargetHeight, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::onTargetHeightChanged);
}

/**
 * @brief Створює загальну картку для відображення числової метрики.
 */
QFrame* MainWindow::createMetricCard(const QString& title, const QString& unit, QLabel*& valueLabelOut) {
    QFrame* card = new QFrame(this);
    card->setObjectName("metricCard");
    card->setFrameShape(QFrame::StyledPanel);
    
    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(5);
    
    QLabel* lblTitle = new QLabel(title, card);
    lblTitle->setStyleSheet("font-size: 11px; font-weight: bold; text-transform: uppercase;");
    
    QHBoxLayout* valLayout = new QHBoxLayout();
    valueLabelOut = new QLabel("0.00", card);
    valueLabelOut->setStyleSheet("font-size: 22px; font-weight: bold;");
    
    QLabel* lblUnit = new QLabel(unit, card);
    lblUnit->setStyleSheet("font-size: 13px; font-weight: bold;");
    
    valLayout->addWidget(valueLabelOut);
    valLayout->addWidget(lblUnit);
    valLayout->addStretch();
    
    layout->addWidget(lblTitle);
    layout->addLayout(valLayout);
    
    return card;
}

/**
 * @brief Створює спеціальну картку часу виконання з підтримкою відображення FPS.
 */
QFrame* MainWindow::createTimeMetricCard(const QString& title, const QString& unit, QLabel*& valueLabelOut, QLabel*& fpsLabelOut) {
    QFrame* card = new QFrame(this);
    card->setObjectName("metricCard");
    card->setFrameShape(QFrame::StyledPanel);
    
    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(5);
    
    QLabel* lblTitle = new QLabel(title, card);
    lblTitle->setStyleSheet("font-size: 11px; font-weight: bold; text-transform: uppercase;");
    
    QHBoxLayout* valLayout = new QHBoxLayout();
    valueLabelOut = new QLabel("0.00", card);
    valueLabelOut->setStyleSheet("font-size: 22px; font-weight: bold;");
    
    QLabel* lblUnit = new QLabel(unit, card);
    lblUnit->setStyleSheet("font-size: 13px; font-weight: bold;");
    
    valLayout->addWidget(valueLabelOut);
    valLayout->addWidget(lblUnit);
    valLayout->addStretch();
    
    fpsLabelOut = new QLabel("--- FPS", card);
    fpsLabelOut->setStyleSheet("font-size: 12px; font-weight: bold; color: #7f8c8d;");
    
    layout->addWidget(lblTitle);
    layout->addLayout(valLayout);
    layout->addWidget(fpsLabelOut);
    
    return card;
}

/**
 * @brief Синхронізує значення повзунка з числовим полем вибору потоків.
 */
void MainWindow::onThreadSliderChanged(int value) {
    if (spinThreads->value() != value) {
        spinThreads->setValue(value);
    }
}

/**
 * @brief Синхронізує значення числового поля вибору потоків із повзунком.
 */
void MainWindow::onThreadSpinChanged(int value) {
    if (sliderThreads->value() != value) {
        sliderThreads->setValue(value);
    }
}

/**
 * @brief Завантажує вхідне зображення через файл-діалог та оновлює стан вікна.
 */
void MainWindow::onLoadImageClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, 
        "Виберіть вхідне зображення", "", "Зображення (*.png *.jpg *.jpeg *.bmp *.tiff)");
    
    if (filePath.isEmpty()) {
        return;
    }
    
    cv::Mat loadedImg = IO_Manager::LoadImage(filePath.toStdString());
    if (loadedImg.empty()) {
        QMessageBox::critical(this, "Помилка завантаження", "Не вдалося зчитати файл зображення!");
        return;
    }
    
    originalImage = loadedImg;
    currentFilePath = filePath;
    
    viewerOrig->setImage(originalImage);
    viewerOrig->setZoom(1.0);
    
    scaledImage = cv::Mat();
    viewerScaled->clear();
    btnSave->setEnabled(false);
    
    QString infoText = QString("Шлях: %1\nРозмір: %2 x %3 px\nКаналів: %4")
        .arg(QFileInfo(filePath).fileName())
        .arg(originalImage.cols)
        .arg(originalImage.rows)
        .arg(originalImage.channels());
    
    spinTargetWidth->blockSignals(true);
    spinTargetHeight->blockSignals(true);
    double currentScale = spinCustomScale->value();
    spinTargetWidth->setValue(static_cast<int>(std::round(originalImage.cols * currentScale)));
    spinTargetHeight->setValue(static_cast<int>(std::round(originalImage.rows * currentScale)));
    spinTargetWidth->blockSignals(false);
    spinTargetHeight->blockSignals(false);

    lblImageInfo->setText(infoText);
    btnProcess->setEnabled(true);
    
    std::stringstream ss;
    ss << "Завантажено файл: " << QFileInfo(filePath).fileName().toStdString() 
       << " [" << originalImage.cols << "x" << originalImage.rows << ", " << originalImage.channels() << " канали].";
    metricsController.AddLog(ss.str());
    updateLogsDisplay();
}

/**
 * @brief Виконує обробку (масштабування) зображення в послідовному та паралельному режимах.
 */
void MainWindow::onProcessClicked() {
    if (originalImage.empty()) {
        QMessageBox::warning(this, "Попередження", "Будь ласка, завантажте зображення!");
        return;
    }

    bilinearScaler.setEnableSharpen(chkEnableSharpen->isChecked());
    bilinearScaler.setEnableOverlap(chkEnableOverlap->isChecked());

    double scaleX = 1.0;
    double scaleY = 1.0;
    int outWidth = 0;
    int outHeight = 0;

    int mode = comboScalingMode->currentIndex();
    if (mode == 0) {
        double scale = spinCustomScale->value();
        scaleX = scale;
        scaleY = scale;
        outWidth = static_cast<int>(std::round(originalImage.cols * scaleX));
        outHeight = static_cast<int>(std::round(originalImage.rows * scaleY));
    } else {
        outWidth = spinTargetWidth->value();
        outHeight = spinTargetHeight->value();
        scaleX = static_cast<double>(outWidth) / originalImage.cols;
        scaleY = static_cast<double>(outHeight) / originalImage.rows;
    }

    int blockSize = comboBlockSize->itemData(comboBlockSize->currentIndex()).toInt();
    int threads = spinThreads->value();

    if (outWidth <= 0 || outHeight <= 0 || outWidth > 16384 || outHeight > 16384) {
        QMessageBox::critical(this, "Помилка масштабування", 
            "Отримані розміри вихідного зображення виходять за дозволені межі (до 16384 px)!");
        return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    cv::Mat singleResult = cv::Mat::zeros(outHeight, outWidth, originalImage.type());
    scaledImage = cv::Mat::zeros(outHeight, outWidth, originalImage.type());

    std::vector<cv::Rect> blocks = ParallelEngine::GenerateGrid(outWidth, outHeight, blockSize, blockSize);

    std::stringstream sLog;
    sLog << "=== ПОЧАТОК МАСШТАБУВАННЯ (" << originalImage.cols << "x" << originalImage.rows 
         << " -> " << outWidth << "x" << outHeight << ") ===";
    metricsController.AddLog(sLog.str());
    metricsController.AddLog("Розбиття вихідної матриці на сітку блоків...");
    
    std::stringstream sGrid;
    sGrid << "Згенеровано " << blocks.size() << " блоків розміром " << blockSize << "x" << blockSize << ".";
    metricsController.AddLog(sGrid.str());
    updateLogsDisplay();

    metricsController.AddLog("Запуск послідовної обробки (1 потік)...");
    updateLogsDisplay();
    QCoreApplication::processEvents();

    auto t1_start = std::chrono::high_resolution_clock::now();
    ParallelEngine::ScaleImage(originalImage, singleResult, blocks, bilinearScaler, scaleX, scaleY, 1);
    auto t1_end = std::chrono::high_resolution_clock::now();
    double t1_duration = std::chrono::duration<double, std::milli>(t1_end - t1_start).count();

    std::stringstream sT1;
    sT1 << std::fixed << std::setprecision(2) << "Послідовний режим завершено за " << t1_duration << " мс.";
    metricsController.AddLog(sT1.str());
    updateLogsDisplay();

    std::stringstream sOMPStart;
    sOMPStart << "Запуск паралельної обробки OpenMP (" << threads << " потоків)...";
    metricsController.AddLog(sOMPStart.str());
    updateLogsDisplay();
    QCoreApplication::processEvents();

    double tp_duration = 0.0;
    if (chkEnableDemo->isChecked()) {
        scaledImage = cv::Mat::zeros(outHeight, outWidth, originalImage.type());
        scaledImage = cv::Scalar(40, 40, 40);
        
        int numWaves = static_cast<int>(std::ceil(static_cast<double>(blocks.size()) / threads));
        int sleepMs = std::max(20, std::min(200, 2000 / (numWaves > 0 ? numWaves : 1)));
        
        QColor threadColors[] = {
            QColor("#3498db"),
            QColor("#2ecc71"),
            QColor("#9b59b6"),
            QColor("#f1c40f"),
            QColor("#e74c3c"),
            QColor("#e67e22"),
            QColor("#1abc9c"),
            QColor("#e84393")
        };
        int numColors = sizeof(threadColors) / sizeof(threadColors[0]);

        for (size_t startIdx = 0; startIdx < blocks.size(); startIdx += threads) {
            int waveSize = std::min(static_cast<int>(threads), static_cast<int>(blocks.size() - startIdx));
            std::vector<int> blockThreads(waveSize, 0);

            auto wave_start = std::chrono::high_resolution_clock::now();
            #pragma omp parallel num_threads(threads)
            {
                int tid = omp_get_thread_num();
                #pragma omp for
                for (int j = 0; j < waveSize; ++j) {
                    int blockIdx = startIdx + j;
                    bilinearScaler.ScaleBlock(originalImage, scaledImage, blocks[blockIdx], scaleX, scaleY);
                    blockThreads[j] = tid;
                }
            }
            auto wave_end = std::chrono::high_resolution_clock::now();
            tp_duration += std::chrono::duration<double, std::milli>(wave_end - wave_start).count();

            QImage qimg = IO_Manager::MatToQImage(scaledImage);
            QPixmap pix = QPixmap::fromImage(qimg);

            QPainter painter(&pix);
            for (int j = 0; j < waveSize; ++j) {
                int blockIdx = startIdx + j;
                int tid = blockThreads[j];
                QPen pen(threadColors[tid % numColors], 3);
                painter.setPen(pen);
                painter.drawRect(blocks[blockIdx].x, blocks[blockIdx].y, blocks[blockIdx].width, blocks[blockIdx].height);
            }
            painter.end();

            double zoomVal = viewerScaled->zoom();
            if (std::abs(zoomVal - 1.0) > 0.001) {
                int w = static_cast<int>(std::round(pix.width() * zoomVal));
                int h = static_cast<int>(std::round(pix.height() * zoomVal));
                if (w > 0 && h > 0) {
                    pix = pix.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }

            viewerScaled->setPixmap(pix);

            int processedBlocks = startIdx + waveSize;
            int pct = static_cast<int>(processedBlocks * 100 / blocks.size());
            viewerScaled->setStatusText(QString("%1x%2 Паралельна обробка (%3 потоків)... %4%")
                .arg(outWidth).arg(outHeight).arg(threads).arg(pct));

            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }

        viewerScaled->setImage(scaledImage);
        viewerScaled->setZoom(1.0);
    } else {
        auto tp_start = std::chrono::high_resolution_clock::now();
        ParallelEngine::ScaleImage(originalImage, scaledImage, blocks, bilinearScaler, scaleX, scaleY, threads);
        auto tp_end = std::chrono::high_resolution_clock::now();
        tp_duration = std::chrono::duration<double, std::milli>(tp_end - tp_start).count();
        
        viewerScaled->setImage(scaledImage);
        viewerScaled->setZoom(1.0);
    }

    std::stringstream sTp;
    sTp << std::fixed << std::setprecision(2) << "Паралельний режим завершено за " << tp_duration << " мс.";
    metricsController.AddLog(sTp.str());
    updateLogsDisplay();

    ScalingMetrics metrics = MetricsController::CalculateMetrics(t1_duration, tp_duration, threads);
    metrics.blockSizeX = blockSize;
    metrics.blockSizeY = blockSize;
    metrics.imageWidth = outWidth;
    metrics.imageHeight = outHeight;

    if (!originalImage.empty() && !scaledImage.empty()) {
        cv::Mat scaledResized;
        cv::resize(scaledImage, scaledResized, originalImage.size(), 0, 0, cv::INTER_LINEAR);
        metrics.psnr = MetricsController::CalculatePSNR(originalImage, scaledResized);
        metrics.ssim = MetricsController::CalculateSSIM(originalImage, scaledResized);
    }

    updateMetricsDisplay(metrics);
    btnSave->setEnabled(true);

    QGuiApplication::restoreOverrideCursor();
    
    QMessageBox::information(this, "Успіх", "Масштабування виконано! Перевірте вкладку 'Аналіз швидкодії' для детальних метрик.");
}

/**
 * @brief Зберігає масштабоване зображення на диск.
 */
void MainWindow::onSaveImageClicked() {
    if (scaledImage.empty()) {
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(this, 
        "Зберегти оброблене зображення", "", "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;BMP Image (*.bmp)");
    
    if (savePath.isEmpty()) {
        return;
    }

    if (IO_Manager::SaveImage(savePath.toStdString(), scaledImage)) {
        metricsController.AddLog("Результат масштабування успішно збережено за шляхом: " + QFileInfo(savePath).fileName().toStdString());
        updateLogsDisplay();
        QMessageBox::information(this, "Збережено", "Зображення успішно збережено на диск!");
    } else {
        QMessageBox::critical(this, "Помилка збереження", "Не вдалося зберегти зображення!");
    }
}

/**
 * @brief Заглушка для логів (перенесено у контролер).
 */
void MainWindow::updateLogsDisplay() {
}

/**
 * @brief Оновлює картки показників та гістограми в GUI на основі розрахованих метрик.
 */
void MainWindow::updateMetricsDisplay(const ScalingMetrics& metrics) {
    lblSingleTimeVal->setText(QString::number(metrics.singleThreadedTimeMs, 'f', 1));
    lblMultiTimeVal->setText(QString::number(metrics.multiThreadedTimeMs, 'f', 1));
    lblSpeedupVal->setText(QString::number(metrics.speedup, 'f', 2));
    lblEfficiencyVal->setText(QString::number(metrics.efficiency, 'f', 1));

    double singleFps = (metrics.singleThreadedTimeMs > 0.0) ? (1000.0 / metrics.singleThreadedTimeMs) : 0.0;
    double multiFps = (metrics.multiThreadedTimeMs > 0.0) ? (1000.0 / metrics.multiThreadedTimeMs) : 0.0;

    lblSingleFps->setText(QString("%1 FPS").arg(singleFps, 0, 'f', 1));
    lblMultiFps->setText(QString("%1 FPS").arg(multiFps, 0, 'f', 1));

    if (singleFps >= 30.0) {
        lblSingleFps->setStyleSheet("font-size: 12px; font-weight: bold; color: #27ae60;");
    } else {
        lblSingleFps->setStyleSheet("font-size: 12px; font-weight: bold; color: #c0392b;");
    }

    if (multiFps >= 30.0) {
        lblMultiFps->setStyleSheet("font-size: 12px; font-weight: bold; color: #27ae60;");
    } else {
        lblMultiFps->setStyleSheet("font-size: 12px; font-weight: bold; color: #c0392b;");
    }

    lblPsnrVal->setText(QString::number(metrics.psnr, 'f', 2));
    lblSsimVal->setText(QString::number(metrics.ssim, 'f', 4));

    int singleVal = static_cast<int>(metrics.singleThreadedTimeMs);
    int multiVal = static_cast<int>(metrics.multiThreadedTimeMs);
    
    int maxVal = std::max({1, singleVal, multiVal});
    
    barSingleTime->setRange(0, maxVal);
    barSingleTime->setValue(singleVal);
    
    barMultiTime->setRange(0, maxVal);
    barMultiTime->setValue(multiVal);

    std::stringstream ssSummary;
    ssSummary << "\n=== СТАТИСТИКА ЕФЕКТИВНОСТІ ===\n"
              << "Розмір зображення: " << metrics.imageWidth << " x " << metrics.imageHeight << " px\n"
              << "Розмір блоку обробки: " << metrics.blockSizeX << " x " << metrics.blockSizeY << " px\n"
              << "Задіяно ядер процесора: " << metrics.threadCount << "\n"
              << "Послідовний режим (T1): " << std::fixed << std::setprecision(2) << metrics.singleThreadedTimeMs << " мс (" << singleFps << " FPS)\n"
              << "Паралельний режим (Tp): " << metrics.multiThreadedTimeMs << " мс (" << multiFps << " FPS)\n"
              << "Отримане прискорення (S): " << metrics.speedup << "x\n"
              << "Ефективність використання ядер (E): " << metrics.efficiency << "%\n"
              << "Якість PSNR: " << metrics.psnr << " dB\n"
              << "Індекс SSIM: " << metrics.ssim << "\n"
              << "=================================";
               
    metricsController.AddLog(ssSummary.str());
}

/**
 * @brief Скидає кастомний табличний стиль вікна (використовує нативний).
 */
void MainWindow::applyStyleSheet() {
    setStyleSheet("");
}

/**
 * @brief Обробляє вибір режиму масштабування (коефіцієнт або довільна роздільна здатність).
 */
void MainWindow::onScalingModeChanged(int index) {
    bool factorMode = (index == 0);
    
    comboScale->setEnabled(factorMode);
    if (factorMode) {
        double val = comboScale->itemData(comboScale->currentIndex()).toDouble();
        spinCustomScale->setEnabled(val < 0);
    } else {
        spinCustomScale->setEnabled(false);
    }
    
    spinTargetWidth->setEnabled(!factorMode);
    spinTargetHeight->setEnabled(!factorMode);
    chkKeepAspectRatio->setEnabled(!factorMode);
    
    if (!factorMode && !originalImage.empty()) {
        double currentScale = spinCustomScale->value();
        spinTargetWidth->blockSignals(true);
        spinTargetHeight->blockSignals(true);
        spinTargetWidth->setValue(static_cast<int>(std::round(originalImage.cols * currentScale)));
        spinTargetHeight->setValue(static_cast<int>(std::round(originalImage.rows * currentScale)));
        spinTargetWidth->blockSignals(false);
        spinTargetHeight->blockSignals(false);
    }
}

/**
 * @brief Оновлює висоту при ручній зміні цільової ширини для збереження пропорцій.
 */
void MainWindow::onTargetWidthChanged(int value) {
    if (originalImage.empty() || !chkKeepAspectRatio->isChecked()) {
        return;
    }
    
    double aspect = static_cast<double>(originalImage.rows) / originalImage.cols;
    int newHeight = static_cast<int>(std::round(value * aspect));
    
    spinTargetHeight->blockSignals(true);
    spinTargetHeight->setValue(newHeight);
    spinTargetHeight->blockSignals(false);
}

/**
 * @brief Оновлює ширину при ручній зміні цільової висоти для збереження пропорцій.
 */
void MainWindow::onTargetHeightChanged(int value) {
    if (originalImage.empty() || !chkKeepAspectRatio->isChecked()) {
        return;
    }
    
    double aspect = static_cast<double>(originalImage.cols) / originalImage.rows;
    int newWidth = static_cast<int>(std::round(value * aspect));
    
    spinTargetWidth->blockSignals(true);
    spinTargetWidth->setValue(newWidth);
    spinTargetWidth->blockSignals(false);
}

/**
 * @brief Оновлює поля цільової роздільної здатності на основі поточного масштабу.
 */
void MainWindow::updateTargetResolutionFromScale() {
    if (originalImage.empty()) {
        return;
    }
    
    double currentScale = spinCustomScale->value();
    int newWidth = static_cast<int>(std::round(originalImage.cols * currentScale));
    int newHeight = static_cast<int>(std::round(originalImage.rows * currentScale));
    
    spinTargetWidth->blockSignals(true);
    spinTargetHeight->blockSignals(true);
    spinTargetWidth->setValue(newWidth);
    spinTargetHeight->setValue(newHeight);
    spinTargetWidth->blockSignals(false);
    spinTargetHeight->blockSignals(false);
}

/**
 * @brief Запускає автоматичний бенчмарк для всіх тестових конфігурацій.
 */
void MainWindow::onRunBenchmarkClicked() {
    if (originalImage.empty()) {
        QMessageBox::warning(this, "Попередження", "Будь ласка, завантажте вхідне зображення!");
        return;
    }

    double scaleX = 1.0;
    double scaleY = 1.0;
    int outWidth = 0;
    int outHeight = 0;

    int mode = comboScalingMode->currentIndex();
    if (mode == 0) {
        double scale = spinCustomScale->value();
        scaleX = scale;
        scaleY = scale;
        outWidth = static_cast<int>(std::round(originalImage.cols * scaleX));
        outHeight = static_cast<int>(std::round(originalImage.rows * scaleY));
    } else {
        outWidth = spinTargetWidth->value();
        outHeight = spinTargetHeight->value();
        scaleX = static_cast<double>(outWidth) / originalImage.cols;
        scaleY = static_cast<double>(outHeight) / originalImage.rows;
    }

    if (outWidth <= 0 || outHeight <= 0 || outWidth > 16384 || outHeight > 16384) {
        QMessageBox::critical(this, "Помилка масштабування", 
            "Отримані розміри вихідного зображення виходять за дозволені межі (до 16384 px)!");
        return;
    }

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    btnRunBenchmark->setEnabled(false);
    
    tableBenchmark->setRowCount(0);
    tablePivotBenchmark->setRowCount(0);
    tablePivotBenchmark->setColumnCount(0);

    bool origSharpen = bilinearScaler.isSharpenEnabled();
    bool origOverlap = bilinearScaler.isOverlapEnabled();
    
    bilinearScaler.setEnableSharpen(chkEnableSharpen->isChecked());
    bilinearScaler.setEnableOverlap(chkEnableOverlap->isChecked());

    cv::Mat cvResult;
    auto cv_start = std::chrono::high_resolution_clock::now();
    cv::resize(originalImage, cvResult, cv::Size(outWidth, outHeight), 0, 0, cv::INTER_LINEAR);
    auto cv_end = std::chrono::high_resolution_clock::now();
    double cvDuration = std::chrono::duration<double, std::milli>(cv_end - cv_start).count();
    double cvFps = (cvDuration > 0.0) ? (1000.0 / cvDuration) : 0.0;

    int rCv = tableBenchmark->rowCount();
    tableBenchmark->insertRow(rCv);
    
    QTableWidgetItem* itemMethod = new QTableWidgetItem("OpenCV cv::resize (Еталон)");
    QTableWidgetItem* itemBlock = new QTableWidgetItem("N/A (Суцільний)");
    QTableWidgetItem* itemThreads = new QTableWidgetItem("Бібліотечні (Макс)");
    QTableWidgetItem* itemTime = new QTableWidgetItem(QString::number(cvDuration, 'f', 1));
    QTableWidgetItem* itemFps = new QTableWidgetItem(QString::number(cvFps, 'f', 1));
    QTableWidgetItem* itemPsnr = new QTableWidgetItem("99.00 (Еталон)");
    QTableWidgetItem* itemSsim = new QTableWidgetItem("1.0000 (Еталон)");

    tableBenchmark->setItem(rCv, 0, itemMethod);
    tableBenchmark->setItem(rCv, 1, itemBlock);
    tableBenchmark->setItem(rCv, 2, itemThreads);
    tableBenchmark->setItem(rCv, 3, itemTime);
    tableBenchmark->setItem(rCv, 4, itemFps);
    tableBenchmark->setItem(rCv, 5, itemPsnr);
    tableBenchmark->setItem(rCv, 6, itemSsim);

    for (int col = 0; col < 7; ++col) {
        tableBenchmark->item(rCv, col)->setBackground(QColor("#f1f2f6"));
        tableBenchmark->item(rCv, col)->setForeground(QColor("#2f3542"));
        tableBenchmark->item(rCv, col)->setTextAlignment(Qt::AlignCenter);
    }
    tableBenchmark->item(rCv, 0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    std::vector<int> testBlockSizes = {16, 64, 256};

    std::vector<int> testThreads = {1, 4, 8};
    int maxCores = omp_get_max_threads();
    if (std::find(testThreads.begin(), testThreads.end(), maxCores) == testThreads.end()) {
        testThreads.push_back(maxCores);
    }
    std::sort(testThreads.begin(), testThreads.end());

    tablePivotBenchmark->setColumnCount(testBlockSizes.size());
    tablePivotBenchmark->setRowCount(testThreads.size());
    
    QStringList horizHeaders;
    for (int bs : testBlockSizes) {
        horizHeaders << QString("%1 x %2 px").arg(bs).arg(bs);
    }
    tablePivotBenchmark->setHorizontalHeaderLabels(horizHeaders);
    
    QStringList vertHeaders;
    for (int th : testThreads) {
        if (th == maxCores) {
            vertHeaders << QString("%1 потоків (Макс)").arg(th);
        } else if (th == 1) {
            vertHeaders << "1 потік";
        } else {
            vertHeaders << QString("%1 потоки").arg(th);
        }
    }
    tablePivotBenchmark->setVerticalHeaderLabels(vertHeaders);

    struct BenchResult {
        int rowIndex;
        int tIndex;
        int bIndex;
        double duration;
    };
    std::vector<BenchResult> customResults;

    for (size_t b = 0; b < testBlockSizes.size(); ++b) {
        int blockSize = testBlockSizes[b];
        std::vector<cv::Rect> blocks = ParallelEngine::GenerateGrid(outWidth, outHeight, blockSize, blockSize);
        
        for (size_t t = 0; t < testThreads.size(); ++t) {
            int threads = testThreads[t];
            cv::Mat tempResult = cv::Mat::zeros(outHeight, outWidth, originalImage.type());

            auto start = std::chrono::high_resolution_clock::now();
            ParallelEngine::ScaleImage(originalImage, tempResult, blocks, bilinearScaler, scaleX, scaleY, threads);
            auto end = std::chrono::high_resolution_clock::now();
            double duration = std::chrono::duration<double, std::milli>(end - start).count();
            double fps = (duration > 0.0) ? (1000.0 / duration) : 0.0;

            double psnr = 0.0;
            double ssim = 1.0;
            if (!originalImage.empty() && !tempResult.empty()) {
                cv::Mat tempResized;
                cv::resize(tempResult, tempResized, originalImage.size(), 0, 0, cv::INTER_LINEAR);
                psnr = MetricsController::CalculatePSNR(originalImage, tempResized);
                ssim = MetricsController::CalculateSSIM(originalImage, tempResized);
            }

            int r = tableBenchmark->rowCount();
            tableBenchmark->insertRow(r);

            QTableWidgetItem* itemM = new QTableWidgetItem("Bilinear method");
            QTableWidgetItem* itemB = new QTableWidgetItem(QString("%1 x %2").arg(blockSize).arg(blockSize));
            QTableWidgetItem* itemT = new QTableWidgetItem(QString::number(threads));
            QTableWidgetItem* itemTi = new QTableWidgetItem(QString::number(duration, 'f', 1));
            QTableWidgetItem* itemF = new QTableWidgetItem(QString::number(fps, 'f', 1));
            QTableWidgetItem* itemP = new QTableWidgetItem(QString::number(psnr, 'f', 2));
            QTableWidgetItem* itemS = new QTableWidgetItem(QString::number(ssim, 'f', 4));

            tableBenchmark->setItem(r, 0, itemM);
            tableBenchmark->setItem(r, 1, itemB);
            tableBenchmark->setItem(r, 2, itemT);
            tableBenchmark->setItem(r, 3, itemTi);
            tableBenchmark->setItem(r, 4, itemF);
            tableBenchmark->setItem(r, 5, itemP);
            tableBenchmark->setItem(r, 6, itemS);

            for (int col = 0; col < 7; ++col) {
                tableBenchmark->item(r, col)->setTextAlignment(Qt::AlignCenter);
            }
            tableBenchmark->item(r, 0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            QTableWidgetItem* itemPivot = new QTableWidgetItem(QString("%1 мс").arg(duration, 0, 'f', 1));
            itemPivot->setTextAlignment(Qt::AlignCenter);
            tablePivotBenchmark->setItem(t, b, itemPivot);

            customResults.push_back({r, static_cast<int>(t), static_cast<int>(b), duration});
            
            QCoreApplication::processEvents();
        }
    }

    bilinearScaler.setEnableSharpen(origSharpen);
    bilinearScaler.setEnableOverlap(origOverlap);

    QGuiApplication::restoreOverrideCursor();
    btnRunBenchmark->setEnabled(true);

    QMessageBox::information(this, "Тестування завершено", 
        "Автоматичний порівняльний бенчмарк успішно виконано!");
}
