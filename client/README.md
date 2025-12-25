# RTSP客户端接收程序

这是一个简单的C++ RTSP客户端，用于接收和处理RTSP视频流。

## 编译

```bash
cd client
make
```

## 使用方法

### 1. 显示模式（实时统计）

只接收视频流并显示统计信息，不保存文件：

```bash
./rtsp_client rtsp://172.22.248.47:8554/live display
```

或者简写（默认就是display模式）：

```bash
./rtsp_client rtsp://172.22.248.47:8554/live
```

### 2. 保存模式

接收视频流并保存为YUV文件：

```bash
# 保存前100帧到output.yuv
./rtsp_client rtsp://172.22.248.47:8554/live save output.yuv 100

# 保存前300帧
./rtsp_client rtsp://172.22.248.47:8554/live save output.yuv 300
```

保存后可以转换为MP4：

```bash
# 假设视频分辨率是352x288（根据实际输出调整）
ffmpeg -f rawvideo -pixel_format yuv420p -video_size 352x288 -i output.yuv output.mp4
```

## 完整测试流程

### 终端1 - 启动服务器（发送端）

```bash
cd ~/rstp/RtspServer/example
./h264_rtsp_server test.h264
```

### 终端2 - 启动客户端（接收端）

```bash
cd ~/rstp/RtspServer/client
./rtsp_client rtsp://172.22.248.47:8554/live
```

## 功能特性

- ✅ 支持RTSP over TCP传输
- ✅ 自动查找视频流
- ✅ H.264解码
- ✅ 实时帧率统计
- ✅ 保存原始YUV数据
- ✅ 优雅退出（Ctrl+C）

## 集成到EC2项目

这个客户端代码可以直接集成到你的EC2项目中：

1. **核心类**: `RtspClient` 类封装了所有RTSP接收逻辑
2. **关键方法**:
   - `init()`: 连接RTSP流
   - `receiveAndDisplay()`: 接收并处理视频帧
   - `receiveAndSave()`: 接收并保存视频数据

3. **集成示例**:

```cpp
// 在你的EC2项目中
RtspClient client("rtsp://无人机IP:8554/live");
if (client.init()) {
    client.receiveAndDisplay();  // 或者自定义处理逻辑
}
```

## 下一步

- [ ] 添加音频支持
- [ ] 添加Qt界面显示
- [ ] 添加多路视频同时接收
- [ ] 添加网络状态监控
- [ ] 添加断线重连机制
