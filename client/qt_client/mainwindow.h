#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QLineEdit>
#include <QFileDialog>
#include <QGridLayout>
#include <QTextEdit>
#include <QGroupBox>
#include <QFrame>
#include <QSplitter>
#include <QStackedLayout>

#include "videothread.h"
#include "audiovisualizer.h"
#include "asrworker.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onToggleStream(); // Combined Start/Stop
    void onBrowseClicked();
    void updateFrame(const QImage &image);
    void updateStats(int frameCount, double fps);
    void handleError(const QString &msg);
    void processOutput();
    
    // New Slots
    void onAudioDataReady(const QByteArray &data);
    void onSpeechRecognized(QString text);

private:
    void setupUi();
    void applyStyles();
    void startVideoMode();
    void startAudioMode();
    void stopAll();
    void log(const QString &msg);
    void ensureMediaMtx();
    void setStatus(const QString &status, const QString &color = "#CDD6F4");
    
    // UI Elements
    QWidget *centralWidget;
    
    // Left Panel
    QFrame *leftPanel;
    QLabel *brandImagePlaceholder;
    QLabel *appTitle;
    
    // Config
    QLineEdit *urlInput;
    QLineEdit *fileInput;
    QPushButton *browseBtn;
    QRadioButton *rbVideo;
    QRadioButton *rbAudio;
    
    // Actions
    QPushButton *btnToggle; // Start/Stop button
    
    // Right Area
    QFrame *rightPanel;
    QFrame *videoContainer;
    QStackedLayout *displayLayout; // Switch between Video and Visualizer
    QLabel *videoLabel;
    AudioVisualizer *audioVisualizer;
    QLabel *videoOverlayText; // For "No Signal" or Loading
    
    // Subtitle
    // QLabel *subtitleLabel; // Removed as per request
    
    // Status & Log
    QFrame *statusBar;
    QLabel *statusIndicator; // Colored dot
    QLabel *statusText;
    QLabel *fpsLabel;
    QTextEdit *miniLog; // Small log area
    
    // Logic
    VideoThread *videoThread;
    AsrWorker *asrWorker;
    QProcess *mediaMtxProcess;
    QProcess *ffmpegProcess;
    QProcess *playerProcess; 
    
    bool isRunning;
};

#endif // MAINWINDOW_H
