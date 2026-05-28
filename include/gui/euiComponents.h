// euiComponents.h — 自定义 EUI-NEO 复合组件
// SliderWithButtons、FrequencyBandBar、自定义 modal 辅助
#pragma once

#include "components/components.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <unordered_map>

namespace gui {

// ── 浮点↔字符串 转换辅助 ──────────────────────────────────────────────

inline std::string formatFloat(float v, const char* fmt) {
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, v);
    return buf;
}

inline float parseFloat(const std::string& s, float fallback) {
    if (s.empty()) return fallback;
    char* end = nullptr;
    float v = strtof(s.c_str(), &end);
    if (end == s.c_str()) return fallback;
    return v;
}

// ── SliderWithButtons ────────────────────────────────────────────────
// 复合组件：标签 + [-]按钮 + 滑块(0-1) + [+]按钮 + 数值显示
// value 范围 [minV, maxV]，内部归一化到 0-1 传给 EUI-NEO slider

inline void sliderWithButtons(
    core::dsl::Ui& ui,
    const std::string& id,
    const std::string& label,
    float& value,
    float minV,
    float maxV,
    float step,
    const char* displayFmt,
    const char* unit,
    float rowWidth,
    float rowHeight,
    float scale,
    std::function<void(float)> onChange = nullptr,
    const char* rightLabel = nullptr,
    bool editable = false)
{
    const float btnSz = 28.f * scale;
    const float gap = 6.f * scale;
    const float valW = editable ? 90.f * scale : 68.f * scale;
    const float labelW = 130.f * scale;
    // 滑块宽度统一使用 68*s 计算，与 Balance（非 editable）对齐；
    // editable 输入框额外宽度溢出到右侧 padding 区（row 默认不 clip）
    const float sliderW = std::max(40.f, rowWidth - labelW - btnSz * 2.f - 68.f * scale - gap * 4.f);
    const float normalized = std::clamp((value - minV) / (maxV - minV), 0.f, 1.f);
    const std::string valText = rightLabel
        ? rightLabel
        : formatFloat(value, displayFmt) + (unit ? unit : "");

    // 静态缓存：可编辑输入框文本跨 compose 帧保留
    static std::unordered_map<std::string, std::string> s_inputCache;

    ui.row(id)
        .size(rowWidth, rowHeight)
        .alignItems(core::Align::CENTER)
        .gap(gap)
        .content([&] {
            ui.text(id + ".label")
                .size(labelW, rowHeight)
                .text(label)
                .fontSize(18.f * scale)
                .color(components::theme::DarkThemeColors().text)
                .verticalAlign(core::VerticalAlign::Center)
                .build();

            components::button(ui, id + ".dec")
                .size(btnSz, btnSz)
                .text("-")
                .fontSize(19.f * scale)
                .radius(8.f * scale)
                .onClick([&value, onChange, minV, step] {
                    value = std::max(minV, value - step);
                    if (onChange) onChange(value);
                })
                .build();

            components::slider(ui, id + ".slider")
                .size(sliderW, rowHeight)
                .value(normalized)
                .onChange([&value, onChange, minV, maxV](float v) {
                    value = minV + v * (maxV - minV);
                    if (onChange) onChange(value);
                })
                .build();

            components::button(ui, id + ".inc")
                .size(btnSz, btnSz)
                .text("+")
                .fontSize(19.f * scale)
                .radius(8.f * scale)
                .onClick([&value, onChange, maxV, step] {
                    value = std::min(maxV, value + step);
                    if (onChange) onChange(value);
                })
                .build();

            if (editable && !rightLabel) {
                const std::string inputId = id + ".input";
                const float unitW = (unit && unit[0]) ? 24.f * scale : 0.f;
                const float inputW = valW - unitW - (unitW > 0.f ? 2.f * scale : 0.f);
                // 聚焦检测：EUI-NEO input 用 .hit 子元素接收焦点
                if (!ui.isFocused(inputId + ".hit")) {
                    s_inputCache[inputId] = formatFloat(value, displayFmt);
                }
                // 自定义样式：聚焦时醒目的蓝色边框
                components::InputStyle inputStyle;
                inputStyle.radius = 6.f * scale;
                inputStyle.focusBorder = {0.35f, 0.65f, 1.0f, 0.95f};
                inputStyle.focused = {0.15f, 0.18f, 0.23f, 1.0f};

                ui.stack(id + ".valWrap")
                    .size(valW, rowHeight)
                    .content([&] {
                        components::input(ui, inputId)
                            .size(inputW, rowHeight - 4.f * scale)
                            .value(s_inputCache[inputId])
                            .fontSize(15.f * scale)
                            .inset(4.f * scale)
                            .style(inputStyle)
                            .onChange([inputId](const std::string& text) {
                                s_inputCache[inputId] = text;
                            })
                            .onEnter([&value, onChange, minV, maxV, inputId] {
                                float parsed = parseFloat(s_inputCache[inputId], value);
                                parsed = std::clamp(parsed, minV, maxV);
                                value = parsed;
                                if (onChange) onChange(value);
                            })
                            .onFocus([&value, onChange, minV, maxV, inputId](bool focused) {
                                if (!focused) {
                                    float parsed = parseFloat(s_inputCache[inputId], value);
                                    parsed = std::clamp(parsed, minV, maxV);
                                    value = parsed;
                                    if (onChange) onChange(value);
                                }
                            })
                            .build();

                        if (unit && unit[0]) {
                            ui.text(id + ".unit")
                                .x(inputW + 2.f * scale)
                                .size(unitW, rowHeight)
                                .text(unit)
                                .fontSize(14.f * scale)
                                .color({0.35f, 0.60f, 1.0f, 1.0f})
                                .verticalAlign(core::VerticalAlign::Center)
                                .build();
                        }
                    })
                    .build();
            } else {
                ui.text(id + ".val")
                    .size(valW, rowHeight)
                    .text(valText)
                    .fontSize(16.f * scale)
                    .color({0.4f, 0.6f, 1.0f, 1.0f})
                    .verticalAlign(core::VerticalAlign::Center)
                    .build();
            }
        })
        .build();
}

