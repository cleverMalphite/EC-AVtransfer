#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QSplitter>
#include <QFormLayout>
#include <QPainter>
#include <QPainterPath>
#include <QCoreApplication>
#include <QStringList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), videoThread(nullptr), 
      mediaMtxProcess(nullptr), ffmpegProcess(nullptr), playerProcess(nullptr),
      isRunning(false)
{
    setupUi();
    applyStyles();
    setWindowTitle("音视频传输客户端");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    stopAll();
}

void MainWindow::setupUi()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Left Panel (Sidebar) ---
    leftPanel = new QFrame(this);
    leftPanel->setObjectName("LeftPanel");
    leftPanel->setFixedWidth(320); // Fixed width sidebar
    
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(20, 40, 20, 20);
    leftLayout->setSpacing(20);

    // 1. Branding / Header
    appTitle = new QLabel("音视频传输系统", this);
    appTitle->setObjectName("AppTitle");
    appTitle->setAlignment(Qt::AlignLeft);
    leftLayout->addWidget(appTitle);

    brandImagePlaceholder = new QLabel(this);
    brandImagePlaceholder->setObjectName("BrandImage");
    brandImagePlaceholder->setFixedSize(280, 160);
    brandImagePlaceholder->setAlignment(Qt::AlignCenter);
    
    // Load Logo
    QString logoPath;
    QStringList logoCandidates;
    logoCandidates << QDir::current().filePath("pic/EClogo.png");
    logoCandidates << QDir(QCoreApplication::applicationDirPath()).filePath("pic/EClogo.png");
    logoCandidates << QDir(QCoreApplication::applicationDirPath()).filePath("../pic/EClogo.png");
    logoCandidates << QDir(QCoreApplication::applicationDirPath()).filePath("../../pic/EClogo.png");
    logoCandidates << QDir(QCoreApplication::applicationDirPath()).filePath("../../../pic/EClogo.png");

    for (const QString &p : logoCandidates) {
        if (QFile::exists(p)) {
            logoPath = QDir::cleanPath(p);
            break;
        }
    }
    
    if (!logoPath.isEmpty()) {
        QPixmap logo(logoPath);
        if (!logo.isNull()) {
            auto makeRoundedCover = [](const QPixmap &src, const QSize &size, int radius) {
                QPixmap out(size);
                out.fill(Qt::transparent);

                QPainter p(&out);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setRenderHint(QPainter::SmoothPixmapTransform, true);

                QPainterPath path;
                path.addRoundedRect(QRectF(0, 0, size.width(), size.height()), radius, radius);
                p.setClipPath(path);

                QPixmap scaled = src.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                int x = (size.width() - scaled.width()) / 2;
                int y = (size.height() - scaled.height()) / 2;
                p.drawPixmap(x, y, scaled);
                return out;
            };

            brandImagePlaceholder->setPixmap(makeRoundedCover(logo, brandImagePlaceholder->size(), 10));
        } else {
            brandImagePlaceholder->setText("LOGO LOAD ERROR");
        }
    } else {
        brandImagePlaceholder->setText("LOGO PLACEHOLDER"); 
    }
    
    leftLayout->addWidget(brandImagePlaceholder);

    // 2. Config Section
    QFrame *configBox = new QFrame(this);
    configBox->setObjectName("ConfigBox");
    QVBoxLayout *configLayout = new QVBoxLayout(configBox);
    configLayout->setContentsMargins(16, 18, 16, 18);
    configLayout->setSpacing(5);

    // URL
    QLabel *lblUrl = new QLabel("STREAM URL", this);
    lblUrl->setObjectName("LabelHeader");
    urlInput = new QLineEdit("rtsp://localhost:8554/live", this);
    configLayout->addWidget(lblUrl);
    configLayout->addWidget(urlInput);

    // File
    QLabel *lblFile = new QLabel("文件选择", this);
    lblFile->setObjectName("LabelHeaderCN");
    
    QHBoxLayout *fileLayout = new QHBoxLayout();
    fileInput = new QLineEdit(this);
    if (QFile::exists("example/testp4.mp4")) {
        fileInput->setText(QFileInfo("example/testp4.mp4").absoluteFilePath());
    }
    browseBtn = new QPushButton("...", this);
    browseBtn->setFixedWidth(60);
    browseBtn->setObjectName("BrowseButton");
    
    fileLayout->addWidget(fileInput);
    fileLayout->addWidget(browseBtn);
    
    configLayout->addWidget(lblFile);
    configLayout->addLayout(fileLayout);

    // Mode
    QLabel *lblMode = new QLabel("传输模式", this);
    lblMode->setObjectName("LabelHeaderCN");
    configLayout->addWidget(lblMode);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    rbVideo = new QRadioButton("Video", this);
    rbAudio = new QRadioButton("Audio", this);
    rbVideo->setChecked(true);
    modeLayout->addWidget(rbVideo);
    modeLayout->addWidget(rbAudio);
    configLayout->addLayout(modeLayout);

    leftLayout->addWidget(configBox);
    
    // Spacer to push button to bottom
    leftLayout->addStretch();

    // 3. Action Button
    btnToggle = new QPushButton("开始传输", this);
    btnToggle->setObjectName("ToggleBtn");
    btnToggle->setFixedHeight(50);
    btnToggle->setCursor(Qt::PointingHandCursor);
    leftLayout->addWidget(btnToggle);

    // --- Right Area (Video + Status) ---
    QFrame *rightPanel = new QFrame(this);
    rightPanel->setObjectName("RightPanel");
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(20, 20, 20, 20);
    rightLayout->setSpacing(0);

    // Video Container
    videoContainer = new QFrame(this);
    videoContainer->setObjectName("VideoContainer");
    QVBoxLayout *videoLayout = new QVBoxLayout(videoContainer);
    videoLayout->setContentsMargins(0, 0, 0, 0);

    videoLabel = new QLabel(this);
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoLayout->addWidget(videoLabel);
    
    videoOverlayText = new QLabel("NO SIGNAL", videoLabel);
    videoOverlayText->setAlignment(Qt::AlignCenter);
    videoOverlayText->setStyleSheet("color: rgba(255,255,255,0.3); font-size: 24px; font-weight: bold;");
    
    rightLayout->addWidget(videoContainer, 1); // Stretch factor 1

    // Status Bar (Overlay or Bottom)
    statusBar = new QFrame(this);
    statusBar->setObjectName("StatusBar");
    statusBar->setFixedHeight(40);
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(15, 0, 15, 0);

    statusIndicator = new QLabel(this);
    statusIndicator->setFixedSize(10, 10);
    statusIndicator->setStyleSheet("background-color: #555; border-radius: 5px;");
    
    statusText = new QLabel("READY", this);
    statusText->setStyleSheet("color: #888; font-weight: bold; margin-left: 5px;");
    
    fpsLabel = new QLabel("", this);
    fpsLabel->setStyleSheet("color: #00E5FF; font-weight: bold;");

    statusLayout->addWidget(statusIndicator);
    statusLayout->addWidget(statusText);
    statusLayout->addStretch();
    statusLayout->addWidget(fpsLabel);

    rightLayout->addSpacing(10);
    rightLayout->addWidget(statusBar);

    // Mini Log (Collapsible look)
    miniLog = new QTextEdit(this);
    miniLog->setObjectName("MiniLog");
    miniLog->setFixedHeight(80);
    miniLog->setReadOnly(true);
    rightLayout->addSpacing(10);
    rightLayout->addWidget(miniLog);

    // Add panels to main layout
    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(rightPanel);

    // Connections
    connect(browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(btnToggle, &QPushButton::clicked, this, &MainWindow::onToggleStream);
}

