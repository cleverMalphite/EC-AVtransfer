#!/bin/bash
# GUI测试脚本 - 演示视频窗口显示

echo "======================================"
echo "GUI视频显示测试"
echo "======================================"
echo ""

mkdir -p log

# 检查文件
if [ ! -f "example/test.h264" ]; then
    echo "错误: 未找到测试文件"
    exit 1
fi

if [ ! -f "./mediamtx" ]; then
    echo "错误: 未找到mediamtx"
    exit 1
fi

if [ ! -f "./build/client/rtsp_client_gui" ]; then
    echo "错误: 未找到GUI客户端"
    echo "正在使用CMake编译..."
    ./build.sh
    if [ ! -f "./build/client/rtsp_client_gui" ]; then
        echo "编译失败"
        exit 1
    fi
fi

echo "步骤1: 启动MediaMTX服务器"
echo "--------------------------------------"
./mediamtx > log/mediamtx.log 2>&1 &
MEDIAMTX_PID=$!
sleep 2

if ! pgrep -x "mediamtx" > /dev/null; then
    echo "错误: MediaMTX启动失败"
    cat log/mediamtx.log
    exit 1
fi
echo "✓ MediaMTX已启动 (PID: $MEDIAMTX_PID)"
echo ""

echo "步骤2: 启动推流"
echo "--------------------------------------"
# 4K视频需要转码降低分辨率，否则会卡顿
echo "转码为720p以提高流畅度..."
ffmpeg -re -stream_loop -1 -i example/testp4.mp4 \
  -c:v libx264 \
  -preset ultrafast \
  -tune zerolatency \
  -b:v 1000k \
  -maxrate 1000k \
  -bufsize 2000k \
  -s 1280x720 \
  -r 25 \
  -an \
  -f rtsp rtsp://localhost:8554/live \
  > log/ffmpeg.log 2>&1 &
FFMPEG_PID=$!
sleep 3

if ! ps -p $FFMPEG_PID > /dev/null; then
    echo "错误: FFmpeg启动失败"
    cat log/ffmpeg.log
    kill $MEDIAMTX_PID 2>/dev/null
    exit 1
fi
echo "✓ 推流已启动 (PID: $FFMPEG_PID)"
echo ""

echo "步骤3: 启动GUI视频窗口"
echo "--------------------------------------"
echo "URL: rtsp://localhost:8554/live"
echo ""
echo "提示:"
echo "  - 按 'q' 或 ESC 键退出"
echo "  - 按 's' 键截图"
echo "  - 窗口会显示实时帧率和帧数"
echo ""
echo "正在打开视频窗口..."
echo ""

# 启动GUI客户端
./build/client/rtsp_client_gui rtsp://localhost:8554/live "Video Transmission"

# 清理
echo ""
echo "清理进程..."
kill $FFMPEG_PID 2>/dev/null
kill $MEDIAMTX_PID 2>/dev/null
wait $FFMPEG_PID 2>/dev/null
wait $MEDIAMTX_PID 2>/dev/null
echo "测试完成"
