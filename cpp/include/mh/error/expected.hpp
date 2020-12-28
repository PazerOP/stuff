#pragma once

#if __cpp_concepts >= 201907

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

	namespace detail::error::expected_hpp
	{
		template<typename TExpected, typename TFunc>
		decltype(auto) map(TExpected&& exp, TFunc&& func)
		{
			using new_value_type = std::decay_t<decltype(func(exp.value()))>;

			if constexpr (std::is_same_v<new_value_type, void>)
			{
				if (exp.has_value())
					func(exp.value());

				return std::forward<TExpected>(exp);
			}
			else
			{
				using ret_type = expected<new_value_type, typename std::decay_t<TExpected>::error_type>;
				if (exp.has_value())
					return ret_type(expect, func(exp.value()));
				else
					return ret_type(unexpect, exp.error());
			}
		}
	}

	template<typename TValue, typename TError>
	class expected final
	{
		using expect_t = detail::error::expected_hpp::expect_t;
		using unexpect_t = detail::error::expected_hpp::unexpect_t;

		static constexpr size_t VALUE_IDX = 0;
		static constexpr size_t ERROR_IDX = 1;

	public:
		using this_type = expected<TValue, TError>;
		using value_type = TValue;
		using error_type = TError;

		static_assert(!std::is_same_v<value_type, void>, "value type cannot be void");
		static_assert(!std::is_same_v<value_type, expect_t>, "value type cannot be expect_t");
		static_assert(!std::is_same_v<value_type, unexpect_t>, "value type cannot be unexpect_t");

		static_assert(!std::is_same_v<error_type, void>, "error type cannot be void");
		static_assert(!std::is_same_v<error_type, expect_t>, "error type cannot be expect_t");
		static_assert(!std::is_same_v<error_type, unexpect_t>, "error type cannot be unexpect_t");

		constexpr expected()
			noexcept(std::is_nothrow_default_constructible_v<TValue>)
			requires std::is_default_constructible_v<TValue>
		{
		}

		constexpr expected(value_type&& value) : expected(expect, std::move(value)) {}
		constexpr expected(const value_type& value) : expected(expect, value) {}
		constexpr expected(error_type&& error) : expected(unexpect, std::move(error)) {}
		constexpr expected(const error_type& error) : expected(unexpect, error) {}

		template<typename T>
		constexpr expected(T&& val)
			requires std::is_constructible_v<TError, T>
			: expected(TError(std::forward<T>(val)))
		{
		}

		constexpr expected(expect_t&, value_type&& value) : m_State(std::in_place_index_t<VALUE_IDX>{}, std::move(value)) {}
		constexpr expected(expect_t&, const value_type& value) : m_State(std::in_place_index_t<VALUE_IDX>{}, value) {}

		constexpr expected(unexpect_t&, error_type&& value) : m_State(std::in_place_index_t<ERROR_IDX>{}, std::move(value)) {}
		constexpr expected(unexpect_t&, const error_type& value) : m_State(std::in_place_index_t<ERROR_IDX>{}, value) {}

		constexpr this_type& operator=(value_type&& value)
			noexcept(noexcept(emplace(expect, std::move(value))))
		{
			emplace(expect, std::move(value));
			return *this;
		}
		constexpr this_type& operator=(const value_type& value)
			noexcept(noexcept(emplace(expect, value)))
		{
			emplace(expect, value);
			return *this;
		}
		constexpr this_type& operator=(error_type&& error)
			noexcept(noexcept(emplace(unexpect, std::move(error))))
		{
			emplace(unexpect, std::move(error));
			return *this;
		}
		constexpr this_type& operator=(const error_type& error)
			noexcept(noexcept(emplace(unexpect, error)))
		{
			emplace(unexpect, error);
			return *this;
		}

		template<typename T>
		constexpr this_type& operator=(T&& error)
			noexcept(noexcept(emplace(unexpect, std::forward<T>(error))))
			requires std::is_constructible_v<TError, T>
		{
			emplace(unexpect, error);
			return *this;
		}

		constexpr bool has_value() const { return m_State.index() == VALUE_IDX; }
		constexpr operator bool() const { return has_value(); }

		constexpr value_type& value() { return std::get<VALUE_IDX>(m_State); }
		constexpr const value_type& value() const { return std::get<VALUE_IDX>(m_State); }

		constexpr error_type& error() { return std::get<ERROR_IDX>(m_State); }
		constexpr const error_type& error() const { return std::get<ERROR_IDX>(m_State); }

		constexpr value_type* operator->() { return &value(); }
		constexpr const value_type* operator->() const { return &value(); }

		template<typename... TArgs>
		value_type& emplace(expect_t&, TArgs&&... args)
		{
			m_State.template emplace<VALUE_IDX>(std::forward<TArgs>(args)...);
			return value();
		}

		template<typename... TArgs>
		error_type& emplace(unexpect_t&, TArgs&&... args)
		{
			m_State.template emplace<ERROR_IDX>(std::forward<TArgs>(args)...);
			return error();
		}

		template<typename TFunc>
		this_type& or_else(TFunc&& func)
		{
			if (!has_value())
				func(error());

			return *this;
		}
		template<typename TFunc>
		const this_type& or_else(TFunc&& func) const
		{
			if (!has_value())
				func(error());

			return *this;
		}

		template<typename TFunc>
		decltype(auto) map(TFunc&& func) { return detail::error::expected_hpp::map(*this, std::forward<TFunc>(func)); }
		template<typename TFunc>
		decltype(auto) map(TFunc&& func) const { return detail::error::expected_hpp::map(*this, std::forward<TFunc>(func)); }

	private:
		std::variant<value_type, error_type> m_State;
	};
}

#endif
