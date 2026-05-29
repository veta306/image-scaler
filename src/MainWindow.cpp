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
#include <qnamespace.h>
#include <sstream>
#include <iomanip>
#include <QPainter>
#include <QHeaderView>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Паралельний масштабувальник зображень - Курсова робота");
    resize(1200, 800);
    
    displayZoomOrig = 1.0;
    displayZoomScaled = 1.0;
    activePanArea = nullptr;
    
    setupUI();
    applyStyleSheet();
    
    // Add initial log
    metricsController.AddLog("Програму успішно ініціалізовано.");
    metricsController.AddLog("Виберіть вхідне зображення для початку роботи.");
    updateLogsDisplay();
}

void MainWindow::setupUI() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(12);

    // ================= SIDEBAR (Панель керування) =================
    QFrame* sidebar = new QFrame(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(310);
    
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(10, 10, 10, 10);
    sidebarLayout->setSpacing(15);

    // 1. Група роботи з файлами
    QGroupBox* fileGroup = new QGroupBox("Вхідні дані", sidebar);
    QVBoxLayout* fileLayout = new QVBoxLayout(fileGroup);
    
    btnLoad = new QPushButton("Завантажити зображення", fileGroup);
    lblImageInfo = new QLabel("Зображення не обрано", fileGroup);
    lblImageInfo->setWordWrap(true);
    lblImageInfo->setStyleSheet("font-size: 11px;");
    
    fileLayout->addWidget(btnLoad);
    fileLayout->addWidget(lblImageInfo);
    sidebarLayout->addWidget(fileGroup);

    // 2. Група налаштувань масштабування
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
    comboScale->setCurrentIndex(2); // Default to 2.0x
    
    spinCustomScale = new QDoubleSpinBox(scaleGroup);
    spinCustomScale->setRange(0.1, 10.0);
    spinCustomScale->setSingleStep(0.1);
    spinCustomScale->setValue(2.0);
    spinCustomScale->setEnabled(false); // Enable only when Custom is chosen
    
    spinTargetWidth = new QSpinBox(scaleGroup);
    spinTargetWidth->setRange(1, 16384);
    spinTargetWidth->setValue(640);
    spinTargetWidth->setEnabled(false);
    
    spinTargetHeight = new QSpinBox(scaleGroup);
    spinTargetHeight->setRange(1, 16384);
    spinTargetHeight->setValue(480);
    spinTargetHeight->setEnabled(false);
    
    chkKeepAspectRatio = new QCheckBox("Зберегти пропорції", scaleGroup);
    chkKeepAspectRatio->setChecked(true);
    chkKeepAspectRatio->setEnabled(false);
    
    scaleForm->addRow("Алгоритм:", comboAlgorithm);
    scaleForm->addRow("Режим:", comboScalingMode);
    scaleForm->addRow("Коефіцієнт:", comboScale);
    scaleForm->addRow("Власний:", spinCustomScale);
    scaleForm->addRow("Ширина (px):", spinTargetWidth);
    scaleForm->addRow("Висота (px):", spinTargetHeight);
    scaleForm->addRow(chkKeepAspectRatio);
    
    connect(comboScale, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        double val = comboScale->itemData(index).toDouble();
        spinCustomScale->setEnabled(val < 0);
        if (val > 0) {
            spinCustomScale->setValue(val);
        }
        updateTargetResolutionFromScale();
    });
    
    connect(spinCustomScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        updateTargetResolutionFromScale();
    });
    
    connect(comboScalingMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onScalingModeChanged);
    connect(spinTargetWidth, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTargetWidthChanged);
    connect(spinTargetHeight, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTargetHeightChanged);
    
    sidebarLayout->addWidget(scaleGroup);

    // 3. Група налаштувань сітки блоків
    QGroupBox* blockGroup = new QGroupBox("Розмір блоків обробки", sidebar);
    QVBoxLayout* blockLayout = new QVBoxLayout(blockGroup);
    
    comboBlockSize = new QComboBox(blockGroup);
    comboBlockSize->addItem("16 x 16 пікселів", 16);
    comboBlockSize->addItem("32 x 32 пікселів", 32);
    comboBlockSize->addItem("64 x 64 пікселів", 64);
    comboBlockSize->addItem("128 x 128 пікселів", 128);
    comboBlockSize->addItem("256 x 256 пікселів", 256);
    comboBlockSize->setCurrentIndex(2); // Default to 64x64
    
    blockLayout->addWidget(comboBlockSize);
    sidebarLayout->addWidget(blockGroup);

    // 3.5. Група покращення зображення та фільтрів
    QGroupBox* filterGroup = new QGroupBox("Покращення та Фільтри", sidebar);
    QVBoxLayout* filterLayout = new QVBoxLayout(filterGroup);
    
    chkEnableSharpen = new QCheckBox("Фільтр різкості (Unsharp Mask)", filterGroup);
    chkEnableSharpen->setChecked(false);
    
    chkEnableOverlap = new QCheckBox("Компенсація швів (Overlap)", filterGroup);
    chkEnableOverlap->setChecked(true);
    
    chkEnableDemo = new QCheckBox("Демонстраційний режим (візуалізація)", filterGroup);
    chkEnableDemo->setChecked(false);
    
    filterLayout->addWidget(chkEnableSharpen);
    filterLayout->addWidget(chkEnableOverlap);
    filterLayout->addWidget(chkEnableDemo);
    sidebarLayout->addWidget(filterGroup);

    // 4. Група керування потоками (OpenMP)
    QGroupBox* threadGroup = new QGroupBox("Паралельність (OpenMP)", sidebar);
    QFormLayout* threadForm = new QFormLayout(threadGroup);
    
    int maxCores = omp_get_max_threads();
    sliderThreads = new QSlider(Qt::Horizontal, threadGroup);
    sliderThreads->setRange(1, maxCores);
    sliderThreads->setValue(maxCores);
    
    spinThreads = new QSpinBox(threadGroup);
    spinThreads->setRange(1, maxCores);
    spinThreads->setValue(maxCores);
    
    threadForm->addRow("Кількість ядер:", spinThreads);
    threadForm->addRow(sliderThreads);
    
    connect(sliderThreads, &QSlider::valueChanged, this, &MainWindow::onThreadSliderChanged);
    connect(spinThreads, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onThreadSpinChanged);
    
    sidebarLayout->addWidget(threadGroup);

    // 5. Кнопки керування
    btnProcess = new QPushButton("ОБРОБИТИ ЗОБРАЖЕННЯ", sidebar);
    btnProcess->setObjectName("btnProcess");
    btnProcess->setCursor(Qt::PointingHandCursor);
    btnProcess->setEnabled(false); // Enabled after image is loaded
    
    btnSave = new QPushButton("Зберегти результат", sidebar);
    btnSave->setEnabled(false); // Enabled after scaling is completed
    
    sidebarLayout->addWidget(btnProcess);
    sidebarLayout->addWidget(btnSave);
    sidebarLayout->addStretch();
    
    mainLayout->addWidget(sidebar);

    // ================= CENTRAL WORKSPACE (Робоча область) =================
    QTabWidget* tabWidget = new QTabWidget(this);
    
    // Tab 1: Порівняння зображень (Side-by-Side)
    QWidget* tabCompare = new QWidget(tabWidget);
    QHBoxLayout* compareLayout = new QHBoxLayout(tabCompare);
    compareLayout->setContentsMargins(5, 5, 5, 5);
    
    QSplitter* splitter = new QSplitter(Qt::Horizontal, tabCompare);
    
    // Оригінальне зображення
    QWidget* leftPanel = new QWidget(splitter);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* lblLeftHeader = new QLabel("ВХІДНЕ ЗОБРАЖЕННЯ (ОРИГІНАЛ)", leftPanel);
    lblLeftHeader->setStyleSheet("font-weight: bold; font-size: 11px;");
    scrollOrig = new QScrollArea(leftPanel);
    scrollOrig->setWidgetResizable(true);
    lblOriginalImage = new QLabel(scrollOrig);
    lblOriginalImage->setAlignment(Qt::AlignCenter);
    lblOriginalImage->setText("Завантажте фото");
    lblOriginalImage->setStyleSheet("font-size: 14px;");
    lblOriginalImage->setCursor(Qt::OpenHandCursor);
    scrollOrig->setWidget(lblOriginalImage);
    
    lblLeftStatus = new QLabel("---x--- 100%", leftPanel);
    lblLeftStatus->setStyleSheet("font-size: 11px; font-weight: bold; color: #6c757d;");
    lblLeftStatus->setAlignment(Qt::AlignRight);
    
    leftLayout->addWidget(lblLeftHeader);
    leftLayout->addWidget(scrollOrig);
    leftLayout->addWidget(lblLeftStatus);
    
    // Масштабоване зображення
    QWidget* rightPanel = new QWidget(splitter);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* lblRightHeader = new QLabel("ОБРОБЛЕНЕ ЗОБРАЖЕННЯ (БІЛІНІЙНЕ)", rightPanel);
    lblRightHeader->setStyleSheet("font-weight: bold; font-size: 11px;");
    scrollScaled = new QScrollArea(rightPanel);
    scrollScaled->setWidgetResizable(true);
    lblScaledImage = new QLabel(scrollScaled);
    lblScaledImage->setAlignment(Qt::AlignCenter);
    lblScaledImage->setText("Масштабуйте зображення");
    lblScaledImage->setStyleSheet("font-size: 14px;");
    lblScaledImage->setCursor(Qt::OpenHandCursor);
    scrollScaled->setWidget(lblScaledImage);
    
    lblRightStatus = new QLabel("---x--- 100%", rightPanel);
    lblRightStatus->setStyleSheet("font-size: 11px; font-weight: bold; color: #6c757d;");
    lblRightStatus->setAlignment(Qt::AlignRight);
    
    rightLayout->addWidget(lblRightHeader);
    rightLayout->addWidget(scrollScaled);
    rightLayout->addWidget(lblRightStatus);
    
    // Встановлюємо фільтри подій для обробки панорамування мишкою
    scrollOrig->viewport()->installEventFilter(this);
    scrollScaled->viewport()->installEventFilter(this);
    lblOriginalImage->installEventFilter(this);
    lblScaledImage->installEventFilter(this);
    
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    compareLayout->addWidget(splitter);
    
    tabWidget->addTab(tabCompare, "Порівняння зображень");

    // Tab 2: Статистика та Метрики
    QWidget* tabPerformance = new QWidget(tabWidget);
    QVBoxLayout* perfLayout = new QVBoxLayout(tabPerformance);
    perfLayout->setContentsMargins(15, 15, 15, 15);
    perfLayout->setSpacing(15);
    
    // Grid of cards
    QGridLayout* cardGrid = new QGridLayout();
    cardGrid->setSpacing(12);
    
    cardGrid->addWidget(createTimeMetricCard("Однопотоковий час", "мс", lblSingleTimeVal, lblSingleFps), 0, 0);
    cardGrid->addWidget(createTimeMetricCard("Багатопотоковий час", "мс", lblMultiTimeVal, lblMultiFps), 0, 1);
    cardGrid->addWidget(createMetricCard("Коефіцієнт прискорення", "", lblSpeedupVal), 0, 2);
    cardGrid->addWidget(createMetricCard("Ефективність ядер", "", lblEfficiencyVal), 0, 3);
    
    cardGrid->addWidget(createMetricCard("Якість PSNR", "дБ", lblPsnrVal), 1, 0, 1, 2);
    cardGrid->addWidget(createMetricCard("Індекс SSIM", "", lblSsimVal), 1, 2, 1, 2);
    
    perfLayout->addLayout(cardGrid);
    
    // Visual Comparison Chart widget (Progress Bars)
    QGroupBox* chartGroup = new QGroupBox("Візуальне порівняння часу обробки", tabPerformance);
    QVBoxLayout* chartLayout = new QVBoxLayout(chartGroup);
    chartLayout->setSpacing(12);
    
    // Single-thread row
    QHBoxLayout* rowSingle = new QHBoxLayout();
    QLabel* lblSingleBarLabel = new QLabel("Послідовний режим (1 потік):", chartGroup);
    lblSingleBarLabel->setFixedWidth(200);
    barSingleTime = new QProgressBar(chartGroup);
    barSingleTime->setTextVisible(true);
    barSingleTime->setFormat("%v мс");
    barSingleTime->setStyleSheet("QProgressBar::chunk { background-color: #e74c3c; }"); // Red for slow
    rowSingle->addWidget(lblSingleBarLabel);
    rowSingle->addWidget(barSingleTime);
    chartLayout->addLayout(rowSingle);
    
    // Multi-thread row
    QHBoxLayout* rowMulti = new QHBoxLayout();
    QLabel* lblMultiBarLabel = new QLabel("Паралельний режим (OpenMP):", chartGroup);
    lblMultiBarLabel->setFixedWidth(200);
    barMultiTime = new QProgressBar(chartGroup);
    barMultiTime->setTextVisible(true);
    barMultiTime->setFormat("%v мс");
    barMultiTime->setStyleSheet("QProgressBar::chunk { background-color: #2ecc71; }"); // Green for fast
    rowMulti->addWidget(lblMultiBarLabel);
    rowMulti->addWidget(barMultiTime);
    chartLayout->addLayout(rowMulti);
    perfLayout->addWidget(chartGroup);
    perfLayout->addStretch();
    
    tabWidget->addTab(tabPerformance, "Аналіз швидкодії та Метрики");

    // Tab 3: Порівняльний Бенчмарк
    QWidget* tabBenchmark = new QWidget(tabWidget);
    QVBoxLayout* benchLayout = new QVBoxLayout(tabBenchmark);
    benchLayout->setContentsMargins(15, 15, 15, 15);
    benchLayout->setSpacing(15);
    
    // Header controls
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

    // Table results
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

    // Pivot table results
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

    // ================= CONNECT SIGNALS =================
    connect(btnLoad, &QPushButton::clicked, this, &MainWindow::onLoadImageClicked);
    connect(btnProcess, &QPushButton::clicked, this, &MainWindow::onProcessClicked);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::onSaveImageClicked);
    connect(btnRunBenchmark, &QPushButton::clicked, this, &MainWindow::onRunBenchmarkClicked);
}

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

