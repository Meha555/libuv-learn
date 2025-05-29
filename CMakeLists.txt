cmake_minimum_required(VERSION 3.15)

project(libuv-learn)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(libuv REQUIRED)

# 读取 .gitignore 文件内容
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/.gitignore GITIGNORE_CONTENT)

# 将 .gitignore 内容按行分割成列表
string(REPLACE "\n" ";" GITIGNORE_LINES ${GITIGNORE_CONTENT})

# 遍历当前文件夹下的所有子文件夹
file(GLOB SUBDIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)

foreach(SUBDIR ${SUBDIRS})
    # 检查子文件夹是否在 .gitignore 中
    list(FIND GITIGNORE_LINES ${SUBDIR} INDEX)
    if(INDEX EQUAL -1 AND IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
        file(GLOB DEMO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/*.c)
        if(DEMO_FILES)
            # 生成伪目标
            add_custom_target(phony-${SUBDIR})
            foreach(FILE ${DEMO_FILES})
                # 提取文件的基本名称
                get_filename_component(FILE_BASENAME ${FILE} NAME_WE)
                # 生成可执行文件名
                set(EXECUTABLE_NAME ${SUBDIR}-${FILE_BASENAME})
                message(STATUS "Building ${EXECUTABLE_NAME}")
                # 添加可执行文件
                add_executable(${EXECUTABLE_NAME} ${FILE})
                # 链接 libuv 库
                target_link_libraries(${EXECUTABLE_NAME} PRIVATE $<IF:$<BOOL:${WIN32}>,$<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>,uv>)
                # 将可执行文件添加到伪目标中
                add_dependencies(phony-${SUBDIR} ${EXECUTABLE_NAME})
            endforeach()
        endif()
    endif()
endforeach()