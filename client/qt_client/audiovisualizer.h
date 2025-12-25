#ifndef AUDIOVISUALIZER_H
#define AUDIOVISUALIZER_H

#include <QWidget>
#include <QVector>

class AudioVisualizer : public QWidget {
    Q_OBJECT

public:
    explicit AudioVisualizer(QWidget *parent = nullptr);
    ~AudioVisualizer() override;

    // Push raw PCM audio data (assuming 16-bit mono)
    void pushAudioData(const char* data, int len);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<short> m_audioBuffer;
    int m_bufferSize;
    int m_currentIndex;
};

#endif // AUDIOVISUALIZER_H
