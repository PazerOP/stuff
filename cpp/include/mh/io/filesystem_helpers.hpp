#pragma once

#include <filesystem>

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh
{
	MH_STUFF_API std::filesystem::path filename_without_extension(std::filesystem::path path);
	MH_STUFF_API std::filesystem::path replace_filename_keep_extension(std::filesystem::path path, std::filesystem::path newFilename);

	// Absolute, weakly-canonicalized path of the currently running executable.
	// Windows: GetModuleFileNameW; macOS (no procfs): _NSGetExecutablePath;
	// other POSIX: /proc/self/exe. Throws std::runtime_error if the OS cannot
	// report it.
	MH_STUFF_API std::filesystem::path this_executable_path();

	// Directory containing the running executable (this_executable_path()'s
	// parent) -- e.g. the anchor for locating sibling data/resource files.
	MH_STUFF_API std::filesystem::path this_executable_dir();
}

#ifndef MH_COMPILE_LIBRARY
#include "filesystem_helpers.inl"
#endif
