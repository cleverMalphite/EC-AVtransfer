#!/bin/bash
# 测试不同音频格式传输

echo "=========================================="
echo "音频格式传输测试"
echo "=========================================="
echo ""

mkdir -p log

# 清理
pkill -9 mediamtx 2>/dev/null
pkill -9 ffmpeg 2>/dev/null
sleep 1

# 启动MediaMTX
./mediamtx > log/mediamtx_format_test.log 2>&1 &
MEDIAMTX_PID=$!
echo "✓ MediaMTX已启动"
sleep 2

# 测试函数
test_format() {
    local file=$1
    local format=$2
    local codec=$3
    
    echo ""
    echo "----------------------------------------"
    echo "测试格式: $format"
    echo "文件: $file"
    echo "----------------------------------------"
    
    # 推流
    ffmpeg -re -i "example/$file" -c:a $codec -f rtsp rtsp://localhost:8554/test_audio \
        > "log/ffmpeg_${format}.log" 2>&1 &
    local FFMPEG_PID=$!
    sleep 3
    
    # 检查流
    echo -n "检查流信息: "
    ffprobe -v quiet -print_format json -show_streams rtsp://localhost:8554/test_audio 2>&1 | \
        grep -q "codec_name" && echo "✓ 成功" || echo "✗ 失败"
    
    # 获取详细信息
    ffprobe -v error -select_streams a:0 -show_entries stream=codec_name,sample_rate,channels,bit_rate \
        -of default=noprint_wrappers=1 rtsp://localhost:8554/test_audio 2>&1 | head -5
    
    # 清理
    kill $FFMPEG_PID 2>/dev/null
    sleep 1
}

# 测试AAC
test_format "test.aac" "AAC" "aac"

# 测试MP3
test_format "test.mp3" "MP3" "libmp3lame"

# 测试Opus
test_format "test.opus" "Opus" "libopus"

# 测试WAV (转为AAC传输)
echo ""
echo "----------------------------------------"
echo "测试格式: WAV (转AAC传输)"
echo "文件: example/test.wav"
echo "----------------------------------------"
ffmpeg -re -i example/test.wav -c:a aac -b:a 128k -f rtsp rtsp://localhost:8554/test_audio \
    > log/ffmpeg_wav.log 2>&1 &
WAV_PID=$!
sleep 3
echo -n "检查流信息: "
ffprobe -v quiet -print_format json -show_streams rtsp://localhost:8554/test_audio 2>&1 | \
    grep -q "codec_name" && echo "✓ 成功" || echo "✗ 失败"
ffprobe -v error -select_streams a:0 -show_entries stream=codec_name,sample_rate,channels,bit_rate \
    -of default=noprint_wrappers=1 rtsp://localhost:8554/test_audio 2>&1 | head -5
kill $WAV_PID 2>/dev/null

# 清理
echo ""
echo "清理进程..."
kill $MEDIAMTX_PID 2>/dev/null
pkill -9 ffmpeg 2>/dev/null

echo ""
echo "=========================================="
echo "测试完成"
echo "=========================================="
