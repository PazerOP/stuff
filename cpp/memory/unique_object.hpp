#pragma once

#ifdef __cpp_impl_three_way_comparison
#include <compare>
#endif
#include <utility>

namespace mh
{
#ifdef __cpp_concepts
	template<typename Traits, typename Object>
	concept UniqueObjectTraits = requires(Traits t, Object o)
	{
		{ t.delete_obj(o) };
		{ t.release_obj(o) } -> Object;
		{ t.is_obj_valid(o) } -> bool;
	};
#endif

	template<typename T, typename Traits>
#ifdef __cpp_concepts
	requires UniqueObjectTraits<Traits, T>
#endif
	class unique_object
	{
		using this_type = unique_object<T, Traits>;
	public:
		unique_object() : m_Object{}, m_Traits{} {}

		explicit unique_object(const T& value, const Traits& traits) :
			m_Object(value), m_Traits(traits) {}
		explicit unique_object(const T& value, Traits&& traits = {}) :
			m_Object(value), m_Traits(std::move(traits)) {}
		explicit unique_object(T&& value, const Traits& traits) :
			m_Object(std::move(value)), m_Traits(traits) {}
		explicit unique_object(T&& value, Traits&& traits) :
			m_Object(std::move(value)), m_Traits(std::move(traits)) {}

		unique_object(const this_type& other) = delete;
		this_type& operator=(const this_type& other) = delete;

		unique_object(this_type&& other) :
			m_Object(std::move(other.release())),
			m_Traits(std::move(other.m_Traits))
		{
		}
		unique_object& operator=(unique_object&& other)
		{
			reset();

			m_Object = std::move(other.release());
			m_Traits = std::move(other.m_Traits);

			return *this;
		}

		~unique_object() { m_Traits.delete_obj(m_Object); }

#ifdef __cpp_impl_three_way_comparison
		auto operator<=>(const unique_object& other) const = default;
		auto operator<=>(const T& other) const { return m_Object <=> other; }
#endif

		T release() { return m_Traits.release_obj(m_Object); }

		void reset() { m_Traits.delete_obj(m_Object); }
		T& reset_and_get_ref()
		{
			reset();
			return m_Object;
		}

		operator bool() const { return m_Traits.is_obj_valid(m_Object); }

		operator const T& () const { return m_Object; }
		const T& value() const { return m_Object; }

	private:
		T m_Object;

		#if __has_cpp_attribute(no_unique_address)
		[[no_unique_address]]
		#endif
		Traits m_Traits;
	};
}

#ifdef __cpp_impl_three_way_comparison
template<typename T, typename Traits> auto operator<=>(const T& lhs, const mh::unique_object<T, Traits>& rhs)
{
	return lhs <=> rhs.value();
}
#endif
