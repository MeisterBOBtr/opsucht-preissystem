cmake_minimum_required(VERSION 3.22)

project(opvantis LANGUAGES CXX)

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

# ============================================================
# Mod
# ============================================================

add_library(
    opvantis
    SHARED
    src/main.cpp
)

target_include_directories(
    opvantis
    PRIVATE
    ${bedrocktools_SOURCE_DIR}/include
    ${bedrocktools_SOURCE_DIR}/src
    ${bedrocktools_SOURCE_DIR}/third_party
)

target_link_libraries(
    opvantis
    PRIVATE
    preloader
)

target_compile_options(
    opvantis
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

set_target_properties(
    opvantis
    PROPERTIES
    OUTPUT_NAME "opvantis"
)
