# VideoTransfer - 实时音视频传输系统

基于MediaMTX的实时音视频传输解决方案，适用于应急通信、无人机回传等场景。

## 📋 项目简介

### 核心功能
- ✅ **视频传输** - H.264实时视频流传输
- ✅ **音频传输** - AAC/MP3/Opus等多格式音频传输
- ✅ **GUI显示** - 实时视频窗口显示和截图
- ✅ **网络适应** - 支持WiFi/4G/专用电台等多种网络

### 技术架构
```
发送端 (无人机/手持)          中转服务器          接收端 (车载)
┌─────────────────┐          ┌─────────────┐    ┌─────────────────┐
│  摄像头/视频文件  │          │  MediaMTX   │    │  GUI客户端       │
│       ↓         │          │ (Go程序)    │    │ (C++程序)       │
│  FFmpeg编码     │  ─RTSP─→ │ RTSP服务器  │ ←─ │ - 解码显示      │
│  推流工具       │   网络    │ 8554端口    │    │ - OpenCV窗口    │
└─────────────────┘          └─────────────┘    │ - 截图功能      │
                                                └─────────────────┘
```

### 为什么选用MediaMTX

**1. 原生支持SRT协议**
- 专为不稳定网络设计（无人机、电台场景）
- 基于UDP，有前向纠错（FEC）和抗丢包机制
- 比传统RTSP (TCP)在弱信号下更稳定

**2. 零依赖部署**
- 单个二进制文件，无需安装依赖
- 适合应急场景快速部署
- 支持Linux/Windows/国产化系统

**3. 多协议支持**
- RTSP、RTMP、HLS、WebRTC、SRT
- 可根据网络环境灵活切换

### 应用场景
```
场景: 两台机器通过电台
┌──────────────┐      电台链路      ┌──────────────┐
│  无人机       │   (192.168.100.x)  │   车载        │
│  FFmpeg推流  │  ←─────────────→  │  MediaMTX    │
└──────────────┘    局域网/专网      └──────────────┘

架构
无人机 (192.168.100.1)
    ↓
专用电台 (数据链路)
    ↓
车载电台 (192.168.100.2)
    ↓
车载计算机内部:
  - MediaMTX监听 localhost:8554
  - C++客户端连接 localhost:8554
```

---

## 🚀 快速开始

### 1. 编译项目
```bash
./build.sh
```

### 2. 本地测试
```bash
# 视频传输测试
./test_gui.sh

# 音频传输测试
./test_audio.sh

# 音视频同时传输
./test_audio_video.sh
```

---

## 📺 视频传输

### 方法1: 使用脚本（推荐）

#### 本地测试
```bash
./test_gui.sh
```

#### 实际部署
```bash
# 接收端（车载）
./receiver.sh
# 选择 3 (GUI模式)

# 发送端（无人机）
./sender.sh 192.168.100.2 /dev/video0 medium
```

### 方法2: 手动命令

#### 步骤1: 启动MediaMTX服务器
```bash
./mediamtx &
```

#### 步骤2: 推送视频流

**从视频文件推流**
```bash
ffmpeg -re -stream_loop -1 -i example/test.h264 \
  -c copy \
  -f rtsp rtsp://localhost:8554/live &
```

**从摄像头推流**
```bash
ffmpeg -f v4l2 -i /dev/video0 \
  -c:v libx264 -preset ultrafast -tune zerolatency \
  -b:v 200k -s 640x480 -r 15 \
  -f rtsp rtsp://localhost:8554/live &
```

**远程推流（无人机→车载）**
```bash
# 无人机端
ffmpeg -f v4l2 -i /dev/video0 \
  -c:v libx264 -preset ultrafast -b:v 200k \
  -f rtsp rtsp://192.168.100.2:8554/drone &
```

#### 步骤3: 接收视频

**GUI客户端（推荐）**
```bash
./build/client/rtsp_client_gui rtsp://localhost:8554/live
```

**命令行客户端**
```bash
./build/client/rtsp_client rtsp://localhost:8554/live
```

**使用ffplay**
```bash
ffplay rtsp://localhost:8554/live
```

### 视频质量参数

| 场景 | 分辨率 | 帧率 | 码率 | 命令参数 |
|------|--------|------|------|----------|
| 高清 | 1280x720 | 25fps | 400k | `-s 1280x720 -r 25 -b:v 400k` |
| 标清 | 640x480 | 15fps | 200k | `-s 640x480 -r 15 -b:v 200k` |
| 低码率 | 320x240 | 8fps | 64k | `-s 320x240 -r 8 -b:v 64k` |

---

## 🎵 音频传输

### 支持的音频格式

