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
        // Prevent multiple instantiations of standard streams
        static bool stdout_created = false;
        static bool stderr_created = false;

        if (fd == STDOUT_FILENO) {
            if (stdout_created) {
                throw std::runtime_error("Attempt to create multiple fd_source instances for STDOUT_FILENO");
            }
            stdout_created = true;
        }
        else if (fd == STDERR_FILENO) {
            if (stderr_created) {
                throw std::runtime_error("Attempt to create multiple fd_source instances for STDERR_FILENO");
            }
            stderr_created = true;
        }
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

    MH_COMPILE_LIBRARY_INLINE source_ptr source::create_file(const std::filesystem::path& filepath)
    {
#ifdef __unix__
        int fd = open(filepath.c_str(), O_RDONLY);
        if (fd == -1)
        {
            throw std::runtime_error("Failed to open file for reading: " + filepath.string());
        }

        return std::make_shared<fd_source>(fd, true);
#else
        throw mh::not_implemented_error();
#endif
    }

    MH_COMPILE_LIBRARY_INLINE source_ptr source::stdout_source()
    {
#ifdef __unix__
        static auto instance = std::make_shared<fd_source>(STDOUT_FILENO, false);
        return instance;
#else
        throw mh::not_implemented_error();
#endif
    }

    MH_COMPILE_LIBRARY_INLINE source_ptr source::stderr_source()
    {
#ifdef __unix__
        static auto instance = std::make_shared<fd_source>(STDERR_FILENO, false);
        return instance;
#else
        throw mh::not_implemented_error();
#endif
    }
}
