cmake_minimum_required(VERSION 3.22)

project(BK_EQ_Hybrid_v2 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# JUCE
#   放在 vendor/JUCE（由 tools/fetch-juce.ps1 下载，不入库）。
#   若系统其它位置已安装 JUCE，可用 -DJUCE_PATH=<路径> 指定。
# ------------------------------------------------------------------------------
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/JUCE" CACHE PATH "JUCE 源码目录")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "未找到 JUCE：${JUCE_PATH}\n"
        "请先运行：powershell -ExecutionPolicy Bypass -File tools/fetch-juce.ps1")
endif()

add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

# ------------------------------------------------------------------------------
# 插件目标
# ------------------------------------------------------------------------------
juce_add_plugin(BK_EQ_Hybrid_v2
    PLUGIN_MANUFACTURER_CODE Rvws          # 与 v1 一致：ReiVerb Work Shop
    PLUGIN_CODE            BkHy            # 四字符，须与 v1 的 Abln 区分
    COMPANY_NAME           "ReiVerb Work Shop"
    COMPANY_WEBSITE        ""
    BUNDLE_ID              "com.reiverb.bkeqhybrid.v2"

    FORMATS                VST3            # 当前只做 Windows x64 / VST3
    IS_SYNTH               FALSE
    NEEDS_MIDI_INPUT       FALSE
    NEEDS_MIDI_OUTPUT      FALSE
    IS_MIDI_EFFECT         FALSE

    VST3_CATEGORIES        Fx EQ
    COPY_PLUGIN_AFTER_BUILD FALSE            # 安装由 tools/install-vst3.ps1 负责
)

target_sources(BK_EQ_Hybrid_v2 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BitmapKnob.cpp
)

target_include_directories(BK_EQ_Hybrid_v2 PRIVATE Source)

# 源码含中文注释，存为 UTF-8 with BOM 以便 MSVC 正确识别编码。
# 下面的设置确保编译器以 UTF-8 处理源码（中文 Windows 默认代码页为 936，
# 无 BOM 的 UTF-8 会被误读并产生大量离奇语法错误）。
if(MSVC)
    target_compile_options(BK_EQ_Hybrid_v2 PRIVATE /utf-8)
endif()

# POC 阶段素材从磁盘加载，不做二进制嵌入。
#   原因：JUCE 的 juceaide binarydata 模式在含非 ASCII 字符的路径下会失败
#         （CMake 按系统 ANSI 写路径、juceaide 按 UTF-8 读），详见 README 常见问题。
#         绕开它可以先验证 UI 对齐这一核心目标。
#
#   路径经 ASCII 目录联接 E:\BKv2assets 传入（由 tools/build.ps1 创建），
#   这样编译期定义里不含中文，Defs.txt 便不会出现乱码路径。
#   若本机无法创建联接，可用 -DBK_ASSETS_DIR=<纯ASCII路径> 覆盖。
set(BK_ASSETS_DIR_DEFAULT "E:/BKv2assets/assets")
set(BK_ASSETS_DIR "${BK_ASSETS_DIR_DEFAULT}" CACHE PATH "素材目录（必须是纯 ASCII 路径）")

# 校验路径为纯 ASCII：把除 ASCII 可见字符（0x21-0x7E）之外的一切都去掉，
# 若结果与原串不同，说明含中文/空格等字符，juceaide 会因此失败。
string(REGEX REPLACE "[^!-~]" "" BK_ASSETS_DIR_ASCII "${BK_ASSETS_DIR}")
if(NOT BK_ASSETS_DIR_ASCII STREQUAL BK_ASSETS_DIR)
    message(FATAL_ERROR
        "BK_ASSETS_DIR 含非 ASCII 字符（或空格）：${BK_ASSETS_DIR}\n"
        "JUCE 的 juceaide 在此情况下会失败。请改用纯 ASCII 路径（见 README 常见问题）。")
endif()

target_compile_definitions(BK_EQ_Hybrid_v2 PRIVATE
    BK_ASSETS_DIR="${BK_ASSETS_DIR}")

target_compile_definitions(BK_EQ_Hybrid_v2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

# 生成模块宏（JUCE_MODULE_AVAILABLE_* 等）。
# 注意：JUCE 的 CMake API 不会生成 Projucer 风格的 JuceHeader.h，
# 源码里要按需直接 include 模块头（见 Source/*.h）。
juce_generate_juce_header(BK_EQ_Hybrid_v2)

target_link_libraries(BK_EQ_Hybrid_v2 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ------------------------------------------------------------------------------
# 素材加载方式：当前从磁盘读取（见上方的 BK_ASSETS_DIR 定义）。
#
# 待 juceaide 的 binarydata 模式在本机可用后，可改回二进制嵌入：
#   取消下面这段注释，删掉 BK_ASSETS_DIR 定义，并把源码里的
#   loadImageFromAssets() 换回 BkEqAssets:: 符号即可。
# ------------------------------------------------------------------------------
# juce_add_binary_data(BK_EQ_Hybrid_v2_Assets
#     HEADER_NAME "Assets.h"
#     NAMESPACE   BkEqAssets
#     SOURCES
#         assets/bg.png
#         assets/fs_pultec.png
#         assets/fs_red.png
#         assets/fs_green.png
#         assets/fs_blue.png
#         assets/fs_brown.png
# )
#
# target_link_libraries(BK_EQ_Hybrid_v2 PRIVATE BK_EQ_Hybrid_v2_Assets)
# target_include_directories(BK_EQ_Hybrid_v2 PRIVATE
#     "${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_BK_EQ_Hybrid_v2_Assets/JuceLibraryCode"
# )
