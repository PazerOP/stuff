#pragma once

#include <mh/memory/unique_object.hpp>
#include <unistd.h>

namespace mh::io
{
    namespace detail::native_handle_hpp
    {
#ifdef __unix__
        // Traits for file descriptors
        struct fd_traits
        {
            static constexpr int invalid() { return -1; }
            
            void delete_obj(int fd) const
            {
                if (fd >= 0)
                    close(fd);
            }
            
            int release_obj(int& fd) const
            {
                int temp = fd;
                fd = invalid();
                return temp;
            }
            
            bool is_obj_valid(int fd) const
            {
                return fd >= 0;
            }
        };
#endif
    }

#ifdef __unix__
    // Platform-agnostic abstract handle type for Unix-like systems
    using native_handle = int; // File descriptor on Unix

    // RAII wrapper for native handles
    using unique_native_handle = mh::unique_object<native_handle, 
        detail::native_handle_hpp::fd_traits>;
#endif
}