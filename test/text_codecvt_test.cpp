#include <catch2/catch_all.hpp>

#include <mh/text/codecvt.hpp>

#include <cstdint>
#include <locale>
#include <stdexcept>
#include <string>

using namespace std::string_view_literals;

namespace
{
	struct global_locale_restorer
	{
		std::locale previous = std::locale();
		~global_locale_restorer() { std::locale::global(previous); }
	};

	// The char <-> wchar_t conversions use the C library's mbrtowc/wcrtomb,
	// which follow the global locale. A UTF-8 locale makes multi-byte inputs
	// (and their error cases) deterministic.
	std::locale try_get_utf8_locale()
	{
		try
		{
			return std::locale("C.UTF-8");
		}
		catch (const std::exception&)
		{
			try
			{
				return std::locale("en_US.UTF-8");
			}
			catch (const std::exception&)
			{
				return std::locale::classic();
			}
		}
	}

#define MH_TEST_REQUIRE_UTF8_LOCALE() \
	const std::locale utf8_locale = try_get_utf8_locale(); \
	if (utf8_locale == std::locale::classic()) \
		SKIP("no UTF-8 locale available on this system"); \
	global_locale_restorer locale_restorer; \
	std::locale::global(utf8_locale)
}

template<typename T>
static void RequireEqual(const std::basic_string_view<T>& a, const std::basic_string_view<T>& b)
{
	std::vector<int64_t> araw(a.begin(), a.end()), braw(b.begin(), b.end());
	CAPTURE(araw, braw);

	REQUIRE(araw == braw);
}

template<typename T1, typename T2>
static void CompareExpected(const std::basic_string_view<T1>& v1, const std::basic_string_view<T2>& v2)
{
	RequireEqual<T2>(mh::change_encoding<T2>(v1), v2);
	RequireEqual<T1>(v1, mh::change_encoding<T1>(v2));
}

namespace
{
	struct dummy {};
}

#if MH_HAS_CHAR8
#define U8_SV(str) u8 ## str
#define U8_SV_REF const std::u8string_view&
#else
#define U8_SV(str) {}
#define U8_SV_REF const dummy&
#endif

#if MH_HAS_UNICODE
#define U16_SV(str) u ## str
#define U32_SV(str) U ## str
#define U16_SV_REF const std::u16string_view&
#define U32_SV_REF const std::u32string_view&
#else
#define U16_SV(str) {}
#define U32_SV(str) {}
#define U16_SV_REF const dummy&
#define U32_SV_REF const dummy&
#endif

static void CompareExpected3([[maybe_unused]] U8_SV_REF v1, [[maybe_unused]] U16_SV_REF v2, [[maybe_unused]] U32_SV_REF v3)
{
#if MH_HAS_UNICODE
#if MH_HAS_CHAR8
	CompareExpected(v1, v2);

	CompareExpected(v1, v3);
#endif

	CompareExpected(v2, v3);
#endif
}

TEST_CASE("change_encoding fundamental", "[mh][text][codecvt][change_encoding]")
{
#define COMPARE_EXPECTED_3(str) CompareExpected3(U8_SV(str), U16_SV(str), U32_SV(str))

	COMPARE_EXPECTED_3("\U00010348");
	COMPARE_EXPECTED_3("\u0024");
	COMPARE_EXPECTED_3("\u00a2");
	COMPARE_EXPECTED_3("\u0939");
	COMPARE_EXPECTED_3("\u20ac");
	COMPARE_EXPECTED_3("\ud55c");
	COMPARE_EXPECTED_3("😐");
}

template<typename TConvertTo, typename TInput>
static void CompareRoundtrip(const std::basic_string_view<TInput>& val)
{
	const auto converted = mh::change_encoding<TConvertTo>(val);
	const auto convertedBack = mh::change_encoding<TInput>(converted);

	REQUIRE(convertedBack.size() == val.size());
	for (size_t i = 0; i < val.size(); i++)
	{
		CAPTURE(i);
		REQUIRE(((int64_t)convertedBack.at(i)) == ((int64_t)val.at(i)));
	}
}

