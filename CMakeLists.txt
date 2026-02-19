cmake_minimum_required(VERSION 3.14)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-mingw-static" CACHE STRING "vcpkg triplet")
endif()

project(BinauralBeats LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Static-link MinGW runtime (libgcc, libstdc++, winpthread) to avoid DLL dependency
if(MINGW OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32))
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")
endif()

add_library(BinauralSrc
    src/gnauralParser.cpp
    src/pinkNoise.cpp
    src/synthesizer.cpp
    src/stubPredictor.cpp
    src/parameterController.cpp
)
target_include_directories(BinauralSrc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(BinauralWaveform src/waveformBuffer.cpp)
target_include_directories(BinauralWaveform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(portaudio CONFIG QUIET)
find_package(imgui CONFIG QUIET)
if(portaudio_FOUND)
    add_library(BinauralAudio src/audioDriverPortaudio.cpp src/wavDriver.cpp)
    target_include_directories(BinauralAudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(BinauralAudio PUBLIC BinauralSrc portaudio_static)

    add_executable(BinauralBeats src/main.cpp)
    target_link_libraries(BinauralBeats PRIVATE BinauralAudio)

    if(imgui_FOUND)
        add_executable(BinauralBeatsGui src/mainGui.cpp)
        target_link_libraries(BinauralBeatsGui PRIVATE BinauralAudio BinauralWaveform imgui::imgui opengl32)
    else()
        message(STATUS "imgui not found, GUI disabled. Install: vcpkg install imgui[glfw-binding,opengl3-binding]:${VCPKG_TARGET_TRIPLET}")
    endif()
else()
    message(WARNING "PortAudio not found. Install via: vcpkg install portaudio:${VCPKG_TARGET_TRIPLET}")
    add_library(BinauralAudio src/audioDriverStub.cpp src/wavDriver.cpp)
    target_include_directories(BinauralAudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(BinauralAudio PUBLIC BinauralSrc)
    add_executable(BinauralBeats src/main.cpp)
    target_link_libraries(BinauralBeats PRIVATE BinauralAudio)
endif()
