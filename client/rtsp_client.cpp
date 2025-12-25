#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <signal.h>
#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctime>
#include <iomanip>
#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

static bool g_running = true;

void signalHandler(int signum) {
    std::cout << "\n接收到停止信号，正在退出..." << std::endl;
    g_running = false;
}

class RtspClient {
public:
    RtspClient(const std::string& url) : url_(url), 
        formatCtx_(nullptr), codecCtx_(nullptr), 
        codec_(nullptr), swsCtx_(nullptr),
        videoStreamIndex_(-1), frameCount_(0) {}
    
    ~RtspClient() {
        cleanup();
    }
    
    bool init() {
        // 打开RTSP流或SDP文件
        formatCtx_ = avformat_alloc_context();
        
        AVDictionary* options = nullptr;
        
        // 检查是否是SDP文件
        bool isSdpFile = (url_.find(".sdp") != std::string::npos);
        
        if (isSdpFile) {
            av_dict_set(&options, "protocol_whitelist", "file,rtp,udp", 0);
            std::cout << "正在打开SDP文件: " << url_ << std::endl;
        } else {
            av_dict_set(&options, "rtsp_transport", "tcp", 0);  // 使用TCP传输，避免UDP抖动
            av_dict_set(&options, "max_delay", "5000000", 0);   // 最大延迟5s
            std::cout << "正在连接RTSP流: " << url_ << std::endl;
        }
        
        int ret = avformat_open_input(&formatCtx_, url_.c_str(), nullptr, &options);
        av_dict_free(&options);
        
        if (ret != 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "无法打开流: " << errbuf << " (错误码: " << ret << ")" << std::endl;
            return false;
        }
        
        std::cout << "流已打开，正在获取流信息..." << std::endl;
        
        // 设置超时和探测参数
        formatCtx_->max_analyze_duration = 5 * AV_TIME_BASE; // 5秒超时
        formatCtx_->probesize = 10000000; // 10MB探测大小
        
        // 获取流信息
        ret = avformat_find_stream_info(formatCtx_, nullptr);
        if (ret < 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "无法获取流信息: " << errbuf << std::endl;
            return false;
        }
        
        std::cout << "流信息获取成功" << std::endl;
        
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
        
        // 设置解码器选项，允许缺少参数集时继续解码
        codecCtx_->flags2 |= AV_CODEC_FLAG2_CHUNKS;
        codecCtx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        
        AVDictionary* codecOpts = nullptr;
        av_dict_set(&codecOpts, "flags", "low_delay", 0);
        
        if (avcodec_open2(codecCtx_, codec_, &codecOpts) < 0) {
            std::cerr << "无法打开解码器" << std::endl;
            av_dict_free(&codecOpts);
            return false;
        }
        av_dict_free(&codecOpts);
        
        std::cout << "连接成功！" << std::endl;
        std::cout << "视频信息: " << codecCtx_->width << "x" << codecCtx_->height 
                  << " " << av_get_pix_fmt_name(codecCtx_->pix_fmt) << std::endl;
        
        return true;
    }
    
    void receiveAndSaveMP4(const std::string& outputFile, int durationSeconds = 0) {
        // 创建输出格式上下文
        AVFormatContext* outFormatCtx = nullptr;
        avformat_alloc_output_context2(&outFormatCtx, nullptr, nullptr, outputFile.c_str());
        if (!outFormatCtx) {
            std::cerr << "无法创建输出格式上下文" << std::endl;
            return;
        }
        
        // 创建视频流
        AVStream* outStream = avformat_new_stream(outFormatCtx, nullptr);
        if (!outStream) {
            std::cerr << "无法创建输出视频流" << std::endl;
            avformat_free_context(outFormatCtx);
            return;
        }
        
        // 复制编解码器参数
        AVStream* inStream = formatCtx_->streams[videoStreamIndex_];
        avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
        outStream->codecpar->codec_tag = 0;
        outStream->time_base = inStream->time_base;
        
        // 打开输出文件
        if (!(outFormatCtx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&outFormatCtx->pb, outputFile.c_str(), AVIO_FLAG_WRITE) < 0) {
                std::cerr << "无法打开输出文件: " << outputFile << std::endl;
                avformat_free_context(outFormatCtx);
                return;
            }
        }
        
        // 写入文件头
        if (avformat_write_header(outFormatCtx, nullptr) < 0) {
            std::cerr << "无法写入文件头" << std::endl;
            if (!(outFormatCtx->oformat->flags & AVFMT_NOFILE))
                avio_closep(&outFormatCtx->pb);
            avformat_free_context(outFormatCtx);
            return;
        }
        
