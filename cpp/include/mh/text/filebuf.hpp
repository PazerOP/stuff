#pragma once

#include <cstdio>
#include <streambuf>

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh
{
	// Custom streambuf that writes to a FILE*
	class filebuf : public std::streambuf
	{
		FILE* file;

	public:
		MH_STUFF_API explicit filebuf(FILE* f);

	protected:
		MH_STUFF_API int_type overflow(int_type c) override;
		MH_STUFF_API std::streamsize xsputn(const char* s, std::streamsize n) override;
		MH_STUFF_API int sync() override;
	};
}

#ifndef MH_COMPILE_LIBRARY
#include "filebuf.inl"
#endif
