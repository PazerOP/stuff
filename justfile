# Build and test commands for mh_stuff

# Default recipe - build and test
default:
    @just --list

# Clean all build directories
clean:
    rm -rf build build-*

# Configure the project (using CMake presets)
configure PRESET="default":
    cmake --preset {{PRESET}}

# Build the project
build PRESET="default": (configure PRESET)
    cmake --build --preset {{PRESET}}

# Run tests
test PRESET="default": (build PRESET)
    ctest --preset {{PRESET}}

# Run tests with coverage report (Linux/macOS only)
coverage: (test "coverage")
    cd {{justfile_directory()}}/build/coverage && gcovr --root="{{justfile_directory()}}/cpp/" --gcov-ignore-errors=all --sort=uncovered-percent --html-details="results.html" --print-summary "{{justfile_directory()}}/build/coverage"

