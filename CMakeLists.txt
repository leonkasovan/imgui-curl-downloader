cmake_minimum_required(VERSION 3.20)

# To use vcpkg, pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
# when invoking CMake.

project(ImGuiDownloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(CURL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(ImGuiDownloader
    src/main.cpp
    src/downloader.cpp
)

# vcpkg's glfw config exports target glfw (lowercase) on some triplets and glfw3::glfw on others.
# Try the canonical CONFIG target first, then fall back.
if(TARGET glfw)
    set(GLFW_TARGET glfw)
elseif(TARGET glfw3)
    set(GLFW_TARGET glfw3)
elseif(TARGET glfw3::glfw)
    set(GLFW_TARGET glfw3::glfw)
else()
    message(FATAL_ERROR "No GLFW target found. Ensure glfw3 is installed via vcpkg and find_package(glfw3 CONFIG) succeeds.")
endif()

# imgui vcpkg port exports imgui::imgui (core) and, when features are enabled, may also export backend targets.
# We'll link core + explicitly compile backends from the package include dir if targets are missing.

# Prefer backend targets if they exist
set(IMGUI_TARGET imgui::imgui)
set(IMGUI_BACKEND_TARGETS "")
if(TARGET imgui::imgui)
    list(APPEND IMGUI_BACKEND_TARGETS imgui::imgui)
endif()
if(TARGET imgui::glfw)
    list(APPEND IMGUI_BACKEND_TARGETS imgui::glfw)
endif()
if(TARGET imgui::opengl3)
    list(APPEND IMGUI_BACKEND_TARGETS imgui::opengl3)
endif()

# If backend targets aren't present, compile the backend .cpp files ourselves.
set(IMGUI_EXTRA_SOURCES "")
if(NOT TARGET imgui::glfw OR NOT TARGET imgui::opengl3)
    get_target_property(IMGUI_INCLUDE_DIR imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)
    list(GET IMGUI_INCLUDE_DIR 0 IMGUI_INCLUDE_DIR0)
    if(EXISTS "${IMGUI_INCLUDE_DIR0}/../share/imgui/backends/imgui_impl_glfw.cpp")
        list(APPEND IMGUI_EXTRA_SOURCES
            "${IMGUI_INCLUDE_DIR0}/../share/imgui/backends/imgui_impl_glfw.cpp"
            "${IMGUI_INCLUDE_DIR0}/../share/imgui/backends/imgui_impl_opengl3.cpp"
        )
    endif()
endif()

target_sources(ImGuiDownloader PRIVATE ${IMGUI_EXTRA_SOURCES})

target_link_libraries(ImGuiDownloader PRIVATE
    CURL::libcurl
    ${GLFW_TARGET}
    imgui::imgui
    OpenGL::GL
)

if(WIN32)
    target_link_libraries(ImGuiDownloader PRIVATE ws2_32)
endif()

install(TARGETS ImGuiDownloader DESTINATION bin)
