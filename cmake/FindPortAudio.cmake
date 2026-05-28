# FindPortAudio.cmake — 跨平台 PortAudio 发现
#
# 查找顺序:
#   Windows: vcpkg find_package(portaudio CONFIG)
#   macOS:   find_package(portaudio CONFIG) → pkg-config → Homebrew 路径
#   Linux:   find_package(portaudio CONFIG) → pkg-config → 系统路径
#
# 输出变量:
#   PORTAUDIO_FOUND       — TRUE 如果找到
#   PORTAUDIO_LIBRARIES   — 链接目标（portaudio_static 或 portaudio）
#   PORTAUDIO_INCLUDE_DIR — 头文件路径（仅在非 CMake 配置模式下设置）

if(WIN32)
    # Windows: 依赖 vcpkg
    find_package(portaudio CONFIG QUIET)
    if(portaudio_FOUND)
    	set(PORTAUDIO_FOUND TRUE)
        # vcpkg 提供 portaudio_static 目标
        if(TARGET portaudio_static)
            set(PORTAUDIO_LIBRARIES portaudio_static)
        elseif(TARGET portaudio)
            set(PORTAUDIO_LIBRARIES portaudio)
        endif()
        message(STATUS "PortAudio found via vcpkg (${VCPKG_TARGET_TRIPLET})")
    else()
        set(PORTAUDIO_FOUND FALSE)
    endif()
elseif(APPLE)
    # macOS: 尝试 CMake 配置 → Homebrew pkg-config → 路径探测
    find_package(portaudio CONFIG QUIET)
    if(portaudio_FOUND)
    	set(PORTAUDIO_FOUND TRUE)
        if(TARGET portaudio_static)
            set(PORTAUDIO_LIBRARIES portaudio_static)
        elseif(TARGET portaudio)
            set(PORTAUDIO_LIBRARIES portaudio)
        endif()
        message(STATUS "PortAudio found via CMake config")
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PORTAUDIO_PC QUIET portaudio-2.0)
        endif()
        if(PORTAUDIO_PC_FOUND)
        	set(PORTAUDIO_FOUND TRUE)
            set(PORTAUDIO_LIBRARIES ${PORTAUDIO_PC_LIBRARIES})
            set(PORTAUDIO_INCLUDE_DIR ${PORTAUDIO_PC_INCLUDE_DIRS})
            message(STATUS "PortAudio found via pkg-config: ${PORTAUDIO_PC_LIBRARIES}")
        else()
            # 探测 Homebrew 路径
            find_library(PORTAUDIO_LIB portaudio
                PATHS /opt/homebrew/lib /usr/local/lib
            )
            find_path(PORTAUDIO_INC portaudio.h
                PATHS /opt/homebrew/include /usr/local/include
            )
            if(PORTAUDIO_LIB AND PORTAUDIO_INC)
            	set(PORTAUDIO_FOUND TRUE)
                set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIB})
                set(PORTAUDIO_INCLUDE_DIR ${PORTAUDIO_INC})
                message(STATUS "PortAudio found at: ${PORTAUDIO_LIB}")
            else()
                set(PORTAUDIO_FOUND FALSE)
            endif()
        endif()
    endif()
elseif(UNIX)
    # Linux: CMake 配置 → pkg-config → 系统路径
    find_package(portaudio CONFIG QUIET)
    if(portaudio_FOUND)
    	set(PORTAUDIO_FOUND TRUE)
        if(TARGET portaudio_static)
            set(PORTAUDIO_LIBRARIES portaudio_static)
        elseif(TARGET portaudio)
            set(PORTAUDIO_LIBRARIES portaudio)
        endif()
        message(STATUS "PortAudio found via CMake config")
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PORTAUDIO_PC QUIET portaudio-2.0)
        endif()
        if(PORTAUDIO_PC_FOUND)
        	set(PORTAUDIO_FOUND TRUE)
            set(PORTAUDIO_LIBRARIES ${PORTAUDIO_PC_LIBRARIES})
            set(PORTAUDIO_INCLUDE_DIR ${PORTAUDIO_PC_INCLUDE_DIRS})
            message(STATUS "PortAudio found via pkg-config: ${PORTAUDIO_PC_LIBRARIES}")
        else()
            find_library(PORTAUDIO_LIB portaudio)
            find_path(PORTAUDIO_INC portaudio.h)
            if(PORTAUDIO_LIB AND PORTAUDIO_INC)
            	set(PORTAUDIO_FOUND TRUE)
                set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIB})
                set(PORTAUDIO_INCLUDE_DIR ${PORTAUDIO_INC})
                message(STATUS "PortAudio found at: ${PORTAUDIO_LIB}")
            else()
                set(PORTAUDIO_FOUND FALSE)
            endif()
        endif()
    endif()
else()
    set(PORTAUDIO_FOUND FALSE)
endif()

if(NOT PORTAUDIO_FOUND)
    message(WARNING "PortAudio not found — audio output disabled, WAV file output only. "
                    "Install via: vcpkg install portaudio (Windows) / brew install portaudio (macOS) / apt install libportaudio2 portaudio19-dev (Linux)")
endif()
