#pragma once

#include <cassert>
#include <cwchar>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace mh
{
	namespace detail::codecvt_hpp
	{
		template<typename From, typename To, typename TEnable = void> struct change_encoding_impl;

#if __cpp_unicode_characters >= 200704
		template<typename T> constexpr bool is_utf_v =
#if __cpp_char8_t >= 201811
			std::is_same_v<T, char8_t> ||
#endif
			std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

		std::size_t convert_to_mb(char* buf, char16_t from, std::mbstate_t& state);
		std::size_t convert_to_mb(char* buf, char32_t from, std::mbstate_t& state);

		std::size_t convert_to_utf(char16_t* buf, const char* mb, std::size_t mbmax, std::mbstate_t& state);
		std::size_t convert_to_utf(char32_t* buf, const char* mb, std::size_t mbmax, std::mbstate_t& state);

		template<typename T>
		[[nodiscard]] inline constexpr size_t convert_to_u8(char32_t in, T out[4])
		{
			const uint32_t in_raw = in;

			if (in_raw <= 0x7F)
			{
				out[0] = in_raw & 0b0111'1111;
				return 1;
			}
			else if (in_raw <= 0x7FF)
			{
				out[0] = 0b1100'0000 | ((in_raw >> 6) & 0b0001'1111);
				out[1] = 0b1000'0000 | (in_raw & 0b0011'1111);
				return 2;
			}
			else if (in_raw <= 0xFFFF)
			{
				out[0] = 0b1110'0000 | ((in_raw >> 12) & 0b0000'1111);
				out[1] = 0b1000'0000 | ((in_raw >> 6) & 0b0011'1111);
				out[2] = 0b1000'0000 | ((in_raw >> 0) & 0b0011'1111);
				return 3;
			}
			else if (in_raw <= 0x10FFFF)
			{
				out[0] = 0b1111'0000 | ((in_raw >> 18) & 0b0000'0111);
				out[1] = 0b1000'0000 | ((in_raw >> 12) & 0b0011'1111);
				out[2] = 0b1000'0000 | ((in_raw >> 6) & 0b0011'1111);
				out[3] = 0b1000'0000 | ((in_raw >> 0) & 0b0011'1111);
				return 4;
			}

			return -1;
		}

		template<typename T>
		inline void utf_to_mb(std::string& out, T u32, std::mbstate_t& state)
		{
			static_assert(is_utf_v<T>);
#ifndef MB_LEN_MAX
			constexpr int MB_LEN_MAX = 64;
#endif
			char tempBuf[MB_LEN_MAX];

			const auto bytesWritten = convert_to_mb(tempBuf, u32, state);
			if (bytesWritten == size_t(-1))
				throw std::invalid_argument("Failed to convert all characters");
			else if (bytesWritten > sizeof(tempBuf))
				throw std::runtime_error("Stack corruption");

			out.append(tempBuf, bytesWritten);
		}

#if __cpp_char8_t >= 201811
		[[nodiscard]] char32_t convert_to_u32(const char8_t*& it, const char8_t* end);
		size_t convert_to_uc(char32_t in, std::basic_string<char8_t>& out);

		template<>
		struct change_encoding_impl<char8_t, char>
		{
			std::basic_string<char> operator()(const char8_t* begin, const char8_t* end) const
			{
				std::basic_string<char> retVal;

				std::mbstate_t state{};
				for (auto it = begin; it != end; )
				{
					const char32_t u32 = convert_to_u32(it, end);
					utf_to_mb(retVal, u32, state);
				}

				return retVal;
			}
		};