void MainWindow::onThreadSliderChanged(int value) {
    if (spinThreads->value() != value) {
        spinThreads->setValue(value);
    }
}

void MainWindow::onThreadSpinChanged(int value) {
    if (sliderThreads->value() != value) {
        sliderThreads->setValue(value);
    }
}

void MainWindow::onLoadImageClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, 
        "Виберіть вхідне зображення", "", "Зображення (*.png *.jpg *.jpeg *.bmp *.tiff)");
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // Load image via OpenCV
    cv::Mat loadedImg = IO_Manager::LoadImage(filePath.toStdString());
    if (loadedImg.empty()) {
        QMessageBox::critical(this, "Помилка завантаження", "Не вдалося зчитати файл зображення!");
        return;
    }
    
    originalImage = loadedImg;
    currentFilePath = filePath;
    
    displayZoomOrig = 1.0;
    updateOrigViewer();
    
    // Reset scaled image view
    scaledImage = cv::Mat();
    displayZoomScaled = 1.0;
    updateScaledViewer();
    btnSave->setEnabled(false);
    
    // Update labels and logs
    QString infoText = QString("Шлях: %1\nРозмір: %2 x %3 px\nКаналів: %4")
        .arg(QFileInfo(filePath).fileName())
        .arg(originalImage.cols)
        .arg(originalImage.rows)
        .arg(originalImage.channels());
    
    // Update target resolution spinboxes
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

