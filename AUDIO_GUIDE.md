# 音频传输指南

## ✅ 测试结果
MediaMTX **完全支持**音频传输！

## 🎵 快速测试

### 方法1: 纯音频传输
```bash
./test_audio.sh
```

### 方法2: 手动测试
```bash
# 1. 启动MediaMTX
./mediamtx &

# 2. 推送音频
ffmpeg -re -i example/test.aac -c:a aac -f rtsp rtsp://localhost:8554/audio &

# 3. 接收音频（播放声音）
ffplay rtsp://localhost:8554/audio
```

## 🎬 音视频同时传输

### 测试脚本
```bash
./test_audio_video.sh
```

### 手动操作
```bash
# 启动MediaMTX
./mediamtx &

# 同时推送音频和视频
ffmpeg -re -stream_loop -1 -i example/test.h264 \
       -re -stream_loop -1 -i example/test.aac \
       -c copy \
       -f rtsp rtsp://localhost:8554/live &

# 接收（有声音+画面）
ffplay rtsp://localhost:8554/live
```

## 📊 支持的音频格式

| 格式 | 编码器 | 推荐码率 | 说明 |
|------|--------|----------|------|
| AAC | aac | 128k | 推荐，兼容性好 |
| MP3 | libmp3lame | 128k | 通用格式 |
| Opus | libopus | 64k | 低码率高质量 |
| PCM | pcm_s16le | 1411k | 无损，文件大 |

## 🔧 实际应用场景

### 场景1: 无人机音频回传
```bash
# 发送端（无人机）
ffmpeg -f alsa -i hw:0 \
  -c:a aac -b:a 64k \
  -f rtsp rtsp://192.168.100.2:8554/drone_audio

# 接收端（车载）
ffplay rtsp://192.168.100.2:8554/drone_audio
```

### 场景2: 对讲功能
```bash
# A端发送
ffmpeg -f alsa -i default -c:a aac -f rtsp rtsp://server:8554/channel_a

# B端发送
ffmpeg -f alsa -i default -c:a aac -f rtsp rtsp://server:8554/channel_b

# A端接收B的声音
ffplay rtsp://server:8554/channel_b

# B端接收A的声音
ffplay rtsp://server:8554/channel_a
```

### 场景3: 音视频一体化
```bash
# 发送端（摄像头+麦克风）
ffmpeg -f v4l2 -i /dev/video0 \
       -f alsa -i hw:0 \
       -c:v libx264 -preset ultrafast -b:v 200k \
       -c:a aac -b:a 64k \
       -f rtsp rtsp://server:8554/live

# 接收端
ffplay rtsp://server:8554/live
```

## 🎯 EC2项目集成建议

### 当前VideoTransfer客户端
- ✅ **视频**: 完美支持
- ❌ **音频**: 不支持（只解码视频）

### 集成方案

#### 方案1: 分离传输（推荐）
```bash
# 视频用GUI客户端
./build/client/rtsp_client_gui rtsp://server:8554/video

# 音频用ffplay（后台播放）
ffplay -nodisp rtsp://server:8554/audio &
```

#### 方案2: Qt集成（需要开发）
在Qt客户端中同时解码音视频：
- 视频 → QLabel显示
- 音频 → QAudioOutput播放

#### 方案3: 使用ffplay（最简单）
```bash
# 直接用ffplay接收音视频
ffplay rtsp://server:8554/live
```

## 💡 性能优化

### 低码率音频（适合弱网）
```bash
ffmpeg -i input.aac -c:a aac -b:a 32k -ar 22050 output.aac
```

### 实时采集优化
```bash
ffmpeg -f alsa -i default \
  -c:a aac -b:a 64k \
  -ar 44100 \
  -ac 1 \
  -f rtsp rtsp://server:8554/audio
```

## 🔍 故障排查

### 问题1: 听不到声音
```bash
# 检查音频流是否存在
ffprobe rtsp://localhost:8554/audio

# 检查系统音量
alsamixer
```

### 问题2: 音频延迟大
```bash
# 降低缓冲
ffplay -fflags nobuffer -flags low_delay rtsp://server:8554/audio
```

### 问题3: 音频卡顿
```bash
# 降低码率
ffmpeg -i input -c:a aac -b:a 32k -f rtsp rtsp://server:8554/audio
```

## 📝 总结

✅ **MediaMTX完全支持音频传输**
- 支持AAC、MP3、Opus等格式
- 可以单独传输音频
- 可以音视频同时传输
- 性能稳定可靠

❌ **当前VideoTransfer GUI客户端不支持音频**
- 只解码视频帧
- 需要额外开发音频播放功能

💡 **推荐方案**
- 视频用GUI客户端显示
- 音频用ffplay后台播放
- 或者集成到Qt项目中统一处理