#endif // __cpp_char8_t >= 201811

		[[nodiscard]] char32_t convert_to_u32(const char16_t*& it, const char16_t* end);
		[[nodiscard]] size_t convert_to_u16(char32_t in, char16_t out[2]);

		inline size_t convert_to_uc(char32_t in, std::basic_string<char16_t>& out);
		inline size_t convert_to_uc(char32_t in, std::basic_string<char32_t>& out);

		template<typename From, typename To>
		struct change_encoding_impl<From, To, std::enable_if_t<!std::is_same_v<From, To>&& is_utf_v<From>&& is_utf_v<To>>>
		{
			std::basic_string<To> operator()(const From* begin, const From* end) const
			{
				std::basic_string<To> retVal;

				for (auto it = begin; it != end; )
				{
					char32_t u32;

					if constexpr (std::is_same_v<From, char32_t>)
						u32 = *it++;
					else
						u32 = convert_to_u32(it, end);

#if __cpp_char8_t >= 201811
					if constexpr (std::is_same_v<To, char8_t>)
					{
						char8_t buf[4];
						const size_t chars = convert_to_u8(u32, buf);
						retVal.append(buf, chars);
					}
					else
#endif
						if constexpr (std::is_same_v<To, char16_t>)
					{
						char16_t buf[2];
						const auto chars = convert_to_u16(u32, buf);
						retVal.append(buf, chars);
					}
					else //if constexpr (std::is_same_v<To, char32_t>)
					{
						retVal += u32;
					}
				}

				return retVal;
			}
		};
#endif // __cpp_unicode_characters >= 200704

		template<typename T>
		struct change_encoding_impl<T, T, std::enable_if_t<std::is_same_v<T, T>>>
		{
			std::basic_string<T> operator()(const T* begin, const T* end) const
			{
				return std::basic_string<T>(begin, end - begin);
			};
		};

		template<typename From>
		struct change_encoding_impl<From, char, std::enable_if_t<is_utf_v<From>>>
		{
			std::string operator()(const From* begin, const From* end) const
			{
				std::string retVal;
				std::mbstate_t state{};

				for (auto it = begin; it != end; ++it)
					utf_to_mb(retVal, *it, state);

				return retVal;
			}
		};

		template<typename To>
		struct change_encoding_impl<char, To, std::enable_if_t<!std::is_same_v<char, To>>>
		{
			std::basic_string<To> operator()(const char* begin, const char* end) const
			{
				static_assert(is_utf_v<To>);
				std::basic_string<To> retVal;

				std::mbstate_t state{};
				for (auto it = begin; it != end; )
				{
					char32_t u32;
					const auto result = convert_to_utf(&u32, it, end - it, state);
					if (result == 0)
					{
						// Stored the null character, we're an std::string so we can continue
						retVal += To(0);
						state = {};
						++it;
					}
					else if (result == -3)
					{
						// Another charN_t needs to be written to the output stream
						convert_to_uc(u32, retVal);
					}
					else if (result == -2)
					{
						// FIXME is this correct? "...forms an incomplete, but so far valid, multibyte character"
						// https://en.cppreference.com/w/cpp/string/multibyte/mbrtoc32
						throw std::invalid_argument("Segment forms invalid UTF character sequence");
					}
					else if (result == -1)
					{
						throw std::runtime_error("Encoding error");
					}
					else
					{
						it += result;
						convert_to_uc(u32, retVal);
					}
				}

				return retVal;
			}
		};

		template<>
		struct change_encoding_impl<char, wchar_t>
		{
			std::basic_string<wchar_t> operator()(const char* begin, const char* end) const
			{
				std::basic_string<wchar_t> retVal;

				std::mbstate_t state{};
				for (auto it = begin; it != end; )
				{
					wchar_t wc;
					const auto result = std::mbrtowc(&wc, it, end - it, &state);

					if (result == 0)
					{
						// Stored the null character, we're an std::string so we can continue
						retVal += wc;
						state = {};
						++it;
					}
					else if (result == -2)
					{
						throw std::invalid_argument("Segment forms invalid multibyte character sequence");
					}
					else if (result == -1)
					{
						throw std::runtime_error("Encoding error");
					}
					else
					{
						it += result;
						retVal += wc;
					}
				}

				return retVal;
			}
		};

		template<>
		struct change_encoding_impl<wchar_t, char>
		{
			std::basic_string<char> operator()(const wchar_t* begin, const wchar_t* end) const
			{
				std::basic_string<char> retVal;

				const auto MB_CUR_MAX_VAL = MB_CUR_MAX;
				char* buf = reinterpret_cast<char*>(alloca(MB_CUR_MAX_VAL));
				std::mbstate_t state{};
				for (auto it = begin; it != end; )
				{
					size_t result;
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
					wcrtomb_s(&result, buf, MB_CUR_MAX_VAL, *it, &state);
#else
					result = std::wcrtomb(buf, *it, &state);
#endif

					if (result == -1)
					{
						throw std::invalid_argument("Invalid wide character");
					}
					else
					{
						assert(result != 0);
						it += result;
						retVal.append(buf, result);
					}
				}

				return retVal;
			}
		};

		template<typename From>
		struct change_encoding_impl<From, wchar_t, std::enable_if_t<!std::is_same_v<wchar_t, From>>>
		{
			std::basic_string<wchar_t> operator()(const From* begin, const From* end) const
			{
				auto converted = change_encoding_impl<From, char>{}(begin, end);
				return change_encoding_impl<char, wchar_t>{}(converted.data(), converted.data() + converted.size());
			}
		};

		template<typename To>
		struct change_encoding_impl<wchar_t, To, std::enable_if_t<!std::is_same_v<wchar_t, To>>>
		{
			std::basic_string<To> operator()(const wchar_t* begin, const wchar_t* end) const
			{
				auto converted = change_encoding_impl<wchar_t, char>{}(begin, end);
				return change_encoding_impl<char, To>{}(converted.data(), converted.data() + converted.size());
			}
		};
	}

	template<typename To, typename From, typename FromTraits = std::char_traits<From>>
	std::basic_string<To> change_encoding(const std::basic_string_view<From, FromTraits>& input)
	{
		const detail::codecvt_hpp::change_encoding_impl<From, To> impl;
		return impl(input.data(), input.data() + input.size());
	}