// ── FrequencyBandBar ──────────────────────────────────────────────────
// 五个频段色条：Delta(0-4)红、Theta(4-8)橙、Alpha(8-12)黄、Beta(12-30)绿、Gamma(30-40)紫

inline void frequencyBandBar(
    core::dsl::Ui& ui,
    const std::string& id,
    float beatFreq,
    float barWidth,
    float barHeight,
    float scale)
{
    // 频率区间边界 (Hz): 与滑块 0-40Hz 范围精确对齐
    constexpr float BAND_EDGES[] = {0.f, 4.f, 8.f, 13.f, 30.f, 40.f};
    constexpr float FREQ_TOTAL = 40.f;
    const core::Color BAND_COLORS[] = {
        {0.9f, 0.25f, 0.2f, 1.f},   // Delta - red
        {0.95f, 0.5f, 0.2f, 1.f},   // Theta - orange
        {0.95f, 0.85f, 0.2f, 1.f},  // Alpha - yellow
        {0.3f, 0.75f, 0.35f, 1.f},  // Beta - green
        {0.6f, 0.35f, 0.85f, 1.f},  // Gamma - purple
    };

    // 绝对定位 stack，避免 row 布局间隙导致偏移
    ui.stack(id)
        .size(barWidth, barHeight)
        .content([&] {
            for (int i = 0; i < 5; ++i) {
                const float x0 = (BAND_EDGES[i] / FREQ_TOTAL) * barWidth;
                const float x1 = (BAND_EDGES[i + 1] / FREQ_TOTAL) * barWidth;
                const float w = x1 - x0;
                if (w >= 2.f) {
                    ui.rect(id + ".band." + std::to_string(i))
                        .x(x0)
                        .y(0.f)
                        .size(w, barHeight)
                        .color(BAND_COLORS[i])
                        .radius(3.f * scale)
                        .build();
                }
            }
        })
        .build();
}

// ── WaveformDisplay ───────────────────────────────────────────────────
// 轻量波形：ImGui PlotLines 等价实现，避免 LineChart 最小高度限制

