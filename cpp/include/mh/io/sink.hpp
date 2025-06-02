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
    class sink;
    
    // Shared pointer type for sinks
    using sink_ptr = std::shared_ptr<sink>;
    
    // Interface for data sinks (writing data to files, pipes, etc.)
    class sink
    {
    public:
        virtual ~sink() = default;

        // Write data to sink asynchronously
        virtual MH_STUFF_API task<size_t> write_async(const void* buffer, size_t size) = 0;

        // Get the native handle for platform-specific operations
        virtual MH_STUFF_API native_handle get_native_handle() const = 0;

        // Close the sink
        virtual MH_STUFF_API void close() = 0;

        // Check if the sink is open
        virtual MH_STUFF_API bool is_open() const = 0;

        // Static factory methods for creating platform-specific sinks
        MH_STUFF_API static sink_ptr create_file(const std::filesystem::path& filepath, bool append = false);
        
        // Static singleton instance for standard input
        MH_STUFF_API static sink_ptr stdin_sink();
    };
}

#ifndef MH_COMPILE_LIBRARY
#include "sink.inl"
#endif

