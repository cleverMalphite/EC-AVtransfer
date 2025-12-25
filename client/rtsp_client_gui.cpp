#include <iostream>
#include <string>
#include <signal.h>
#include <chrono>
#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

static bool g_running = true;

void signalHandler(int signum) {
    std::cout << "\n接收到停止信号，正在退出..." << std::endl;
    g_running = false;
    cv::destroyAllWindows();
}

class RtspClientGUI {
public:
    RtspClientGUI(const std::string& url, const std::string& windowName = "Video Transmission") 
        : url_(url), windowName_(windowName),
          formatCtx_(nullptr), codecCtx_(nullptr), 
          codec_(nullptr), swsCtx_(nullptr),
          videoStreamIndex_(-1), frameCount_(0) {}
    
    ~RtspClientGUI() {
        cleanup();
    }
    
    bool init() {
        formatCtx_ = avformat_alloc_context();
        
        AVDictionary* options = nullptr;
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "max_delay", "500000", 0);
        
        std::cout << "正在连接RTSP流: " << url_ << std::endl;
        
        int ret = avformat_open_input(&formatCtx_, url_.c_str(), nullptr, &options);
        av_dict_free(&options);
        
        if (ret != 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "无法打开流: " << errbuf << std::endl;
            return false;
        }
        
        std::cout << "流已打开，正在获取流信息..." << std::endl;
        
        formatCtx_->max_analyze_duration = 5 * AV_TIME_BASE;
        formatCtx_->probesize = 10000000;
        
        ret = avformat_find_stream_info(formatCtx_, nullptr);
        if (ret < 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "无法获取流信息: " << errbuf << std::endl;
            return false;
        }
        
