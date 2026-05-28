# PlatformConfig.cmake — 跨平台编译器检测、vcpkg triplet 自动选择、平台特定配置

# ── vcpkg 自动检测（project() 前调用，仅 CMAKE_GENERATOR 可用）────────────
function(binaural_detect_vcpkg)
    if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "vcpkg toolchain file")
    endif()

    # 根据 CMake 生成器推断 triplet（project() 之前编译器变量不可用）
    if(CMAKE_GENERATOR MATCHES "MinGW")
        set(VCPKG_TARGET_TRIPLET "x64-mingw-static" CACHE STRING "vcpkg triplet" FORCE)
    elseif(CMAKE_GENERATOR MATCHES "Visual Studio")
        set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg triplet" FORCE)
    elseif(CMAKE_GENERATOR MATCHES "Ninja" AND NOT DEFINED VCPKG_TARGET_TRIPLET)
        # Ninja 可能是 MSVC 或 Clang，保留已有缓存值或默认 MSVC
        set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg triplet")
    elseif(APPLE AND NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "vcpkg triplet")
    elseif(UNIX AND NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "vcpkg triplet")
    endif()
    message(STATUS "vcpkg triplet: ${VCPKG_TARGET_TRIPLET}")
endfunction()

# ── GUI 可执行文件平台特定配置 ──────────────────────────────────────────────
function(binaural_configure_gui target_name)
	if(WIN32)
		target_link_libraries(${target_name} PRIVATE winmm urlmon shell32 user32 imm32)
		if(MSVC)
			set_target_properties(${target_name} PROPERTIES
				WIN32_EXECUTABLE TRUE
			)
			target_link_options(${target_name} PRIVATE
				/ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS
			)
		endif()
		# 拷贝 glfw3.dll 到可执行文件目录
		add_custom_command(TARGET ${target_name} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:glfw>
			$<TARGET_FILE_DIR:${target_name}>
		)
    elseif(APPLE)
        target_link_libraries(${target_name} PRIVATE "-framework Cocoa" objc)
        set_target_properties(${target_name} PROPERTIES
            MACOSX_BUNDLE TRUE
        )
    elseif(UNIX)
        target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS})
    endif()
endfunction()

# ── 平台特定编译定义 ────────────────────────────────────────────────────────
function(binaural_platform_defines target_name)
    if(WIN32)
        target_compile_definitions(${target_name} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
    endif()
endfunction()