        AVPacket* packet = av_packet_alloc();

        std::cout << "开始录制视频到: " << outputFile << std::endl;
        if (durationSeconds > 0) {
            std::cout << "录制时长: " << durationSeconds << " 秒" << std::endl;
        } else {
            std::cout << "持续录制，按 Ctrl+C 停止" << std::endl;
        }

        auto startTime = std::chrono::steady_clock::now();
        int readErrorCount = 0;

        // 根据输入流估算帧率，生成恒定间隔时间戳
        AVRational fps = inStream->avg_frame_rate.num > 0 && inStream->avg_frame_rate.den > 0
                           ? inStream->avg_frame_rate
                           : inStream->r_frame_rate;
        if (fps.num <= 0 || fps.den <= 0) {
            fps.num = 25;
            fps.den = 1;
        }

        int64_t ticksPerFrame = av_rescale_q(1, av_inv_q(fps), outStream->time_base);
        if (ticksPerFrame <= 0) {
            // 兜底：如果计算失败，默认按 25fps 计算
            AVRational defaultFps = {25, 1};
            ticksPerFrame = av_rescale_q(1, av_inv_q(defaultFps), outStream->time_base);
        }
        int64_t frameIndex = 0;

        int readResult = 0;
        while (g_running) {
            readResult = av_read_frame(formatCtx_, packet);

            if (readResult < 0) {
                readErrorCount++;
                if (readErrorCount > 100) {
                    std::cerr << "\n读取数据包失败次数过多，停止录制" << std::endl;
                    char errBuf[128];
                    av_strerror(readResult, errBuf, sizeof(errBuf));
                    std::cerr << "错误: " << errBuf << std::endl;
                    break;
                }
                av_usleep(10000); // 等待 10ms
                continue;
            }

            readErrorCount = 0; // 重置错误计数

            if (packet->stream_index == videoStreamIndex_) {
                frameCount_++;

                // 使用恒定帧率生成时间戳，完全忽略网络抖动带来的原始时间戳
                packet->stream_index = 0;
                packet->dts = frameIndex * ticksPerFrame;
                packet->pts = packet->dts;
                packet->duration = ticksPerFrame;
                frameIndex++;

                // 写入数据包
                int ret = av_interleaved_write_frame(outFormatCtx, packet);
                if (ret < 0) {
                    char errBuf[128];
                    av_strerror(ret, errBuf, sizeof(errBuf));
                    std::cerr << "写入失败: " << errBuf << std::endl;
                }

                if (frameCount_ % 30 == 0) {
                    auto currentTime = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration<double>(currentTime - startTime).count();
                    std::cout << "已录制 " << frameCount_ << " 帧 (" 
                              << std::fixed << std::setprecision(1) << elapsed << " 秒)" << std::endl;
                }

                // 检查是否达到指定录制时长（按真实时间判断）
                if (durationSeconds > 0) {
                    auto currentTime = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration<double>(currentTime - startTime).count();
                    if (elapsed >= durationSeconds) {
                        std::cout << "已达到指定录制时长，停止录制" << std::endl;
                        g_running = false;
                        break;
                    }
                }
            }
            av_packet_unref(packet);
        }
        
        // 写入文件尾
        av_write_trailer(outFormatCtx);
        
        // 清理资源
        av_packet_free(&packet);
        if (!(outFormatCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&outFormatCtx->pb);
        avformat_free_context(outFormatCtx);
        
        auto endTime = std::chrono::steady_clock::now();
        double totalTime = std::chrono::duration<double>(endTime - startTime).count();
        
        std::cout << "\n录制完成！" << std::endl;
        std::cout << "总帧数: " << frameCount_ << " 帧" << std::endl;
        std::cout << "总时长: " << std::fixed << std::setprecision(2) << totalTime << " 秒" << std::endl;
        std::cout << "平均帧率: " << std::fixed << std::setprecision(2) << (frameCount_ / totalTime) << " fps" << std::endl;
        std::cout << "文件已保存到: " << outputFile << std::endl;
    }
    
