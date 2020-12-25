#define _SILENCE_CXX20_CODECVT_FACETS_DEPRECATION_WARNING

#include <mh/io/file.hpp>

template std::basic_string<char> mh::read_file<char>(const std::filesystem::path&);
template std::basic_string<wchar_t> mh::read_file<wchar_t>(const std::filesystem::path&);
template std::basic_string<char8_t> mh::read_file<char8_t>(const std::filesystem::path&);
template std::basic_string<char16_t> mh::read_file<char16_t>(const std::filesystem::path&);
template std::basic_string<char32_t> mh::read_file<char32_t>(const std::filesystem::path&);

template void mh::write_file<char>(const std::filesystem::path&, const std::string_view&);
template void mh::write_file<wchar_t>(const std::filesystem::path&, const std::wstring_view&);
template void mh::write_file<char8_t>(const std::filesystem::path&, const std::u8string_view&);
template void mh::write_file<char16_t>(const std::filesystem::path&, const std::u16string_view&);
template void mh::write_file<char32_t>(const std::filesystem::path&, const std::u32string_view&);
