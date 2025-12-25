#!/bin/bash
# 音频传输测试

echo "=== 音频传输测试 ==="

mkdir -p log

# 清理
pkill -9 mediamtx 2>/dev/null
pkill -9 ffmpeg 2>/dev/null
sleep 1

# 启动MediaMTX
./mediamtx > log/mediamtx_audio.log 2>&1 &
MEDIAMTX_PID=$!
echo "✓ MediaMTX已启动 (PID: $MEDIAMTX_PID)"
sleep 2

# 推送音频
ffmpeg -re -stream_loop -1 -i example/test.aac \
  -c:a aac -b:a 128k \
  -f rtsp rtsp://localhost:8554/audio \
  > log/ffmpeg_audio_push.log 2>&1 &
FFMPEG_PID=$!
echo "✓ 音频推流已启动 (PID: $FFMPEG_PID)"
sleep 3

# 测试接收
echo ""
echo "测试接收音频流..."
timeout 5 ffplay -nodisp -autoexit -t 3 rtsp://localhost:8554/audio 2>&1 | grep -E "Stream|Audio|Input" > log/ffplay_audio.log

echo ""
echo "清理进程..."
kill $FFMPEG_PID 2>/dev/null
kill $MEDIAMTX_PID 2>/dev/null

echo "测试完成"
