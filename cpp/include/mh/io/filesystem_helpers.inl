#ifdef MH_COMPILE_LIBRARY
#include "filesystem_helpers.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#elif defined(__APPLE__)
#include <cstdint>
#include <string>
#include <mach-o/dyld.h> // _NSGetExecutablePath
#else
#include <system_error>
#endif

MH_COMPILE_LIBRARY_INLINE std::filesystem::path mh::filename_without_extension(std::filesystem::path path)
{
	return std::move(path.filename().replace_extension());
}

MH_COMPILE_LIBRARY_INLINE std::filesystem::path mh::replace_filename_keep_extension(std::filesystem::path path, std::filesystem::path newFilename)
{
	return std::move(path.replace_filename(newFilename.replace_extension(path.extension())));
}

MH_COMPILE_LIBRARY_INLINE std::filesystem::path mh::this_executable_path()
{
	std::error_code ec;
#if defined(_WIN32)
	// GetModuleFileNameW truncates and returns the buffer size when it does not
	// fit; grow and retry until the whole path is written.
	std::wstring buf(MAX_PATH, L'\0');
	for (;;)
	{
		const DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
		if (len == 0)
			throw std::runtime_error("mh::this_executable_path: GetModuleFileNameW failed");
		if (len < buf.size())
		{
			buf.resize(len);
			break;
		}
		buf.resize(buf.size() * 2);
	}
	std::filesystem::path result(std::move(buf));
#elif defined(__APPLE__)
	// _NSGetExecutablePath fills a caller buffer; the first call (null buffer)
	// reports the required size, the second fills it. The path may contain
	// symlinks/.. -- weakly_canonical below resolves them.
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::string buf(size, '\0');
	if (_NSGetExecutablePath(buf.data(), &size) != 0)
		throw std::runtime_error("mh::this_executable_path: _NSGetExecutablePath failed");
	std::filesystem::path result(buf.c_str());
#else
	std::filesystem::path result = std::filesystem::read_symlink("/proc/self/exe", ec);
	if (ec)
		throw std::runtime_error("mh::this_executable_path: read_symlink(/proc/self/exe) failed: " + ec.message());
#endif

	std::filesystem::path canonical = std::filesystem::weakly_canonical(result, ec);
	return ec ? result : canonical;
}

MH_COMPILE_LIBRARY_INLINE std::filesystem::path mh::this_executable_dir()
{
	return this_executable_path().parent_path();
}
