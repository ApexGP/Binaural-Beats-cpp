#pragma once

#include "gui/guiPanels.hpp"
#include <string>

namespace gui {

/// 音频播放控制器：封装 driver start/stop、AI 模式切换、program 加载等控制逻辑
/// guiPanels 只负责渲染，所有控制操作委托给此类
class PlaybackController {
public:
    /// 启动播放（已在播放中则无操作）
    static void start(AppContext &ctx);

    /// 停止播放，清除 AI 状态，重置 manualElapsedSec（如果 wasAiDriven）
    static void stop(AppContext &ctx);

    /// 从 AI 驱动或 Gnaural 模式返回手动模式
    static void returnToManual(AppContext &ctx);

    /// 退出定时播放模式
    static void exitTimedPlayback(AppContext &ctx);

    /// 加载 Gnaural 文件（路径来自 ctx.loadPathBuf）
    /// 成功返回 true
    static bool loadGnaural(AppContext &ctx);
};

} // namespace gui
