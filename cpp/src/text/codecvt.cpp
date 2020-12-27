#include "mh/text/codecvt.hpp"

template std::string mh::change_encoding<char, char>(const std::string_view&);
template std::string mh::change_encoding<char, wchar_t>(const std::wstring_view&);

template std::wstring mh::change_encoding<wchar_t, char>(const std::string_view&);
template std::wstring mh::change_encoding<wchar_t, wchar_t>(const std::wstring_view&);

#if __cpp_unicode_characters >= 200704
template std::u16string mh::change_encoding<char16_t, char>(const std::string_view&);
template std::u16string mh::change_encoding<char16_t, wchar_t>(const std::wstring_view&);
template std::u16string mh::change_encoding<char16_t, char16_t>(const std::u16string_view&);
template std::u16string mh::change_encoding<char16_t, char32_t>(const std::u32string_view&);

template std::u32string mh::change_encoding<char32_t, char>(const std::string_view&);
template std::u32string mh::change_encoding<char32_t, wchar_t>(const std::wstring_view&);
template std::u32string mh::change_encoding<char32_t, char16_t>(const std::u16string_view&);
template std::u32string mh::change_encoding<char32_t, char32_t>(const std::u32string_view&);

template std::string mh::change_encoding<char, char16_t>(const std::u16string_view&);
template std::string mh::change_encoding<char, char32_t>(const std::u32string_view&);

template std::wstring mh::change_encoding<wchar_t, char16_t>(const std::u16string_view&);
template std::wstring mh::change_encoding<wchar_t, char32_t>(const std::u32string_view&);

#if __cpp_char8_t >= 201811
template std::string mh::change_encoding<char, char8_t>(const std::u8string_view&);

template std::wstring mh::change_encoding<wchar_t, char8_t>(const std::u8string_view&);

template std::u8string mh::change_encoding<char8_t, char>(const std::string_view&);
template std::u8string mh::change_encoding<char8_t, wchar_t>(const std::wstring_view&);
template std::u8string mh::change_encoding<char8_t, char8_t>(const std::u8string_view&);
template std::u8string mh::change_encoding<char8_t, char16_t>(const std::u16string_view&);
template std::u8string mh::change_encoding<char8_t, char32_t>(const std::u32string_view&);

template std::u16string mh::change_encoding<char16_t, char8_t>(const std::u8string_view&);

template std::u32string mh::change_encoding<char32_t, char8_t>(const std::u8string_view&);
#endif
#endif
