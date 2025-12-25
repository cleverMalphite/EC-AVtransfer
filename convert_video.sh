#!/bin/bash
# 视频转换脚本 - 将任意视频转换为H.264格式用于测试

if [ $# -lt 1 ]; then
    echo "用法: $0 <输入视频文件> [输出文件名]"
    echo ""
    echo "示例:"
    echo "  $0 myvideo.mp4"
    echo "  $0 myvideo.avi my_test.h264"
    echo ""
    echo "支持的输入格式: MP4, AVI, MOV, MKV, FLV, WMV 等"
    exit 1
fi

INPUT=$1
OUTPUT=${2:-example/test_new.h264}

if [ ! -f "$INPUT" ]; then
    echo "错误: 文件 '$INPUT' 不存在"
    exit 1
fi

echo "======================================"
echo "视频转换工具"
echo "======================================"
echo ""
echo "输入文件: $INPUT"
echo "输出文件: $OUTPUT"
echo ""

# 获取输入视频信息
echo "输入视频信息:"
ffprobe "$INPUT" 2>&1 | grep -E "(Duration|Stream.*Video)" | head -2
echo ""

echo "选择转换质量:"
echo "1. 低质量  (320x240,  8fps,  64kbps)  - 适合低带宽测试"
echo "2. 中质量  (640x480,  15fps, 200kbps) - 推荐"
echo "3. 高质量  (960x544,  25fps, 400kbps) - 原始质量"
echo "4. 自定义"
echo ""
read -p "请选择 (1-4): " quality

case $quality in
    1)
        RESOLUTION="320x240"
        FPS="8"
        BITRATE="64k"
        ;;
    2)
        RESOLUTION="640x480"
        FPS="15"
        BITRATE="200k"
        ;;
    3)
        RESOLUTION="960x544"
        FPS="25"
        BITRATE="400k"
        ;;
    4)
        read -p "分辨率 (如: 640x480): " RESOLUTION
        read -p "帧率 (如: 15): " FPS
        read -p "码率 (如: 200k): " BITRATE
        ;;
    *)
        echo "无效选择"
        exit 1
        ;;
esac

echo ""
echo "转换参数:"
echo "  分辨率: $RESOLUTION"
echo "  帧率: ${FPS}fps"
echo "  码率: $BITRATE"
echo ""
echo "开始转换..."
echo ""

# 转换视频
ffmpeg -i "$INPUT" \
    -c:v libx264 \
    -preset fast \
    -profile:v baseline \
    -level 3.0 \
    -b:v "$BITRATE" \
    -s "$RESOLUTION" \
    -r "$FPS" \
    -an \
    -f h264 \
    "$OUTPUT"

if [ $? -eq 0 ]; then
    echo ""
    echo "======================================"
    echo "转换成功！"
    echo "======================================"
    echo ""
    echo "输出文件: $OUTPUT"
    ls -lh "$OUTPUT"
    echo ""
    echo "现在可以使用这个文件测试:"
    echo "  ./test_gui.sh  # 需要修改脚本中的文件路径"
    echo ""
    echo "或者替换原文件:"
    echo "  cp $OUTPUT example/test.h264"
    echo "  ./test_gui.sh"
else
    echo ""
    echo "转换失败"
    exit 1
fi
