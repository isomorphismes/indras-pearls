cmake_minimum_required(VERSION 3.22.1)
project(indras_pearls_native C)

set(NATIVE_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")

get_filename_component(APPLICATION_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(AICI_ICK_ROOT "${APPLICATION_ROOT}/.ai-ci-ick" CACHE PATH "Pinned ai-ci checkout")
set(ICK_STAGE_ROOT "${APPLICATION_ROOT}/build/ick" CACHE PATH "Qualified ICK ABI stages")
include("${AICI_ICK_ROOT}/ick-android/OwnedC.cmake")
if(NOT ICK_COMPILER)
    ick_android_stage(ROOT "${ICK_STAGE_ROOT}/${ANDROID_ABI}" FORTIFY_SOURCE 2)
endif()
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    # Preserve this application's NDK Thumb-2/NEON/softfp profile in both stages.
    set(ICK_TARGET_FLAGS "-march=armv7-a -mthumb -mfpu=neon -mfloat-abi=softfp")
endif()

set(OWNED_C_SOURCES
    original_texture_renderer.c
    original_trace_controls.c
    native_app_glue_stub.c
)

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
    list(APPEND OWNED_C_SOURCES original_kleinian.c)
    set(ORIGINAL_KLEINIAN_SOURCE)
endif()

ick_android_objects(owned_objects NAME indras_pearls STANDARD 11
    SOURCES ${OWNED_C_SOURCES}
    INCLUDES "${NATIVE_GLUE_DIR}"
    OPTIONS -Wall -Wextra -Wpedantic)

add_library(indras_pearls SHARED
    ${ORIGINAL_KLEINIAN_SOURCE}
    ${owned_objects}
)
set_target_properties(indras_pearls PROPERTIES LINKER_LANGUAGE C)
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
