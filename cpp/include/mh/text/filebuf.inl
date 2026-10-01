#ifdef MH_COMPILE_LIBRARY
#include "filebuf.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

MH_COMPILE_LIBRARY_INLINE mh::filebuf::filebuf(FILE* f)
    : file(f)
{
}

MH_COMPILE_LIBRARY_INLINE mh::filebuf::int_type mh::filebuf::overflow(int_type c)
{
	if (c != EOF)
	{
		if (fputc(c, file) == EOF)
			return EOF;
	}
	return c;
}

MH_COMPILE_LIBRARY_INLINE std::streamsize mh::filebuf::xsputn(const char* s, std::streamsize n)
{
	return fwrite(s, 1, n, file);
}

MH_COMPILE_LIBRARY_INLINE int mh::filebuf::sync()
{
	return fflush(file) == 0 ? 0 : -1;
}
