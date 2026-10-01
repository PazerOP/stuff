#include "mh/source_location.hpp"

#if defined(MH_FORMATTER) && MH_FORMATTER != MH_FORMATTER_NONE
template struct mh::formatter<mh::source_location, char>;
template struct mh::formatter<mh::source_location, wchar_t>;
#endif
