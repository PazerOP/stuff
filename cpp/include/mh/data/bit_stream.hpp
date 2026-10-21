#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace mh
{
	// A little-endian, LSB-first bit stream over memory that the caller owns.
	//
	// Errors are sticky, as in a zstd or Iris bit stream. An access that would
	// leave the buffer reads zero or writes nothing, and sets failed(). No
	// access touches memory outside the buffer. The read and write paths make no
	// call and cannot throw, so a compiler can keep the position in a register
	// across a loop.
	namespace detail::bit_stream_hpp
	{
		template<unsigned N> using uint_for = std::conditional_t<(N <= 8), std::uint8_t,
			std::conditional_t<(N <= 16), std::uint16_t, std::conditional_t<(N <= 32), std::uint32_t, std::uint64_t>>>;

		inline std::uint64_t load64(const std::byte* p) noexcept
		{
			std::uint64_t v;
			std::memcpy(&v, p, sizeof(v));
			return v;
		}
		inline void store64(std::byte* p, std::uint64_t v) noexcept { std::memcpy(p, &v, sizeof(v)); }

		constexpr std::uint64_t low_mask(unsigned bits) noexcept { return ~std::uint64_t(0) >> (64 - bits); }

		template<class T> using bits_of = uint_for<sizeof(T) * 8>;

		static_assert(std::endian::native == std::endian::little, "mh::bit_reader and mh::bit_writer assume a little-endian target");
	}

	class bit_reader
	{
	public:
		bit_reader() noexcept = default;
		bit_reader(const void* data, std::size_t size_bytes) noexcept :
			m_Data(static_cast<const std::byte*>(data)),
			m_EndBit(size_bytes * 8),
			m_WordEndBit(size_bytes >= 9 ? (size_bytes - 8) * 8 : 0)
		{
		}

		std::size_t position() const noexcept { return m_Pos; }
		std::size_t size_bits() const noexcept { return m_EndBit; }
		std::size_t remaining_bits() const noexcept { return m_EndBit - m_Pos; }

		bool failed() const noexcept { return m_Failed; }
		void throw_if_failed() const
		{
			if (m_Failed)
				throw std::out_of_range("mh::bit_reader: an access went past the end of the buffer");
		}

		// A position past the end fails and moves to the end.
		void seek(std::size_t bit) noexcept
		{
			if (bit > m_EndBit) [[unlikely]]
			{
				m_Failed = true;
				bit = m_EndBit;
			}
			m_Pos = bit;
		}
		void skip(std::size_t bits) noexcept { seek(m_Pos + bits); }

		// Reads N bits, with N known at compile time.
		template<unsigned N> detail::bit_stream_hpp::uint_for<N> read() noexcept
		{
			static_assert(N >= 1 && N <= 64);
			using namespace detail::bit_stream_hpp;
			// One compare: a position below m_WordEndBit has several bytes after its byte, which holds any field of 64 bits or fewer.
			if (m_Pos < m_WordEndBit) [[likely]]
			{
				const std::byte* p = m_Data + (m_Pos >> 3);
				const unsigned off = unsigned(m_Pos & 7);
				std::uint64_t v = load64(p) >> off;
				if constexpr (N > 57)
				{
					if (off + N > 64)
						v |= std::uint64_t(std::to_integer<std::uint8_t>(p[8])) << (64 - off);
				}
				m_Pos += N;
				return uint_for<N>(v & low_mask(N));
			}
			return uint_for<N>(read_tail(N));
		}

		// Any other count fails and reads zero.
		std::uint64_t read(unsigned bits) noexcept
		{
			using namespace detail::bit_stream_hpp;
			if (m_Pos < m_WordEndBit && bits - 1u < 64u) [[likely]]
			{
				const std::byte* p = m_Data + (m_Pos >> 3);
				const unsigned off = unsigned(m_Pos & 7);
				std::uint64_t v = load64(p) >> off;
				if (off + bits > 64) [[unlikely]]
					v |= std::uint64_t(std::to_integer<std::uint8_t>(p[8])) << (64 - off);
				m_Pos += bits;
				return v & low_mask(bits);
			}
			return read_tail(bits);
		}

		bool read_bit() noexcept { return read<1>() != 0; }

		// Reads all the bits of an integral, enum or floating point T.
		template<class T> T read_as() noexcept
		{
			static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= 8);
			using U = detail::bit_stream_hpp::bits_of<T>;
			return std::bit_cast<T>(U(read<sizeof(T) * 8>()));
		}

	private:
		// The last bytes of the buffer, and every bad request. Loads stay inside the buffer.
		std::uint64_t read_tail(unsigned bits) noexcept
		{
			if (bits - 1u >= 64u || m_Pos + bits > m_EndBit)
			{
				m_Failed = true;
				m_Pos = m_EndBit;
				return 0;
			}
			const std::byte* p = m_Data + (m_Pos >> 3);
			const unsigned off = unsigned(m_Pos & 7);
			const unsigned n = (off + bits + 7) / 8;
			std::uint64_t lo = 0;
			for (unsigned i = 0; i < n && i < 8; i++)
				lo |= std::uint64_t(std::to_integer<std::uint8_t>(p[i])) << (8 * i);
			std::uint64_t v = lo >> off;
			if (n > 8)
				v |= std::uint64_t(std::to_integer<std::uint8_t>(p[8])) << (64 - off);
			m_Pos += bits;
			return v & detail::bit_stream_hpp::low_mask(bits);
		}

		const std::byte* m_Data = nullptr;
		std::size_t m_EndBit = 0;
		std::size_t m_WordEndBit = 0;
		std::size_t m_Pos = 0;
		bool m_Failed = false;
	};

	// The bits collect in a 64-bit register, and a whole word goes to memory every few bits. Call flush() before the bytes are read. flush() writes the last
	// partial byte whole, so the bits after the cursor in that byte become zero.
	class bit_writer
	{
	public:
		bit_writer() noexcept = default;
		bit_writer(void* data, std::size_t size_bytes) noexcept :
			m_Data(static_cast<std::byte*>(data)), m_Out(m_Data), m_End(m_Data + size_bytes)
		{
		}
		~bit_writer() noexcept { flush(); }
		bit_writer(const bit_writer&) = delete;
		bit_writer& operator=(const bit_writer&) = delete;

		std::size_t position() const noexcept { return std::size_t(m_Out - m_Data) * 8 + m_AccBits; }
		std::size_t size_bits() const noexcept { return std::size_t(m_End - m_Data) * 8; }

		bool failed() const noexcept { return m_Failed; }
		void throw_if_failed() const
		{
			if (m_Failed)
				throw std::out_of_range("mh::bit_writer: a write went past the end of the buffer");
		}

		// Moves the cursor. The bits before it in its byte are kept. A position past the end fails and moves to the end.
		void seek(std::size_t bit) noexcept
		{
			if (bit > size_bits()) [[unlikely]]
			{
				m_Failed = true;
				bit = size_bits();
			}
			flush();
			m_Out = m_Data + bit / 8;
			m_AccBits = unsigned(bit % 8);
			m_Acc = m_AccBits ? (std::to_integer<std::uint64_t>(*m_Out) & detail::bit_stream_hpp::low_mask(m_AccBits)) : 0;
		}

		template<unsigned N> void write(std::uint64_t value) noexcept
		{
			static_assert(N >= 1 && N <= 64);
			if constexpr (N == 64)
				put(value, 64);
			else
				put(value & detail::bit_stream_hpp::low_mask(N), N);
		}

		// Any other count fails and writes nothing.
		void write(std::uint64_t value, unsigned bits) noexcept
		{
			if (bits - 1u >= 64u) [[unlikely]]
			{
				m_Failed = true;
				return;
			}
			put(value & detail::bit_stream_hpp::low_mask(bits), bits);
		}

		void write_bit(bool value) noexcept { put(value, 1); }

		template<class T> void write_as(T value) noexcept
		{
			static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= 8);
			using U = detail::bit_stream_hpp::bits_of<T>;
			put(std::uint64_t(std::bit_cast<U>(value)), sizeof(T) * 8);
		}

		// Writes the bits still in the register. The cursor does not move.
		void flush() noexcept
		{
			if (!m_AccBits)
				return;
			const unsigned n = (m_AccBits + 7) / 8;
			for (unsigned i = 0; i < n; i++)
				m_Out[i] = std::byte(std::uint8_t(m_Acc >> (8 * i)));
			// The partial byte stays in the register, so a later write continues it.
			const unsigned whole = m_AccBits / 8;
			m_Out += whole;
			m_Acc = whole < 8 ? (m_Acc >> (8 * whole)) : 0;
			m_AccBits -= whole * 8;
		}

	private:
		// value has no bits above the low `bits`.
		void put(std::uint64_t value, unsigned bits) noexcept
		{
			// One compare: several bytes after m_Out hold any word this call can store, and the end check is the same compare.
			if (m_End - m_Out >= 16) [[likely]]
			{
				m_Acc |= value << m_AccBits;
				m_AccBits += bits;
				if (m_AccBits >= 64)
				{
					detail::bit_stream_hpp::store64(m_Out, m_Acc);
					m_Out += 8;
					m_AccBits -= 64;
					// The bits of value that did not fit in the stored word. A shift by a set amount is undefined, so a value that fit exactly leaves zero.
					m_Acc = m_AccBits ? (value >> (bits - m_AccBits)) : 0;
				}
				return;
			}
			put_tail(value, bits);
		}

		// The last several bytes of the buffer: an exact bounds check, then one byte at a time.
		void put_tail(std::uint64_t value, unsigned bits) noexcept
		{
			if (position() + bits > size_bits())
			{
				m_Failed = true;
				return;
			}
			// The register can hold a bounded number of bits from the word path.
			flush();
			for (unsigned i = 0; i < bits; i++)
			{
				m_Acc |= ((value >> i) & 1) << m_AccBits;
				if (++m_AccBits == 8)
				{
					*m_Out++ = std::byte(std::uint8_t(m_Acc));
					m_Acc = 0;
					m_AccBits = 0;
				}
			}
		}

		std::byte* m_Data = nullptr;
		std::byte* m_Out = nullptr;
		std::byte* m_End = nullptr;
		std::uint64_t m_Acc = 0;
		unsigned m_AccBits = 0;
		bool m_Failed = false;
	};
}
