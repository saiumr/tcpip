cmake_minimum_required(VERSION 3.20)

project(learn_tcp_ip_dev)

file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

file(GLOB THREAD_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/thread/*.c")

foreach(SRC ${SRC_FILES})
    get_filename_component(EXE_NAME ${SRC} NAME_WE)
    add_executable(${EXE_NAME} ${SRC})
endforeach()

foreach(SRC ${THREAD_SRC_FILES})
    get_filename_component(EXE_NAME ${SRC} NAME_WE)
    add_executable(${EXE_NAME} ${SRC})
    target_link_libraries(${EXE_NAME} pthread)
endforeach()


