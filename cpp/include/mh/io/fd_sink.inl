#pragma once

#include "fd_sink.hpp"
#include <mh/error/not_implemented_error.hpp>
#ifdef __unix__
#include <unistd.h>
#include <fcntl.h>
#endif

#include <cerrno>
#include <system_error>

#ifndef MH_COMPILE_LIBRARY_INLINE
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

namespace mh::io
{
#ifdef __unix__
    MH_COMPILE_LIBRARY_INLINE fd_sink::fd_sink(native_handle fd, bool take_ownership)
        : fd_(take_ownership ? unique_native_handle(fd) : unique_native_handle(dup(fd))),
          is_open_(static_cast<bool>(fd_)) // dup() may fail: reflect the handle we actually hold
    {
        // Prevent multiple instantiations of standard streams
        static bool stdin_created = false;

        if (fd == STDIN_FILENO) {
            if (stdin_created) {
                throw std::runtime_error("Attempt to create multiple fd_sink instances for STDIN_FILENO");
            }
            stdin_created = true;
        }
    }

    MH_COMPILE_LIBRARY_INLINE fd_sink::~fd_sink() = default;

    MH_COMPILE_LIBRARY_INLINE task<size_t> fd_sink::write_async(const void* buffer, size_t size)
    {
        if (!is_open_)
            throw std::runtime_error("fd_sink is not open");

        // Retry on EINTR: e.g. this library's own SIGCHLD (process_manager) may
        // interrupt the syscall; that is not a write failure.
        ssize_t bytes_written;
        do
        {
            bytes_written = ::write(fd_.value(), buffer, size);
        } while (bytes_written < 0 && errno == EINTR);


        if (bytes_written < 0)
            throw std::system_error(errno, std::generic_category(), "fd_sink::write_async");

        // NOTE: short writes are passed through per the task<size_t> contract;
        // callers that need all bytes written must loop.
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

    MH_COMPILE_LIBRARY_INLINE sink_ptr sink::create_file(const std::filesystem::path& filepath, bool append)
    {
        int fd = ::open(filepath.c_str(), O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0666);
        if (fd < 0)
            throw std::system_error(errno, std::generic_category(), "sink::create_file");

        return std::make_shared<fd_sink>(fd, true);
    }
#endif

    MH_COMPILE_LIBRARY_INLINE sink_ptr sink::stdin_sink()
    {
#ifdef __unix__
        static auto instance = std::make_shared<fd_sink>(STDIN_FILENO, false);
        return instance;
#else
        throw mh::not_implemented_error();
#endif
    }
}
