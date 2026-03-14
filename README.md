# imgui-curl-downloader

A minimal, self-contained C++ GUI downloader built with **Dear ImGui** (OpenGL3 + GLFW backend) and **libcurl**, featuring:

- Threaded downloads (one `std::thread` per download task)
- Pause / Resume per download
- Progress bars with size and speed overlays
- Builds on **Windows** (MSVC and MinGW-w64) and **Linux** with **CMake + vcpkg**

## Repository layout

```
imgui-curl-downloader/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── vcpkg.json          # vcpkg manifest for dependencies
└── src/
    ├── main.cpp        # entry point + ImGui render loop
    ├── downloader.hpp  # DownloadTask struct + DownloadManager class
    └── downloader.cpp  # libcurl implementation, thread logic
```

## Prerequisites

- [CMake](https://cmake.org/) ≥ 3.20
- [vcpkg](https://github.com/microsoft/vcpkg) (for dependencies)
- A C++17-capable compiler (MSVC 2019+, GCC 10+, Clang 11+)
- On Windows: an OpenGL-capable GPU driver

## Building

### 1. Install vcpkg (first time only)

```bash
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh   # Linux/macOS
# or
.\vcpkg\bootstrap-vcpkg.bat  # Windows
```

### 2. Configure and build

```bash
cmake -B build \
      -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

On Windows with Visual Studio generator:

```bat
cmake -B build ^
      -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

### 3. Run

```bash
./build/ImGuiDownloader      # Linux
build\Release\ImGuiDownloader.exe  # Windows
```

## Usage

1. Enter a direct download URL in the **URL** field.
2. (Optional) Enter an output directory in the **Dir** field (defaults to `.`).
3. Click **Add** to start downloading.
4. Use **Pause / Resume / Cancel** buttons to control each download.