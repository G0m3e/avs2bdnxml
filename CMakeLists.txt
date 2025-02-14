cmake_minimum_required(VERSION 3.10)
project(avs2bdnxml C)
# 设置编译器
set(CMAKE_C_COMPILER gcc)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)

# 设置编译选项
set(CMAKE_C_FLAGS "-Wall -DLE_ARCH")
include_directories(inc)
# link_directories(lib)
# 源文件
set(SOURCES
    auto_split.c
    palletize.c
    sup.c
    sort.c
    ass.c
)
add_executable(avs2bdnxml ${SOURCES}
    avs2bdnxml.c
    common.c)
# 链接库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    PNG::PNG
    vfw32
    ZLIB::ZLIB
)

add_library(avs2sup ${SOURCES}
    avs2sup.c
    common.c)

target_include_directories(avs2sup PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:avs2sup/include>
)

target_link_libraries(avs2sup
    PRIVATE
    PNG::PNG
    vfw32
    ZLIB::ZLIB
)

