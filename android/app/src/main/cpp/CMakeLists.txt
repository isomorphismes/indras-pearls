cmake_minimum_required(VERSION 3.22.1)
project(indras_pearls_native C)

set(NATIVE_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")

add_library(native_app_glue STATIC
    "${NATIVE_GLUE_DIR}/android_native_app_glue.c"
)
target_include_directories(native_app_glue PUBLIC "${NATIVE_GLUE_DIR}")

if(ICK_ARMV7_OBJECTS)
    if(NOT ANDROID_ABI STREQUAL "armeabi-v7a")
        message(FATAL_ERROR "ICK_ARMV7_OBJECTS requires ANDROID_ABI=armeabi-v7a")
    endif()
    if(NOT EXISTS "${ICK_ORIGINAL_KLEINIAN_OBJECT}")
        message(FATAL_ERROR "ICK original Kleinian object not found: ${ICK_ORIGINAL_KLEINIAN_OBJECT}")
    endif()
    set(ORIGINAL_KLEINIAN_SOURCE "${ICK_ORIGINAL_KLEINIAN_OBJECT}")
    set_source_files_properties(
        "${ICK_ORIGINAL_KLEINIAN_OBJECT}"
        PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE
    )
else()
    set(ORIGINAL_KLEINIAN_SOURCE original_kleinian.c)
endif()

add_library(indras_pearls SHARED
    ${ORIGINAL_KLEINIAN_SOURCE}
    original_texture_renderer.c
    original_trace_controls.c
    native_app_glue_stub.c
)
target_compile_features(indras_pearls PRIVATE c_std_11)
target_compile_options(indras_pearls PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(indras_pearls PRIVATE -u ANativeActivity_onCreate)

target_link_libraries(indras_pearls
    native_app_glue
    android
    EGL
    GLESv3
    log
    m
)
