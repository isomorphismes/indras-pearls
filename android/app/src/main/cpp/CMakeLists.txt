cmake_minimum_required(VERSION 3.22.1)
project(indras_pearls_native C)

set(NATIVE_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")

add_library(native_app_glue STATIC
    "${NATIVE_GLUE_DIR}/android_native_app_glue.c"
)
target_include_directories(native_app_glue PUBLIC "${NATIVE_GLUE_DIR}")

add_library(indras_pearls SHARED
    group_state.c
    limit_set_renderer.c
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
    m)
