- main.cpp ：程序入口。创建 QApplication 、创建主窗口 MainWindow 并 show() ，最后进入事件循环 a.exec() （ client/qt_client/main.cpp:4-10 ）。没有它程序不会“跑起来”。
- mainwindow.h / mainwindow.cpp ：主界面 + 业务编排层。
  - setupUi() 里用代码创建所有控件、布局、默认值，并把按钮点击事件连接到槽函数（ client/qt_client/mainwindow.cpp:22-98 ）。
  - onStartClicked() 根据用户输入的 URL、文件路径、模式，启动 mediamtx / ffmpeg / 播放器，或者启动视频接收线程（ client/qt_client/mainwindow.cpp:146-184 ）。
  - updateFrame() 收到一帧 QImage 后，把它转成 QPixmap 显示到 videoLabel （ client/qt_client/mainwindow.cpp:331-335 ）。
- videothread.h / videothread.cpp ：视频接收与解码线程（后台干活）。
  - 继承 QThread 并重写 run() （ client/qt_client/videothread.h:16-32 ）。
  - run() 内部用 FFmpeg 打开 RTSP、找视频流、解码、转 RGB，然后把每帧通过信号 frameReady(const QImage&) 发给界面线程（ client/qt_client/videothread.cpp:41-154 ，尤其是 emit frameReady(img.copy()) 在 client/qt_client/videothread.cpp:125-138 ）。
把它们串起来就是： main.cpp 创建并显示 MainWindow → 用户点 Start 触发 MainWindow::onStartClicked() → 视频模式下启动 VideoThread → VideoThread 在后台解码并 emit frameReady() → MainWindow::updateFrame() 在界面上显示。

Qt 的运行逻辑（你需要掌握的核心）

- 事件循环（Event Loop） ： a.exec() （ client/qt_client/main.cpp:9 ）之后，Qt 会一直“等事件”（鼠标点击、键盘输入、定时器、窗口重绘、跨线程信号等），并把事件分发给对应对象处理。没有 exec() ，按钮点了也不会触发槽函数。
- 信号/槽（Signals/Slots） ：Qt 的“消息机制”。
  - 在 VideoThread 里 emit frameReady(...) （后台线程）时，Qt 会把信号投递到 MainWindow 所在的主线程执行 updateFrame(...) （因为 UI 只能在主线程更新）。这就是为什么你可以在子线程解码，但仍安全地更新界面。
  - connect(btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked); 这种连接让点击按钮自动调用槽函数（ client/qt_client/mainwindow.cpp:94-98 ）。
- 对象生命周期 ：你这里很多对象 new ... (this) 把 MainWindow 作为 parent（比如 new QProcess(this) ），Qt 会在父对象析构时自动回收子对象。线程/进程这种还需要你显式 stop/terminate，所以 stopAll() 做了统一清理（ client/qt_client/mainwindow.cpp:269-324 ）。
如果我要修改界面，应该改哪个文件？

- 只改界面布局/控件 ：改 client/qt_client/mainwindow.cpp 的 setupUi() （ client/qt_client/mainwindow.cpp:22-98 ）。
  - 例如：调整按钮位置、增加输入框/下拉框、改默认值、改样式（ setStyleSheet ）、改窗口大小等。
- 增加/修改某个按钮行为 ：改 mainwindow.h 里声明槽函数（ client/qt_client/mainwindow.h:27-35 ），并在 mainwindow.cpp 里实现对应槽（比如 onStartClicked() 在 client/qt_client/mainwindow.cpp:146 ）。
- 修改视频显示逻辑 （缩放策略、叠字、帧率展示方式）：主要改 MainWindow::updateFrame() / updateStats() （ client/qt_client/mainwindow.cpp:331-340 ）。
- 修改“怎么接收/怎么解码 RTSP” ：改 client/qt_client/videothread.cpp 的 run() （ client/qt_client/videothread.cpp:41 开始）。
  - 比如：改 RTSP 连接参数、加断线重连、改像素格式转换、减少拷贝等。