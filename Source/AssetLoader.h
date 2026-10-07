#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>

/**
    素材（PNG）加载工具。

    POC 阶段素材从磁盘读取而非嵌入二进制，原因是 JUCE 的 juceaide binarydata
    模式无法处理含非 ASCII 的工程路径（详见 README 常见问题）。

    路径来源按优先级：
      1. 编译期宏 BK_ASSETS_DIR（由 CMake 传入）
      2. 可执行文件同级目录下的 assets\（便于插件与素材一起拷贝分发）
      3. 可执行文件所在目录逐级向上查找 assets\（最多 6 层，方便开发时直接从
         build 目录读取工程里的素材）

    这样即便编译期路径失效，也不会直接变成一块黑屏。
*/
namespace AssetLoader
{
    /** 返回实际使用的素材目录；找不到时返回空的 File。 */
    juce::File findAssetsDirectory();

    /** 按文件名加载一张图；失败返回无效 Image 并输出调试信息。 */
    juce::Image loadImage (const juce::String& fileName);
}
