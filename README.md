# 双耳节拍 (Binaural Beats) C++ 实现

基于 `guideline.md` 与 `algorithm_spec.md` 的 C++ 实现，预留 AI 闭环接口。

## 构建

```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make
```

### 可选：PortAudio + GUI（vcpkg + MinGW 静态）

```powershell
# 安装依赖（x64-mingw-static）
vcpkg install portaudio:x64-mingw-static raylib:x64-mingw-static

# 验证
vcpkg list portaudio raylib

# 配置（VCPKG_ROOT 已设时自动使用 vcpkg toolchain）
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
make -j4
```

`compile_commands.json` 会生成在 build 目录，供 clangd 等使用。

无 PortAudio 时，程序会生成 `output.wav`（10 秒）用于验证。

## 运行

**控制台版**（无 UI）：
```powershell
.\BinauralBeats.exe
```

**GUI 版**（需 raylib）：
```powershell
.\BinauralBeatsGui.exe
```
- 节拍频率滑块 (3–30 Hz)
- 音量滑块
- Play/Stop 按钮
- 实时波形显示

## 目录结构

```
include/binaural/   # 头文件
src/             # 实现
  sinTable.cpp
  synthesizer.cpp
  audioDriverPortaudio.cpp  # PortAudio 驱动
  audioDriverStub.cpp      # WAV 回退
  wavDriver.cpp
  main.cpp
```