inline void drawWaveformChannel(
    core::dsl::Ui& ui,
    const std::string& id,
    const std::vector<float>& samples,
    float x,
    float y,
    float w,
    float h,
    const core::Color& color)
{
    if (samples.size() < 2 || w < 2.f || h < 2.f) return;

    const size_t n = samples.size();
    const size_t maxSegs = static_cast<size_t>(std::max(1.f, w * 0.5f));
    const size_t step = std::max(size_t(1), n / maxSegs);
    const float lineH = 2.f;
    int segIdx = 0;

    for (size_t i = 0; i + step < n; i += step) {
        const float v0 = std::clamp(samples[i], -1.f, 1.f);
        const float v1 = std::clamp(samples[i + step], -1.f, 1.f);
        const float denom = static_cast<float>(std::max(size_t(1), n - 1));
        const float x0 = x + w * static_cast<float>(i) / denom;
        const float x1 = x + w * static_cast<float>(i + step) / denom;
        const float y0 = y + h * 0.5f * (1.f - v0);
        const float y1 = y + h * 0.5f * (1.f - v1);
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.5f) continue;
        const float angle = std::atan2(dy, dx);
        const float midX = (x0 + x1) * 0.5f;
        const float midY = (y0 + y1) * 0.5f;

        ui.rect(id + ".seg." + std::to_string(segIdx++))
            .x(midX - len * 0.5f)
            .y(midY - lineH * 0.5f)
            .size(len, lineH)
            .color(color)
            .radius(lineH * 0.5f)
            .rotate(angle)
            .transformOrigin(0.5f, 0.5f)
            .build();
    }
}

inline void waveformDisplay(
    core::dsl::Ui& ui,
    const std::string& id,
    const std::vector<float>& samplesL,
    const std::vector<float>& samplesR,
    float width,
    float height,
    float scale)
{
    const float halfH = height * 0.5f;
    const float pad = 4.f * scale;

    ui.stack(id)
        .size(width, height)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, height)
                .color({0.11f, 0.12f, 0.15f, 1.f})
                .radius(8.f * scale)
                .border(1.f, {0.28f, 0.30f, 0.35f, 1.f})
                .build();

            if (!samplesL.empty()) {
                drawWaveformChannel(ui, id + ".L", samplesL, pad, pad,
                                    width - pad * 2.f, halfH - pad * 1.5f,
                                    {0.95f, 0.50f, 0.20f, 1.f});
            }
            if (!samplesR.empty()) {
                drawWaveformChannel(ui, id + ".R", samplesR, pad, halfH + pad * 0.5f,
                                    width - pad * 2.f, halfH - pad * 1.5f,
                                    {0.40f, 0.65f, 1.00f, 1.f});
            }

            ui.rect(id + ".divider")
                .x(pad)
                .y(halfH - 0.5f)
                .size(width - pad * 2.f, 1.f)
                .color({0.28f, 0.30f, 0.35f, 0.6f})
                .build();
        })
        .build();
}

// ── 自定义 Modal 辅助 ─────────────────────────────────────────────────
// EUI-NEO DialogBuilder 布局固定（title+message+2 buttons），无法嵌入自定义内容。
// 以下提供 backdrop + 居中 panel 的自定义 modal 模式。

inline void beginCustomModal(
    core::dsl::Ui& ui,
    const std::string& id,
    float screenW,
    float screenH,
    float modalW,
    float modalH)
{
    // 半透明 backdrop
    ui.rect(id + ".backdrop")
        .size(screenW, screenH)
        .color({0.0f, 0.0f, 0.0f, 0.46f})
        .interactive()  // 阻止点击穿透
        .z(100)
        .build();

    // 居中 panel 背景
    const float x = (screenW - modalW) * 0.5f;
    const float y = (screenH - modalH) * 0.5f;
    ui.rect(id + ".panel")
        .x(x)
        .y(y)
        .size(modalW, modalH)
        .color({0.14f, 0.16f, 0.19f, 1.0f})
        .radius(12.f)
        .border(1.f, {0.30f, 0.33f, 0.38f, 0.82f})
        .shadow(24.f, 0.f, 8.f, {0.0f, 0.0f, 0.0f, 0.35f})
        .z(101)
        .build();
}

}  // namespace gui
