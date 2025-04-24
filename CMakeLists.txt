cmake_minimum_required(VERSION 3.15)

project(libuv-learn)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(libuv CONFIG REQUIRED)

# 遍历当前文件夹下的所有0开头的子文件夹
file(GLOB SUBDIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} 0*)

foreach(SUBDIR ${SUBDIRS})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
        # 检查子文件夹中是否存在以demo.c结尾的文件
        file(GLOB DEMO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/*demo.c)
        if(DEMO_FILES)
            foreach(FILE ${DEMO_FILES})
                # 提取文件的基本名称
                get_filename_component(FILE_BASENAME ${FILE} NAME_WE)
                # 生成可执行文件名
                set(EXECUTABLE_NAME ${SUBDIR}-${FILE_BASENAME})
                message(STATUS "Building ${EXECUTABLE_NAME}")
                # 添加可执行文件
                add_executable(${EXECUTABLE_NAME} ${FILE})
                # 链接libuv库
                target_link_libraries(${EXECUTABLE_NAME} PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>)
            endforeach()
        endif()
    endif()
endforeach()