| 格式 | 编码器 | 码率 | 文件大小 | 说明 |
|------|--------|------|----------|------|
| **AAC** | aac | 72k | 264KB | ✅ 推荐，兼容性好 |
| **MP3** | libmp3lame | 128k | 471KB | ✅ 通用格式 |
| **Opus** | libopus | 64k | 236KB | ✅ 低码率高质量 |
| **WAV** | pcm_s16le | 1411k | 2.6MB | 无损，文件大 |

### 方法1: 使用脚本

```bash
# 纯音频传输
./test_audio.sh

# 音视频同时传输
./test_audio_video.sh

# 测试所有音频格式
./test_audio_formats.sh
```

### 方法2: 手动命令

#### AAC音频传输
```bash
# 启动MediaMTX
./mediamtx &

# 推送AAC音频
ffmpeg -re -stream_loop -1 -i example/test.aac \
  -c copy \
  -f rtsp rtsp://localhost:8554/audio &

# 接收音频（播放声音）
ffplay rtsp://localhost:8554/audio
```

#### MP3音频传输
```bash
ffmpeg -re -i example/test.mp3 \
  -c:a libmp3lame -b:a 128k \
  -f rtsp rtsp://localhost:8554/audio &
```

#### Opus音频传输
```bash
ffmpeg -re -i example/test.opus \
  -c:a libopus -b:a 64k \
  -f rtsp rtsp://localhost:8554/audio &
```

#### WAV音频传输（转AAC）
```bash
ffmpeg -re -i example/test.wav \
  -c:a aac -b:a 128k \
  -f rtsp rtsp://localhost:8554/audio &
```

#### 实时麦克风采集
```bash
# 从麦克风采集并推流
ffmpeg -f alsa -i default \
  -c:a aac -b:a 64k \
  -f rtsp rtsp://localhost:8554/audio &
```

---

## 🎬 音视频同时传输

### 方法1: 使用脚本
```bash
./test_audio_video.sh
```

### 方法2: 手动命令

#### 从文件推流
```bash
# 启动MediaMTX
./mediamtx &

# 同时推送视频和音频
ffmpeg -re -stream_loop -1 -i example/test.h264 \
       -re -stream_loop -1 -i example/test.aac \
       -c copy \
       -f rtsp rtsp://localhost:8554/live &

# 接收（有声音+画面）
ffplay rtsp://localhost:8554/live
```

#### 从摄像头+麦克风推流
```bash
ffmpeg -f v4l2 -i /dev/video0 \
       -f alsa -i hw:0 \
       -c:v libx264 -preset ultrafast -b:v 200k \
       -c:a aac -b:a 64k \
       -f rtsp rtsp://localhost:8554/live &
```

#### 远程推流（无人机→车载）
```bash
# 无人机端
ffmpeg -f v4l2 -i /dev/video0 \
       -f alsa -i default \
       -c:v libx264 -preset ultrafast -b:v 200k \
       -c:a aac -b:a 64k \
       -f rtsp rtsp://192.168.100.2:8554/drone &

# 车载端接收
ffplay rtsp://localhost:8554/drone
```

### 注意事项
⚠️ **当前GUI客户端只支持视频显示，不支持音频播放**

如需音视频同时接收，请使用：
```bash
# 方案1: 使用ffplay（推荐）
ffplay rtsp://localhost:8554/live

# 方案2: 分离接收
./build/client/rtsp_client_gui rtsp://localhost:8554/video  # 视频
ffplay -nodisp rtsp://localhost:8554/audio &                # 音频后台播放
```

---

## 🔧 实际应用场景

### 场景1: 无人机视频回传
```bash
# 无人机端（192.168.100.1）
ffmpeg -f v4l2 -i /dev/video0 \
  -c:v libx264 -preset ultrafast -b:v 200k -s 640x480 \
  -f rtsp rtsp://192.168.100.2:8554/drone &

# 车载端（192.168.100.2）
./mediamtx &
./build/client/rtsp_client_gui rtsp://localhost:8554/drone
```

### 场景2: 手持终端现场回传
```bash
# 手持端
ffmpeg -f v4l2 -i /dev/video0 \
       -f alsa -i default \
       -c:v libx264 -b:v 150k \
       -c:a aac -b:a 32k \
       -f rtsp rtsp://192.168.100.2:8554/handheld &

# 车载端
ffplay rtsp://localhost:8554/handheld
```

### 场景3: 双向对讲
```bash
# A端发送
ffmpeg -f alsa -i default -c:a aac -f rtsp rtsp://server:8554/channel_a &

# B端发送
ffmpeg -f alsa -i default -c:a aac -f rtsp rtsp://server:8554/channel_b &

# A端接收B的声音
ffplay -nodisp rtsp://server:8554/channel_b &

# B端接收A的声音
ffplay -nodisp rtsp://server:8554/channel_a &
```

