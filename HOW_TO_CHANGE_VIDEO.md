# 如何更换测试视频

## 快速方法（推荐）

假设你有一个 `myvideo.mp4` 文件：

### 步骤1: 复制视频文件
```bash
cp /path/to/myvideo.mp4 ~/rstp/RtspServer/example/
```

### 步骤2: 修改测试脚本
编辑 `test_gui.sh`，找到这一行：
```bash
ffmpeg -re -stream_loop -1 -i example/test.h264 \
```

改成：
```bash
ffmpeg -re -stream_loop -1 -i example/myvideo.mp4 \
```

### 步骤3: 运行测试
```bash
./test_gui.sh
```

就这么简单！

---

## 方法对比

### 直接用MP4（推荐）
✅ 简单，只需改一行代码
✅ 支持任何格式（MP4/AVI/MOV等）
✅ 不需要转换

### 转换成H.264
✅ 文件更小
✅ 推流效率更高
✅ 可以调整分辨率和码率

---

## 详细说明

### 如果要转换视频

```bash
# 使用转换脚本
./convert_video.sh myvideo.mp4

# 按提示选择质量
# 1 = 低质量 (320x240, 适合低带宽)
# 2 = 中质量 (640x480, 推荐)
# 3 = 高质量 (960x544, 原始质量)

# 转换完成后替换
cp example/test_new.h264 example/test.h264

# 运行测试
./test_gui.sh
```

### 支持的视频格式

可以直接使用：
- MP4
- AVI
- MOV
- MKV
- FLV
- WMV
- 等等...

FFmpeg会自动处理。

---

## 示例

### 示例1: 使用自己的MP4
```bash
# 1. 复制文件
cp ~/Videos/demo.mp4 example/

# 2. 编辑 test_gui.sh
# 把 example/test.h264 改成 example/demo.mp4

# 3. 运行
./test_gui.sh
```

### 示例2: 使用网络视频
```bash
# 下载视频
wget https://example.com/video.mp4 -O example/myvideo.mp4

# 编辑 test_gui.sh
# 把 example/test.h264 改成 example/myvideo.mp4

# 运行
./test_gui.sh
```

### 示例3: 转换并优化
```bash
# 转换成适合电台传输的低码率版本
./convert_video.sh myvideo.mp4
# 选择 1 (低质量)

# 替换
cp example/test_new.h264 example/test.h264

# 测试
./test_gui.sh
```

---

## 一行命令版本

如果你只是想快速测试一个MP4文件：

```bash
# 直接推流（不需要修改脚本）
./mediamtx &
sleep 2
ffmpeg -re -i your_video.mp4 -c copy -f rtsp rtsp://localhost:8554/live &
sleep 2
./client/rtsp_client_gui rtsp://localhost:8554/live
```

---

## 总结

**最简单的方法**：
1. 把MP4文件放到 `example/` 目录
2. 修改 `test_gui.sh` 中的文件名
3. 运行 `./test_gui.sh`

就这三步！
