cmake_minimum_required(VERSION 3.22.1)
project("magic_camera_sr")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -U_FORTIFY_SOURCE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector -U_FORTIFY_SOURCE")

set(LOCAL_JNI_DIR ${CMAKE_SOURCE_DIR})
# cpp → main → src → app → android → demo → repo-root/lib/android
set(MAGIC_LIB_DIR "${CMAKE_SOURCE_DIR}/../../../../../../lib/android")
set(MAGIC_LIB_PATH "${MAGIC_LIB_DIR}/libmagic_sr.a")
set(MAGIC_INTERFACE_DIR "${CMAKE_SOURCE_DIR}/../../../../../../interface")
if(NOT EXISTS "${MAGIC_LIB_PATH}")
    message(FATAL_ERROR "Missing core library: ${MAGIC_LIB_PATH}")
endif()

add_library(superresolution SHARED
    ${LOCAL_JNI_DIR}/native-lib.cpp
)

add_library(magic_sr STATIC IMPORTED)
set_target_properties(magic_sr PROPERTIES IMPORTED_LOCATION ${MAGIC_LIB_PATH})

target_include_directories(superresolution PRIVATE
    ${LOCAL_JNI_DIR}
    ${MAGIC_INTERFACE_DIR}
    ${ANDROID_NDK}/sources/third_party/vulkan/src/include
)

find_library(log-lib log)
find_library(dl-lib dl)
find_library(gles-lib GLESv3)
find_library(egl-lib EGL)
find_library(vulkan-lib vulkan)

target_link_libraries(superresolution
    magic_sr
    ${gles-lib}
    ${egl-lib}
    ${vulkan-lib}
    ${log-lib}
    ${dl-lib}
    android
)
