#pragma once

#define MH_FORMATTER_NONE 0
#define MH_FORMATTER_FMTLIB 1
#define MH_FORMATTER_STL 2

// The build system may pre-define MH_FORMATTER to force a backend (the mh-stuff
// CMake target does this with MH_FORMATTER_NONE when fmt's headers are visible
// but the compiled fmt library is not linkable with the chosen toolchain, e.g.
// a libstdc++-built distro libfmt while building with clang + libc++).
// __has_include only proves the headers are reachable, not that libfmt's
// compiled symbols will resolve at link time, so an explicit decision wins.
#ifndef MH_FORMATTER
	#if __has_include(<fmt/format.h>)
		#define MH_FORMATTER MH_FORMATTER_FMTLIB
	#elif __has_include(<format>) && 0 // std::format honestly kind of awful
		#define MH_FORMATTER MH_FORMATTER_STL
	#else
		#define MH_FORMATTER MH_FORMATTER_NONE
	#endif
#endif

#if MH_FORMATTER == MH_FORMATTER_FMTLIB

#include <fmt/format.h>

#if __has_include(<fmt/xchar.h>)
	#include <fmt/xchar.h>
#endif
#if __has_include(<fmt/ostream.h>)
	#include <fmt/ostream.h>
#endif
namespace mh::detail::format_hpp
{
#define MH_FMT_STRING(...) FMT_STRING(__VA_ARGS__)
	namespace fmtns = ::fmt;

	// Typed format string aliases (fmt >= 8): their consteval converting
	// constructor checks literal format strings against the argument types at
	// compile time. fmt::wformat_string comes from fmt/xchar.h, included
	// above whenever it is available.
	template<typename... TArgs>
	using format_string_t = fmtns::format_string<TArgs...>;
	template<typename... TArgs>
	using wformat_string_t = fmtns::wformat_string<TArgs...>;
}

#elif MH_FORMATTER == MH_FORMATTER_STL

#include <format>
namespace mh::detail::format_hpp
{
#define MH_FMT_STRING(...) __VA_ARGS__
	namespace fmtns = ::std;

	// std::format is natively compile-time checked via std::format_string
	// (P2508, shipped as a C++20 DR by every <format> implementation this
	// backend could select). NOTE: this also fixes a latent bug - the old
	// code called fmtns::runtime(), which does not exist in std.
	template<typename... TArgs>
	using format_string_t = std::format_string<TArgs...>;
	template<typename... TArgs>
	using wformat_string_t = std::wformat_string<TArgs...>;
}

#endif

#if MH_FORMATTER != MH_FORMATTER_NONE
#include <string>
#include <string_view>
#include <iomanip>
#include <utility>

namespace mh
{
	using detail::format_hpp::fmtns::format_error;
	using detail::format_hpp::fmtns::formatter;
	using detail::format_hpp::fmtns::basic_format_parse_context;
	using detail::format_hpp::fmtns::basic_format_context;
	using detail::format_hpp::fmtns::format_parse_context;
	using detail::format_hpp::fmtns::wformat_parse_context;
	using detail::format_hpp::fmtns::format_context;
	using detail::format_hpp::fmtns::wformat_context;

	namespace detail::format_hpp
	{
		template<typename T>
		struct make_dependent
		{
			using type = T;
		};

		template<typename T, typename TChar, typename = typename make_dependent<mh::formatter<T, TChar>>::type>
		inline constexpr bool formatter_check(T*, TChar*) { return true; }

		inline constexpr bool formatter_check(void*, void*) { return false; }

		// Intellisense dies if you SFINAE on an explicit operator call
#ifndef __INTELLISENSE__
		template<typename T, typename TTo, typename = std::enable_if_t<std::is_convertible_v<T, TTo>>>
		inline constexpr auto implicit_conversion_check(T* t, TTo*) -> decltype(t->operator TTo())
		{
			return true;
		}
#endif

		inline constexpr bool implicit_conversion_check(void*, void*) { return false; }