### 场景4: 多路视频监控
```bash
# 车载端启动MediaMTX
./mediamtx &

# 无人机1
ffmpeg -i /dev/video0 -c:v libx264 -b:v 200k \
  -f rtsp rtsp://192.168.100.2:8554/drone1 &

# 无人机2
ffmpeg -i /dev/video0 -c:v libx264 -b:v 200k \
  -f rtsp rtsp://192.168.100.2:8554/drone2 &

# 车载端同时显示
./build/client/rtsp_client_gui rtsp://localhost:8554/drone1 &
./build/client/rtsp_client_gui rtsp://localhost:8554/drone2 &
```

---

## 📁 项目结构

```
VideoTransfer/
├── mediamtx                 # RTSP服务器(二进制)
├── mediamtx.yml            # MediaMTX配置文件
├── build.sh                # 一键编译脚本
├── CMakeLists.txt          # 主构建配置
│
├── client/                 # 客户端程序
│   ├── rtsp_client.cpp    # CLI版本
│   └── rtsp_client_gui.cpp # GUI版本 ⭐
│
├── example/                # 测试文件
│   ├── test.h264          # 测试视频
│   ├── test.aac           # 测试音频(AAC)
│   ├── test.mp3           # 测试音频(MP3)
│   ├── test.opus          # 测试音频(Opus)
│   └── test.wav           # 测试音频(WAV)
│
├── 测试脚本/
│   ├── test_gui.sh        # GUI视频测试
│   ├── test_audio.sh      # 音频测试
│   ├── test_audio_video.sh # 音视频测试
│   ├── test_audio_formats.sh # 音频格式测试
│   ├── sender.sh          # 发送端脚本
│   └── receiver.sh        # 接收端脚本
│
└── qt_integration/         # Qt集成代码（开发中）
    ├── include/
    └── src/
```

---

## 🛠️ 性能优化

### 低延迟优化
```bash
ffmpeg -f v4l2 -i /dev/video0 \
  -c:v libx264 -preset ultrafast -tune zerolatency \
  -b:v 200k -g 15 \
  -f rtsp rtsp://server:8554/live
```

### 弱网环境优化
```bash
# 降低分辨率和帧率
ffmpeg -f v4l2 -i /dev/video0 \
  -c:v libx264 -preset ultrafast \
  -s 320x240 -r 8 -b:v 64k \
  -f rtsp rtsp://server:8554/live
```

### 音频低码率优化
```bash
# 使用Opus编码，32k码率
ffmpeg -f alsa -i default \
  -c:a libopus -b:a 32k -ar 16000 -ac 1 \
  -f rtsp rtsp://server:8554/audio
```

---

## 🔍 故障排查

### 问题1: 连接失败
```bash
# 检查MediaMTX是否运行
ps aux | grep mediamtx

# 检查端口是否监听
netstat -tuln | grep 8554

# 查看MediaMTX日志
tail -f log/mediamtx.log
```

### 问题2: 视频卡顿
```bash
# 降低码率和分辨率
-s 320x240 -r 8 -b:v 64k

# 使用UDP传输
-rtsp_transport udp
```

### 问题3: 听不到声音
```bash
# 检查音频流是否存在
ffprobe rtsp://localhost:8554/audio

# 检查系统音量
alsamixer

# 使用ffplay测试
ffplay rtsp://localhost:8554/audio
```

### 问题4: 推流失败
```bash
# 查看ffmpeg日志
cat log/ffmpeg_push.log

# 测试网络连接
ping 192.168.100.2
telnet 192.168.100.2 8554
```

---

## 💡 技术指标

| 指标 | 数值 |
|------|------|
| 视频分辨率 | 320x240 ~ 1280x720 |
| 视频帧率 | 8-25 fps |
| 视频码率 | 64k-400k bps |
| 音频码率 | 32k-128k bps |
| 传输延迟 | 200-500ms |
| 传输距离 | 10-50km (电台) |
| 支持协议 | RTSP, RTMP, HLS, WebRTC, SRT |

---

## 📝 依赖项

- **FFmpeg** - 音视频编解码
- **OpenCV** - 视频显示
- **MediaMTX** - RTSP服务器
- **CMake** - 构建系统

---

## 🎯 EC2项目集成

本项目可直接集成到EC2应急通信系统中：

1. **作为独立模块** - 通过进程调用
2. **Qt原生集成** - 参考 `qt_integration/` 目录
3. **库形式集成** - 编译为动态库供EC2调用

详见 `qt_integration/` 目录中的集成示例代码。

---

## 📄 许可证

本项目基于开源协议发布。MediaMTX使用MIT许可证。
