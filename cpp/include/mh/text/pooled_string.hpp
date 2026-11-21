#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string_view>

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh
{
	namespace detail::pooled_string_hpp
	{
		// Sits directly before the characters of a pooled string.
		struct header
		{
			std::uint32_t size;
			std::uint32_t hash;
		};

		// Returns the pooled copy of str, null-terminated. Returns nullptr for an empty string.
		MH_STUFF_API const char* intern(std::string_view str);
	}

	// An immutable char string (ANSI or UTF-8) that lives in one program-wide pool.
	//
	// Equal strings share one pointer. A copy, an equality test and a hash never read the characters.
	// The pool never frees a string, so a pooled_string stays valid until the program exits.
	// Construction is thread-safe.
	class pooled_string
	{
	public:
		constexpr pooled_string() noexcept = default;
		explicit pooled_string(std::string_view str) : m_Str(detail::pooled_string_hpp::intern(str)) {}

		constexpr bool empty() const noexcept { return !m_Str; }
		std::size_t size() const noexcept { return m_Str ? (reinterpret_cast<const detail::pooled_string_hpp::header*>(m_Str) - 1)->size : 0; }

		// Always null-terminated.
		constexpr const char* c_str() const noexcept { return m_Str ? m_Str : ""; }
		constexpr const char* data() const noexcept { return c_str(); }

		std::string_view view() const noexcept { return { c_str(), size() }; }
		operator std::string_view() const noexcept { return view(); }

		const char* begin() const noexcept { return c_str(); }
		const char* end() const noexcept { return c_str() + size(); }

		// The value differs from one run of the program to the next.
		std::size_t hash() const noexcept
		{
			// The multiply and the shift move entropy into the low bits, which are zero in an aligned pointer.
			const std::uint64_t x = reinterpret_cast<std::uintptr_t>(m_Str) * 0x9E3779B97F4A7C15ull;
			return static_cast<std::size_t>(x ^ (x >> 32));
		}

		friend constexpr bool operator==(pooled_string lhs, pooled_string rhs) noexcept { return lhs.m_Str == rhs.m_Str; }
		friend bool operator==(pooled_string lhs, std::string_view rhs) noexcept { return lhs.view() == rhs; }

		// Orders by the characters, not by the pointer.
		friend std::strong_ordering operator<=>(pooled_string lhs, pooled_string rhs) noexcept { return lhs.view().compare(rhs.view()) <=> 0; }

	private:
		const char* m_Str = nullptr;
	};

	template<typename Traits>
	std::basic_ostream<char, Traits>& operator<<(std::basic_ostream<char, Traits>& os, pooled_string str)
	{
		return os << std::basic_string_view<char, Traits>(str.data(), str.size());
	}
}

template<>
struct std::hash<mh::pooled_string>
{
	std::size_t operator()(mh::pooled_string str) const noexcept { return str.hash(); }
};

#ifndef MH_COMPILE_LIBRARY
#include "pooled_string.inl"
#endif