        // 查找视频流
        for (unsigned int i = 0; i < formatCtx_->nb_streams; i++) {
            if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex_ = i;
                break;
            }
        }
        
        if (videoStreamIndex_ == -1) {
            std::cerr << "未找到视频流" << std::endl;
            return false;
        }
        
        // 获取解码器
        AVCodecParameters* codecParams = formatCtx_->streams[videoStreamIndex_]->codecpar;
        codec_ = avcodec_find_decoder(codecParams->codec_id);
        if (!codec_) {
            std::cerr << "未找到解码器" << std::endl;
            return false;
        }
        
        codecCtx_ = avcodec_alloc_context3(codec_);
        if (avcodec_parameters_to_context(codecCtx_, codecParams) < 0) {
            std::cerr << "无法复制解码器参数" << std::endl;
            return false;
        }
        
        codecCtx_->flags2 |= AV_CODEC_FLAG2_CHUNKS;
        codecCtx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        
        if (avcodec_open2(codecCtx_, codec_, nullptr) < 0) {
            std::cerr << "无法打开解码器" << std::endl;
            return false;
        }
        
        std::cout << "连接成功！" << std::endl;
        std::cout << "视频信息: " << codecCtx_->width << "x" << codecCtx_->height << std::endl;
        
        // 创建显示窗口
        cv::namedWindow(windowName_, cv::WINDOW_NORMAL);
        
        // 限制窗口大小，避免超出屏幕
        int maxWidth = 1280;
        int maxHeight = 720;
        int displayWidth = codecCtx_->width;
        int displayHeight = codecCtx_->height;
        
        // 如果视频太大，按比例缩小
        if (displayWidth > maxWidth || displayHeight > maxHeight) {
            float scale = std::min((float)maxWidth / displayWidth, 
                                  (float)maxHeight / displayHeight);
            displayWidth = (int)(displayWidth * scale);
            displayHeight = (int)(displayHeight * scale);
            std::cout << "视频分辨率过大，窗口调整为: " << displayWidth << "x" << displayHeight << std::endl;
        }
        
        cv::resizeWindow(windowName_, displayWidth, displayHeight);
        
        return true;
    }
    
    void displayVideo() {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        AVFrame* frameRGB = av_frame_alloc();
        
        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, codecCtx_->width, 
                                                 codecCtx_->height, 1);
        uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
        
        av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, 
                            AV_PIX_FMT_BGR24, codecCtx_->width, codecCtx_->height, 1);
        
        // 创建转换上下文
        swsCtx_ = sws_getContext(codecCtx_->width, codecCtx_->height, codecCtx_->pix_fmt,
                                 codecCtx_->width, codecCtx_->height, AV_PIX_FMT_BGR24,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
        
        std::cout << "开始接收视频流并显示" << std::endl;
        std::cout << "按 'q' 或 ESC 键退出，按 's' 键截图" << std::endl;
        
        auto startTime = std::chrono::steady_clock::now();
        int screenshotCount = 0;
        
        while (g_running && av_read_frame(formatCtx_, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex_) {
                if (avcodec_send_packet(codecCtx_, packet) == 0) {
                    while (avcodec_receive_frame(codecCtx_, frame) == 0) {
                        frameCount_++;
                        
                        // 转换为BGR格式
                        sws_scale(swsCtx_, frame->data, frame->linesize, 0, 
                                 codecCtx_->height, frameRGB->data, frameRGB->linesize);
                        
                        // 创建OpenCV Mat
                        cv::Mat img(codecCtx_->height, codecCtx_->width, CV_8UC3, 
                                   frameRGB->data[0], frameRGB->linesize[0]);
                        
                        // 添加信息叠加
                        auto currentTime = std::chrono::steady_clock::now();
                        double elapsed = std::chrono::duration<double>(currentTime - startTime).count();
                        double fps = frameCount_ / elapsed;
                        
                        std::string info = "FPS: " + std::to_string((int)fps) + 
                                         " | Frames: " + std::to_string(frameCount_);
                        cv::putText(img, info, cv::Point(10, 30), 
                                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
                        
                        // 显示图像
                        cv::imshow(windowName_, img);
                        
                        // 处理键盘事件
                        int key = cv::waitKey(1);
                        if (key == 'q' || key == 27) { // 'q' 或 ESC
                            g_running = false;
                            break;
                        } else if (key == 's') { // 截图
                            std::string filename = "screenshot_" + 
                                                 std::to_string(++screenshotCount) + ".jpg";
                            cv::imwrite(filename, img);
                            std::cout << "截图已保存: " << filename << std::endl;
                        }
                        
                        // 每25帧打印一次统计
                        if (frameCount_ % 25 == 0) {
                            std::cout << "已接收 " << frameCount_ << " 帧 | "
                                      << "实时帧率: " << fps << " fps" << std::endl;
                        }
                    }
                }
            }
            av_packet_unref(packet);
        }
        
        av_free(buffer);
        av_frame_free(&frameRGB);
        av_frame_free(&frame);
        av_packet_free(&packet);
        
        cv::destroyAllWindows();
        
        std::cout << "\n接收完成！总共接收 " << frameCount_ << " 帧" << std::endl;
    }
    
private:
    void cleanup() {
        if (swsCtx_) {
            sws_freeContext(swsCtx_);
        }
        if (codecCtx_) {
            avcodec_free_context(&codecCtx_);
        }
        if (formatCtx_) {
            avformat_close_input(&formatCtx_);
        }
    }
    
    std::string url_;
    std::string windowName_;
    AVFormatContext* formatCtx_;
    AVCodecContext* codecCtx_;
    const AVCodec* codec_;
    SwsContext* swsCtx_;
    int videoStreamIndex_;
    int frameCount_;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " <rtsp_url> [窗口标题]" << std::endl;
        std::cout << "示例:" << std::endl;
        std::cout << "  " << argv[0] << " rtsp://localhost:8554/live" << std::endl;
        std::cout << "  " << argv[0] << " rtsp://192.168.100.2:8554/live \"无人机视频\"" << std::endl;
        return -1;
    }
    
    signal(SIGINT, signalHandler);
    
    std::string url = argv[1];
    std::string windowName = argc > 2 ? argv[2] : "RTSP视频接收";
    
    RtspClientGUI client(url, windowName);
    
    if (!client.init()) {
        std::cerr << "初始化失败" << std::endl;
        return -1;
    }
    
    client.displayVideo();
    
    return 0;
}