    void receiveAndDisplay() {
        std::cout << "尝试使用 OpenCV 打开流: " << url_ << std::endl;
        // 使用 FFmpeg 后端
        cv::VideoCapture cap(url_, cv::CAP_FFMPEG);
        
        if (!cap.isOpened()) {
            std::cerr << "OpenCV 无法打开流" << std::endl;
            return;
        }

        std::cout << "开始接收视频流（实时显示）" << std::endl;
        std::cout << "按 'q' 或 Ctrl+C 停止接收" << std::endl;
        
        int width = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
        int height = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        std::cout << "分辨率: " << width << "x" << height << std::endl;

        cv::Mat frame;
        auto startTime = std::chrono::steady_clock::now();
        frameCount_ = 0;

        while (g_running) {
            if (!cap.read(frame)) {
                 // 如果读取失败，可能是流断了或者结束了
                 // 尝试简单的重连或者退出
                 // 这里选择退出，让外层决定
                 std::cout << "读取帧失败或流结束" << std::endl;
                 break;
            }
            
            if (frame.empty()) continue;
            
            frameCount_++;
            cv::imshow("RTSP Player", frame);
            
            // 必须有 waitKey 才能刷新窗口
            char c = (char)cv::waitKey(1);
            if (c == 'q' || c == 27) { // q 或 ESC
                g_running = false;
            }
            
            if (frameCount_ % 100 == 0) {
                auto currentTime = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(currentTime - startTime).count();
                double fps = frameCount_ / elapsed;
                std::cout << "已播放 " << frameCount_ << " 帧 | FPS: " << std::fixed << std::setprecision(1) << fps << "\r" << std::flush;
            }
        }
        cv::destroyAllWindows();
        cap.release();
        std::cout << "\n播放结束" << std::endl;
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
    AVFormatContext* formatCtx_;
    AVCodecContext* codecCtx_;
    const AVCodec* codec_;
    SwsContext* swsCtx_;
    int videoStreamIndex_;
    int frameCount_;
};

std::string generateTimestampFilename(const std::string& prefix = "video") {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_r(&time_t_now, &tm_now);
    
    std::ostringstream oss;
    oss << prefix << "_" 
        << std::setfill('0') 
        << std::setw(4) << (tm_now.tm_year + 1900)
        << std::setw(2) << (tm_now.tm_mon + 1)
        << std::setw(2) << tm_now.tm_mday << "_"
        << std::setw(2) << tm_now.tm_hour
        << std::setw(2) << tm_now.tm_min
        << std::setw(2) << tm_now.tm_sec
        << ".mp4";
    return oss.str();
}

bool createDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true; // 目录已存在
        } else {
            std::cerr << "路径已存在但不是目录: " << path << std::endl;
            return false;
        }
    }
    
    if (mkdir(path.c_str(), 0755) == 0) {
        std::cout << "创建输出目录: " << path << std::endl;
        return true;
    } else {
        std::cerr << "无法创建目录: " << path << std::endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " <rtsp_url|sdp_file> [record|display] [duration_seconds]" << std::endl;
        std::cout << "\n示例:" << std::endl;
        std::cout << "  " << argv[0] << " rtsp://172.22.248.47:8554/live display" << std::endl;
        std::cout << "    - 仅显示统计信息，不保存文件" << std::endl;
        std::cout << "\n  " << argv[0] << " rtsp://172.22.248.47:8554/live record" << std::endl;
        std::cout << "    - 持续录制到 output/ 目录，按 Ctrl+C 停止" << std::endl;
        std::cout << "\n  " << argv[0] << " rtsp://172.22.248.47:8554/live record 30" << std::endl;
        std::cout << "    - 录制30秒后自动停止" << std::endl;
        std::cout << "\n  " << argv[0] << " stream.sdp record 60" << std::endl;
        std::cout << "    - 从SDP文件录制60秒" << std::endl;
        return -1;
    }
    
    signal(SIGINT, signalHandler);
    
    std::string url = argv[1];
    std::string mode = argc > 2 ? argv[2] : "display";
    
    RtspClient client(url);
    
    if (!client.init()) {
        std::cerr << "初始化失败" << std::endl;
        return -1;
    }
    
    if (mode == "record") {
        // 创建 output 目录
        if (!createDirectory("output")) {
            std::cerr << "无法创建输出目录" << std::endl;
            return -1;
        }
        
        // 生成带时间戳的文件名
        std::string filename = generateTimestampFilename("video");
        std::string outputPath = "output/" + filename;
        
        int duration = argc > 3 ? std::stoi(argv[3]) : 0;
        client.receiveAndSaveMP4(outputPath, duration);
    } else {
        client.receiveAndDisplay();
    }
    
    return 0;
}
