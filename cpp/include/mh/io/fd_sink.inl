#pragma once

#include "fd_sink.hpp"
#include <mh/error/not_implemented_error.hpp>
#include <unistd.h>
#include <fcntl.h>

#ifndef MH_COMPILE_LIBRARY_INLINE
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

namespace mh::io
{
#ifdef __unix__
    MH_COMPILE_LIBRARY_INLINE fd_sink::fd_sink(native_handle fd, bool take_ownership)
        : fd_(take_ownership ? unique_native_handle(fd) : unique_native_handle(dup(fd))), 
          is_open_(fd >= 0)
    {
    }

    MH_COMPILE_LIBRARY_INLINE fd_sink::~fd_sink() = default;

    MH_COMPILE_LIBRARY_INLINE task<size_t> fd_sink::write_async(const void* buffer, size_t size)
    {
        if (!is_open_)
            throw std::runtime_error("fd_sink is not open");
            
        ssize_t bytes_written = ::write(fd_.value(), buffer, size);
        if (bytes_written < 0)
            throw std::runtime_error("Failed to write to file descriptor");
            
        co_return static_cast<size_t>(bytes_written);
    }

    MH_COMPILE_LIBRARY_INLINE native_handle fd_sink::get_native_handle() const
    {
        return fd_.value();
    }

    MH_COMPILE_LIBRARY_INLINE void fd_sink::close()
    {
        if (is_open_)
        {
            fd_.reset();
            is_open_ = false;
        }
    }

    MH_COMPILE_LIBRARY_INLINE bool fd_sink::is_open() const
    {
        return is_open_ && fd_;
    }
#endif

    MH_COMPILE_LIBRARY_INLINE sink_ptr sink::create_file(const std::filesystem::path& filepath, bool append)
    {
#ifdef __unix__
        int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
        int fd = open(filepath.c_str(), flags, 0644);
        if (fd == -1)
        {
            throw std::runtime_error("Failed to open file for writing: " + filepath.string());
        }
        
        return std::make_shared<fd_sink>(fd, true);
#else
        throw mh::not_implemented_error();
#endif
    }
}