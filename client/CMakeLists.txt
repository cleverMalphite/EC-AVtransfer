# 客户端程序

# 查找 Qt5
find_package(Qt5 COMPONENTS Widgets Core Gui REQUIRED)

# 命令行版本客户端
add_executable(rtsp_client
    rtsp_client.cpp
)

target_link_libraries(rtsp_client
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${OPENCV_LIBRARIES}
)

# 旧的 GUI 版本客户端 (OpenCV) - Renamed to legacy
add_executable(rtsp_client_legacy
    rtsp_client_gui.cpp
)

target_link_libraries(rtsp_client_legacy
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${OPENCV_LIBRARIES}
)

# 新的 Qt 客户端 - Renamed to rtsp_client_gui
set(QT_CLIENT_SOURCES
    qt_client/main.cpp
    qt_client/mainwindow.cpp
    qt_client/videothread.cpp
    qt_client/audiovisualizer.cpp
    qt_client/audiovisualizer.h
    qt_client/asrworker.cpp
    qt_client/asrworker.h
)

# 需要打开 AUTOMOC 处理 Q_OBJECT
set(CMAKE_AUTOMOC ON)

add_executable(rtsp_client_gui
    ${QT_CLIENT_SOURCES}
)

target_link_libraries(rtsp_client_gui
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${OPENCV_LIBRARIES}
    whisper
)

# 安装客户端程序
install(TARGETS rtsp_client rtsp_client_legacy rtsp_client_gui
    RUNTIME DESTINATION bin
)

message(STATUS "Client programs configured:")
message(STATUS "  - rtsp_client (CLI)")
message(STATUS "  - rtsp_client_legacy (Old OpenCV GUI)")
message(STATUS "  - rtsp_client_gui (New Qt GUI)")