void MainWindow::applyStyles()
{
    // 使用 R"(...)" 原始字符串包裹 CSS 样式
    QString qss = R"(
        QMainWindow {
            background-color: #1E1E2E;
        }
        QFrame#LeftPanel {
            background-color: #252535;
            border-right: 1px solid #303040;
        }
        
        /* 【修改点1】：全局字体改为 微软雅黑 (Microsoft YaHei) */
        /* 这样中英文的大小、粗细看起来就一致了 */
        QLabel {
            color: #CDD6F4;
            font-family: 'Microsoft YaHei', sans-serif; 
            font-size: 13px;
        }
        
        QLabel#AppTitle {
            font-size: 20px;
            font-weight: 800;
            color: #E6E9FF;
        }
        QLabel#BrandImage {
            background-color: #303040;
            border-radius: 10px;
            color: #606070;
            font-weight: bold;
        }
        QFrame#ConfigBox {
            background-color: rgba(255, 255, 255, 0.03);
            border-radius: 12px;
        }
        
        /* 【修改点2】：小标题样式调整 */
        QLabel#LabelHeader {
            color: #B7BDF8;
            font-size: 14px;      
            font-weight: bold;
            text-transform: uppercase;
            /* 这样英文不会强制变成大写，和中文高度更匹配 */
            margin-bottom: 0px;   
            margin-top: 15px;    
        }
        
       
        QLabel#LabelHeaderCN {
            color: #B7BDF8;
            font-size: 16px;      
            font-weight: bold;
            font-family: 'Microsoft YaHei', sans-serif;
            /* 这样英文不会强制变成大写，和中文高度更匹配 */
            margin-bottom: 0px;   
            margin-top: 15px;    
        }

        QLineEdit {
            background-color: #181825;
            border: 1px solid #313244;
            border-radius: 6px;
            color: #CDD6F4;
            /* 输入框里的字也统一用微软雅黑 */
            font-family: 'Microsoft YaHei', sans-serif;
            padding: 8px;
            selection-background-color: #45475A;
        }
        QLineEdit:focus {
            border: 1px solid #89B4FA;
        }
        
        QPushButton#BrowseButton {
            background-color: #313244;
            border-radius: 6px;
            color: #CDD6F4;
            border: none;
            font-size: 20px;      
            font-weight: 900;     
            padding-bottom: 5px;  
        }
        
        QPushButton#BrowseButton:hover {
            background-color: #45475A;
        }
        QRadioButton {
            color: #CDD6F4;
            spacing: 8px;
            font-family: 'Microsoft YaHei', sans-serif;
        }
        QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border-radius: 8px;
            border: 2px solid #45475A;
        }
        QRadioButton::indicator:checked {
            background-color: #89B4FA;
            border-color: #89B4FA;
        }
        QPushButton#ToggleBtn {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00E5FF, stop:1 #00B0FF);
            color: #000000;
            font-weight: bold;
            font-size: 15px;
            font-family: 'Microsoft YaHei', sans-serif;
            border-radius: 8px;
            border: none;
        }
        QPushButton#ToggleBtn:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #40EFFF, stop:1 #40C0FF);
        }
        QPushButton#ToggleBtn:disabled {
            background-color: #313244;
            color: #585B70;
        }
        QFrame#VideoContainer {
            background-color: #000000;
            border-radius: 12px;
            border: 1px solid #313244;
        }
        QFrame#StatusBar {
            background-color: #252535;
            border-radius: 8px;
        }
        QTextEdit#MiniLog {
            background-color: #11111B;
            border: 1px solid #313244;
            border-radius: 8px;
            color: #A6ADC8;
            font-family: 'Consolas', monospace; /* 日志还是用等宽字体好看，不用改 */
            font-size: 12px;
            padding: 5px;
        }
    )";
    this->setStyleSheet(qss);
}

