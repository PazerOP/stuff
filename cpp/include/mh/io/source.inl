#ifdef MH_COMPILE_LIBRARY
#include "source.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef __unix__

#include "fd_source.hpp"
#include <unistd.h>

namespace mh::io
{
    MH_COMPILE_LIBRARY_INLINE source_ptr source::stdout_source()
    {
        static auto instance = std::make_shared<fd_source>(STDOUT_FILENO, false);
        return instance;
    }

    MH_COMPILE_LIBRARY_INLINE source_ptr source::stderr_source()
    {
        static auto instance = std::make_shared<fd_source>(STDERR_FILENO, false);
        return instance;
    }
}

#endif // __unix__