# 双耳节拍 (Binaural Beats) C++ 实现

支持双耳节拍、等时节拍、粉红/白噪声、Gnaural 文件解析，预留 AI 闭环接口。

## 构建

### 依赖（vcpkg + MinGW toolchain）

```powershell
# 安装 PortAudio + ImGui
vcpkg install portaudio:x64-mingw-static imgui[glfw-binding,opengl3-binding]:x64-mingw-static

# 编译（需配置VCPKG_ROOT）
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
make -j4
```

无 PortAudio 时，程序会生成 `output.wav`（10 秒）用于验证。无 imgui 时，GUI 目标不构建。

## 运行

**CLI**：

```powershell
.\build\BinauralBeats.exe
```

**GUI**（需 PortAudio + imgui）：

```powershell
.\build\BinauralBeatsGui.exe
```

### GUI 功能

- 节拍频率 (0.5–40 Hz)、基频、平衡、音量
- 五波段颜色标识（Delta/Theta/Alpha/Beta/Gamma）
- 蓝色数值可点击直接输入
- 等时节拍 (Isochronic) 开关
- 背景噪声：无 / 粉红 / 白噪声，可调音量
- 实时波形显示
- 加载 Gnaural 文件（菜单 ⋮ → Load Gnaural...）：支持 `.txt` 旧格式与 `.gnaural` XML

### 字体

GUI 从 `font/` 目录加载字体（可执行文件同目录或上级）。主字体 Noto Sans，其余 `.ttf`/`.otf` 作为 fallback。若不存在则使用系统默认字体。

## TODO

- [x] 核心合成、PortAudio 播放
- [x] 等时节拍、粉红噪声、Gnaural 解析
- [ ] StubPredictor、参数控制层、无锁队列
- [ ] 集成 ONNX/LibTorch，对接 CNN-LSTM 闭环
