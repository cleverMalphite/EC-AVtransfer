#!/bin/bash
# CMake构建脚本

set -e  # 遇到错误立即退出

echo "======================================"
echo "RtspVideoTransfer - CMake构建"
echo "======================================"
echo ""

# 解析参数
BUILD_TYPE="Release"
BUILD_EXAMPLES="OFF"
CLEAN_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --examples)
            BUILD_EXAMPLES="ON"
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --help)
            echo "用法: $0 [选项]"
            echo ""
            echo "选项:"
            echo "  --debug      Debug模式编译"
            echo "  --examples   编译示例程序"
            echo "  --clean      清理后重新编译"
            echo "  --help       显示帮助"
            echo ""
            echo "示例:"
            echo "  $0                    # Release模式，仅编译客户端"
            echo "  $0 --debug            # Debug模式"
            echo "  $0 --examples         # 编译示例程序"
            echo "  $0 --clean --debug    # 清理后Debug编译"
            exit 0
            ;;
        *)
            echo "未知选项: $1"
            echo "使用 --help 查看帮助"
            exit 1
            ;;
    esac
done

# 创建构建目录
BUILD_DIR="build"

if [ "$CLEAN_BUILD" = true ]; then
    echo "清理构建目录..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "配置参数:"
echo "  构建类型: $BUILD_TYPE"
echo "  编译示例: $BUILD_EXAMPLES"
echo ""

# 运行CMake配置
echo "运行CMake配置..."
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_EXAMPLES="$BUILD_EXAMPLES"

echo ""
echo "开始编译..."
echo ""

# 编译
make -j$(nproc)

echo ""
echo "======================================"
echo "编译完成！"
echo "======================================"
echo ""
echo "可执行文件位置:"
echo "  客户端(CLI):  $BUILD_DIR/client/rtsp_client"
echo "  客户端(GUI):  $BUILD_DIR/client/rtsp_client_gui"

if [ "$BUILD_EXAMPLES" = "ON" ]; then
    echo "  示例程序:     $BUILD_DIR/example/"
fi

echo ""
echo "运行测试:"
echo "  cd $BUILD_DIR/client"
echo "  ./rtsp_client_gui rtsp://localhost:8554/live"
echo ""
