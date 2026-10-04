#!/bin/bash

# Check that last_include.hpp is the final #include in all test files

test_dir="$(dirname "$0")"
exit_code=0

for file in "$test_dir"/*.cpp; do
    if [[ -f "$file" ]]; then
        # Get the last #include line in the file
        last_include=$(grep -n '#include' "$file" | tail -1)
        
        if [[ -n "$last_include" ]]; then
            # Check if the last include is last_include.hpp
            if ! echo "$last_include" | grep -q 'last_include.hpp'; then
                echo "ERROR: $file does not have last_include.hpp as the final #include"
                echo "  Last include line: $last_include"
                echo "  Add: #include \"last_include.hpp\""
                exit_code=1
            fi
        fi
    fi
done

if [[ $exit_code -eq 0 ]]; then
    echo "All test files have last_include.hpp as the final #include"
fi

exit $exit_code