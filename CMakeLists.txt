cmake_minimum_required(VERSION 3.14)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── 平台配置 + vcpkg 检测 ────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PlatformConfig)
binaural_detect_vcpkg()

project(BinauralBeats LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# ── 核心音频库（无外部依赖）─────────────────────────────────────────────────

add_library(BinauralSrc
    src/gnauralParser.cpp
    src/pinkNoise.cpp
    src/sinTable.cpp
    src/synthesizer.cpp
    src/stubPredictor.cpp
    src/parameterController.cpp
)
target_include_directories(BinauralSrc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
binaural_platform_defines(BinauralSrc)

add_library(BinauralWaveform src/waveformBuffer.cpp)
target_include_directories(BinauralWaveform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
binaural_platform_defines(BinauralWaveform)

# ── 音频驱动（跨平台 PortAudio 发现）─────────────────────────────────────────

include(FindPortAudio)

if(PORTAUDIO_FOUND)
    add_library(BinauralAudio src/audioDriverPortaudio.cpp src/wavDriver.cpp)
    target_include_directories(BinauralAudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(BinauralAudio PUBLIC BinauralSrc ${PORTAUDIO_LIBRARIES})
    if(DEFINED PORTAUDIO_INCLUDE_DIR)
        target_include_directories(BinauralAudio PRIVATE ${PORTAUDIO_INCLUDE_DIR})
    endif()
    binaural_platform_defines(BinauralAudio)

    add_executable(BinauralBeats src/main.cpp)
    target_link_libraries(BinauralBeats PRIVATE BinauralAudio)
    binaural_platform_defines(BinauralBeats)
else()
    add_library(BinauralAudio src/audioDriverStub.cpp src/wavDriver.cpp)
    target_include_directories(BinauralAudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(BinauralAudio PUBLIC BinauralSrc)
    binaural_platform_defines(BinauralAudio)

    add_executable(BinauralBeats src/main.cpp)
    target_link_libraries(BinauralBeats PRIVATE BinauralAudio)
    binaural_platform_defines(BinauralBeats)
endif()

# ── GUI 可执行文件（集成 EUI-NEO）───────────────────────────────────────────

# 将 EUI-NEO 作为子目录添加，不构建其示例 app
set(EUI_BUILD_APPS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/EUI-NEO EXCLUDE_FROM_ALL)

add_executable(BinauralBeatsGui
    vendor/EUI-NEO/main.cpp
    src/gui/euiApp.cpp
    src/gui/euiMainPage.cpp
    src/gui/euiModals.cpp
    src/gui/guiUtils.cpp
    src/gui/playbackController.cpp
)
target_include_directories(BinauralBeatsGui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gui
)
target_link_libraries(BinauralBeatsGui PRIVATE
    BinauralAudio
    BinauralWaveform
    eui_core
)
binaural_configure_gui(BinauralBeatsGui)
binaural_platform_defines(BinauralBeatsGui)
