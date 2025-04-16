cmake_minimum_required(VERSION 3.15)

project(libuv-learn)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(libuv CONFIG REQUIRED)

# 遍历当前文件夹下的所有0开头的子文件夹
file(GLOB SUBDIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} 0*)

foreach(SUBDIR ${SUBDIRS})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
        # 检查子文件夹中是否存在demo.c文件
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/demo.c)
            # 生成可执行文件名
            set(EXECUTABLE_NAME ${SUBDIR}-demo)
            # 添加可执行文件
            add_executable(${EXECUTABLE_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/demo.c)
            # 链接libuv库
            target_link_libraries(${EXECUTABLE_NAME} $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>)
        endif()
    endif()
endforeach()
