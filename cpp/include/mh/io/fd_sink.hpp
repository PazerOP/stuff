#pragma once

#ifdef __unix__

#include "sink.hpp"
#include "native_handle.hpp"
#include <filesystem>

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh::io
{
    // File descriptor sink implementation for writing to files, pipes, etc.
    class fd_sink : public sink
    {
    public:

        // Constructor for file descriptor
        MH_STUFF_API fd_sink(native_handle fd, bool take_ownership = true);
        
        // Destructor
        MH_STUFF_API ~fd_sink() override;

        // Write data to file asynchronously
        MH_STUFF_API task<size_t> write_async(const void* buffer, size_t size) override;

        // Get the native handle (file descriptor)
        MH_STUFF_API native_handle get_native_handle() const override;

        // Close the file
        MH_STUFF_API void close() override;

        // Check if the file is open
        MH_STUFF_API bool is_open() const override;

    private:
        unique_native_handle fd_;
        bool is_open_;
    };
}

#endif // __unix__