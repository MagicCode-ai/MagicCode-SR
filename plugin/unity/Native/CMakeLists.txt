cmake_minimum_required(VERSION 3.21.3)
project(magic_sr_unity_plugin)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -U_FORTIFY_SOURCE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector -U_FORTIFY_SOURCE")

set(MAGIC_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(MAGIC_LIB_PATH ${MAGIC_ROOT}/lib/android/libmagic_sr.a)
set(MAGIC_HEADER_DIR ${MAGIC_ROOT}/header)

if(NOT EXISTS ${MAGIC_LIB_PATH})
    message(FATAL_ERROR "Missing Android libmagic_sr.a at ${MAGIC_LIB_PATH}.")
endif()

add_library(magic_sr_unity SHARED magic_sr_plugin.cpp)

add_library(magic_sr STATIC IMPORTED)
set_target_properties(magic_sr PROPERTIES IMPORTED_LOCATION ${MAGIC_LIB_PATH})

target_include_directories(magic_sr_unity PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${MAGIC_HEADER_DIR})

find_library(log-lib log)
find_library(gles-lib GLESv3)
find_library(egl-lib EGL)
find_library(vulkan-lib vulkan)

target_link_libraries(magic_sr_unity
    magic_sr
    ${gles-lib}
    ${egl-lib}
    ${vulkan-lib}
    ${log-lib}
    android
)
