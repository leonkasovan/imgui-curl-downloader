cmake_minimum_required(VERSION 3.20)

# To use vcpkg, pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
# when invoking CMake, or set the VCPKG_ROOT environment variable.

project(ImGuiDownloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC-specific: enable UTF-8 source and execution character sets
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(CURL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(ImGuiDownloader
    src/main.cpp
    src/downloader.cpp
)

target_link_libraries(ImGuiDownloader PRIVATE
    CURL::libcurl
    glfw
    imgui::imgui
    OpenGL::GL
)

# On Windows link ws2_32 (required by curl on MinGW)
if(WIN32)
    target_link_libraries(ImGuiDownloader PRIVATE ws2_32)
endif()

install(TARGETS ImGuiDownloader DESTINATION bin)
