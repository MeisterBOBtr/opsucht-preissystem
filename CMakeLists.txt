cmake_minimum_required(VERSION 3.22)

project(opsucht_inventarwert LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ============================================================
# Preloader Android
# ============================================================

FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    GIT_TAG 0.2.3
)

FetchContent_MakeAvailable(preloader_android)

# ============================================================
# BedrockTools SDK
# ============================================================

FetchContent_Declare(
    bedrocktools
    GIT_REPOSITORY https://github.com/QYCottage/BedrockTools.git
    GIT_TAG main
)

FetchContent_MakeAvailable(bedrocktools)


# EnTT is a header-only dependency used by BedrockTools.
FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.14.0
)
FetchContent_MakeAvailable(EnTT)

# ============================================================
# Mod
# ============================================================

add_library(
    opsucht_inventarwert
    SHARED
    src/main.cpp
)

target_include_directories(
    opsucht_inventarwert
    PRIVATE
    ${bedrocktools_SOURCE_DIR}/include
    ${bedrocktools_SOURCE_DIR}/src
    ${bedrocktools_SOURCE_DIR}/third_party
)

target_link_libraries(
    opsucht_inventarwert
    PRIVATE
    preloader
    EnTT::EnTT
    android
    log
)

target_compile_options(
    opsucht_inventarwert
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

set_target_properties(
    opsucht_inventarwert
    PROPERTIES
    OUTPUT_NAME "opsucht_inventarwert"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
)
