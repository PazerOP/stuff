#pragma once

#include "native_handle.hpp"
#include "source.hpp"

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh::io {
// File descriptor source implementation for reading from files, pipes, etc.
class fd_source : public source {
public:
  // Constructor for file descriptor
  MH_STUFF_API fd_source(native_handle fd, bool take_ownership = true);

  // Destructor
  MH_STUFF_API ~fd_source() override;

  // Read data from file asynchronously
  MH_STUFF_API task<size_t> read_async(void *buffer, size_t size) override;

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
} // namespace mh::io
