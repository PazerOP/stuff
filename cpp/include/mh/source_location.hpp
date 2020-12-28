#pragma once

#include <cstdint>

#if __has_include(<source_location>)
#include <source_location>
#endif

namespace mh
{
#if __cpp_lib_source_location >= 201907

	using source_location = std::source_location;
#define MH_SOURCE_LOCATION_CURRENT() ::mh::source_location::current()

#else

	class source_location final
	{
	public:
		constexpr source_location() noexcept = default;
		constexpr source_location(std::uint_least32_t line, const char* fileName, const char* functionName) noexcept :
			m_Line(line), m_FileName(fileName), m_FunctionName(functionName)
		{
		}

		constexpr std::uint_least32_t line() const noexcept { return m_Line; }
		constexpr std::uint_least32_t column() const noexcept { return 0; } // unknown
		constexpr const char* file_name() const noexcept { return m_FileName; }
		constexpr const char* function_name() const noexcept { return m_FunctionName; }

	private:
		std::uint_least32_t m_Line{};
		const char* m_FileName = nullptr;
		const char* m_FunctionName = nullptr;
	};

#define MH_SOURCE_LOCATION_CURRENT() ::mh::source_location(__LINE__, __FILE__, __func__)

#endif
}

#if __has_include(<mh/text/format.hpp>)
#include <mh/text/format.hpp>

#if MH_FORMATTER != MH_FORMATTER_NONE
template<typename CharT>
struct mh::formatter<mh::source_location, CharT>
{
	static constexpr CharT PRES_PATH_FULL = 'P';
	static constexpr CharT PRES_PATH_SHORT = 'p';
	static constexpr CharT PRES_LINE = 'l';
	static constexpr CharT PRES_FUNCTION = 'f';

	enum class PathPresentation
	{
		None,
		Short,
		Full,

	} m_Path = PathPresentation::Short;

	bool m_Line = true;
	bool m_Function = true;

	constexpr auto parse(const basic_format_parse_context<CharT>& ctx)
	{
		auto it = ctx.begin();
		const auto end = ctx.end();
		if (it != end)
		{
			if (*it != '}')
			{
				m_Path = PathPresentation::None;
				m_Line = false;
				m_Function = false;
			}

			for (; it != end; ++it)
			{
				if (*it == PRES_PATH_FULL || *it == PRES_PATH_SHORT)
				{
					if (m_Path != PathPresentation::None)
					{
						throw format_error(mh::format("Path presentation was already set: '{}' and '{}' are mutually exclusive",
							PRES_PATH_SHORT, PRES_PATH_FULL));
					}

					if (*it == PRES_PATH_SHORT)
						m_Path = PathPresentation::Short;
					else
						m_Path = PathPresentation::Full;
				}
				else if (*it == PRES_LINE)
					m_Line = true;
				else if (*it == PRES_FUNCTION)
					m_Function = true;
				else if (*it == '}')
					return it;
				else
					throw format_error(mh::format("Unexpected character '{}' in formatting string", *it));
			}

			throw format_error("Unexpected end of format string when looking for '}'");
		}

		return it;
	}

	template<typename FormatContext>
	auto format(const mh::source_location& loc, FormatContext& ctx)
	{
		const auto path = [&]() -> std::string_view
		{
			if (m_Path == PathPresentation::None)
			{
				return {};
			}
			else if (m_Path == PathPresentation::Short)
			{
				const auto view = std::string_view(loc.file_name());
				const auto found = view.find_last_of("\\/");
				if (found == view.npos)
					return view;

				return view.substr(found + 1);
			}
			else if (m_Path == PathPresentation::Full)
			{
				return loc.file_name();
			}
			else
			{
				throw format_error(mh::format("Unexpected PathPresentation value {}",
					+std::underlying_type_t<PathPresentation>(m_Path)));
			}
		}();

		char fmtStrBuf[64];
		fmtStrBuf[0] = '\0';
		size_t fmtStrPos = 0;

		if (!path.empty())
		{
			fmtStrBuf[fmtStrPos++] = '{';
			fmtStrBuf[fmtStrPos++] = '0';
			fmtStrBuf[fmtStrPos++] = '}';
		}

		if (m_Line)
		{
			fmtStrBuf[fmtStrPos++] = '(';
			fmtStrBuf[fmtStrPos++] = '{';
			fmtStrBuf[fmtStrPos++] = '1';
			fmtStrBuf[fmtStrPos++] = '}';
			fmtStrBuf[fmtStrPos++] = ')';
		}

		if (m_Function)
		{
			if (fmtStrPos != 0)
				fmtStrBuf[fmtStrPos++] = ':';

			fmtStrBuf[fmtStrPos++] = '{';
			fmtStrBuf[fmtStrPos++] = '2';
			fmtStrBuf[fmtStrPos++] = '}';
		}

		fmtStrBuf[fmtStrPos] = '\0';

		return format_to(ctx.out(), fmtStrBuf, path, loc.line(), loc.function_name());
	}
};

extern template struct mh::formatter<mh::source_location, char>;
extern template struct mh::formatter<mh::source_location, wchar_t>;
#endif
#endif