template<typename T>
static void CompareStringsAll(const std::basic_string_view<T>& val)
{
#if MH_HAS_CHAR8
	CompareRoundtrip<char8_t>(val);
#endif

#if MH_HAS_UNICODE
	CompareRoundtrip<char16_t>(val);
	CompareRoundtrip<char32_t>(val);
#endif
}

#if MH_HAS_CHAR8
TEST_CASE("change_encoding roundtrip - u8", "[mh][text][codecvt][change_encoding]")
{
	constexpr const std::u8string_view value_u8 = u8"😐";
	CompareStringsAll(value_u8);
}
#endif

#if MH_HAS_UNICODE
TEST_CASE("change_encoding roundtrip - u16/u32", "[mh][text][codecvt][change_encoding]")
{
	constexpr const std::u16string_view value_u16 = u"😐";
	constexpr const std::u32string_view value_u32 = U"😐";
	CompareStringsAll(value_u16);
	CompareStringsAll(value_u32);
}
#endif

#if MH_HAS_CHAR8 && MH_HAS_CUCHAR
TEST_CASE("change_encoding - char <--> char8_t", "[mh][text][codecvt][change_encoding]")
{
	{
		auto u8 = u8"this is a test!"sv;
		auto c = "this is a test!"sv;

		auto u8_2_c = mh::change_encoding<char>(u8);
		auto c_2_u8 = mh::change_encoding<char8_t>(c);
		REQUIRE(u8_2_c == c);
		REQUIRE(c_2_u8 == u8);
	}
}
#endif

#if MH_HAS_UNICODE
TEST_CASE("change_encoding - invalid scalar values are rejected", "[mh][text][codecvt][change_encoding]")
{
	// Scalar values above U+10FFFF cannot be encoded; they must raise a clear
	// error instead of silently producing corrupt output
	for (const char32_t cp : { char32_t(0x110000), char32_t(0xFFFFFFFF) })
	{
		const auto cpValue = static_cast<uint32_t>(cp);
		CAPTURE(cpValue);

		const std::u32string input(1, cp);
		REQUIRE_THROWS_AS(mh::change_encoding<char16_t>(input), std::invalid_argument);
#if MH_HAS_CHAR8
		REQUIRE_THROWS_AS(mh::change_encoding<char8_t>(input), std::invalid_argument);
#endif
	}

	// Lone surrogates are not valid scalar values; encoding them to UTF-16
	// must not silently produce a corrupt (or underflowed) code unit sequence
	for (const char32_t cp : { char32_t(0xD800), char32_t(0xDBFF), char32_t(0xDC00), char32_t(0xDFFF) })
	{
		const auto cpValue = static_cast<uint32_t>(cp);
		CAPTURE(cpValue);

		REQUIRE_THROWS_AS(mh::change_encoding<char16_t>(std::u32string(1, cp)), std::invalid_argument);
	}
}

TEST_CASE("change_encoding - boundary code points round-trip", "[mh][text][codecvt][change_encoding]")
{
	// Valid code points at the boundaries of the encoding ranges - including
	// supplementary-plane values whose low 16 bits happen to look like
	// surrogates (e.g. U+1D800) - must encode and decode unchanged
	constexpr char32_t BOUNDARY_CODE_POINTS[] = {
		0x7F, 0x80, 0x7FF, 0x800, 0xD7FF, 0xE000, 0xFFFF,
		0x10000, 0x1D800, 0x1DC00, 0x2D800, 0x10FFFF,
	};

	for (const char32_t cp : BOUNDARY_CODE_POINTS)
	{
		const auto cpValue = static_cast<uint32_t>(cp);
		CAPTURE(cpValue);

		const std::u32string original(1, cp);

		const std::u16string asU16 = mh::change_encoding<char16_t>(original);
		REQUIRE(mh::change_encoding<char32_t>(asU16) == original);

#if MH_HAS_CHAR8
		const std::u8string asU8 = mh::change_encoding<char8_t>(original);
		REQUIRE(mh::change_encoding<char32_t>(asU8) == original);
#endif
	}

	// single-unit vs surrogate-pair boundary
	CHECK(mh::change_encoding<char16_t>(std::u32string(1, char32_t(0xFFFF))).size() == 1);
	CHECK(mh::change_encoding<char16_t>(std::u32string(1, char32_t(0x10000))).size() == 2);
	CHECK(mh::change_encoding<char16_t>(std::u32string(1, char32_t(0x10FFFF))).size() == 2);
}
#endif // MH_HAS_UNICODE

