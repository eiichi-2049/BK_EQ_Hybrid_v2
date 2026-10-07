#include "AssetLoader.h"

namespace AssetLoader
{

juce::File findAssetsDirectory()
{
    // --- 1) 编译期指定的路径 -------------------------------------------------
   #ifdef BK_ASSETS_DIR
    const juce::File fromBuild (juce::String (BK_ASSETS_DIR));
    if (fromBuild.isDirectory())
        return fromBuild;
   #endif

    // --- 2) 可执行文件同级目录 -------------------------------------------------
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                            .getParentDirectory();

    const auto sibling = exeDir.getChildFile ("assets");
    if (sibling.isDirectory())
        return sibling;

    // --- 3) 逐级向上查找 ------------------------------------------------------
    // 开发时插件位于 build/.../VST3/ 下，素材在工程根的 assets/，
    // 向上找几层即可命中。
    auto dir = exeDir;
    for (int i = 0; i < 6; ++i)
    {
        const auto candidate = dir.getChildFile ("assets");
        if (candidate.isDirectory())
            return candidate;

        if (! dir.isDirectory() || dir.getParentDirectory() == dir)
            break;

        dir = dir.getParentDirectory();
    }

    DBG ("AssetLoader：未找到素材目录。编译期路径 = "
       #ifdef BK_ASSETS_DIR
         + juce::String (BK_ASSETS_DIR)
       #else
         + juce::String ("<未定义>")
       #endif
         + "，可执行文件 = " + exeDir.getFullPathName());

    return {};
}

juce::Image loadImage (const juce::String& fileName)
{
    const auto dir = findAssetsDirectory();

    if (dir == juce::File{})
        return {};

    const auto file = dir.getChildFile (fileName);

    if (! file.existsAsFile())
    {
        DBG ("AssetLoader：素材缺失 " + file.getFullPathName());
        return {};
    }

    auto image = juce::ImageFileFormat::loadFrom (file);

    if (! image.isValid())
        DBG ("AssetLoader：解码失败 " + file.getFullPathName());

    return image;
}

} // namespace AssetLoader
