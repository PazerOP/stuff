#pragma once

#include "fd_source.hpp"
#include <mh/error/not_implemented_error.hpp>
#ifdef __unix__
#include <unistd.h>
#include <fcntl.h>
#endif

#ifndef MH_COMPILE_LIBRARY_INLINE
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

namespace mh::io
{
#ifdef __unix__
    MH_COMPILE_LIBRARY_INLINE fd_source::fd_source(native_handle fd, bool take_ownership)
        : fd_(take_ownership ? unique_native_handle(fd) : unique_native_handle(dup(fd))), 
          is_open_(fd >= 0)
    {
    }

    MH_COMPILE_LIBRARY_INLINE fd_source::~fd_source() = default;

    MH_COMPILE_LIBRARY_INLINE task<size_t> fd_source::read_async(void* buffer, size_t size)
    {
        if (!is_open_)
            throw std::runtime_error("fd_source is not open");
            
        ssize_t bytes_read = ::read(fd_.value(), buffer, size);
        if (bytes_read < 0)
            throw std::runtime_error("Failed to read from file descriptor");
            
        co_return static_cast<size_t>(bytes_read);
    }

    MH_COMPILE_LIBRARY_INLINE native_handle fd_source::get_native_handle() const
    {
        return fd_.value();
    }

    MH_COMPILE_LIBRARY_INLINE void fd_source::close()
    {
        if (is_open_)
        {
            fd_.reset();
            is_open_ = false;
        }
    }

    MH_COMPILE_LIBRARY_INLINE bool fd_source::is_open() const
    {
        return is_open_ && fd_;
    }
#endif
}