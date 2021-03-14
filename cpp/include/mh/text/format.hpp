#pragma once

#define MH_FORMATTER_NONE 0
#define MH_FORMATTER_FMTLIB 1
#define MH_FORMATTER_STL 2

#if __has_include(<format>)

#include <format>
#define MH_FORMATTER MH_FORMATTER_STL
namespace mh::detail::format_hpp
{
#define MH_FMT_STRING(...) __VA_ARGS__
	namespace fmtns = ::std;
}

#elif __has_include(<fmt/format.h>)

#include <fmt/format.h>
#include <fmt/ostream.h>
#define MH_FORMATTER MH_FORMATTER_FMTLIB
namespace mh::detail::format_hpp
{
#define MH_FMT_STRING(...) FMT_STRING(__VA_ARGS__)
	namespace fmtns = ::fmt;
}

#else
#define MH_FORMATTER MH_FORMATTER_NONE
#endif

#if MH_FORMATTER != MH_FORMATTER_NONE
#include <string_view>
#include <iomanip>

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

	template<typename TChar, typename T>
	inline auto fmtarg(const TChar* argName, const T& argValue)
	{
		return detail::format_hpp::fmtns::arg(argName, argValue);
	}

	using format_args = detail::format_hpp::fmtns::format_args;
	using wformat_args = detail::format_hpp::fmtns::wformat_args;

	template<typename... TArgs, typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
	inline auto make_format_args(const TArgs&... args) ->
		decltype(detail::format_hpp::fmtns::make_format_args(args...))
	{
		return detail::format_hpp::fmtns::make_format_args(args...);
	}

	template<typename TFmtStr, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format(const TFmtStr& fmtStr, TArgs&&... args) ->
		decltype(detail::format_hpp::fmtns::format(fmtStr, std::forward<TArgs>(args)...))
	{
		return detail::format_hpp::fmtns::format(fmtStr, std::forward<TArgs>(args)...);
	}

	template<typename TFmtStr>
	inline auto vformat(const TFmtStr& fmtStr, const format_args& args) ->
		decltype(detail::format_hpp::fmtns::vformat(fmtStr, args))
	{
		return detail::format_hpp::fmtns::vformat(fmtStr, args);
	}

	template<typename TOutputIt, typename TFmtStr, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format_to(TOutputIt&& outputIt, const TFmtStr& fmtStr, const TArgs&... args) ->
		decltype(detail::format_hpp::fmtns::format_to(std::forward<TOutputIt>(outputIt), fmtStr, args...))
	{
		return detail::format_hpp::fmtns::format_to(std::forward<TOutputIt>(outputIt), fmtStr, args...);
	}

	template<typename TContainer, typename TFmtStr, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format_to_container(TContainer& container, const TFmtStr& fmtStr, const TArgs&... args)
	{
		return format_to(std::back_inserter(container), fmtStr, args...);
	}

	template<typename TOutputIt, typename TFmtStr, typename... TArgs,
		typename = std::enable_if_t<detail::format_hpp::check_type<TArgs...>()>>
		inline auto format_to_n(TOutputIt&& outputIt, size_t n, const TFmtStr& fmtStr, const TArgs&... args)
	{
		return detail::format_hpp::fmtns::format_to_n(std::forward<TOutputIt>(outputIt), n, fmtStr, args...);
	}

	template<typename TFmtStr, typename... TArgs>
	inline auto try_format(const TFmtStr& fmtStr, const TArgs&... args) -> decltype(format(fmtStr, args...)) try
	{
		return ::mh::format(fmtStr, args...);
	}
	catch (const format_error& e)
	{
		return ::mh::format(MH_FMT_STRING("FORMATTING ERROR: Unable to construct string with fmtstr {}: {}"), std::quoted(fmtStr), e.what());
	}

	inline std::string try_vformat(const std::string_view& fmtStr, const format_args& args) try
	{
		return vformat(fmtStr, args);
	}
	catch (const format_error& e)
	{
		return ::mh::format(MH_FMT_STRING("FORMATTING ERROR: Unable to construct string with fmtstr {}: {}"), std::quoted(fmtStr), e.what());
	}

	inline std::wstring try_vformat(const std::wstring_view& fmtStr, const wformat_args& args) try
	{
		return vformat(fmtStr, args);
	}
	catch (const format_error& /*e*/)
	{
		// Can't print error message from exception because fmt does not handle conversion from char -> wchar_t on its own unfortunately
		return ::mh::format(MH_FMT_STRING(L"FORMATTING ERROR: Unable to construct string with fmtstr {}"), std::quoted(fmtStr));
	}

	template<typename TChar = char, typename TTraits = std::char_traits<TChar>, typename TAlloc = std::allocator<TChar>, typename... TArgs>
	inline std::basic_string<TChar, TTraits, TAlloc> build_string(const TArgs&... args)
	{
		std::basic_string<TChar, TTraits, TAlloc> str;

		auto inserter = std::back_inserter(str);
		(format_to(inserter, MH_FMT_STRING("{}"), args), ...);

		return str;
	}
}
#endif
