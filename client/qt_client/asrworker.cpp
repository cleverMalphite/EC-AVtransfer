#include "asrworker.h"
#include "whisper.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <cmath>

AsrWorker::AsrWorker(QObject *parent)
    : QThread(parent)
{
    // Default model path, can be changed via setModelPath
    m_modelPath = QCoreApplication::applicationDirPath() + "/models/ggml-base.bin";
}

AsrWorker::~AsrWorker()
{
    stop();
    wait();
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
}

void AsrWorker::setModelPath(const QString &path)
{
    m_modelPath = path;
}

void AsrWorker::setInputSampleRate(int rate)
{
    m_inputSampleRate = rate;
}

void AsrWorker::receiveAudio(const char* data, int len)
{
    QMutexLocker locker(&m_mutex);
    m_incomingBuffer.append(data, len);
    m_condition.wakeOne();
}

void AsrWorker::stop()
{
    QMutexLocker locker(&m_mutex);
    m_running = false;
    m_condition.wakeOne();
}

void AsrWorker::run()
{
    // Initialize Whisper Context
    struct whisper_context_params cparams = whisper_context_default_params();
    
    // Check if model exists
    if (!QFile::exists(m_modelPath)) {
        emit logMessage("ASR Error: Model file not found at " + m_modelPath);
        
        // Try to look in source dir if not in build dir
        QStringList candidates;
        candidates << "/home/itzhou/rstp/VideoTransfer/3rdparty/whisper/models/ggml-base.bin"
                   << "/home/itzhou/rstp/VideoTransfer/3rdparty/whisper/models/ggml-tiny.bin"
                   << "/home/itzhou/rstp/VideoTransfer/3rdparty/whisper/models/for-tests-ggml-tiny.en.bin";
                   
        bool found = false;
        for (const QString &alt : candidates) {
            if (QFile::exists(alt) && QFile(alt).size() > 0) {
                m_modelPath = alt;
                emit logMessage("ASR Info: Found model at " + m_modelPath);
                found = true;
                break;
            }
        }
        
        if (!found) {
             emit logMessage("ASR Error: No valid model found. Please download ggml-base.bin or ggml-tiny.bin.");
             return; 
        }
    }

    m_ctx = whisper_init_from_file_with_params(m_modelPath.toStdString().c_str(), cparams);
    if (!m_ctx) {
        emit logMessage("ASR Error: Failed to initialize whisper context");
        return;
    }

    emit logMessage("ASR Worker started. Model loaded.");

    while (m_running) {
        QByteArray rawData;
        
        {
            QMutexLocker locker(&m_mutex);
            while (m_incomingBuffer.isEmpty() && m_running) {
                // Wait for data or timeout to check running status
                m_condition.wait(&m_mutex, 500);
            }
            if (!m_running) break;
            
            rawData = m_incomingBuffer;
            m_incomingBuffer.clear();
        }

        if (rawData.isEmpty()) continue;
        
        // Debug log for data flow (throttle)
        static int logCounter = 0;
        if (logCounter++ % 50 == 0) {
            // qDebug() << "ASR received " << rawData.size() << " bytes";
        }

        // Process Raw Data (Resample and Convert)
        // Input: 16-bit PCM (short), Mono
        // Output: Float, 16kHz
        
        int nSamples = rawData.size() / 2;
        const short* pcm = reinterpret_cast<const short*>(rawData.constData());
        
        // Simple resampling
        double ratio = (double)m_inputSampleRate / m_targetSampleRate;
        
        // Estimated output size
        int outSamples = (int)(nSamples / ratio);
        
        // We accumulate into m_audioBuffer (which is member variable, but accessed only by this thread 
        // OR we need lock if we want to clear it safely? 
        // Actually receiveAudio touches m_incomingBuffer, run touches m_audioBuffer. 
        // So m_audioBuffer is safe to access without lock here EXCEPT if we want to share it.
        // But for now it's local to run loop logic mostly.
        
        for (int i = 0; i < outSamples; ++i) {
            int inputIdx = (int)(i * ratio);
            if (inputIdx >= nSamples) break;
            
            // Convert short to float [-1.0, 1.0]
            float sample = (float)pcm[inputIdx] / 32768.0f;
            m_audioBuffer.append(sample);
        }

        // Check if we have enough data (3 seconds = 3 * 16000 = 48000 samples)
        if (m_audioBuffer.size() >= (3 * 16000)) {
            transcribe();
        }
    }
}

void AsrWorker::transcribe()
{
    if (m_audioBuffer.isEmpty()) return;
    
    // Whisper parameters
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress = false;
    wparams.print_special = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.translate = false;
    // Force English if using the test model, otherwise auto or chinese
    if (m_modelPath.contains("tiny.en")) {
        wparams.language = "en";
    } else {
        wparams.language = "zh"; // Default to Chinese
    }
    wparams.n_threads = 4;
    wparams.no_context = true; // Don't use past context for now to keep it simple

    // Run inference
    int ret = whisper_full(m_ctx, wparams, m_audioBuffer.data(), m_audioBuffer.size());
    if (ret != 0) {
        emit logMessage("ASR Error: failed to process audio, code: " + QString::number(ret));
        return;
    }

    // Get text
    QString resultText;
    int n_segments = whisper_full_n_segments(m_ctx);
    if (n_segments == 0) {
        // emit logMessage("ASR: No speech detected.");
    }
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(m_ctx, i);
        resultText += QString::fromUtf8(text);
    }

    if (!resultText.trimmed().isEmpty()) {
        emit speechRecognized(resultText.trimmed());
    }

    // Clear buffer after processing
    // In a real continuous dictation, you might want to keep the last part 
    // or use a sliding window. For this task, we clear.
    m_audioBuffer.clear();
}
