cmake_minimum_required(VERSION 3.22)
project(opsucht_inventarwert LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT ANDROID_ABI)
    set(ANDROID_ABI arm64-v8a)
endif()

include(FetchContent)

FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    GIT_TAG 0.2.3
)
FetchContent_MakeAvailable(preloader_android)

add_library(opsucht_inventarwert SHARED
    src/main.cpp
)

target_include_directories(opsucht_inventarwert PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(opsucht_inventarwert PRIVATE
    preloader
)

if(ANDROID)
    set_target_properties(opsucht_inventarwert PROPERTIES
        OUTPUT_NAME "opsucht_inventarwert"
    )
endif()

# Einfache Paketstruktur für LeviLauncher.
set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/out/opsucht_inventarwert")
add_custom_command(TARGET opsucht_inventarwert POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
            "${PACKAGE_DIR}/manifest.json"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:opsucht_inventarwert>"
            "${PACKAGE_DIR}/libopsucht_inventarwert.so"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "${PACKAGE_DIR}/resources"
)

add_custom_target(levi_package ALL
    DEPENDS opsucht_inventarwert
    COMMAND ${CMAKE_COMMAND} -E echo
            "Mod-Paket vorbereitet: ${PACKAGE_DIR}"
)
