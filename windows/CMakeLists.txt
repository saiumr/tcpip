cmake_minimum_required(VERSION 3.20)
project(for_learn_ws_iocp)

file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

file(GLOB THREAD_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/thread/*.c")

foreach(SRC ${SRC_FILES})
    get_filename_component(EXE_NAME ${SRC} NAME_WE)
    add_executable(${EXE_NAME} ${SRC})
    target_link_libraries(${EXE_NAME} ws2_32)
endforeach()


set(WEB_RESOURCE_DIR "ws_res")

foreach(SRC ${THREAD_SRC_FILES})
    get_filename_component(EXE_NAME ${SRC} NAME_WE)
    add_executable(${EXE_NAME} ${SRC})
    target_link_libraries(${EXE_NAME} ws2_32)
    
    if(MINGW)
        target_compile_options(${EXE_NAME} PRIVATE -mthreads)
        target_link_options(${EXE_NAME} PRIVATE -mthreads)
    elseif(MSVC)
        set_property(TARGET ${EXE_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    if(EXE_NAME STREQUAL "web_server")
        target_compile_definitions(${EXE_NAME} PRIVATE
            WEB_ROOT_PATH="${WEB_RESOURCE_DIR}"
        )
        file(COPY thread/${WEB_RESOURCE_DIR} DESTINATION ${CMAKE_BINARY_DIR})
    endif()

endforeach()
