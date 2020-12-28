#ifdef MH_COMPILE_LIBRARY
#include "codecvt.hpp"
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

#if __cpp_char8_t >= 201811
		[[nodiscard]] MH_COMPILE_LIBRARY_INLINE constexpr char32_t convert_to_u32(const char8_t*& it, const char8_t* end)
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