#ifdef MH_COMPILE_LIBRARY
#include "codecvt.hpp"
#endif

#if __has_include(<cuchar>)
#include <cuchar>
#endif

#ifndef MH_COMPILE_LIBRARY_INLINE
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

namespace mh
{
	namespace detail::codecvt_hpp
	{
#if __cpp_unicode_characters >= 200704
		MH_COMPILE_LIBRARY_INLINE std::size_t convert_to_mb(char* buf, char16_t from,
			std::mbstate_t& state)
		{
			return std::c16rtomb(buf, from, &state);
		}
		MH_COMPILE_LIBRARY_INLINE std::size_t convert_to_mb(char* buf, char32_t from,
			std::mbstate_t& state)
		{
			return std::c32rtomb(buf, from, &state);
		}

		MH_COMPILE_LIBRARY_INLINE std::size_t convert_to_utf(char16_t* buf, const char* mb,
			std::size_t mbmax, std::mbstate_t& state)
		{
			return std::mbrtoc16(buf, mb, mbmax, &state);
		}
		MH_COMPILE_LIBRARY_INLINE std::size_t convert_to_utf(char32_t* buf, const char* mb,
			std::size_t mbmax, std::mbstate_t& state)
		{
			return std::mbrtoc32(buf, mb, mbmax, &state);
		}

		MH_COMPILE_LIBRARY_INLINE char32_t convert_to_u32(const char16_t*& it, const char16_t* end)
		{
			// https://en.wikipedia.org/wiki/UTF-16#Description
			constexpr uint16_t TOP6_MASK = 0xFC00;

			const uint16_t firstByte = uint16_t(*it++);
			if ((firstByte & TOP6_MASK) == 0xD800)
			{
				uint32_t retVal = uint32_t(firstByte & (~TOP6_MASK)) << 10;

				if (it != end)
					retVal |= uint32_t(uint32_t(*it++) & (~TOP6_MASK));

				return 0x10000 + retVal;
			}

			return firstByte;
		}

		MH_COMPILE_LIBRARY_INLINE size_t convert_to_u16(char32_t in, char16_t out[2])
		{
			constexpr uint16_t TOP6_MASK = 0xFC00;
			constexpr uint16_t BYTE0_MARKER = 0xD800;
			constexpr uint16_t BYTE1_MARKER = 0xDC00;

			uint32_t in_raw = in;
			if (in_raw > 0xFFFF || ((in_raw & TOP6_MASK) == BYTE0_MARKER))
			{
				in_raw -= 0x10000;

				// Two bytes
				out[0] = BYTE0_MARKER | ((in_raw >> 10) & (~TOP6_MASK));
				out[1] = BYTE1_MARKER | (in_raw & (~TOP6_MASK));
				return 2;
			}
			else
			{
				// One byte
				out[0] = in_raw & 0xFFFF;
				return 1;
			}
		}

		MH_COMPILE_LIBRARY_INLINE size_t convert_to_uc(char32_t in, std::basic_string<char16_t>& out)
		{
			char16_t buf[2];
			const size_t chars = convert_to_u16(in, buf);
			out.append(buf, chars);
			return chars;
		}
		MH_COMPILE_LIBRARY_INLINE size_t convert_to_uc(char32_t in, std::basic_string<char32_t>& out)
		{
			out += in;
			return 1;
		}

#if __cpp_char8_t >= 201811
		[[nodiscard]] MH_COMPILE_LIBRARY_INLINE char32_t convert_to_u32(
			const char8_t*& it, const char8_t* end)
		{
			unsigned continuationBytes = 0;

			uint32_t retVal{};

			// https://en.wikipedia.org/wiki/UTF-8#Examples
			const uint8_t firstByte = uint8_t(*it++);
			if ((firstByte & 0b1111'0000) == 0b1111'0000)
			{
				retVal = uint32_t(0b0000'0111 & firstByte) << 18;
				continuationBytes = 3;
			}
			else if ((firstByte & 0b1110'0000) == 0b1110'0000)
			{
				retVal = uint32_t(0b0000'1111 & firstByte) << 12;
				continuationBytes = 2;
			}
			else if ((firstByte & 0b1100'0000) == 0b1100'0000)
			{
				retVal = uint32_t(0b0001'1111 & firstByte) << 6;
				continuationBytes = 1;
			}
			else //if ((firstByte & 0b1000'0000) == 0b0000'0000)
			{
				return char32_t(0b0111'1111 & firstByte);
				//retVal = 0b0111'1111 & firstByte;
				//continuationBytes = 0;
			}

			// Stop at continuationBytes == 0 or reaching the end iterator, whichever is sooner
			for (; it != end && continuationBytes--; ++it)
			{
				retVal |= uint32_t((*it) & 0b0011'1111) << (6 * continuationBytes);
			}

			return char32_t(retVal);
		}

		MH_COMPILE_LIBRARY_INLINE size_t convert_to_uc(char32_t in, std::basic_string<char8_t>& out)
		{
			char8_t buf[4];
			const size_t chars = convert_to_u8(in, buf);
			out.append(buf, chars);
			return chars;
		}
#endif
#endif
	}
}
