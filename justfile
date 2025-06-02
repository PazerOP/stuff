# Build and test commands for mh_stuff

# Default recipe - build and test
default:
    @just --list

# Clean all build directories
clean:
    rm -rf build build-* 

# Configure the project (using CMake presets)
configure:
    cmake --preset default

# Build the project
build: configure
    cmake --build --preset default

# Run tests
test: build
    ctest --preset default

# Debug build
debug:
    cmake --preset debug
    cmake --build --preset debug

# Run tests with coverage report (Linux/macOS only)
coverage:
    cmake --preset coverage
    cmake --build --preset coverage
    ctest --preset coverage
    cd build-coverage && gcovr --root "../" --exclude ".*/catch.hpp" --exclude ".*/test_compile_file/.*" --exclude ".*/test/.*" --sort-percentage --html-details "results.html" .

# Build as shared library
build-shared:
    cmake --preset shared
    cmake --build --preset shared

# Build header-only mode
build-header-only:
    cmake --preset header-only
    cmake --build --preset header-only
