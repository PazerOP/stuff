#pragma once

#include <climits>
#include <cstdint>
#include <type_traits>

namespace mh
{
	// Stores all the possible compile-time representations of a character.
	struct multi_char final
	{
		consteval multi_char(
			const char(&narrow_)[2],
			const wchar_t(&wide_)[2],
			const char8_t(&u8_)[2],
			const char16_t(&u16_)[2],
			const char32_t(&u32_)[2]) :
			narrow(narrow_[0]),
			wide(wide_[0]),
			u8(u8_[0]),
			u16(u16_[0]),
			u32(u32_[0])
		{
		}

		char narrow;
		wchar_t wide;
		char8_t u8;
		char16_t u16;
		char32_t u32;

		template<typename T> constexpr auto get() const
		{
			using type = std::decay_t<std::remove_pointer_t<std::decay_t<T>>>;
			if constexpr (std::is_same_v<type, char>)
				return narrow;
			else if constexpr (std::is_same_v<type, wchar_t>)
				return wide;
			else if constexpr (std::is_same_v<type, char8_t>)
				return u8;
			else if constexpr (std::is_same_v<type, char16_t>)
				return u16;
			else if constexpr (std::is_same_v<type, char32_t>)
				return u32;
		}
	};

	inline constexpr auto operator==(const mh::multi_char& lhs, char rhs) { return lhs.narrow == rhs; }
	inline constexpr auto operator==(const mh::multi_char& lhs, wchar_t rhs) { return lhs.wide == rhs; }
	inline constexpr auto operator==(const mh::multi_char& lhs, char8_t rhs) { return lhs.u8 == rhs; }
	inline constexpr auto operator==(const mh::multi_char& lhs, char16_t rhs) { return lhs.u16 == rhs; }
	inline constexpr auto operator==(const mh::multi_char& lhs, char32_t rhs) { return lhs.u32 == rhs; }
	inline constexpr auto operator==(char lhs, const mh::multi_char& rhs) { return lhs == rhs.narrow; }
	inline constexpr auto operator==(wchar_t lhs, const mh::multi_char& rhs) { return lhs == rhs.wide; }
	inline constexpr auto operator==(char8_t lhs, const mh::multi_char& rhs) { return lhs == rhs.u8; }
	inline constexpr auto operator==(char16_t lhs, const mh::multi_char& rhs) { return lhs == rhs.u16; }
	inline constexpr auto operator==(char32_t lhs, const mh::multi_char& rhs) { return lhs == rhs.u32; }
#define mh_make_multi_char(c) ::mh::multi_char(#c, L ## #c, u8 ## #c, u ## #c, U ## #c)
}

#if __has_include(<compare>)
#include <compare>
namespace mh
{
	inline constexpr auto operator<=>(const mh::multi_char& lhs, char rhs) { return lhs.narrow <=> rhs; }
	inline constexpr auto operator<=>(const mh::multi_char& lhs, wchar_t rhs) { return lhs.wide <=> rhs; }
	inline constexpr auto operator<=>(const mh::multi_char& lhs, char8_t rhs) { return lhs.u8 <=> rhs; }
	inline constexpr auto operator<=>(const mh::multi_char& lhs, char16_t rhs) { return lhs.u16 <=> rhs; }
	inline constexpr auto operator<=>(const mh::multi_char& lhs, char32_t rhs) { return lhs.u32 <=> rhs; }
	inline constexpr auto operator<=>(char lhs, const mh::multi_char& rhs) { return lhs <=> rhs.narrow; }
	inline constexpr auto operator<=>(wchar_t lhs, const mh::multi_char& rhs) { return lhs <=> rhs.wide; }
	inline constexpr auto operator<=>(char8_t lhs, const mh::multi_char& rhs) { return lhs <=> rhs.u8; }
	inline constexpr auto operator<=>(char16_t lhs, const mh::multi_char& rhs) { return lhs <=> rhs.u16; }
	inline constexpr auto operator<=>(char32_t lhs, const mh::multi_char& rhs) { return lhs <=> rhs.u32; }
}
#endif