#ifdef MH_COMPILE_LIBRARY
	extern template std::string change_encoding<char, char>(const std::string_view&);
	extern template std::string change_encoding<char, wchar_t>(const std::wstring_view&);

	extern template std::wstring change_encoding<wchar_t, char>(const std::string_view&);
	extern template std::wstring change_encoding<wchar_t, wchar_t>(const std::wstring_view&);

#if __cpp_unicode_characters >= 200704
	extern template std::u16string change_encoding<char16_t, char>(const std::string_view&);
	extern template std::u16string change_encoding<char16_t, wchar_t>(const std::wstring_view&);
	extern template std::u16string change_encoding<char16_t, char16_t>(const std::u16string_view&);
	extern template std::u16string change_encoding<char16_t, char32_t>(const std::u32string_view&);

	extern template std::u32string change_encoding<char32_t, char>(const std::string_view&);
	extern template std::u32string change_encoding<char32_t, wchar_t>(const std::wstring_view&);
	extern template std::u32string change_encoding<char32_t, char16_t>(const std::u16string_view&);
	extern template std::u32string change_encoding<char32_t, char32_t>(const std::u32string_view&);

	extern template std::string change_encoding<char, char16_t>(const std::u16string_view&);
	extern template std::string change_encoding<char, char32_t>(const std::u32string_view&);

	extern template std::wstring change_encoding<wchar_t, char16_t>(const std::u16string_view&);
	extern template std::wstring change_encoding<wchar_t, char32_t>(const std::u32string_view&);

#if __cpp_char8_t >= 201811
	extern template std::string change_encoding<char, char8_t>(const std::u8string_view&);

	extern template std::wstring change_encoding<wchar_t, char8_t>(const std::u8string_view&);

	extern template std::u8string change_encoding<char8_t, char>(const std::string_view&);
	extern template std::u8string change_encoding<char8_t, wchar_t>(const std::wstring_view&);
	extern template std::u8string change_encoding<char8_t, char8_t>(const std::u8string_view&);
	extern template std::u8string change_encoding<char8_t, char16_t>(const std::u16string_view&);
	extern template std::u8string change_encoding<char8_t, char32_t>(const std::u32string_view&);

	extern template std::u16string change_encoding<char16_t, char8_t>(const std::u8string_view&);

	extern template std::u32string change_encoding<char32_t, char8_t>(const std::u8string_view&);
#endif
#endif
#endif

	template<typename To, typename From>
	inline auto change_encoding(const From* input)
	{
		return change_encoding<To>(std::basic_string_view<From>(input));
	}
	template<typename To, typename From,
		typename FromTraits = std::char_traits<From>,
		typename FromAlloc = std::allocator<From>>
	inline auto change_encoding(const std::basic_string<From, FromTraits, FromAlloc>& input)
	{
		return change_encoding<To>(std::basic_string_view<From, FromTraits>(input));
	}
}

#ifndef MH_COMPILE_LIBRARY
#include "codecvt.inl"
#endif
