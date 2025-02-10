cmake_minimum_required(VERSION 3.10)
project(avs2bdnxml C)
# 设置编译器
set(CMAKE_C_COMPILER gcc)
set(CMAKE_ASM_COMPILER "D:/Tools/yasm.exe")  # Windows
set(CMAKE_ASM_NASM_COMPILER "D:/Tools/yasm.exe")  # Windows
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)

# 设置编译选项
set(CMAKE_C_FLAGS "-Wall -DLE_ARCH")
include_directories(inc)
# link_directories(lib)

# 源文件
set(SOURCES
    avs2bdnxml.c
    auto_split.c
    palletize.c
    sup.c
    sort.c
    ass.c
)

# 汇编文件
set(ASM_SOURCES frame-a.asm x86inc.asm)


message(STATUS "CMAKE_ASM_COMPILER: ${CMAKE_ASM_COMPILER}")
message(STATUS "CMAKE_ASM_NASM_COMPILER: ${CMAKE_ASM_NASM_COMPILER}")
# 处理汇编文件
enable_language(ASM)
set(CMAKE_ASM_FLAGS "-f win64 -m amd64 -DARCH_X86_64=1")
# set(CMAKE_ASM_FLAGS "-f win32 -m x86 -DARCH_X86_64=0 -DPREFIX=1")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")

# 生成可执行文件
add_executable(avs2bdnxml ${SOURCES})

# set_source_files_properties(${ASM_SOURCES} PROPERTIES LANGUAGE ASM)
# 链接库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    PNG::PNG
    vfw32
    ZLIB::ZLIB
)

# 处理打包目标
# target_compile_options(avs2bdnxml PRIVATE "-s")
# target_compile_definitions(avs2bdnxml PRIVATE ARCH_X86_64)

