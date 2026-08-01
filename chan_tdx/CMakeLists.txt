cmake_minimum_required(VERSION 3.10)
project(chan_tdx C)

# 32位 Windows DLL
# 交叉编译工具链文件或直接设置
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

# 尝试查找交叉编译工具链
find_program(MINGW_GCC i686-w64-mingw32-gcc)
if(MINGW_GCC)
    set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
    set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
else()
    # 回退：假设本机就是mingw
    message(STATUS "Cross compiler not found, using native compiler")
endif()

# 源文件
set(SOURCES
    main.c
    macd.c
    kline.c
    bi.c
    seg.c
    zs.c
    bsp.c
)

# 头文件
set(HEADERS
    chan_tdx.h
    macd.h
    kline.h
    bi.h
    seg.h
    zs.h
    bsp.h
)

# 共享库（DLL）
add_library(chan_tdx SHARED ${SOURCES} ${HEADERS})

# 设置输出名称
set_target_properties(chan_tdx PROPERTIES
    PREFIX ""
    OUTPUT_NAME "chan_tdx"
    SUFFIX ".dll"
)

# 32位编译选项
target_compile_options(chan_tdx PRIVATE
    -m32
    -O2
    -Wall
    -Wextra
    -Wno-unused-parameter
    -std=c99
)

target_link_options(chan_tdx PRIVATE -m32)

# Windows特定定义
target_compile_definitions(chan_tdx PRIVATE
    _WIN32
    WIN32
    DLL_EXPORTS
)

# 安装
install(TARGETS chan_tdx
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES ${HEADERS} DESTINATION include)
