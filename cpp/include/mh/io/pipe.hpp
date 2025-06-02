#pragma once

#include "sink.hpp"
#include "source.hpp"

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh::io
{
class pipe;

using pipe_ptr = std::shared_ptr<pipe>;

// Pipe type that holds both ends of a pipe
class pipe
{
public:
	MH_STUFF_API pipe(const source_ptr& src, const sink_ptr& snk);

	MH_STUFF_API static pipe_ptr create();

	const sink_ptr in;
	const source_ptr out;
};

// Connect a source to a sink by duplicating the source's fd to the sink's fd
// This is useful for redirecting child process I/O
// Returns a pipe object on success, nullptr on failure
MH_STUFF_API pipe_ptr connect_io(const source_ptr& source, const sink_ptr& sink);
} // namespace mh::io

#ifndef MH_COMPILE_LIBRARY
#include "pipe.inl"
#endif
