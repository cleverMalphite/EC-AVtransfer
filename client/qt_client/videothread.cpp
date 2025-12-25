#include "videothread.h"
#include <QDebug>
#include <QDateTime>

VideoThread::VideoThread(const QString &url, QObject *parent)
    : QThread(parent), url_(url), running_(true),
      formatCtx_(nullptr), 
      vCodecCtx_(nullptr), vCodec_(nullptr), swsCtx_(nullptr), videoStreamIndex_(-1), frameCount_(0),
      aCodecCtx_(nullptr), aCodec_(nullptr), swrCtx_(nullptr), audioStreamIndex_(-1)
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
    if (swrCtx_) {
        swr_free(&swrCtx_);
        swrCtx_ = nullptr;
    }
    if (vCodecCtx_) {
        avcodec_free_context(&vCodecCtx_);
        vCodecCtx_ = nullptr;
    }
    if (aCodecCtx_) {
        avcodec_free_context(&aCodecCtx_);
        aCodecCtx_ = nullptr;
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

    // Find Video Stream
    videoStreamIndex_ = -1;
    for (unsigned int i = 0; i < formatCtx_->nb_streams; i++) {
        if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            break;
        }
    }

    if (videoStreamIndex_ != -1) {
        AVCodecParameters* codecParams = formatCtx_->streams[videoStreamIndex_]->codecpar;
        vCodec_ = avcodec_find_decoder(codecParams->codec_id);
        if (vCodec_) {
            vCodecCtx_ = avcodec_alloc_context3(vCodec_);
            avcodec_parameters_to_context(vCodecCtx_, codecParams);
            vCodecCtx_->flags2 |= AV_CODEC_FLAG2_CHUNKS;
            vCodecCtx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
            if (avcodec_open2(vCodecCtx_, vCodec_, nullptr) < 0) {
                // Failed to open video codec
                avcodec_free_context(&vCodecCtx_);
                videoStreamIndex_ = -1;
            }
        } else {
            videoStreamIndex_ = -1;
        }
    }

    // Find Audio Stream
    audioStreamIndex_ = -1;
    for (unsigned int i = 0; i < formatCtx_->nb_streams; i++) {
        if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex_ = i;
            break;
        }
    }

    if (audioStreamIndex_ != -1) {
        AVCodecParameters* codecParams = formatCtx_->streams[audioStreamIndex_]->codecpar;
        aCodec_ = avcodec_find_decoder(codecParams->codec_id);
        if (aCodec_) {
            aCodecCtx_ = avcodec_alloc_context3(aCodec_);
            avcodec_parameters_to_context(aCodecCtx_, codecParams);
            if (avcodec_open2(aCodecCtx_, aCodec_, nullptr) < 0) {
                 avcodec_free_context(&aCodecCtx_);
                 audioStreamIndex_ = -1;
            } else {
                 // Initialize Resampler to S16 Mono
                 // We don't know sample rate yet? We do from codecCtx
                 swrCtx_ = swr_alloc_set_opts(nullptr,
                                              AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_S16, aCodecCtx_->sample_rate,
                                              aCodecCtx_->channel_layout, aCodecCtx_->sample_fmt, aCodecCtx_->sample_rate,
                                              0, nullptr);
                 swr_init(swrCtx_);
            }
        } else {
            audioStreamIndex_ = -1;
        }
    }

    if (videoStreamIndex_ == -1 && audioStreamIndex_ == -1) {
        emit errorOccurred("No usable video or audio stream found");
        return;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    // Video Buffers
    AVFrame* frameRGB = nullptr;
    uint8_t* buffer = nullptr;
    
    if (videoStreamIndex_ != -1) {
        frameRGB = av_frame_alloc();
        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, vCodecCtx_->width, vCodecCtx_->height, 1);
        buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
        av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, vCodecCtx_->width, vCodecCtx_->height, 1);
        swsCtx_ = sws_getContext(vCodecCtx_->width, vCodecCtx_->height, vCodecCtx_->pix_fmt,
                                 vCodecCtx_->width, vCodecCtx_->height, AV_PIX_FMT_RGB24,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    frameCount_ = 0;
    int64_t startTime = QDateTime::currentMSecsSinceEpoch();

    while (running_) {
        {
            QMutexLocker locker(&mutex_);
            if (!running_) break;
        }

        if (av_read_frame(formatCtx_, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex_ && videoStreamIndex_ != -1) {
                if (avcodec_send_packet(vCodecCtx_, packet) == 0) {
                    while (avcodec_receive_frame(vCodecCtx_, frame) == 0) {
                        frameCount_++;
                        sws_scale(swsCtx_, frame->data, frame->linesize, 0,
                                  vCodecCtx_->height, frameRGB->data, frameRGB->linesize);

                        QImage img(frameRGB->data[0], vCodecCtx_->width, vCodecCtx_->height, 
                                   frameRGB->linesize[0], QImage::Format_RGB888);
                        
                        emit frameReady(img.copy());

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
            else if (packet->stream_index == audioStreamIndex_ && audioStreamIndex_ != -1) {
                if (avcodec_send_packet(aCodecCtx_, packet) == 0) {
                    while (avcodec_receive_frame(aCodecCtx_, frame) == 0) {
                         // Resample
                         int dst_nb_samples = av_rescale_rnd(swr_get_delay(swrCtx_, aCodecCtx_->sample_rate) +
                                                            frame->nb_samples, aCodecCtx_->sample_rate, aCodecCtx_->sample_rate, AV_ROUND_UP);
                         
                         QByteArray outputBytes;
                         outputBytes.resize(dst_nb_samples * 2); // 2 bytes per sample (S16)
                         uint8_t* outData[1] = { (uint8_t*)outputBytes.data() };
                         
                         int ret = swr_convert(swrCtx_, outData, dst_nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
                         if (ret > 0) {
                             outputBytes.resize(ret * 2); // Adjust size to actual converted samples
                             emit audioDataReady(outputBytes);
                         }
                    }
                }
            }
            av_packet_unref(packet);
        } else {
             // Loop or error
        }
    }

    if (buffer) av_free(buffer);
    if (frameRGB) av_frame_free(&frameRGB);
    av_frame_free(&frame);
    av_packet_free(&packet);
}
