cmake_minimum_required(VERSION 3.16)
project(ISP_Downloader_C VERSION 0.6 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ISP_C_SOURCES
    src/win_main.c
    src/ui_main.c
    src/ui_debug.c
    src/ui_optbytes.c
    src/ui_hexlog.c
    src/ui_input.c
    src/ui_lang.c
    src/app_logic.c
    src/isp_cli.c
    src/isp_config.c
    src/isp_logname.c
    src/serial_port.c
    src/bl_control.c
    src/an3155.c
    src/flash_prog.c
    src/hex_parser.c
    src/chipdb.c
    src/log.c
)

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc")
    list(APPEND ISP_C_SOURCES src/app.rc)
endif()

add_executable(ISP_Downloader_C WIN32 ${ISP_C_SOURCES})

target_include_directories(ISP_Downloader_C PRIVATE src)

if(WIN32)
    target_link_libraries(ISP_Downloader_C PRIVATE
        comctl32 comdlg32 setupapi user32 gdi32 advapi32 shell32 ole32
    )
    target_link_options(ISP_Downloader_C PRIVATE
        -static -static-libgcc -mwindows
        -Wl,--gc-sections
    )
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(ISP_Downloader_C PRIVATE
            -finput-charset=UTF-8 -fexec-charset=GBK
            -ffunction-sections -fdata-sections
        )
        # 体积优化与符号剥离仅 Release；Debug 需保留符号供 gdb 调试
        if(CMAKE_BUILD_TYPE MATCHES "Release|MinSizeRel")
            target_compile_options(ISP_Downloader_C PRIVATE -Os)
            target_link_options(ISP_Downloader_C PRIVATE -s)
        endif()
    endif()
    target_compile_definitions(ISP_Downloader_C PRIVATE
        _WIN32_WINNT=0x0601
        ISP_VERSION_STR="0.6"
    )
endif()
