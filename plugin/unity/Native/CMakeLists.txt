cmake_minimum_required(VERSION 3.21.3)
project(magic_sr_unity_plugin)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -U_FORTIFY_SOURCE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector -U_FORTIFY_SOURCE")

set(MAGIC_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(MAGIC_RELEASE_ROOT ${MAGIC_ROOT}/../release/v1.1.0)
set(MAGIC_LIB_PATH ${MAGIC_RELEASE_ROOT}/lib/android/libmagic_sr.a)

if(NOT EXISTS ${MAGIC_LIB_PATH})
    set(MAGIC_LIB_PATH ${MAGIC_ROOT}/build/android/build/libmagic_sr.a)
endif()

if(NOT EXISTS ${MAGIC_LIB_PATH})
    message(FATAL_ERROR "Missing Android libmagic_sr.a. Tried ${MAGIC_RELEASE_ROOT}/lib/android/libmagic_sr.a and ${MAGIC_ROOT}/build/android/build/libmagic_sr.a")
endif()

add_library(magic_sr_unity SHARED magic_sr_plugin.cpp)

add_library(magic_sr STATIC IMPORTED)
set_target_properties(magic_sr PROPERTIES IMPORTED_LOCATION ${MAGIC_LIB_PATH})

target_include_directories(magic_sr_unity PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${MAGIC_ROOT}/interface ${MAGIC_ROOT}/src)

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
