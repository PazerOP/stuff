#pragma once

#include <codecvt>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#undef max
#undef min

namespace mh
{
	namespace detail::reflection::enum_hpp
	{
		template<typename T, typename... TArgs> using all_defined = T;

		template<typename TSelf, typename TValue>
		class enum_type_base
		{
		public:
			using this_type = TSelf;
			using value_type = TValue;
			using underlying_type = std::underlying_type_t<value_type>;

			static constexpr std::string_view type_name()
			{
				auto typeName = TSelf::type_name_full();
				auto last = typeName.rfind("::");
				if (last == typeName.npos)
					throw std::invalid_argument("Invalid type name");

				return typeName.substr(last + 2);
			}

			static constexpr std::string_view try_find_name(value_type value)
			{
				for (size_t i = 0; i < std::size(TSelf::VALUES); i++)
				{
					if (TSelf::VALUES[i].value() == value)
						return TSelf::VALUES[i].value_name();
				}

				return std::string_view{};
			}

			static constexpr std::string_view find_name(value_type value)
			{
				auto found = try_find_name(value);
				if (found.empty())
				{
					throw std::invalid_argument(mh::format("Unable to find name for {} ({})",
						+underlying_type(value), typeid(value_type).name()));
				}

				return found;
			}

			static constexpr std::optional<value_type> try_find_value(const std::string_view& name)
			{
				for (size_t i = 0; i < std::size(TSelf::VALUES); i++)
				{
					if (TSelf::VALUES[i].value_name() == name)
						return TSelf::VALUES[i].value();
				}

				return std::nullopt;
			}

			static constexpr value_type find_value(const std::string_view& name)
			{
				auto found = try_find_value(name);
				if (!found)
				{
					throw std::invalid_argument(mh::format("Unable to find value of type {} for name \"{}\"",
						typeid(value_type).name(), name));
				}

				return *found;
			}
		};
	}

	template<typename T> class enum_type;

	template<typename T>
	class enum_value
	{
	public:
		using value_type = T;
		using underlying_value_type = std::underlying_type_t<value_type>;

		constexpr enum_value(value_type value, const std::string_view& value_name) :
			m_Value(value),
			m_ValueName(value_name)
		{
		}

		static constexpr std::string_view type_name() { return enum_type<T>::type_name(); }
		constexpr std::string_view value_name() const { return m_ValueName; }

		constexpr value_type value() const { return m_Value; }
		constexpr underlying_value_type underlying_value() const { return static_cast<underlying_value_type>(value()); }

	private:
		value_type m_Value;
		std::string_view m_ValueName;
	};

#define MH_ENUM_REFLECT_BEGIN(enumType) \
	template<> \
	class ::mh::enum_type<enumType> final : \
		public ::mh::detail::reflection::enum_hpp::enum_type_base<::mh::enum_type<enumType>, enumType> \
	{ \
	public: \
		static constexpr std::string_view type_name_full() { return #enumType; } \
		\
		static constexpr ::mh::enum_value<value_type> VALUES[] = {

#define MH_ENUM_REFLECT_VALUE(value) ::mh::enum_value{ value_type::value, #value },

#define MH_ENUM_REFLECT_END() \
		}; \
	};

	template<typename T, typename TReflect = enum_type<std::decay_t<T>>::this_type>
	inline constexpr std::string_view find_enum_value_name(T val)
	{
		return TReflect::find_name(val);
	}
	template<typename T, typename TReflect = enum_type<std::decay_t<T>>::this_type>
	inline constexpr std::string_view try_find_enum_value_name(T val)
	{
		return TReflect::try_find_name(val);
	}

	template<typename T, typename TReflect = enum_type<std::decay_t<T>>::this_type>
	inline constexpr T find_enum_value(const std::string_view& name)
	{
		return TReflect::find_value(name);
	}
	template<typename T, typename TReflect = enum_type<std::decay_t<T>>::this_type>
	inline constexpr void find_enum_value(const std::string_view& name, T& value)
	{
		value = find_enum_value<T>(name);
	}
	template<typename T, typename TReflect = enum_type<std::decay_t<T>>::this_type>
	inline constexpr std::optional<T> try_find_enum_value(const std::string_view& name)
	{
		return TReflect::try_find_value(name);
	}
	template<typename T, typename TReflect = enum_type<std::decay_t<T>>::this_type>
	inline constexpr bool try_find_enum_value(const std::string_view& name, T& value)
	{
		if (auto found = try_find_enum_value<T>(name))
		{
			value = *found;
			return true;
		}

		return false;
	}

	template<typename T, typename = typename enum_type<T>::value_type>
	struct enum_fmt_t final
	{
		using value_type = T;
		T m_Value{};
	};

	template<typename T, typename TWrapper = enum_fmt_t<T>>
	constexpr auto enum_fmt(T val)
	{
		return TWrapper{ .m_Value = val };
	}
}

#if __has_include(<mh/text/format.hpp>)
#include <mh/text/format.hpp>

#undef max
#undef min

template<typename T, typename CharT>
struct mh::formatter<::mh::enum_fmt_t<T>, CharT>
{
	static constexpr CharT PRES_TYPE_LONG = 'T';
	static constexpr CharT PRES_TYPE_SHORT = 't';
	static constexpr CharT PRES_VALUE = 'v';

