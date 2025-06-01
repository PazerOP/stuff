#ifdef MH_COMPILE_LIBRARY
#include "pipe.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef __unix__

#include <unistd.h>
#include <stdexcept>
#include "fd_sink.hpp"
#include "fd_source.hpp"

namespace mh::io
{
	MH_COMPILE_LIBRARY_INLINE pipe::pipe(const source_ptr& src, const sink_ptr& snk) : in(snk), out(src) {}

	MH_COMPILE_LIBRARY_INLINE pipe_ptr pipe::create()
	{
		int pipe_fds[2];
		if (::pipe(pipe_fds) != 0)
		{
			throw std::runtime_error("Failed to create pipe");
		}

		auto source = std::make_shared<fd_source>(pipe_fds[0], true);
		auto sink = std::make_shared<fd_sink>(pipe_fds[1], true);

		return std::make_shared<pipe>(source, sink);
	}
}

#endif // __unix__
