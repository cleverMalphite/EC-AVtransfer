#include "videothread.h"
#include <QDebug>
#include <QDateTime>

VideoThread::VideoThread(const QString &url, QObject *parent)
    : QThread(parent), url_(url), running_(true),
      formatCtx_(nullptr), codecCtx_(nullptr), codec_(nullptr), swsCtx_(nullptr),
      videoStreamIndex_(-1), frameCount_(0)
{
}

VideoThread::~VideoThread()
{
    stop();
    wait();
    cleanup();
}

void VideoThread::stop()
{
    QMutexLocker locker(&mutex_);
    running_ = false;
}

void VideoThread::cleanup()
{
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }
    if (formatCtx_) {
        avformat_close_input(&formatCtx_);
        formatCtx_ = nullptr;
    }
}

void VideoThread::run()
{
    formatCtx_ = avformat_alloc_context();
    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "max_delay", "500000", 0);

    // Try to open input
    if (avformat_open_input(&formatCtx_, url_.toStdString().c_str(), nullptr, &options) != 0) {
        emit errorOccurred("Failed to open RTSP stream");
        return;
    }
    av_dict_free(&options);

    if (avformat_find_stream_info(formatCtx_, nullptr) < 0) {
        emit errorOccurred("Failed to find stream info");
        return;
    }

    videoStreamIndex_ = -1;
    for (unsigned int i = 0; i < formatCtx_->nb_streams; i++) {
        if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            break;
        }
    }

    if (videoStreamIndex_ == -1) {
        emit errorOccurred("No video stream found");
        return;
    }

    AVCodecParameters* codecParams = formatCtx_->streams[videoStreamIndex_]->codecpar;
    codec_ = avcodec_find_decoder(codecParams->codec_id);
    if (!codec_) {
        emit errorOccurred("Decoder not found");
        return;
    }

    codecCtx_ = avcodec_alloc_context3(codec_);
    avcodec_parameters_to_context(codecCtx_, codecParams);
    
    // Low delay flags
    codecCtx_->flags2 |= AV_CODEC_FLAG2_CHUNKS;
    codecCtx_->flags |= AV_CODEC_FLAG_LOW_DELAY;

    if (avcodec_open2(codecCtx_, codec_, nullptr) < 0) {
        emit errorOccurred("Failed to open codec");
        return;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* frameRGB = av_frame_alloc();

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecCtx_->width, codecCtx_->height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecCtx_->width, codecCtx_->height, 1);

    swsCtx_ = sws_getContext(codecCtx_->width, codecCtx_->height, codecCtx_->pix_fmt,
                             codecCtx_->width, codecCtx_->height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);

    frameCount_ = 0;
    int64_t startTime = QDateTime::currentMSecsSinceEpoch();

    while (running_) {
        {
            QMutexLocker locker(&mutex_);
            if (!running_) break;
        }

        if (av_read_frame(formatCtx_, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex_) {
                if (avcodec_send_packet(codecCtx_, packet) == 0) {
                    while (avcodec_receive_frame(codecCtx_, frame) == 0) {
                        frameCount_++;

                        // Scale to RGB
                        sws_scale(swsCtx_, frame->data, frame->linesize, 0,
                                  codecCtx_->height, frameRGB->data, frameRGB->linesize);

                        // Create QImage
                        // QImage::Format_RGB888 expects data in R, G, B order
                        QImage img(frameRGB->data[0], codecCtx_->width, codecCtx_->height, 
                                   frameRGB->linesize[0], QImage::Format_RGB888);
                        
                        emit frameReady(img.copy()); // Deep copy to ensure thread safety

                        // Calculate FPS every 25 frames
                        if (frameCount_ % 25 == 0) {
                            int64_t now = QDateTime::currentMSecsSinceEpoch();
                            double elapsed = (now - startTime) / 1000.0;
                            if (elapsed > 0) {
                                double fps = frameCount_ / elapsed;
                                emit statsUpdated(frameCount_, fps);
                            }
                        }
                    }
                }
            }
            av_packet_unref(packet);
        } else {
            // End of stream or error, verify if we should exit or retry?
            // For now, just break loop
            // QThread::msleep(10); 
        }
    }

    av_free(buffer);
    av_frame_free(&frameRGB);
    av_frame_free(&frame);
    av_packet_free(&packet);
}
