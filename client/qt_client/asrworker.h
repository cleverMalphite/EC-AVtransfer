#ifndef ASRWORKER_H
#define ASRWORKER_H

#include <QThread>
#include <QMutex>
#include <QVector>
#include <QString>
#include <QByteArray>
#include <QWaitCondition>

// Forward declaration to avoid including whisper.h in header
struct whisper_context;

class AsrWorker : public QThread {
    Q_OBJECT

public:
    explicit AsrWorker(QObject *parent = nullptr);
    ~AsrWorker() override;

    void setModelPath(const QString &path);
    void setInputSampleRate(int rate);
    
    // Thread-safe audio input
    void receiveAudio(const char* data, int len);

    void stop();

signals:
    void speechRecognized(QString text);
    void logMessage(QString msg);

protected:
    void run() override;

private:
    // Helper to process buffered audio
    void transcribe();

    QString m_modelPath;
    struct whisper_context *m_ctx = nullptr;

    QMutex m_mutex;
    QWaitCondition m_condition;
    bool m_running = true;
    bool m_hasNewData = false;

    // Buffer for accumulation (16kHz float samples)
    QVector<float> m_audioBuffer;
    
    // Temporary buffer for incoming raw data
    QByteArray m_incomingBuffer;
    
    int m_inputSampleRate = 44100; // Default
    int m_targetSampleRate = 16000;
};

#endif // ASRWORKER_H