		template<typename T>
		inline constexpr bool check_type_single()
		{
			using type = std::decay_t<T>;

			constexpr bool HAS_FORMATTER = formatter_check((type*)nullptr, (char*)nullptr);
			constexpr bool HAS_IMPLICIT_CONVERSION = implicit_conversion_check((type*)nullptr, (bool*)nullptr);
			constexpr bool IS_ENUM_CLASS = std::is_enum_v<type> && !std::is_convertible_v<type, int>;

			static_assert(HAS_FORMATTER, "Formatter for type missing");
			static_assert(!HAS_IMPLICIT_CONVERSION, "Formatting this type will result in it being formatted as one of its implicit converted types");
			static_assert(!IS_ENUM_CLASS, "enum class formatting requires mh::enum_fmt");

			return HAS_FORMATTER && !IS_ENUM_CLASS;
		}

		template<typename... T>
		inline constexpr bool check_type()
		{
			return (check_type_single<T>() && ...);
		}
	}

#if MH_FORMATTER == MH_FORMATTER_FMTLIB
	template<typename TChar, typename T>
	inline auto fmtarg(const TChar* argName, const T& argValue)
	{
		return detail::format_hpp::fmtns::arg(argName, argValue);
	}

	// Explicit opt-out of compile-time format string checking:
	// mh::format(mh::runtime(fmtStr), args...) defers checking fmtStr against
	// the arguments to runtime. Narrow and wide strings are both handled.
	using detail::format_hpp::fmtns::runtime;
#elif MH_FORMATTER == MH_FORMATTER_STL
#if defined(__cpp_lib_format) && __cpp_lib_format >= 202311L
	// Explicit opt-out of compile-time format string checking:
	// mh::format(mh::runtime(fmtStr), args...) defers checking fmtStr against
	// the arguments to runtime (std::runtime_format, C++26).
	inline auto runtime(std::string_view fmtStr) { return std::runtime_format(fmtStr); }
	inline auto runtime(std::wstring_view fmtStr) { return std::runtime_format(fmtStr); }
#endif
	// Pre-C++26 STLs have no runtime_format escape hatch (and this backend's
	// old fmtns::runtime() call never compiled anyway); vformat remains the
	// untyped path.
#endif

	using format_args = detail::format_hpp::fmtns::format_args;
	using wformat_args = detail::format_hpp::fmtns::wformat_args;

	template<typename... TArgs, typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
	inline auto make_format_args(const TArgs&... args) ->
		decltype(detail::format_hpp::fmtns::make_format_args(args...))
	{
		return detail::format_hpp::fmtns::make_format_args(args...);
	}

	// format/format_to/format_to_container/format_to_n check literal format
	// strings against the argument types at compile time. A format string only
	// known at runtime must be explicitly wrapped: mh::format(mh::runtime(str),
	// args...) - or use vformat/try_format/try_vformat, which are runtime-
	// checked by design.
	template<typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format(detail::format_hpp::format_string_t<TArgs...> fmtStr, TArgs&&... args) ->
		decltype(detail::format_hpp::fmtns::format(std::move(fmtStr), std::forward<TArgs>(args)...))
	{
		return detail::format_hpp::fmtns::format(std::move(fmtStr), std::forward<TArgs>(args)...);
	}

	template<typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format(detail::format_hpp::wformat_string_t<TArgs...> fmtStr, TArgs&&... args) ->
		decltype(detail::format_hpp::fmtns::format(std::move(fmtStr), std::forward<TArgs>(args)...))
	{
		return detail::format_hpp::fmtns::format(std::move(fmtStr), std::forward<TArgs>(args)...);
	}

	template<typename TFmtStr, typename TFmtArgs>
	inline auto vformat(const TFmtStr& fmtStr, const TFmtArgs& args) ->
		decltype(detail::format_hpp::fmtns::vformat(fmtStr, args))
	{
		return detail::format_hpp::fmtns::vformat(fmtStr, args);
	}

	template<typename TOutputIt, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format_to(TOutputIt&& outputIt, detail::format_hpp::format_string_t<TArgs...> fmtStr, TArgs&&... args) ->
		decltype(detail::format_hpp::fmtns::format_to(std::forward<TOutputIt>(outputIt), std::move(fmtStr), std::forward<TArgs>(args)...))
	{
		return detail::format_hpp::fmtns::format_to(std::forward<TOutputIt>(outputIt), std::move(fmtStr), std::forward<TArgs>(args)...);
	}

