#pragma once

#if (__cpp_lib_three_way_comparison >= 201907) && (__cpp_impl_three_way_comparison >= 201907)
#include <compare>
#endif
#include <cstddef>
#include <cstdlib>
#include <memory>

namespace mh
{
	class buffer final
	{
	public:
		buffer() = default;
		buffer(buffer&& other);
		explicit buffer(const buffer& other);
		explicit buffer(size_t initialSize);
		buffer(const std::byte* ptr, size_t bytes);

		void resize(size_t newSize);
		bool reserve(size_t minSize);

#if (__cpp_lib_three_way_comparison >= 201907) && (__cpp_impl_three_way_comparison >= 201907)
		std::strong_ordering operator<=>(const buffer& other) const;
#endif

		void clear();
		size_t size() const { return m_Size; }

		std::byte* data() { return m_Data.get(); }
		const std::byte* data() const { return m_Data.get(); }

	private:
		struct free_deleter final { void operator()(std::byte* p) { free(p); } };

		std::unique_ptr<std::byte, free_deleter> m_Data;
		size_t m_Size = 0;
	};
}

#ifndef MH_COMPILE_LIBRARY
#include "buffer.inl"
#endif
