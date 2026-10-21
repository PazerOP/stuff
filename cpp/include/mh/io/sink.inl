#ifdef MH_COMPILE_LIBRARY
#include "sink.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef __unix__

#include "fd_sink.hpp"
#include <unistd.h>

namespace mh::io
{
    MH_COMPILE_LIBRARY_INLINE sink_ptr sink::stdin_sink()
    {
        static auto instance = std::make_shared<fd_sink>(STDIN_FILENO, false);
        return instance;
    }
}

#endif // __unix__