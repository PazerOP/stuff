#pragma once

#include <array>
#include <cstdarg>
#include <cstdio>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

#if __has_include(<mh/text/format.hpp>)
#include <mh/text/format.hpp>
#endif

namespace mh
{
	template<size_t N, typename CharT = char, typename Traits = std::char_traits<CharT>>
	class base_format_string
	{
	public:
		using this_type = base_format_string;
		using value_type = CharT;
		using traits_type = Traits;
		using array_type = std::array<value_type, N>;
		using view_type = std::basic_string_view<value_type, traits_type>;

		constexpr base_format_string()
		{
			m_String[0] = value_type(0);
		}
		constexpr this_type& operator=(const this_type&) = default;

		constexpr size_t size() const { return m_Length; }
		static constexpr size_t max_size() { return N - 1; }

		size_t vsprintf(const value_type* fmtStr, va_list args)
		{
			m_Length = std::vsnprintf(m_String.data(), max_size(), fmtStr, args);
			if (m_Length > max_size())
				m_Length = max_size();

			return m_Length;
		}
		size_t sprintf(const value_type* fmtStr, ...)
		{
			va_list args;
			va_start(args, fmtStr);
			const auto retVal = vsprintf(fmtStr, args);
			va_end(args);
			return retVal;
		}
		constexpr size_t puts(const view_type& str)
		{
			m_Length = std::min(max_size(), str.size());
#if __cpp_lib_is_constant_evaluated >= 201811
			if (!std::is_constant_evaluated())
			{
				std::memcpy(m_String.data(), str.data(), m_Length * sizeof(value_type));
			}
			else
#endif
			{
				for (size_t i = 0; i < m_Length; i++)
					m_String[i] = str[i];
			}

			m_String[m_Length] = 0;
			return m_Length;
		}

#if __has_include(<mh/text/format.hpp>)
		template<typename... TArgs>
		size_t fmt(const view_type& fmtStr, const TArgs&... args)
		{
			const auto result = mh::format_to_n(m_String.data(), max_size(), fmtStr, args...);
			m_Length = result.out - m_String.data();
			m_String[m_Length] = value_type(0);
			return m_Length;
		}
#endif

		constexpr this_type& operator=(const view_type& rhs)
		{
			puts(rhs);
			return *this;
		}

		constexpr const value_type* c_str() const { return m_String.data(); }
		constexpr const array_type& array() const { return m_String; }
		constexpr view_type view() const { return view_type(m_String.data(), m_Length); }

		template<typename TAlloc = std::allocator<value_type>>
		std::basic_string<value_type, traits_type, TAlloc> str() const
		{
			return std::basic_string<value_type, traits_type, TAlloc>(view());
		}

		constexpr operator view_type() const { return view(); }

	protected:
		size_t m_Length = 0;
		array_type m_String;
	};

	template<size_t N, typename CharT = char, typename Traits = std::char_traits<CharT>>
	class printf_string : public base_format_string<N, CharT, Traits>
	{
	public:
		using base_type = base_format_string;
		using this_type = printf_string;
		using value_type = base_type::value_type;
		using traits_type = base_type::traits_type;

		constexpr printf_string() = default;
		printf_string(const value_type* fmtStr, ...)
		{
			va_list args;
			va_start(args, fmtStr);
			vsprintf(fmtStr, args);
			va_end(args);
		}

		constexpr this_type& operator=(const view_type& rhs)
		{
			puts(rhs);
			return *this;
		}
	};
	template<size_t N, typename CharT = char, typename Traits = std::char_traits<CharT>>
	using pfstr = printf_string<N, CharT, Traits>;

	template<size_t N, typename CharT = char, typename Traits = std::char_traits<CharT>>
	class format_string : public base_format_string<N, CharT, Traits>
	{
	public:
		using base_type = base_format_string;
		using this_type = format_string;
		using value_type = base_type::value_type;
		using traits_type = base_type::traits_type;
		using view_type = base_type::view_type;
		using array_type = base_type::array_type;

		constexpr format_string() = default;
		template<typename... TArgs>
		format_string(const view_type& fmtStr, TArgs&&... args)
		{
			fmt(fmtStr, std::forward<TArgs>(args)...);
		}

		constexpr this_type& operator=(const view_type& rhs)
		{
			puts(rhs);
			return *this;
		}
	};
	template<size_t N, typename CharT = char, typename Traits = std::char_traits<CharT>>
	using fmtstr = format_string<N, CharT, Traits>;
}

template<typename CharT, typename Traits, size_t N>
inline std::basic_ostream<CharT, Traits>& operator<<(
	std::basic_ostream<CharT, Traits>& os, const mh::base_format_string<N, CharT, Traits>& str)
{
	return os << str.view();
}
