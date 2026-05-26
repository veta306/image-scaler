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
#include <sstream>
#include <iomanip>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Паралельний масштабувальник зображень - Курсова робота");
    resize(1200, 800);
    
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
    
    comboScale = new QComboBox(scaleGroup);
    comboScale->addItem("0.5 x", 0.5);
    comboScale->addItem("1.5 x", 1.5);
    comboScale->addItem("2.0 x", 2.0);
    comboScale->addItem("3.0 x", 3.0);
    comboScale->addItem("4.0 x", 4.0);
    comboScale->addItem("8.0 x", 8.0);
    comboScale->addItem("Свій варіант...", -1.0);
    comboScale->setCurrentIndex(2); // Default to 2.0x
    
    comboAlgorithm = new QComboBox(scaleGroup);
    comboAlgorithm->addItem("Білінійна інтерполяція", 0);
    
    spinCustomScale = new QDoubleSpinBox(scaleGroup);
    spinCustomScale->setRange(0.1, 10.0);
    spinCustomScale->setSingleStep(0.1);
    spinCustomScale->setValue(2.0);
    spinCustomScale->setEnabled(false); // Enable only when Custom is chosen
    
    scaleForm->addRow("Алгоритм:", comboAlgorithm);
    scaleForm->addRow("Коефіцієнт:", comboScale);
    scaleForm->addRow("Власний:", spinCustomScale);
    
    connect(comboScale, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        double val = comboScale->itemData(index).toDouble();
        spinCustomScale->setEnabled(val < 0);
        if (val > 0) {
            spinCustomScale->setValue(val);
        }
    });
    
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
    QScrollArea* scrollOrig = new QScrollArea(leftPanel);
    scrollOrig->setWidgetResizable(true);
    lblOriginalImage = new QLabel(scrollOrig);
    lblOriginalImage->setAlignment(Qt::AlignCenter);
    lblOriginalImage->setText("Завантажте фото");
    lblOriginalImage->setStyleSheet("font-size: 14px;");
    scrollOrig->setWidget(lblOriginalImage);
    leftLayout->addWidget(lblLeftHeader);
    leftLayout->addWidget(scrollOrig);
    
    // Масштабоване зображення
    QWidget* rightPanel = new QWidget(splitter);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* lblRightHeader = new QLabel("ОБРОБЛЕНЕ ЗОБРАЖЕННЯ (БІЛІНІЙНЕ)", rightPanel);
    lblRightHeader->setStyleSheet("font-weight: bold; font-size: 11px;");
    QScrollArea* scrollScaled = new QScrollArea(rightPanel);
    scrollScaled->setWidgetResizable(true);
    lblScaledImage = new QLabel(scrollScaled);
    lblScaledImage->setAlignment(Qt::AlignCenter);
    lblScaledImage->setText("Масштабуйте зображення");
    lblScaledImage->setStyleSheet("font-size: 14px;");
    scrollScaled->setWidget(lblScaledImage);
    rightLayout->addWidget(lblRightHeader);
    rightLayout->addWidget(scrollScaled);
    
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
    
    cardGrid->addWidget(createMetricCard("Однопотоковий час", "мс", lblSingleTimeVal), 0, 0);
    cardGrid->addWidget(createMetricCard("Багатопотоковий час", "мс", lblMultiTimeVal), 0, 1);
    cardGrid->addWidget(createMetricCard("Коефіцієнт прискорення", "", lblSpeedupVal), 0, 2);
    cardGrid->addWidget(createMetricCard("Ефективність ядер", "", lblEfficiencyVal), 0, 3);
    
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
    
    // Logs Console
    QLabel* lblConsole = new QLabel("Консоль налагодження та метрик:", tabPerformance);
    lblConsole->setStyleSheet("font-weight: bold;");
    logConsole = new QPlainTextEdit(tabPerformance);
    logConsole->setObjectName("logConsole");
    logConsole->setReadOnly(true);
    
    perfLayout->addWidget(lblConsole);
    perfLayout->addWidget(logConsole);
    
    tabWidget->addTab(tabPerformance, "Аналіз швидкодії та Метрики");
    
    mainLayout->addWidget(tabWidget);

    // ================= CONNECT SIGNALS =================
    connect(btnLoad, &QPushButton::clicked, this, &MainWindow::onLoadImageClicked);
    connect(btnProcess, &QPushButton::clicked, this, &MainWindow::onProcessClicked);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::onSaveImageClicked);
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
    
    // Convert to Qt Format and show
    QImage qimg = IO_Manager::MatToQImage(originalImage);
    lblOriginalImage->setPixmap(QPixmap::fromImage(qimg));
    
    // Reset scaled image view
    lblScaledImage->clear();
    lblScaledImage->setText("Масштабуйте зображення");
    btnSave->setEnabled(false);
    
    // Update labels and logs
    QString infoText = QString("Шлях: %1\nРозмір: %2 x %3 px\nКаналів: %4")
        .arg(QFileInfo(filePath).fileName())
        .arg(originalImage.cols)
        .arg(originalImage.rows)
        .arg(originalImage.channels());
    
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

    double scale = spinCustomScale->value();
    int blockSize = comboBlockSize->itemData(comboBlockSize->currentIndex()).toInt();
    int threads = spinThreads->value();

    int outWidth = static_cast<int>(std::round(originalImage.cols * scale));
    int outHeight = static_cast<int>(std::round(originalImage.rows * scale));

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
    ParallelEngine::ScaleImage(originalImage, singleResult, blocks, bilinearScaler, scale, scale, 1);
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

    auto tp_start = std::chrono::high_resolution_clock::now();
    ParallelEngine::ScaleImage(originalImage, scaledImage, blocks, bilinearScaler, scale, scale, threads);
    auto tp_end = std::chrono::high_resolution_clock::now();
    double tp_duration = std::chrono::duration<double, std::milli>(tp_end - tp_start).count();

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

    // Display output image
    QImage qimgScaled = IO_Manager::MatToQImage(scaledImage);
    lblScaledImage->setPixmap(QPixmap::fromImage(qimgScaled));

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
    logConsole->clear();
    const auto& logs = metricsController.GetLogs();
    for (const auto& log : logs) {
        logConsole->appendPlainText(QString::fromStdString(log));
    }
}

void MainWindow::updateMetricsDisplay(const ScalingMetrics& metrics) {
    // Update numerical cards
    lblSingleTimeVal->setText(QString::number(metrics.singleThreadedTimeMs, 'f', 1));
    lblMultiTimeVal->setText(QString::number(metrics.multiThreadedTimeMs, 'f', 1));
    lblSpeedupVal->setText(QString::number(metrics.speedup, 'f', 2) + " x");
    lblEfficiencyVal->setText(QString::number(metrics.efficiency, 'f', 1) + " %");

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
              << "Послідовний режим (T1): " << std::fixed << std::setprecision(2) << metrics.singleThreadedTimeMs << " мс\n"
              << "Паралельний режим (Tp): " << metrics.multiThreadedTimeMs << " мс\n"
              << "Отримане прискорення (S): " << metrics.speedup << "x\n"
              << "Ефективність використання ядер (E): " << metrics.efficiency << "%\n"
              << "=================================";
              
    metricsController.AddLog(ssSummary.str());
    updateLogsDisplay();
}

void MainWindow::applyStyleSheet() {
    // Порожній стиль для повного відновлення стандартного нативного вигляду системи
    setStyleSheet("");
}