void MainWindow::onBrowseClicked()
{
    QString filter = rbVideo->isChecked() ? "Video Files (*.mp4 *.h264 *.avi *.mkv)" : "Audio Files (*.aac *.mp3 *.wav)";
    QString fileName = QFileDialog::getOpenFileName(this, "Select Source File", QDir::currentPath(), filter);
    
    if (!fileName.isEmpty()) {
        fileInput->setText(fileName);
    }
}

void MainWindow::log(const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    miniLog->append(QString("<span style='color:#585B70;'>[%1]</span> %2").arg(timestamp, msg));
    // Auto scroll
    QTextCursor c = miniLog->textCursor();
    c.movePosition(QTextCursor::End);
    miniLog->setTextCursor(c);
}

void MainWindow::setStatus(const QString &status, const QString &color)
{
    statusText->setText(status.toUpper());
    statusText->setStyleSheet(QString("color: %1; font-weight: bold; margin-left: 5px;").arg(color));
    statusIndicator->setStyleSheet(QString("background-color: %1; border-radius: 5px;").arg(color));
}

void MainWindow::ensureMediaMtx()
{
    // Check if mediamtx is already running
    QProcess check;
    check.start("pgrep", QStringList() << "-x" << "mediamtx");
    check.waitForFinished();
    if (check.exitCode() == 0) {
        log("MediaMTX is already running.");
        return;
    }

    log("Starting MediaMTX...");
    mediaMtxProcess = new QProcess(this);
    
    QStringList paths;
    paths << "./mediamtx" << "../mediamtx" << "../../mediamtx" 
          << "/home/itzhou/rstp/VideoTransfer/mediamtx";
    
    QString program = "mediamtx"; 
    for (const QString &path : paths) {
        if (QFile::exists(path)) {
            program = QFileInfo(path).absoluteFilePath();
            log("Found MediaMTX at: " + program);
            mediaMtxProcess->setWorkingDirectory(QFileInfo(path).absolutePath());
            break;
        }
    }
    
    mediaMtxProcess->setProgram(program);
    mediaMtxProcess->start();
    
    if (!mediaMtxProcess->waitForStarted()) {
        log("<font color='#FF5555'>Failed to start MediaMTX!</font>");
    } else {
        log("MediaMTX started.");
    }
}