	// The wide overloads of format_to/format_to_n delegate through the untyped
	// vformat* layer because the shape of the backend's typed wide overloads
	// varies across supported backend versions; the compile-time check already
	// happened while constructing the wformat_string_t parameter.
	template<typename TOutputIt, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format_to(TOutputIt&& outputIt, detail::format_hpp::wformat_string_t<TArgs...> fmtStr, TArgs&&... args) ->
		decltype(detail::format_hpp::fmtns::vformat_to(std::forward<TOutputIt>(outputIt),
			detail::format_hpp::fmtns::wstring_view(fmtStr), detail::format_hpp::fmtns::make_wformat_args(args...)))
	{
		return detail::format_hpp::fmtns::vformat_to(std::forward<TOutputIt>(outputIt),
			detail::format_hpp::fmtns::wstring_view(fmtStr), detail::format_hpp::fmtns::make_wformat_args(args...));
	}

	template<typename TContainer, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format_to_container(TContainer& container, detail::format_hpp::format_string_t<TArgs...> fmtStr, TArgs&&... args)
	{
		return ::mh::format_to(std::back_inserter(container), std::move(fmtStr), std::forward<TArgs>(args)...);
	}

	template<typename TContainer, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format_to_container(TContainer& container, detail::format_hpp::wformat_string_t<TArgs...> fmtStr, TArgs&&... args)
	{
		return ::mh::format_to(std::back_inserter(container), std::move(fmtStr), std::forward<TArgs>(args)...);
	}

	template<typename TOutputIt, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format_to_n(TOutputIt&& outputIt, size_t n, detail::format_hpp::format_string_t<TArgs...> fmtStr, TArgs&&... args)
	{
		return detail::format_hpp::fmtns::format_to_n(std::forward<TOutputIt>(outputIt), n, std::move(fmtStr), std::forward<TArgs>(args)...);
	}

	template<typename TOutputIt, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format_to_n(TOutputIt&& outputIt, size_t n, detail::format_hpp::wformat_string_t<TArgs...> fmtStr, TArgs&&... args)
	{
		return detail::format_hpp::fmtns::vformat_to_n(std::forward<TOutputIt>(outputIt), n,
			detail::format_hpp::fmtns::wstring_view(fmtStr), detail::format_hpp::fmtns::make_wformat_args(args...));
	}

	template<typename TFmtStr, typename... TArgs>
	inline auto try_format(const TFmtStr& fmtStr, const TArgs&... args) ->
		decltype(::mh::format(::mh::runtime(fmtStr), args...)) try
	{
		return ::mh::format(::mh::runtime(fmtStr), args...);
	}
	catch (const format_error& e)
	{
		return ::mh::format("FORMATTING ERROR: Unable to construct string with fmtstr \"{}\": {}", fmtStr, e.what());
	}

	template<typename TFmtStr, typename TFmtArgs>
	inline auto try_vformat(const TFmtStr& fmtStr, const TFmtArgs& args) try
	{
		return ::mh::vformat(fmtStr, args);
	}
	catch (const format_error& e)
	{
		using char_type_t = std::decay_t<decltype(fmtStr[0])>;
		if constexpr (std::is_same_v<char_type_t, char>)
		{
			return ::mh::format("FORMATTING ERROR: Unable to construct string with fmtstr \"{}\": {}", fmtStr, e.what());
		}
		else if constexpr (std::is_same_v<char_type_t, wchar_t>)
		{
			// Can't print error message from exception because fmt does not handle conversion from char -> wchar_t on its own unfortunately
			return ::mh::format(L"FORMATTING ERROR: Unable to construct string with fmtstr \"{}\"", fmtStr);
		}
		else
		{
			// Other character types are a compile error for now
			static_assert(std::is_same_v<char_type_t, char> || std::is_same_v<char_type_t, wchar_t>,
				"try_vformat only supports char and wchar_t format strings");
		}
	}

	template<typename TChar = char, typename TTraits = std::char_traits<TChar>, typename TAlloc = std::allocator<TChar>, typename... TArgs>
	inline std::basic_string<TChar, TTraits, TAlloc> build_string(const TArgs&... args)
	{
		std::basic_string<TChar, TTraits, TAlloc> str;

		auto inserter = std::back_inserter(str);
		(format_to(inserter, "{}", args), ...);

		return str;
	}
}
#endif
