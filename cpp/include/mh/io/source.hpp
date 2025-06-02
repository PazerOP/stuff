#pragma once

#include "native_handle.hpp"
#include <mh/coroutine/task.hpp>
#include <cstddef>
#include <memory>
#include <filesystem>

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh::io
{
    // Forward declaration
    class source;
    
    // Shared pointer type for sources
    using source_ptr = std::shared_ptr<source>;
    
    // Interface for data sources (reading data from files, pipes, etc.)
    class source
    {
    public:
        virtual ~source() = default;

        // Read data from source asynchronously
        virtual MH_STUFF_API task<size_t> read_async(void *buffer, size_t size) = 0;

        // Get the native handle for platform-specific operations
        virtual MH_STUFF_API native_handle get_native_handle() const = 0;

        // Close the source
        virtual MH_STUFF_API void close() = 0;

        // Check if the source is open
        virtual MH_STUFF_API bool is_open() const = 0;

        // Static factory methods for creating platform-specific sources
        MH_STUFF_API static source_ptr create_file(const std::filesystem::path& filepath);
        
        // Static singleton instances for standard streams
        MH_STUFF_API static source_ptr stdout_source();
        MH_STUFF_API static source_ptr stderr_source();
    };
}

#ifndef MH_COMPILE_LIBRARY
#include "source.inl"
#endif

