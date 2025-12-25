#ifndef VIDEOTHREAD_H
#define VIDEOTHREAD_H

#include <QThread>
#include <QImage>
#include <QMutex>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class VideoThread : public QThread
{
    Q_OBJECT
public:
    explicit VideoThread(const QString &url, QObject *parent = nullptr);
    ~VideoThread();

    void stop();

signals:
    void frameReady(const QImage &image);
    void errorOccurred(const QString &msg);
    void statsUpdated(int frameCount, double fps);

protected:
    void run() override;

private:
    QString url_;
    bool running_;
    QMutex mutex_;

    AVFormatContext* formatCtx_;
    AVCodecContext* codecCtx_;
    const AVCodec* codec_;
    SwsContext* swsCtx_;
    int videoStreamIndex_;
    int frameCount_;
    
    void cleanup();
};

#endif // VIDEOTHREAD_H