void MainWindow::onToggleStream()
{
    if (isRunning) {
        stopAll();
    } else {
        // Start Logic
        QString url = urlInput->text().trimmed();
        QString file = fileInput->text().trimmed();
        
        if (url.isEmpty()) {
            QMessageBox::warning(this, "Error", "Please enter an RTSP URL.");
            return;
        }
        if (file.isEmpty() || !QFile::exists(file)) {
            QMessageBox::warning(this, "Error", "Please select a valid source file.");
            return;
        }

        // Lock UI
        isRunning = true;
        urlInput->setEnabled(false);
        fileInput->setEnabled(false);
        browseBtn->setEnabled(false);
        rbVideo->setEnabled(false);
        rbAudio->setEnabled(false);
        
        // Update Button Style for Stop
        btnToggle->setText("STOP STREAM");
        btnToggle->setStyleSheet(R"(
            QPushButton#ToggleBtn {
                background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FF5555, stop:1 #FF0000);
                color: #FFFFFF;
                font-weight: bold;
                font-size: 15px;
                border-radius: 8px;
                border: none;
            }
            QPushButton#ToggleBtn:hover {
                background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FF8888, stop:1 #FF3333);
            }
        )");

        setStatus("INITIALIZING...", "#FAB387"); // Orange

        ensureMediaMtx();
        QThread::msleep(500);

        if (rbVideo->isChecked()) {
            startVideoMode();
        } else {
            startAudioMode();
        }
    }
}

void MainWindow::startVideoMode()
{
    log("Starting Video Mode...");
    QString url = urlInput->text().trimmed();
    QString file = fileInput->text().trimmed();
    
    ffmpegProcess = new QProcess(this);
    QStringList args;
    args << "-re" << "-stream_loop" << "-1" << "-i" << file
         << "-c:v" << "libx264" << "-preset" << "ultrafast" << "-tune" << "zerolatency"
         << "-b:v" << "1000k" << "-s" << "1280x720" << "-r" << "25" << "-an"
         << "-f" << "rtsp" << url;
    
    ffmpegProcess->setProgram("ffmpeg");
    ffmpegProcess->setArguments(args);
    connect(ffmpegProcess, &QProcess::readyReadStandardError, this, &MainWindow::processOutput);
    
    ffmpegProcess->start();
    if (!ffmpegProcess->waitForStarted()) {
        log("<font color='#FF5555'>Failed to start FFmpeg!</font>");
        stopAll();
        return;
    }
    
    setStatus("TRANSMITTING", "#A6E3A1"); // Green
    videoOverlayText->setText("CONNECTING...");

    QThread::msleep(1500);

    videoThread = new VideoThread(url, this);
    connect(videoThread, &VideoThread::frameReady, this, &MainWindow::updateFrame);
    connect(videoThread, &VideoThread::statsUpdated, this, &MainWindow::updateStats);
    connect(videoThread, &VideoThread::errorOccurred, this, &MainWindow::handleError);
    videoThread->start();
}

