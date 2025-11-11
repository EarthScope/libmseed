# Building libmseed with CMake

This document describes how to build and install libmseed using CMake.

## Quick Start

### Building the library

```bash
# Configure the build
cmake -B build -S .

# Build the library
cmake --build build

# Install the library
cmake --install build
```

### Using in another CMake project

After installing libmseed, you can use it in your CMake project:

```cmake
# Find the package
find_package(libmseed REQUIRED)

# Link against your target
add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE mseed::mseed)
```

## Build Options

The following options can be configured during the CMake configuration step:

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | `ON` | Build shared libraries |
| `BUILD_STATIC_LIBS` | `ON` | Build static libraries |
| `BUILD_EXAMPLES` | `OFF` | Build example programs |
| `BUILD_TESTS` | `OFF` | Build test suite |
| `LIBMSEED_URL` | `OFF` | Enable URL support via libcurl |

### Examples

Build only static library:
```bash
cmake -B build -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON
```

Build with examples and tests:
```bash
cmake -B build -DBUILD_EXAMPLES=ON -DBUILD_TESTS=ON
```

Enable URL support with libcurl:
```bash
cmake -B build -DLIBMSEED_URL=ON
```

Custom installation prefix:
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/opt/libmseed
cmake --build build
cmake --install build
```

## Installation

The install step will install:
- Library files (static and/or shared) to `lib/`
- Header file (`libmseed.h`) to `include/`
- CMake config files to `lib/cmake/libmseed/`
- pkg-config file to `lib/pkgconfig/`
- Documentation files to `share/doc/libmseed/`
- Example source code to `share/doc/libmseed/examples/`

## Using libmseed in Your Project

### Method 1: CMake find_package (Recommended)

After installing libmseed, you can use it in your CMake project:

```cmake
cmake_minimum_required(VERSION 3.12)
project(MyProject)

# Find libmseed
find_package(libmseed REQUIRED)

# Create your executable
add_executable(myapp main.c)

# Link against libmseed
target_link_libraries(myapp PRIVATE mseed::mseed)
```

If libmseed is installed in a non-standard location:
```bash
cmake -B build -DCMAKE_PREFIX_PATH=/opt/libmseed
```

### Method 2: Using as a subdirectory

You can also include libmseed directly in your project:

```cmake
cmake_minimum_required(VERSION 3.12)
project(MyProject)

# Add libmseed as subdirectory
add_subdirectory(external/libmseed)

# Create your executable
add_executable(myapp main.c)

# Link against libmseed
target_link_libraries(myapp PRIVATE mseed::mseed)
```

### Method 3: pkg-config

If you prefer pkg-config:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMSEED REQUIRED mseed)

add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE ${LIBMSEED_LIBRARIES})
target_include_directories(myapp PRIVATE ${LIBMSEED_INCLUDE_DIRS})
```

## Running Tests

If you built with tests enabled:

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build
cd build
ctest
```

## Cross-compilation

CMake makes cross-compilation straightforward. Example for cross-compiling to ARM:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/arm-toolchain.cmake
cmake --build build
```

## Integration with FetchContent

You can also use CMake's FetchContent to automatically download and build libmseed:

```cmake
include(FetchContent)

FetchContent_Declare(
    libmseed
    GIT_REPOSITORY https://github.com/EarthScope/libmseed.git
    GIT_TAG        v3.1.11
)

FetchContent_MakeAvailable(libmseed)

add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE mseed::mseed)
```

## Troubleshooting

### Library not found
If CMake cannot find libmseed, ensure it's installed and either:
1. Set `CMAKE_PREFIX_PATH` to the installation directory
2. Set `libmseed_DIR` to the directory containing `libmseedConfig.cmake`

Example:
```bash
cmake -B build -Dlibmseed_DIR=/usr/local/lib/cmake/libmseed
```

### URL support issues
If enabling URL support fails, ensure libcurl is installed:
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev

# macOS
brew install curl

# Fedora/RHEL
sudo dnf install libcurl-devel
```

## Comparison with Makefile

The CMake build system provides several advantages over the traditional Makefile:

- Better cross-platform support (Windows, macOS, Linux)
- Easier integration with other CMake projects
- Automatic dependency detection
- Support for multiple build systems (Make, Ninja, Visual Studio, Xcode)
- Better support for IDEs
- Modern package management (find_package, FetchContent)

Both build systems are maintained and fully supported.