TEST_CASE("change_encoding - identity conversions", "[mh][text][codecvt][change_encoding]")
{
	// To == From must produce a byte-for-byte copy, including embedded nulls
	const auto narrow = mh::change_encoding<char>("a\0b"sv);
	REQUIRE(narrow.size() == 3);
	CHECK(narrow == std::string("a\0b", 3));

	const auto wide = mh::change_encoding<wchar_t>(L"w\0x"sv);
	REQUIRE(wide.size() == 3);
	CHECK(wide == std::wstring(L"w\0x", 3));

	CHECK(mh::change_encoding<char>(""sv).empty());
}

TEST_CASE("change_encoding - char <-> wchar_t ASCII round trip", "[mh][text][codecvt][change_encoding]")
{
	// ASCII is single-byte in every locale, including the default "C" locale
	CompareExpected("Hello, world! 123"sv, L"Hello, world! 123"sv);

	// mbrtowc reports an embedded null specially (returns 0); the conversion
	// must store it and keep going instead of stopping at the null
	const auto wide = mh::change_encoding<wchar_t>("a\0b"sv);
	REQUIRE(wide.size() == 3);
	CHECK(wide == std::wstring(L"a\0b", 3));

	// ...and a null wide character must survive the trip back
	const auto narrow = mh::change_encoding<char>(L"a\0b"sv);
	REQUIRE(narrow.size() == 3);
	CHECK(narrow == std::string("a\0b", 3));

	CHECK(mh::change_encoding<wchar_t>(""sv).empty());
	CHECK(mh::change_encoding<char>(L""sv).empty());
}

TEST_CASE("change_encoding - char <-> wchar_t multi-byte characters", "[mh][text][codecvt][change_encoding]")
{
	MH_TEST_REQUIRE_UTF8_LOCALE();

	// UTF-8 -> wide: multi-byte sequences decode to single wide characters
	const auto wide = mh::change_encoding<wchar_t>("h\xC3\xA9llo"sv); // "hello" with e-acute
	REQUIRE(wide.size() == 5);
	CHECK(wide == L"h\u00E9llo");

	// wide -> UTF-8: every input character must be consumed exactly once.
	// regression: the conversion advanced the input iterator by the number of
	// OUTPUT bytes wcrtomb produced, silently dropping input characters
	// whenever a character encoded to more than one byte
	const auto narrow = mh::change_encoding<char>(L"\u00E9\u00E9"sv);
	REQUIRE(narrow.size() == 4);
	CHECK(narrow == "\xC3\xA9\xC3\xA9");

	// pre-fix, a character encoding to 3+ bytes advanced the iterator PAST the
	// end of the input, so the loop ran off the end of the string
	const auto euro = mh::change_encoding<char>(L"\u20AC"sv);
	REQUIRE(euro.size() == 3);
	CHECK(euro == "\xE2\x82\xAC");

	// mixed 1- and 2-byte characters round trip
	CompareExpected("h\xC3\xA9llo w\xC3\xB6rld"sv, L"h\u00E9llo w\u00F6rld"sv);
}

