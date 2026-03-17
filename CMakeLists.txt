cmake_minimum_required(VERSION 3.20)

project(ImGuiDownloader LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(CURL REQUIRED)

add_executable(ImGuiDownloader
  src/main.cpp
  src/downloader.cpp

  # ImGui core
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp
  # optional:
  # third_party/imgui/imgui_demo.cpp

  # ImGui backends
  third_party/imgui/backends/imgui_impl_glfw.cpp
  third_party/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(ImGuiDownloader PRIVATE
  third_party/imgui
  third_party/imgui/backends
)

# If you want to force w64devkit headers/libs, uncomment and adjust:
# target_include_directories(ImGuiDownloader PRIVATE /w64devkit/include)
# target_link_directories(ImGuiDownloader PRIVATE /w64devkit/lib)

if(WIN32)
  target_link_libraries(ImGuiDownloader PRIVATE
    CURL::libcurl
    glfw3
    opengl32
    gdi32
    user32
    shell32
    ws2_32
  )
else()
  target_link_libraries(ImGuiDownloader PRIVATE CURL::libcurl glfw3 OpenGL::GL)
endif()