void MainWindow::onProcessClicked() {
    if (originalImage.empty()) {
        QMessageBox::warning(this, "Попередження", "Будь ласка, завантажте зображення!");
        return;
    }

    // Configure the scaler enhancements from the UI checkbox states
    bilinearScaler.setEnableSharpen(chkEnableSharpen->isChecked());
    bilinearScaler.setEnableOverlap(chkEnableOverlap->isChecked());

    double scaleX = 1.0;
    double scaleY = 1.0;
    int outWidth = 0;
    int outHeight = 0;

    int mode = comboScalingMode->currentIndex();
    if (mode == 0) {
        // By factor
        double scale = spinCustomScale->value();
        scaleX = scale;
        scaleY = scale;
        outWidth = static_cast<int>(std::round(originalImage.cols * scaleX));
        outHeight = static_cast<int>(std::round(originalImage.rows * scaleY));
    } else {
        // By resolution
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

    // Set waiting cursor
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    // Prepare outputs
    cv::Mat singleResult = cv::Mat::zeros(outHeight, outWidth, originalImage.type());
    scaledImage = cv::Mat::zeros(outHeight, outWidth, originalImage.type());

    // Generate coordinate blocks
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

    // 1. Однопотоковий запуск
    metricsController.AddLog("Запуск послідовної обробки (1 потік)...");
    updateLogsDisplay();
    QCoreApplication::processEvents(); // Allow log render

    auto t1_start = std::chrono::high_resolution_clock::now();
    ParallelEngine::ScaleImage(originalImage, singleResult, blocks, bilinearScaler, scaleX, scaleY, 1);
    auto t1_end = std::chrono::high_resolution_clock::now();
    double t1_duration = std::chrono::duration<double, std::milli>(t1_end - t1_start).count();

    std::stringstream sT1;
    sT1 << std::fixed << std::setprecision(2) << "Послідовний режим завершено за " << t1_duration << " мс.";
    metricsController.AddLog(sT1.str());
    updateLogsDisplay();

    // 2. Багатопотоковий запуск (OpenMP)
    std::stringstream sOMPStart;
    sOMPStart << "Запуск паралельної обробки OpenMP (" << threads << " потоків)...";
    metricsController.AddLog(sOMPStart.str());
    updateLogsDisplay();
    QCoreApplication::processEvents();

    double tp_duration = 0.0;
    if (chkEnableDemo->isChecked()) {
        // Progressive Demonstration Mode: tiled rendering with neon green border
        scaledImage = cv::Mat::zeros(outHeight, outWidth, originalImage.type());
        scaledImage = cv::Scalar(40, 40, 40); // Dark gray canvas
        
        int sleepMs = std::max(5, static_cast<int>(2000 / blocks.size()));
        
        for (size_t i = 0; i < blocks.size(); ++i) {
            auto t_block_start = std::chrono::high_resolution_clock::now();
            bilinearScaler.ScaleBlock(originalImage, scaledImage, blocks[i], scaleX, scaleY);
            auto t_block_end = std::chrono::high_resolution_clock::now();
            tp_duration += std::chrono::duration<double, std::milli>(t_block_end - t_block_start).count();
            
            // Convert to QImage and QPixmap
            QImage qimg = IO_Manager::MatToQImage(scaledImage);
            QPixmap pix = QPixmap::fromImage(qimg);
            
            // Draw neon border over the just-processed block
            QPainter painter(&pix);
            painter.setPen(QPen(QColor("#2ecc71"), 3)); // Glowing green
            painter.drawRect(blocks[i].x, blocks[i].y, blocks[i].width, blocks[i].height);
            painter.end();
            
            // Apply zoom
            if (std::abs(displayZoomScaled - 1.0) > 0.001) {
                int w = static_cast<int>(std::round(pix.width() * displayZoomScaled));
                int h = static_cast<int>(std::round(pix.height() * displayZoomScaled));
                if (w > 0 && h > 0) {
                    pix = pix.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }
            
            lblScaledImage->setPixmap(pix);
            
            int pct = static_cast<int>((i + 1) * 100 / blocks.size());
            lblRightStatus->setText(QString("%1x%2 Обробка... %3%").arg(outWidth).arg(outHeight).arg(pct));
            
            QCoreApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
        
        // Clean final update
        updateScaledViewer();
    } else {
        auto tp_start = std::chrono::high_resolution_clock::now();
        ParallelEngine::ScaleImage(originalImage, scaledImage, blocks, bilinearScaler, scaleX, scaleY, threads);
        auto tp_end = std::chrono::high_resolution_clock::now();
        tp_duration = std::chrono::duration<double, std::milli>(tp_end - tp_start).count();
    }

    std::stringstream sTp;
    sTp << std::fixed << std::setprecision(2) << "Паралельний режим завершено за " << tp_duration << " мс.";
    metricsController.AddLog(sTp.str());
    updateLogsDisplay();

    // Calculate metrics
    ScalingMetrics metrics = MetricsController::CalculateMetrics(t1_duration, tp_duration, threads);
    metrics.blockSizeX = blockSize;
    metrics.blockSizeY = blockSize;
    metrics.imageWidth = outWidth;
    metrics.imageHeight = outHeight;

    // Calculate quality metrics (PSNR & SSIM)
    if (!originalImage.empty() && !scaledImage.empty()) {
        cv::Mat scaledResized;
        cv::resize(scaledImage, scaledResized, originalImage.size(), 0, 0, cv::INTER_LINEAR);
        metrics.psnr = MetricsController::CalculatePSNR(originalImage, scaledResized);
        metrics.ssim = MetricsController::CalculateSSIM(originalImage, scaledResized);
    }

    displayZoomScaled = 1.0;
    updateScaledViewer();

    // Update UI dashboard
    updateMetricsDisplay(metrics);
    btnSave->setEnabled(true);

    // Restore standard cursor
    QGuiApplication::restoreOverrideCursor();
    
    QMessageBox::information(this, "Успіх", "Масштабування виконано! Перевірте вкладку 'Аналіз швидкодії' для детальних метрик.");
}

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

void MainWindow::updateLogsDisplay() {
    // Console widget was removed, so logs are only kept in the metricsController model.
}

void MainWindow::updateMetricsDisplay(const ScalingMetrics& metrics) {
    // Update numerical cards
    lblSingleTimeVal->setText(QString::number(metrics.singleThreadedTimeMs, 'f', 1));
    lblMultiTimeVal->setText(QString::number(metrics.multiThreadedTimeMs, 'f', 1));
    lblSpeedupVal->setText(QString::number(metrics.speedup, 'f', 2) + " x");
    lblEfficiencyVal->setText(QString::number(metrics.efficiency, 'f', 1) + " %");

    // Update FPS values
    double singleFps = (metrics.singleThreadedTimeMs > 0.0) ? (1000.0 / metrics.singleThreadedTimeMs) : 0.0;
    double multiFps = (metrics.multiThreadedTimeMs > 0.0) ? (1000.0 / metrics.multiThreadedTimeMs) : 0.0;

    lblSingleFps->setText(QString("%1 FPS").arg(singleFps, 0, 'f', 1));
    lblMultiFps->setText(QString("%1 FPS").arg(multiFps, 0, 'f', 1));

    // Color-code FPS based on real-time standard (>= 30 FPS is green/real-time, < 30 FPS is red)
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

    // Update quality metrics cards
    lblPsnrVal->setText(QString::number(metrics.psnr, 'f', 2) + " dB");
    lblSsimVal->setText(QString::number(metrics.ssim, 'f', 4));

    // Update graphical progress bars
    int singleVal = static_cast<int>(metrics.singleThreadedTimeMs);
    int multiVal = static_cast<int>(metrics.multiThreadedTimeMs);
    
    // Set ranges to match largest time
    int maxVal = std::max({1, singleVal, multiVal});
    
    barSingleTime->setRange(0, maxVal);
    barSingleTime->setValue(singleVal);
    
    barMultiTime->setRange(0, maxVal);
    barMultiTime->setValue(multiVal);

    // Format logs with performance summary
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

void MainWindow::applyStyleSheet() {
    // Порожній стиль для повного відновлення стандартного нативного вигляду системи
    setStyleSheet("");
}

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

void MainWindow::updateOrigViewer() {
    if (!originalImage.empty()) {
        QImage qimg = IO_Manager::MatToQImage(originalImage);
        if (std::abs(displayZoomOrig - 1.0) > 0.001) {
            int w = static_cast<int>(std::round(qimg.width() * displayZoomOrig));
            int h = static_cast<int>(std::round(qimg.height() * displayZoomOrig));
            if (w > 0 && h > 0) {
                qimg = qimg.scaled(w, h, Qt::KeepAspectRatio, Qt::FastTransformation);
            }
        }
        lblOriginalImage->setPixmap(QPixmap::fromImage(qimg));
        
        int zoomPct = static_cast<int>(std::round(displayZoomOrig * 100));
        lblLeftStatus->setText(QString("%1x%2 %3%")
            .arg(originalImage.cols)
            .arg(originalImage.rows)
            .arg(zoomPct));
    } else {
        lblOriginalImage->clear();
        lblOriginalImage->setText("Завантажте фото");
        lblLeftStatus->setText("---x--- 100%");
    }
}

void MainWindow::updateScaledViewer() {
    if (!scaledImage.empty()) {
        QImage qimg = IO_Manager::MatToQImage(scaledImage);
        if (std::abs(displayZoomScaled - 1.0) > 0.001) {
            int w = static_cast<int>(std::round(qimg.width() * displayZoomScaled));
            int h = static_cast<int>(std::round(qimg.height() * displayZoomScaled));
            if (w > 0 && h > 0) {
                qimg = qimg.scaled(w, h, Qt::KeepAspectRatio, Qt::FastTransformation);
            }
        }
        lblScaledImage->setPixmap(QPixmap::fromImage(qimg));
        
        int zoomPct = static_cast<int>(std::round(displayZoomScaled * 100));
        lblRightStatus->setText(QString("%1x%2 %3%")
            .arg(scaledImage.cols)
            .arg(scaledImage.rows)
            .arg(zoomPct));
    } else {
        lblScaledImage->clear();
        lblScaledImage->setText("Масштабуйте зображення");
        lblRightStatus->setText("---x--- 100%");
    }
}

#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    // 1. Зум за допомогою Ctrl + Wheel
    if (event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            double angleDelta = wheelEvent->angleDelta().y();
            double factor = (angleDelta > 0) ? 1.1 : 0.9;
            
            bool isOrig = (obj == scrollOrig->viewport() || obj == lblOriginalImage);
            if (isOrig) {
                double newZoom = displayZoomOrig * factor;
                if (newZoom >= 0.1 && newZoom <= 10.0) {
                    displayZoomOrig = newZoom;
                    updateOrigViewer();
                }
            } else {
                double newZoom = displayZoomScaled * factor;
                if (newZoom >= 0.1 && newZoom <= 10.0) {
                    displayZoomScaled = newZoom;
                    updateScaledViewer();
                }
            }
            wheelEvent->accept();
            return true;
        }
    }
    
    // 2. Панорамування (Pan) шляхом перетягування лівою кнопкою миші
    bool isViewport = (obj == scrollOrig->viewport() || obj == scrollScaled->viewport());
    bool isLabel = (obj == lblOriginalImage || obj == lblScaledImage);
    
    if (isViewport || isLabel) {
        QScrollArea* targetArea = nullptr;
        if (obj == scrollOrig->viewport() || obj == lblOriginalImage) {
            targetArea = scrollOrig;
        } else {
            targetArea = scrollScaled;
        }
        
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                activePanArea = targetArea;
                panStartPos = mouseEvent->globalPosition().toPoint();
                startScrollX = targetArea->horizontalScrollBar()->value();
                startScrollY = targetArea->verticalScrollBar()->value();
                
                if (isLabel) {
                    static_cast<QWidget*>(obj)->setCursor(Qt::ClosedHandCursor);
                } else {
                    targetArea->viewport()->setCursor(Qt::ClosedHandCursor);
                }
                
                mouseEvent->accept();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (activePanArea && activePanArea == targetArea) {
                QPoint currentPos = mouseEvent->globalPosition().toPoint();
                QPoint delta = currentPos - panStartPos;
                
                targetArea->horizontalScrollBar()->setValue(startScrollX - delta.x());
                targetArea->verticalScrollBar()->setValue(startScrollY - delta.y());
                
                mouseEvent->accept();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && activePanArea == targetArea) {
                activePanArea = nullptr;
                
                if (isLabel) {
                    static_cast<QWidget*>(obj)->setCursor(Qt::OpenHandCursor);
                } else {
                    targetArea->viewport()->setCursor(Qt::ArrowCursor);
                }
                
                mouseEvent->accept();
                return true;
            }
        }
    }
    
    return QMainWindow::eventFilter(obj, event);
}

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

    // Set waiting cursor and disable the benchmark button
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    btnRunBenchmark->setEnabled(false);
    
    // Clear all rows from both tables
    tableBenchmark->setRowCount(0);
    tablePivotBenchmark->setRowCount(0);
    tablePivotBenchmark->setColumnCount(0);

    // Save active configurations of scaler to restore them at the end
    bool origSharpen = bilinearScaler.isSharpenEnabled();
    bool origOverlap = bilinearScaler.isOverlapEnabled();
    
    // Configure bilinearScaler for benchmark from UI checkboxes
    bilinearScaler.setEnableSharpen(chkEnableSharpen->isChecked());
    bilinearScaler.setEnableOverlap(chkEnableOverlap->isChecked());

    // 1. Benchmark OpenCV Baseline
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

    // Style OpenCV row with light gray background
    for (int col = 0; col < 7; ++col) {
        tableBenchmark->item(rCv, col)->setBackground(QColor("#f1f2f6"));
        tableBenchmark->item(rCv, col)->setForeground(QColor("#2f3542"));
        tableBenchmark->item(rCv, col)->setTextAlignment(Qt::AlignCenter);
    }
    tableBenchmark->item(rCv, 0)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Define standard block sizes to test
    std::vector<int> testBlockSizes = {16, 64, 256};

    // Dynamically define thread counts to test (tailored to system CPU)
    std::vector<int> testThreads = {1, 4, 8};
    int maxCores = omp_get_max_threads();
    if (std::find(testThreads.begin(), testThreads.end(), maxCores) == testThreads.end()) {
        testThreads.push_back(maxCores);
    }
    std::sort(testThreads.begin(), testThreads.end());

    // Configure Pivot table structure dynamically based on unique tested parameters
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

    // 2. Loop through all block size and thread configurations
    for (size_t b = 0; b < testBlockSizes.size(); ++b) {
        int blockSize = testBlockSizes[b];
        // Generate grid coordinates
        std::vector<cv::Rect> blocks = ParallelEngine::GenerateGrid(outWidth, outHeight, blockSize, blockSize);
        
        for (size_t t = 0; t < testThreads.size(); ++t) {
            int threads = testThreads[t];
            cv::Mat tempResult = cv::Mat::zeros(outHeight, outWidth, originalImage.type());

            // Run scaling computation
            auto start = std::chrono::high_resolution_clock::now();
            ParallelEngine::ScaleImage(originalImage, tempResult, blocks, bilinearScaler, scaleX, scaleY, threads);
            auto end = std::chrono::high_resolution_clock::now();
            double duration = std::chrono::duration<double, std::milli>(end - start).count();
            double fps = (duration > 0.0) ? (1000.0 / duration) : 0.0;

            // Calculate PSNR & SSIM quality metrics
            double psnr = 0.0;
            double ssim = 1.0;
            if (!originalImage.empty() && !tempResult.empty()) {
                cv::Mat tempResized;
                cv::resize(tempResult, tempResized, originalImage.size(), 0, 0, cv::INTER_LINEAR);
                psnr = MetricsController::CalculatePSNR(originalImage, tempResized);
                ssim = MetricsController::CalculateSSIM(originalImage, tempResized);
            }

            // Insert into detailed flat table
            int r = tableBenchmark->rowCount();
            tableBenchmark->insertRow(r);

            QTableWidgetItem* itemM = new QTableWidgetItem("Наш Bilinear Scaler");
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

            // Insert into Pivot Performance Matrix table
            QTableWidgetItem* itemPivot = new QTableWidgetItem(QString("%1 мс").arg(duration, 0, 'f', 1));
            itemPivot->setTextAlignment(Qt::AlignCenter);
            tablePivotBenchmark->setItem(t, b, itemPivot);

            customResults.push_back({r, static_cast<int>(t), static_cast<int>(b), duration});
            
            // Allow GUI update during processing
            QCoreApplication::processEvents();
        }
    }

    // Restore original scaler settings
    bilinearScaler.setEnableSharpen(origSharpen);
    bilinearScaler.setEnableOverlap(origOverlap);

    // Restore wait cursor and enable benchmark button
    QGuiApplication::restoreOverrideCursor();
    btnRunBenchmark->setEnabled(true);

    QMessageBox::information(this, "Тестування завершено", 
        "Автоматичний порівняльний бенчмарк успішно виконано!");
}