TEST_CASE("change_encoding - char -> wchar_t error paths", "[mh][text][codecvt][change_encoding]")
{
	MH_TEST_REQUIRE_UTF8_LOCALE();

	// A truncated (but so far valid) multi-byte sequence: mbrtowc returns -2
	CHECK_THROWS_AS(mh::change_encoding<wchar_t>("\xC3"sv), std::invalid_argument);
	CHECK_THROWS_AS(mh::change_encoding<wchar_t>("h\xC3"sv), std::invalid_argument);

	// Bytes that can never begin a UTF-8 character: mbrtowc returns -1
	CHECK_THROWS_AS(mh::change_encoding<wchar_t>("\xFF"sv), std::runtime_error);
	CHECK_THROWS_AS(mh::change_encoding<wchar_t>("\x80"sv), std::runtime_error);
}

TEST_CASE("change_encoding - wchar_t -> char error paths", "[mh][text][codecvt][change_encoding]")
{
	MH_TEST_REQUIRE_UTF8_LOCALE();

	// Lone surrogates are not encodable in UTF-8: wcrtomb returns -1
	const std::wstring surrogate(1, wchar_t(0xD800));
	CHECK_THROWS_AS(mh::change_encoding<char>(surrogate), std::invalid_argument);
}

#if MH_HAS_UNICODE
#ifdef MH_COMPILE_LIBRARY
// In compiled-library mode the detail helpers are compiled into the library but
// only declared by codecvt.inl, which consumers never include. In header-only
// mode the definitions are already visible through codecvt.hpp.
namespace mh::detail::codecvt_hpp
{
#if MH_HAS_CHAR8
	size_t convert_to_uc(char32_t in, std::basic_string<char8_t>& out);
#endif
	size_t convert_to_uc(char32_t in, std::basic_string<char16_t>& out);
	size_t convert_to_uc(char32_t in, std::basic_string<char32_t>& out);
}
#endif

#if MH_HAS_CHAR8
TEST_CASE("codecvt detail - convert_to_uc appends UTF-8", "[mh][text][codecvt]")
{
	using mh::detail::codecvt_hpp::convert_to_uc;

	std::u8string out;
	CHECK(convert_to_uc(U'$', out) == 1); // '$', 1 byte
	CHECK(convert_to_uc(char32_t(0x00A2), out) == 2); // cent sign, 2 bytes
	CHECK(convert_to_uc(char32_t(0x20AC), out) == 3); // euro sign, 3 bytes
	CHECK(convert_to_uc(U'\U00010348', out) == 4); // Gothic hwair, 4 bytes

	// return values are the appended lengths; the string accumulates
	CHECK(out == u8"$\u00A2\u20AC\U00010348");

	// scalar values above U+10FFFF are not encodable
	std::u8string reject;
	CHECK_THROWS_AS(convert_to_uc(char32_t(0x110000), reject), std::invalid_argument);
	CHECK(reject.empty());
}
#endif

TEST_CASE("codecvt detail - convert_to_uc appends UTF-16", "[mh][text][codecvt]")
{
	using mh::detail::codecvt_hpp::convert_to_uc;

	std::u16string out;
	CHECK(convert_to_uc(U'$', out) == 1); // BMP: single unit
	CHECK(convert_to_uc(U'\U00010348', out) == 2); // supplementary: surrogate pair

	REQUIRE(out.size() == 3);
	CHECK(out[0] == u'$');
	CHECK(out[1] == char16_t(0xD800)); // high surrogate of U+10348
	CHECK(out[2] == char16_t(0xDF48)); // low surrogate of U+10348

	// surrogate code points and values above U+10FFFF are not encodable
	std::u16string reject;
	CHECK_THROWS_AS(convert_to_uc(char32_t(0xD800), reject), std::invalid_argument);
	CHECK_THROWS_AS(convert_to_uc(char32_t(0x110000), reject), std::invalid_argument);
	CHECK(reject.empty());
}

TEST_CASE("codecvt detail - convert_to_uc appends UTF-32", "[mh][text][codecvt]")
{
	using mh::detail::codecvt_hpp::convert_to_uc;

	std::u32string out;
	CHECK(convert_to_uc(U'$', out) == 1);
	CHECK(convert_to_uc(U'\U0001F600', out) == 1); // always exactly one unit

	CHECK(out == U"$\U0001F600");
}
#endif // MH_HAS_UNICODE
