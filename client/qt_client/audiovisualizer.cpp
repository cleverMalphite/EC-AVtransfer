#include "audiovisualizer.h"
#include <QPainter>
#include <QDebug>

AudioVisualizer::AudioVisualizer(QWidget *parent)
    : QWidget(parent)
    , m_bufferSize(2000)
    , m_currentIndex(0)
{
    // Initialize buffer with zeros
    m_audioBuffer.resize(m_bufferSize);
    m_audioBuffer.fill(0);
    
    // Set background color to black effectively by ensuring the widget paints its background
    setAttribute(Qt::WA_OpaquePaintEvent);
    
    // Set a minimum size to ensure it's visible
    setMinimumHeight(100);
}

AudioVisualizer::~AudioVisualizer()
{
}

void AudioVisualizer::pushAudioData(const char* data, int len)
{
    if (!data || len <= 0) return;

    // 16-bit audio, so we interpret bytes as shorts
    // len is in bytes, so number of samples is len / 2
    int sampleCount = len / 2;
    const short* samples = reinterpret_cast<const short*>(data);

    for (int i = 0; i < sampleCount; ++i) {
        m_audioBuffer[m_currentIndex] = samples[i];
        m_currentIndex = (m_currentIndex + 1) % m_bufferSize;
    }

    update(); // Trigger repaint
}

void AudioVisualizer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    
    // Fill background with black
    painter.fillRect(rect(), Qt::black);

    // Setup pen for waveform
    QPen pen(QColor(0, 255, 0)); // Fluorescent Green
    pen.setWidth(1);
    painter.setPen(pen);

    int w = width();
    int h = height();
    int centerY = h / 2;

    if (w <= 0 || h <= 0) return;

    // We want to map the ring buffer to the screen width.
    // Start drawing from the oldest sample which is at m_currentIndex
    
    int startIndex = m_currentIndex;
    
    // Step size in buffer index per pixel
    // If buffer size < width, we might want to just draw points, but usually buffer size (2000) > width
    // If we have more samples than pixels, we might skip some.
    // If we have fewer samples than pixels, we step < 1.
    
    double samplesPerPixel = (double)m_bufferSize / w;

    QPointF lastPoint;
    bool firstPoint = true;

    for (int x = 0; x < w; ++x) {
        // Find which sample corresponds to this x
        int offset = (int)(x * samplesPerPixel);
        int bufferIdx = (startIndex + offset) % m_bufferSize;
        short sampleVal = m_audioBuffer[bufferIdx];

        // Map sample value (-32768 to 32767) to height (0 to h)
        // Normalize to -1.0 to 1.0
        double normalized = (double)sampleVal / 32768.0;
        
        // Scale to half height
        // Invert Y so positive is up
        double plotY = centerY - (normalized * (h / 2.0));
        
        // Clamp logic to stay within bounds (optional but good for safety)
        if (plotY < 0) plotY = 0;
        if (plotY > h) plotY = h;

        QPointF currentPoint(x, plotY);
        
        if (firstPoint) {
            lastPoint = currentPoint;
            firstPoint = false;
        } else {
            painter.drawLine(lastPoint, currentPoint);
            lastPoint = currentPoint;
        }
    }
}
