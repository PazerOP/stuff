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
} // namespace mh::io

#ifndef MH_COMPILE_LIBRARY
#include "pipe.inl"
#endif
