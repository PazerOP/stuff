#pragma once

#include <filesystem>

namespace mh
{
	std::filesystem::path filename_without_extension(std::filesystem::path path);
	std::filesystem::path replace_filename_keep_extension(std::filesystem::path path, std::filesystem::path newFilename);
}

#ifndef MH_COMPILE_LIBRARY
#include "filesystem_helpers.inl"
#endif
