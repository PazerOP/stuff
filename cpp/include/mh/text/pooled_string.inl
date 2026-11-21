#ifdef MH_COMPILE_LIBRARY
#include "pooled_string.hpp"
#endif

#ifndef MH_COMPILE_LIBRARY_INLINE
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>

namespace mh::detail::pooled_string_hpp
{
	// An insert-only hash set of strings. Nothing is ever freed: a lock-free reader, or a
	// pooled_string in a static object, can hold any pointer for the life of the program.
	class pool
	{
	public:
		const char* intern(std::string_view str)
		{
			const auto hash = static_cast<std::uint32_t>(std::hash<std::string_view>()(str));

			// A hit takes no lock. A slot only changes from null to a finished string, so a reader
			// of an old table can miss a string but cannot see a bad one.
			if (const char* found = find(*m_Table.load(std::memory_order_acquire), str, hash))
				return found;

			std::lock_guard lock(m_Mutex);
			table* t = m_Table.load(std::memory_order_relaxed);
			if (const char* found = find(*t, str, hash))
				return found;

			const char* chars = allocate(str, hash);
			if (m_Count >= (t->mask + 1) / 2)
				t = grow(*t);

			insert(*t, chars, hash);
			m_Count++;
			return chars;
		}

	private:
		// Open addressing with linear probing. At most half of the slots are in use, so a probe always ends.
		struct table
		{
			table(std::size_t size, table* prev) : mask(size - 1), slots(new std::atomic<const char*>[size]()), prev(prev) {}

			std::size_t mask;
			std::atomic<const char*>* slots;
			table* prev;  // Keeps an old table reachable for a leak checker.
		};

		static const header& header_of(const char* chars) noexcept { return reinterpret_cast<const header*>(chars)[-1]; }

		static const char* find(const table& t, std::string_view str, std::uint32_t hash) noexcept
		{
			for (std::size_t i = hash;; i++)
			{
				const char* chars = t.slots[i & t.mask].load(std::memory_order_acquire);
				if (!chars)
					return nullptr;

				const header& h = header_of(chars);
				if (h.hash == hash && h.size == str.size() && !std::memcmp(chars, str.data(), str.size()))
					return chars;
			}
		}

		static void insert(table& t, const char* chars, std::uint32_t hash) noexcept
		{
			std::size_t i = hash;
			while (t.slots[i & t.mask].load(std::memory_order_relaxed))
				i++;

			t.slots[i & t.mask].store(chars, std::memory_order_release);
		}

		table* grow(const table& old)
		{
			table* t = new table((old.mask + 1) * 2, m_Table.load(std::memory_order_relaxed));
			for (std::size_t i = 0; i <= old.mask; i++)
			{
				if (const char* chars = old.slots[i].load(std::memory_order_relaxed))
					insert(*t, chars, header_of(chars).hash);
			}

			m_Table.store(t, std::memory_order_release);
			return t;
		}

		const char* allocate(std::string_view str, std::uint32_t hash)
		{
			constexpr std::size_t BLOCK_SIZE = 64 * 1024;
			constexpr std::size_t ALIGN = alignof(header);
			const std::size_t bytes = (sizeof(header) + str.size() + 1 + ALIGN - 1) & ~(ALIGN - 1);

			char* p;
			if (bytes > BLOCK_SIZE / 4)
			{
				// A long string gets its own allocation, so that the rest of the current block stays usable.
				p = static_cast<char*>(::operator new(bytes));
			}
			else
			{
				if (bytes > static_cast<std::size_t>(m_BlockEnd - m_BlockCur))
				{
					m_BlockCur = static_cast<char*>(::operator new(BLOCK_SIZE));
					m_BlockEnd = m_BlockCur + BLOCK_SIZE;
				}

				p = m_BlockCur;
				m_BlockCur += bytes;
			}

			new (p) header{ static_cast<std::uint32_t>(str.size()), hash };
			char* chars = p + sizeof(header);
			std::memcpy(chars, str.data(), str.size());
			chars[str.size()] = '\0';
			return chars;
		}

		std::atomic<table*> m_Table{ new table(1024, nullptr) };
		std::mutex m_Mutex;
		std::size_t m_Count = 0;
		char* m_BlockCur = nullptr;
		char* m_BlockEnd = nullptr;
	};

	MH_COMPILE_LIBRARY_INLINE const char* intern(std::string_view str)
	{
		if (str.empty())
			return nullptr;
		if (str.size() > std::numeric_limits<std::uint32_t>::max())
			throw std::length_error("mh::pooled_string: string is too long");

		static pool& s_Pool = *new pool;
		return s_Pool.intern(str);
	}
}