void MainWindow::startAudioMode()
{
    log("Starting Audio Mode...");
    QString url = urlInput->text().trimmed();
    QString file = fileInput->text().trimmed();
    
    ffmpegProcess = new QProcess(this);
    QStringList args;
    args << "-re" << "-stream_loop" << "-1" << "-i" << file
         << "-c:a" << "aac" << "-b:a" << "128k"
         << "-f" << "rtsp" << url;

    ffmpegProcess->setProgram("ffmpeg");
    ffmpegProcess->setArguments(args);
    ffmpegProcess->start();
    
    if (!ffmpegProcess->waitForStarted()) {
        log("<font color='#FF5555'>Failed to start FFmpeg!</font>");
        stopAll();
        return;
    }

    setStatus("AUDIO STREAMING", "#A6E3A1");
    videoOverlayText->setText("AUDIO ONLY MODE");

    QThread::msleep(1500);

    playerProcess = new QProcess(this);
    QStringList playerArgs;
    playerArgs << "-nodisp" << "-autoexit" << url;
    
    playerProcess->setProgram("ffplay");
    playerProcess->setArguments(playerArgs);
    playerProcess->start();
}

void MainWindow::stopAll()
{
    log("Stopping stream...");
    
    if (videoThread) {
        videoThread->stop();
        videoThread->wait();
        delete videoThread;
        videoThread = nullptr;
    }

    if (ffmpegProcess) {
        ffmpegProcess->terminate();
        ffmpegProcess->waitForFinished(1000);
        ffmpegProcess->kill();
        delete ffmpegProcess;
        ffmpegProcess = nullptr;
    }

    if (playerProcess) {
        playerProcess->terminate();
        playerProcess->waitForFinished(1000);
        playerProcess->kill();
        delete playerProcess;
        playerProcess = nullptr;
    }

    if (mediaMtxProcess) {
        mediaMtxProcess->terminate();
        mediaMtxProcess->kill();
        delete mediaMtxProcess;
        mediaMtxProcess = nullptr;
    }

    videoLabel->clear();
    videoOverlayText->setText("NO SIGNAL");
    videoOverlayText->raise(); // Make sure overlay is visible
    fpsLabel->clear();
    
    isRunning = false;
    
    // Restore UI State
    urlInput->setEnabled(true);
    fileInput->setEnabled(true);
    browseBtn->setEnabled(true);
    rbVideo->setEnabled(true);
    rbAudio->setEnabled(true);

    setStatus("READY", "#888888");

    // Restore Start Button Style
    btnToggle->setText("开始传输");
    btnToggle->setStyleSheet(R"(
        QPushButton#ToggleBtn {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00E5FF, stop:1 #00B0FF);
            color: #000000;
            font-weight: bold;
            font-size: 15px;
            border-radius: 8px;
            border: none;
        }
        QPushButton#ToggleBtn:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #40EFFF, stop:1 #40C0FF);
        }
    )");
}

void MainWindow::updateFrame(const QImage &image)
{
    videoOverlayText->setText(""); // Hide text when video plays
    QPixmap p = QPixmap::fromImage(image);
    videoLabel->setPixmap(p.scaled(videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::updateStats(int frameCount, double fps)
{
    fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
}

void MainWindow::handleError(const QString &msg)
{
    log("<font color='#FF5555'>Error: " + msg + "</font>");
}

void MainWindow::processOutput()
{
    QByteArray data = ffmpegProcess->readAllStandardError();
    if (!data.isEmpty()) {
        QString output = QString(data);
        if (output.contains("Error", Qt::CaseInsensitive) || output.contains("Failed", Qt::CaseInsensitive)) {
             log("FFmpeg: " + output.trimmed());
        }
    }
}