	enum class TypePresentation
	{
		None,
		Short,
		Long,
	};

	TypePresentation m_Type = TypePresentation::Short;
	bool m_Value = true;

	constexpr auto parse(basic_format_parse_context<CharT>& ctx)
	{
		auto it = ctx.begin();
		const auto end = ctx.end();
		if (it != ctx.end())
		{
			if (*it != '}')
			{
				m_Type = TypePresentation::None;
				m_Value = false;
			}

			for (; it != end; ++it)
			{
				if (*it == PRES_TYPE_SHORT || *it == PRES_TYPE_LONG)
				{
					if (m_Type != TypePresentation::None)
						throw format_error("Type presentation was already set: 't' and 'T' are mututally exclusive");

					if (*it == PRES_TYPE_SHORT)
						m_Type = TypePresentation::Short;
					else
						m_Type = TypePresentation::Long;
				}
				else if (*it == PRES_VALUE)
					m_Value = true;
				else if (*it == '}')
					return it;
				else
					throw format_error("Unexpected character in formatting string");
			}
		}

		throw format_error("Unexpected end of format string when looking for '}'");
	}

	template<typename FormatContext>
	auto format(const ::mh::enum_fmt_t<T>& rc, FormatContext& ctx)
	{
		const auto valueName = ::mh::enum_type<T>::try_find_name(rc.m_Value);

		CharT fmtStr[64];
		size_t fmtStrPos = 0;

		if (m_Type == TypePresentation::Short)
		{
			fmtStr[fmtStrPos++] = '{';
			fmtStr[fmtStrPos++] = '0';
			fmtStr[fmtStrPos++] = '}';
		}
		else if (m_Type == TypePresentation::Long)
		{
			fmtStr[fmtStrPos++] = '{';
			fmtStr[fmtStrPos++] = '1';
			fmtStr[fmtStrPos++] = '}';
		}

		const bool needsSpacer = fmtStrPos != 0;

		if (m_Value && !valueName.empty())
		{
			if (needsSpacer)
			{
				fmtStr[fmtStrPos++] = ':';
				fmtStr[fmtStrPos++] = ':';
			}
			fmtStr[fmtStrPos++] = '{';
			fmtStr[fmtStrPos++] = '2';
			fmtStr[fmtStrPos++] = '}';
		}
		else
		{
			if (needsSpacer)
				fmtStr[fmtStrPos++] = '(';

			fmtStr[fmtStrPos++] = '{';
			fmtStr[fmtStrPos++] = '3';
			fmtStr[fmtStrPos++] = '}';

			if (needsSpacer)
				fmtStr[fmtStrPos++] = ')';
		}

		fmtStr[fmtStrPos] = '\0';

#if 0
		const auto get_format_string = [&]
		{
			using namespace std::string_view_literals;
			const bool withName = m_Value && !valueName.empty();

			if (m_Type == TypePresentation::None)
				return withName ? "{2}"sv : throw format_error("Invalid flags combination: result cannot be blank");
			else if (m_Type == TypePresentation::Short)
				return withName ? "{0}::{2}"sv : "{0}({3})"sv;
			else if (m_Type == TypePresentation::Long)
				return withName ? "{1}::{2}"sv : "{1}({3})"sv;
			else
				throw format_error("Invalid TypePresentation value");
		};
#endif

		if constexpr (std::is_same_v<CharT, char>)
		{
			return format_to(ctx.out(), fmtStr,
				::mh::enum_type<T>::type_name(), ::mh::enum_type<T>::type_name_full(),
				valueName, +std::underlying_type_t<T>(rc.m_Value));
		}
		else
		{
			const auto convert_string = [](const std::string_view& input)
			{
				struct design_by_committee : std::codecvt<CharT, char, std::mbstate_t> {} cvt;
				std::basic_string<CharT> converted;
				std::mbstate_t state{};

				converted.resize(cvt.length(state, input.data(), input.data() + input.size(),
					std::numeric_limits<size_t>::max()));

				state = {};

				const char* fromNext;
				CharT* toNext;
				cvt.in(state,
					input.data(), input.data() + input.size(), fromNext,
					converted.data(), converted.data() + converted.size(), toNext);

				return converted;
			};

			return format_to(ctx.out(), fmtStr,
				convert_string(::mh::enum_type<T>::type_name()), convert_string(::mh::enum_type<T>::type_name_full()),
				convert_string(valueName), +std::underlying_type_t<T>(rc.m_Value));
		}
	}
};

#endif
