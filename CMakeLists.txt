cmake_minimum_required(VERSION 3.15)
project(ScreenShare VERSION 1.1.0 LANGUAGES CXX RC)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 设置目标平台（Windows）
if(WIN32)
    add_definitions(-DWIN32 -D_WINDOWS -D_UNICODE -DUNICODE)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()

# 添加可执行文件
add_executable(ScreenShare
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sender.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ScreenShare.rc
)

# 设置目标属性
target_compile_features(ScreenShare PRIVATE cxx_std_17)
target_compile_definitions(ScreenShare PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

# 包含目录
target_include_directories(ScreenShare PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 设置RC编译器的包含目录
if(WIN32 AND CMAKE_RC_COMPILER)
    string(REPLACE ";" " " RC_INCLUDES "${CMAKE_INCLUDE_PATH}")
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /i \"${CMAKE_CURRENT_SOURCE_DIR}\" /i \"${CMAKE_CURRENT_SOURCE_DIR}/res\"")
endif()

# 链接库
target_link_libraries(ScreenShare PRIVATE
    gdiplus
    d3d11
    dxgi
    d3dcompiler
    winmm
    ws2_32
    iphlpapi
    comctl32
    gdi32
    user32
    kernel32
    ole32
    oleaut32
    uuid
)

# 设置Windows子系统（GUI应用程序）
if(WIN32)
    set_target_properties(ScreenShare PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/ScreenShare.manifest"
    )
endif()

# 安装目标
install(TARGETS ScreenShare
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)
