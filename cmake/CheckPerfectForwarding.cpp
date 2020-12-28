#if 0

#include <string>
#include <utility>

struct MyType
{
	MyType(int&& i, std::string&& s, float&& f) :
		intArg(std::move(i)), stringArg(std::move(s)), floatArg(std::move(f))
	{
	}

	int intArg;
	std::string stringArg;
	float floatArg;
};

template<typename... TArgs>
static void TestForwarding(TArgs&&... args)
{
	MyType type(std::forward<TArgs>(args)...);
}

int main(int argc, char** argv)
{
	TestForwarding(3, "hello world", 42.1234f);
}
#else

#include <utility>
#include <variant>

namespace mh
{
	template<typename TValue, typename TError> class expected;

	namespace detail::error::expected_hpp
	{
		struct expect_t
		{
			explicit constexpr expect_t() = default;
		};
		struct unexpect_t
		{
			explicit constexpr unexpect_t() = default;
		};
	}

	inline constexpr detail::error::expected_hpp::expect_t expect;
	inline constexpr static detail::error::expected_hpp::unexpect_t unexpect;

	template<typename TValue, typename TError>
	class expected final
	{
	public:
		using expect_t = detail::error::expected_hpp::expect_t;
		using unexpect_t = detail::error::expected_hpp::unexpect_t;

		static constexpr size_t VALUE_IDX = 0;
		static constexpr size_t ERROR_IDX = 1;

		using this_type = expected<TValue, TError>;
		using value_type = TValue;
		using error_type = TError;

		constexpr bool has_value() const { return m_State.index() == VALUE_IDX; }
		constexpr operator bool() const { return has_value(); }

		constexpr value_type& value() { return std::get<VALUE_IDX>(m_State); }
		constexpr const value_type& value() const { return std::get<VALUE_IDX>(m_State); }

		constexpr error_type& error() { return std::get<ERROR_IDX>(m_State); }
		constexpr const error_type& error() const { return std::get<ERROR_IDX>(m_State); }

		template<size_t I, typename... TArgs>
		void arg_eater(TArgs&&... args)
		{
		}

		template<typename... TArgs>
		value_type& emplace(expect_t, TArgs&&... args)
		{
			//m_State.emplace<0>(std::forward<TArgs>(args)...);
			arg_eater<0>(std::forward<TArgs>(args)...);
			return value();
		}

		template<typename... TArgs>
		error_type& emplace(unexpect_t, TArgs&&... args)
		{
			m_State.emplace<ERROR_IDX>(std::forward<TArgs>(args)...);
			return error();
		}

		std::variant<value_type, error_type> m_State;
	};
}

int main(int argc, char** argv)
{
	mh::expected<float, int> test;
	test.emplace(mh::expect, 42.0f);
	test.emplace(mh::unexpect, -1);
}

#endif
