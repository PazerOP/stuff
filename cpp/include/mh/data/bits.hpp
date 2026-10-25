#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <iostream>

namespace mh
{
#ifndef MH_RESTRICT
#ifdef __GNUC__
#define MH_RESTRICT __restrict__
#elif defined(_MSC_VER)
#define MH_RESTRICT __restrict
#else
#define MH_RESTRICT
#endif
#endif

	enum class int_for_bits_mode
	{
		exact,
		least,
		fast,
	};

	template<typename T> constexpr std::array<T, std::numeric_limits<std::make_unsigned_t<T>>::digits + 1> BIT_MASKS = []
	{
		using unsigned_t = std::make_unsigned_t<T>;
		constexpr unsigned_t digits = std::numeric_limits<unsigned_t>::digits;
		std::array<T, digits + 1> retVal{};

		for (unsigned_t i = 1; i <= digits; i++)
		{
			constexpr auto maxBits = sizeof(uintmax_t) * std::numeric_limits<unsigned char>::digits;
			constexpr auto mask = std::numeric_limits<uintmax_t>::max();
			retVal[i] = mask >> (maxBits - i);
		}

		return retVal;
	}();

	namespace detail::bits_hpp
	{
		static constexpr unsigned BITS_PER_BYTE = std::numeric_limits<std::underlying_type_t<std::byte>>::digits;

		template<unsigned bits, int_for_bits_mode mode>
		constexpr auto uint_for_bits_helper()
		{
			if constexpr (bits > 64)
			{
				static_assert(bits >= 64 && sizeof(uintmax_t) > 8, "Requested more bits than the platform supports");
				return uintmax_t{};
			}
			else if constexpr (bits > 32)
			{
				if constexpr (mode == int_for_bits_mode::exact)
					return uint64_t{};
				else if constexpr (mode == int_for_bits_mode::least)
					return uint_least64_t{};
				else if constexpr (mode == int_for_bits_mode::fast)
					return uint_fast64_t{};
			}
			else if constexpr (bits > 16)
			{
				if constexpr (mode == int_for_bits_mode::exact)
					return uint32_t{};
				else if constexpr (mode == int_for_bits_mode::least)
					return uint_least32_t{};
				else if constexpr (mode == int_for_bits_mode::fast)
					return uint_fast32_t{};
			}
			else if constexpr (bits > 8)
			{
				if constexpr (mode == int_for_bits_mode::exact)
					return uint16_t{};
				else if constexpr (mode == int_for_bits_mode::least)
					return uint_least16_t{};
				else if constexpr (mode == int_for_bits_mode::fast)
					return uint_fast16_t{};
			}
			else
			{
				if constexpr (mode == int_for_bits_mode::exact)
					return uint8_t{};
				else if constexpr (mode == int_for_bits_mode::least)
					return uint_least8_t{};
				else if constexpr (mode == int_for_bits_mode::fast)
					return uint_fast8_t{};
			}
		}

		template<typename T> constexpr T min(T a, T b) { return a < b ? a : b; }
		template<typename T> constexpr T min(T a, T b, T c) { return min(min(a, b), c); }
		template<typename T> constexpr T min(T a, T b, T c, T d) { return min(min(a, b), min(c, d)); }

		template<typename T> constexpr T max(T a, T b) { return a > b ? a : b; }
		template<typename T> constexpr T max(T a, T b, T c) { return max(max(a, b), c); }

		template<typename T> constexpr auto intprint(T value) { return +value; }
		constexpr auto intprint(std::byte value) { return unsigned(value); }

		template<typename TFunc> constexpr void debug([[maybe_unused]] const TFunc& f)
		{
			//if (!std::is_constant_evaluated())
			//	f();
		}

		template<unsigned shift_left, unsigned shift_right, typename T = void>
		[[nodiscard]] constexpr T shift_both(T value)
		{
			if constexpr (shift_left >= shift_right)
				return T(value << (shift_left - shift_right));
			else
				return T(value >> (shift_right - shift_left));
		}


		template<typename TDst>
		constexpr void byte_write(TDst* dst, size_t index, std::byte value)
		{
			using namespace detail::bits_hpp;
			using TDstR = std::conditional_t<std::is_same_v<TDst, std::byte>, std::underlying_type_t<std::byte>, TDst>;

			debug([&]{ std::cerr << "byte_write " << std::hex << unsigned(value) << " @ " << index << '\n'; });

			if constexpr (sizeof(TDst) == 1)
			{
				dst[index] = TDst(value);
			}
			else
			{
				const auto dst_index = index / sizeof(TDst);
				constexpr TDstR mask = BIT_MASKS<TDstR>[BITS_PER_BYTE];

				const auto bit_offset = (index % sizeof(TDst)) * BITS_PER_BYTE;
				dst[dst_index] &= ~(mask << bit_offset);
				dst[dst_index] |= TDstR(value) << bit_offset;
			}
		}

		template<typename TDst = std::byte, typename TSrc = void>
		[[nodiscard]] constexpr TDst byte_read(const TSrc* src, size_t index)
		{
			using namespace detail::bits_hpp;
			const auto real_index = index / sizeof(TSrc);
			const auto offset = (index % sizeof(TSrc)) * BITS_PER_BYTE;
			auto value = src[real_index];
			value >>= offset;
			debug([&]{ std::cerr << "byte_read 0x" << std::hex << intprint(std::byte(value)) << " @ "
				<< std::dec << index << ", real_index " << real_index << ", offset " << offset << '\n'; });
			return TDst(value);
		}

		template<typename TSrc>
		[[nodiscard]] constexpr uint_fast16_t u16_read(const TSrc* src, size_t byte_index)
		{
			using namespace detail::bits_hpp;

			uint_fast16_t retVal{};
			if (sizeof(TSrc) == sizeof(uint16_t) && (byte_index % sizeof(TSrc)) == 0)
			{
				retVal = uint_fast16_t(src[byte_index / sizeof(TSrc)]);
			}
			else
			{
				retVal = byte_read<uint_fast16_t>(src, byte_index);
				retVal |= byte_read<uint_fast16_t>(src, byte_index + 1) << BITS_PER_BYTE;
			}

			debug([&]{ std::cerr << "u16_read " << std::hex << retVal << " @ " << byte_index << '\n'; });
			return retVal;
		}

		template<typename TDst>
		constexpr void u16_write(TDst* dst, size_t byte_index, uint_fast16_t value)
		{
			using namespace detail::bits_hpp;

			debug([&]{ std::cerr << "u16_write " << std::hex << value << " @ " << byte_index << '\n'; });

			if constexpr (sizeof(TDst) == sizeof(uint16_t))
			{
				if ((byte_index % sizeof(TDst)) == 0)
				{
					dst[byte_index / sizeof(TDst)] = TDst(value);
					return;
				}
			}

			byte_write(dst, byte_index, std::byte(value));
			byte_write(dst, byte_index + 1, std::byte(value >> BITS_PER_BYTE));
		}

		template<size_t bit_count, size_t dst_offset = 0, size_t src_offset = 0, typename TDst = void, typename TSrc = void>
		[[nodiscard]] constexpr TDst bit_merge(TDst existing_bits, TSrc new_bits_source)
		{
			using namespace detail::bits_hpp;
			static_assert((bit_count + src_offset) <= (sizeof(TSrc) * BITS_PER_BYTE));
			static_assert((bit_count + dst_offset) <= (sizeof(TDst) * BITS_PER_BYTE));
			using TSrcR = std::conditional_t<std::is_same_v<TSrc, std::byte>, std::underlying_type_t<std::byte>, TSrc>;
			using TDstR = std::conditional_t<std::is_same_v<TDst, std::byte>, std::underlying_type_t<std::byte>, TDst>;

			using ct = std::make_unsigned_t<std::common_type_t<TSrcR, TDstR>>;

			ct src_value = ct(new_bits_source);
			ct dst_value = ct(existing_bits);
			debug([&]{ std::cerr << "bit_merge: src_value = " << src_value << ", " << dst_value << '\n'; });

			src_value >>= src_offset;
			src_value &= BIT_MASKS<ct>[bit_count];
			src_value <<= dst_offset;

			dst_value &= ~(BIT_MASKS<ct>[bit_count] << dst_offset);
			dst_value |= src_value;

			return TDst(dst_value);
		}
	}

	template<unsigned bits, int_for_bits_mode mode = int_for_bits_mode::exact> using uint_for_bits_t =
		decltype(detail::bits_hpp::uint_for_bits_helper<bits, mode>());
	template<unsigned bits, int_for_bits_mode mode = int_for_bits_mode::exact> using sint_for_bits_t =
		std::make_signed_t<uint_for_bits_t<bits, mode>>;
	template<bool signed_, unsigned bits, int_for_bits_mode mode = int_for_bits_mode::exact> using int_for_bits_t =
		std::conditional_t<signed_, sint_for_bits_t<bits, mode>, uint_for_bits_t<bits, mode>>;

	enum class bit_clear_mode
	{
		// Doesn't clear anything. Only OR's stuff into dst.
		none,

		// Clears only the bits being written, leaving others untouched.
		clear_bits,

		// Clears all touched TDst objects.
		clear_objects,

		// Assume objects are pre-zeroed. Use the fastest code path for writing into dst.
		contents_zero,
	};

	template<typename TDst, typename TSrc>
	constexpr TDst bit_read(TSrc src, size_t bit_count, size_t src_offset = 0, size_t dst_offset = 0)
	{
		using namespace detail::bits_hpp;
		if ((bit_count + src_offset) > (sizeof(TSrc) * BITS_PER_BYTE))
			throw "(bit_count + src_offset) > (sizeof(TSrc) * BITS_PER_BYTE)";
		if ((bit_count + dst_offset) > (sizeof(TDst) * BITS_PER_BYTE))
			throw "(bit_count + dst_offset) > (sizeof(TDst) * BITS_PER_BYTE)";

		using ct = std::make_unsigned_t<std::common_type_t<TSrc, TDst>>;
		ct value = ct(src);
		value >>= src_offset;
		value &= BIT_MASKS<ct>[bit_count];
		value <<= dst_offset;
		return value;
	}

	template<typename TDst, size_t bit_count, size_t src_offset = 0, size_t dst_offset = 0, typename TSrc = void>
	[[nodiscard]] constexpr TDst bit_read(TSrc src)
	{
		using namespace detail::bits_hpp;
		static_assert((bit_count + src_offset) <= (sizeof(TSrc) * BITS_PER_BYTE));
		static_assert((bit_count + dst_offset) <= (sizeof(TDst) * BITS_PER_BYTE));
		return bit_read<TDst, TSrc>(src, bit_count, src_offset, dst_offset);
	}

	template<size_t byte_count, typename TSrc, typename TDst>
	constexpr void byte_copy(TDst* MH_RESTRICT dst, const TSrc* MH_RESTRICT src)
	{
		using namespace detail::bits_hpp;

		if constexpr (byte_count > 0)
		{
			if (!std::is_constant_evaluated())
			{
				if constexpr (std::endian::native == std::endian::little)
				{
					std::memcpy(dst, src, byte_count);
					return;
				}
			}
			for (size_t i = 0; i < byte_count; i++)
				byte_write(dst, i, byte_read(src, i));
		}
	}

	namespace detail::bits_hpp
	{
#ifdef __GNUC__
#define MH_BITS_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define MH_BITS_ALWAYS_INLINE inline
#endif

		// The run-time path. On a little-endian target, byte i of an array of T is byte i of its memory. Loads and stores of the touched bytes match byte_read and byte_write. Each
		// step moves a bounded number of bits: several bits of offset plus many bits fit in the bytes of one load. No step reads or writes a byte that the copy does not touch.
		template<size_t bits, size_t src_bit, size_t dst_bit, bool clear>
		MH_BITS_ALWAYS_INLINE void word_bit_copy(std::byte* MH_RESTRICT dst, const std::byte* MH_RESTRICT src) noexcept
		{
			if constexpr (bits > 0)
			{
				constexpr size_t n = bits < 56 ? bits : 56;
				constexpr size_t so = src_bit % 8;
				constexpr size_t doff = dst_bit % 8;
				constexpr size_t sspan = (so + n + 7) / 8;
				constexpr size_t dspan = (doff + n + 7) / 8;
				constexpr std::uint64_t m = ~std::uint64_t(0) >> (64 - n);

				std::uint64_t v = 0;
				std::memcpy(&v, src + src_bit / 8, sspan);
				v = (v >> so) & m;

				std::uint64_t w = 0;
				std::memcpy(&w, dst + dst_bit / 8, dspan);
				if constexpr (clear)
					w &= ~(m << doff);
				w |= v << doff;
				std::memcpy(dst + dst_bit / 8, &w, dspan);

				word_bit_copy<bits - n, src_bit + n, dst_bit + n, clear>(dst, src);
			}
		}

		template<typename T> MH_BITS_ALWAYS_INLINE std::uint64_t load_le(const std::byte* p) noexcept
		{
			T v;
			std::memcpy(&v, p, sizeof(T));
			return v;
		}
		template<typename T> MH_BITS_ALWAYS_INLINE void store_le(std::byte* p, std::uint64_t v) noexcept
		{
			const T t = T(v);
			std::memcpy(p, &t, sizeof(T));
		}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
// GCC inlines these into callers with a short array and warns on a branch that the test on n keeps from running there.
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wstringop-overread"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
		MH_BITS_ALWAYS_INLINE std::uint64_t load_upto8(const std::byte* p, size_t n) noexcept
		{
			if (n == 8)
				return load_le<std::uint64_t>(p);
			if (n >= 4)
				return load_le<std::uint32_t>(p) | (load_le<std::uint32_t>(p + n - 4) << (8 * (n - 4)));
			if (n >= 2)
				return load_le<std::uint16_t>(p) | (load_le<std::uint16_t>(p + n - 2) << (8 * (n - 2)));
			return load_le<std::uint8_t>(p);
		}
		MH_BITS_ALWAYS_INLINE void store_upto8(std::byte* p, std::uint64_t v, size_t n) noexcept
		{
			if (n == 8)
				return store_le<std::uint64_t>(p, v);
			if (n >= 4)
			{
				store_le<std::uint32_t>(p + n - 4, v >> (8 * (n - 4)));
				return store_le<std::uint32_t>(p, v);
			}
			if (n >= 2)
			{
				store_le<std::uint16_t>(p + n - 2, v >> (8 * (n - 2)));
				return store_le<std::uint16_t>(p, v);
			}
			store_le<std::uint8_t>(p, v);
		}

		MH_BITS_ALWAYS_INLINE std::uint64_t window_read(const std::byte* src, size_t bits, size_t src_bit) noexcept
		{
			if (bits == 0)
				return 0;
			const std::byte* p = src + src_bit / 8;
			const size_t so = src_bit % 8;
			const size_t span = (so + bits + 7) / 8;
			std::uint64_t v;
			if (span <= 8)
				v = load_upto8(p, span) >> so;
			else
				v = (load_le<std::uint64_t>(p) >> so) | (load_le<std::uint8_t>(p + 8) << (64 - so));
			return bits == 64 ? v : v & ((std::uint64_t(1) << bits) - 1);
		}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

		MH_BITS_ALWAYS_INLINE void word_bit_copy_dynamic(std::byte* MH_RESTRICT dst, const std::byte* MH_RESTRICT src,
			size_t bits, size_t src_bit, size_t dst_bit, bool clear) noexcept
		{
			while (bits > 0)
			{
				const size_t n = bits < 56 ? bits : 56;
				const size_t so = src_bit % 8;
				const size_t doff = dst_bit % 8;
				const std::uint64_t m = ~std::uint64_t(0) >> (64 - n);

				const std::uint64_t v = (load_upto8(src + src_bit / 8, (so + n + 7) / 8) >> so) & m;
				const size_t dspan = (doff + n + 7) / 8;
				std::uint64_t w = load_upto8(dst + dst_bit / 8, dspan);
				if (clear)
					w &= ~(m << doff);
				w |= v << doff;
				store_upto8(dst + dst_bit / 8, w, dspan);

				bits -= n;
				src_bit += n;
				dst_bit += n;
			}
		}

		template<size_t src_offset, size_t dst_offset,
			bool src_multibyte, bool dst_multibyte, bit_clear_mode clear_mode, uint_fast16_t mask,
			typename TSrc = void, typename TDst = void>
		constexpr void bit_copy_helper(TDst* MH_RESTRICT dst, const TSrc* MH_RESTRICT src, size_t index)
		{
			using namespace detail::bits_hpp;

			constexpr size_t src_offset_bits = src_offset % BITS_PER_BYTE;
			constexpr size_t src_offset_bytes = src_offset / BITS_PER_BYTE;
			constexpr size_t dst_offset_bits = dst_offset % BITS_PER_BYTE;
			constexpr size_t dst_offset_bytes = dst_offset / BITS_PER_BYTE;

			uint_fast16_t src_buf{};
			if constexpr (src_multibyte)
				src_buf = u16_read(src, src_offset_bytes + index);
			else
				src_buf = byte_read<uint_fast16_t>(src, src_offset_bytes + index);

			src_buf = shift_both<dst_offset_bits, src_offset_bits>(src_buf);
			src_buf &= mask;

			uint_fast16_t dst_buf{};
			if constexpr (dst_multibyte)
				dst_buf = u16_read(dst, dst_offset_bytes + index);
			else
				dst_buf = byte_read<uint_fast16_t>(dst, dst_offset_bytes + index);

			if constexpr (clear_mode == bit_clear_mode::clear_bits)
				dst_buf &= ~mask;

			dst_buf |= src_buf;

			if constexpr (dst_multibyte)
				u16_write(dst, dst_offset_bytes + index, dst_buf);
			else
				byte_write(dst, dst_offset_bytes + index, std::byte(dst_buf));

			debug([&]{ std::cerr << "bit_copy byte " << index << " (mask " << std::hex << mask << ")\n"; });
		}
	}

	template<size_t bits_to_copy,
		size_t src_offset = 0, size_t dst_offset = 0,
		bit_clear_mode clear_mode = bit_clear_mode::clear_bits,
		typename TSrc = void, typename TDst = void>
	constexpr void bit_copy(TDst* MH_RESTRICT dst, const TSrc* MH_RESTRICT src)
	{
		using namespace detail::bits_hpp;

		static_assert(std::is_same_v<TSrc, std::byte> || std::is_unsigned_v<TSrc>);
		static_assert(std::is_same_v<TDst, std::byte> || std::is_unsigned_v<TDst>);

		using ubyte_t = std::underlying_type_t<std::byte>;
		using TSrcR = std::conditional_t<std::is_same_v<TSrc, std::byte>, ubyte_t, TSrc>;
		using TDstR = std::conditional_t<std::is_same_v<TDst, std::byte>, ubyte_t, TDst>;

		[[maybe_unused]] constexpr size_t bits_per_src = sizeof(TSrcR) * BITS_PER_BYTE;
		[[maybe_unused]] constexpr size_t bits_per_dst = sizeof(TDstR) * BITS_PER_BYTE;

		[[maybe_unused]] constexpr size_t src_offset_bits = src_offset % bits_per_src;
		[[maybe_unused]] constexpr size_t src_offset_bytes = src_offset / BITS_PER_BYTE;
		[[maybe_unused]] constexpr size_t src_offset_obj = src_offset / bits_per_src;
		[[maybe_unused]] constexpr size_t dst_offset_bits = dst_offset % bits_per_dst;
		[[maybe_unused]] constexpr size_t dst_offset_bytes = dst_offset / BITS_PER_BYTE;
		[[maybe_unused]] constexpr size_t dst_offset_obj = dst_offset / bits_per_dst;

		[[maybe_unused]] constexpr size_t dst_touched_bits = dst_offset_bits + bits_to_copy;
		[[maybe_unused]] constexpr size_t src_touched_bits = src_offset_bits + bits_to_copy;

		if constexpr (clear_mode == bit_clear_mode::clear_objects)
		{
			constexpr size_t dst_count_obj =
				(dst_offset_bits + bits_to_copy + (bits_per_dst - 1)) / bits_per_dst;
			for (size_t i = 0; i < dst_count_obj; i++)
				dst[dst_offset_obj + i] = {};
		}

		if (!std::is_constant_evaluated())
		{
			if constexpr (std::endian::native == std::endian::little)
			{
				word_bit_copy<bits_to_copy, src_offset, dst_offset, clear_mode == bit_clear_mode::clear_bits>(
					reinterpret_cast<std::byte*>(dst), reinterpret_cast<const std::byte*>(src));
				return;
			}
		}

		// The rest is the constant-evaluation path, and the path for a big-endian target.
		// Optimization: if everything is byte-aligned, just memcpy.
		if constexpr (src_offset_bits == 0 && dst_offset_bits == 0 &&
			(bits_to_copy % BITS_PER_BYTE) == 0 &&
			(clear_mode == bit_clear_mode::clear_bits || clear_mode == bit_clear_mode::contents_zero ||
			(clear_mode == bit_clear_mode::clear_objects && !(bits_to_copy % bits_per_dst))))
		{
			byte_copy<bits_to_copy / BITS_PER_BYTE>(&dst[dst_offset_obj], &src[src_offset_obj]);
			return;
		}

		// Optimization: if evertying can be processed in a single read/write, do that.
		if constexpr (dst_touched_bits <= bits_per_dst && src_touched_bits <= bits_per_src)
		{
			const auto value = bit_read<TDstR, bits_to_copy, src_offset_bits, dst_offset_bits>(TSrcR(src[src_offset_obj]));
			constexpr TDstR mask = TDstR(BIT_MASKS<TDstR>[bits_to_copy] << dst_offset_bits);

			switch (clear_mode)
			{
				case bit_clear_mode::clear_objects:
				case bit_clear_mode::contents_zero:
					dst[dst_offset_obj] = TDst(value);
					return;

				case bit_clear_mode::clear_bits:
					dst[dst_offset_obj] = TDst(TDstR(dst[dst_offset_obj]) & ~mask);
					[[fallthrough]];
				case bit_clear_mode::none:
					dst[dst_offset_obj] |= TDst(value & mask);
					return;
			}
		}

		debug([]{ std::cerr << "\nslow path\n"; });
		constexpr size_t loop_bytes = (bits_to_copy + (BITS_PER_BYTE - 1)) / BITS_PER_BYTE;
		[[maybe_unused]] constexpr size_t src_offset_bits8 = src_offset % BITS_PER_BYTE;
		[[maybe_unused]] constexpr size_t dst_offset_bits8 = dst_offset % BITS_PER_BYTE;

		debug([&]{ std::cerr
			<< "src_offset_bytes: " << src_offset_bytes << '\n'
			<< "src_offset_bits: " << src_offset_bits << '\n'
			<< "dst_offset_bytes: " << dst_offset_bytes << '\n'
			<< "dst_offset_bits: " << dst_offset_bits << '\n'
			<< "loop_bytes: " << loop_bytes << '\n'
			; });

		if constexpr (bits_to_copy > 0)
		{
			debug([]{ std::cerr << "pre-loop\n"; });
			constexpr bool src_multibyte = ((bits_to_copy + src_offset_bits8) > BITS_PER_BYTE);
			constexpr bool dst_multibyte = ((bits_to_copy + dst_offset_bits8) > BITS_PER_BYTE);
			constexpr uint_fast16_t mask = BIT_MASKS<uint_fast16_t>[min<size_t>(bits_to_copy, BITS_PER_BYTE)]
				<< dst_offset_bits8;
			bit_copy_helper<src_offset, dst_offset, src_multibyte, dst_multibyte, clear_mode, mask>(dst, src, 0);
		}

		if constexpr (loop_bytes > 0)
		{
			debug([]{ std::cerr << "loop\n"; });
			for (size_t b = 1; b < loop_bytes - 1; b++)
			{
				constexpr bool src_multibyte = src_offset_bits8 > 0;
				constexpr bool dst_multibyte = dst_offset_bits8 > 0;
				constexpr uint_fast16_t mask = BIT_MASKS<uint_fast16_t>[BITS_PER_BYTE] << dst_offset_bits8;
				bit_copy_helper<src_offset, dst_offset, src_multibyte, dst_multibyte, clear_mode, mask>(dst, src, b);
			}
		}

		if constexpr (loop_bytes > 1)
		{
			debug([]{ std::cerr << "post-loop\n"; });
			constexpr size_t remaining_bits = bits_to_copy - ((loop_bytes - 1) * BITS_PER_BYTE);
			constexpr bool dst_multibyte = ((dst_offset_bits8 + remaining_bits) > BITS_PER_BYTE);
			constexpr bool src_multibyte = ((src_offset_bits8 + remaining_bits) > BITS_PER_BYTE);

			constexpr uint_fast16_t mask = BIT_MASKS<uint_fast16_t>[remaining_bits]
				<< dst_offset_bits8;

			bit_copy_helper<src_offset, dst_offset, src_multibyte, dst_multibyte, clear_mode, mask>(
				dst, src, loop_bytes - 1);
		}
	}

	template<typename TOut,
		size_t bits_to_read = sizeof(TOut) * detail::bits_hpp::BITS_PER_BYTE,
		unsigned src_offset = 0,
		typename TIn = void>
	constexpr TOut bit_read(const TIn* src)
	{
		static_assert(bits_to_read <= (sizeof(TOut) * detail::bits_hpp::BITS_PER_BYTE));
		TOut retVal{};
		bit_copy<bits_to_read, src_offset, 0, bit_clear_mode::contents_zero>(&retVal, src);
		return retVal;
	}

	// bit_copy with the width and both offsets known only at run time. It needs a little-endian target.
	template<typename TSrc, typename TDst>
	void bit_copy_dynamic(TDst* MH_RESTRICT dst, const TSrc* MH_RESTRICT src, size_t bits_to_copy, size_t src_offset, size_t dst_offset,
		bit_clear_mode clear_mode = bit_clear_mode::clear_bits)
	{
		static_assert(std::is_same_v<TSrc, std::byte> || std::is_unsigned_v<TSrc>);
		static_assert(std::is_same_v<TDst, std::byte> || std::is_unsigned_v<TDst>);
		static_assert(std::endian::native == std::endian::little, "mh::bit_copy_dynamic needs a little-endian target");

		if (clear_mode == bit_clear_mode::clear_objects && bits_to_copy > 0)
		{
			constexpr size_t bits_per_dst = sizeof(TDst) * detail::bits_hpp::BITS_PER_BYTE;
			const size_t first = dst_offset / bits_per_dst;
			const size_t last = (dst_offset + bits_to_copy - 1) / bits_per_dst;
			for (size_t i = first; i <= last; i++)
				dst[i] = {};
		}
		detail::bits_hpp::word_bit_copy_dynamic(reinterpret_cast<std::byte*>(dst), reinterpret_cast<const std::byte*>(src),
			bits_to_copy, src_offset, dst_offset, clear_mode == bit_clear_mode::clear_bits);
	}

	// bit_read with the width and the offset known only at run time.
	template<typename TOut, typename TIn>
	TOut bit_read_dynamic(const TIn* src, size_t bits_to_read, size_t src_offset = 0)
	{
		if (bits_to_read > sizeof(TOut) * detail::bits_hpp::BITS_PER_BYTE)
			throw std::invalid_argument("mh::bit_read_dynamic: bits_to_read is wider than TOut");
		static_assert(std::endian::native == std::endian::little, "mh::bit_read_dynamic needs a little-endian target");
		if constexpr (sizeof(TOut) <= sizeof(std::uint64_t) && std::is_integral_v<TOut>)
		{
			return TOut(detail::bits_hpp::window_read(reinterpret_cast<const std::byte*>(src), bits_to_read, src_offset));
		}
		else
		{
			TOut retVal{};
			bit_copy_dynamic(&retVal, src, bits_to_read, src_offset, 0, bit_clear_mode::contents_zero);
			return retVal;
		}
	}

	// bit_read_dynamic for a source of src_size_bytes bytes. Where several bytes after the read position exist, a read is
	// one unconditional load. A stream of mixed widths does not branch on the width.
	template<typename TOut, typename TIn>
	TOut bit_read_dynamic(const TIn* src, size_t bits_to_read, size_t src_offset, size_t src_size_bytes)
	{
		static_assert(std::is_integral_v<TOut> && sizeof(TOut) <= sizeof(std::uint64_t));
		static_assert(std::endian::native == std::endian::little, "mh::bit_read_dynamic needs a little-endian target");
		if (bits_to_read > sizeof(TOut) * detail::bits_hpp::BITS_PER_BYTE)
			throw std::invalid_argument("mh::bit_read_dynamic: bits_to_read is wider than TOut");

		const std::byte* p = reinterpret_cast<const std::byte*>(src) + src_offset / 8;
		const size_t so = src_offset % 8;
		if (src_offset / 8 + 9 <= src_size_bytes)
		{
			std::uint64_t v = detail::bits_hpp::load_le<std::uint64_t>(p) >> so;
			v |= (detail::bits_hpp::load_le<std::uint8_t>(p + 8) << 1) << (63 - so);
			const std::uint64_t mask = bits_to_read >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits_to_read) - 1;
			return TOut(v & mask);
		}
		return TOut(detail::bits_hpp::window_read(reinterpret_cast<const std::byte*>(src), bits_to_read, src_offset));
	}

	// Writes the low bits_to_write bits of value at bit dst_offset of a destination of dst_size_bytes bytes. It keeps
	// every other bit. Where several bytes after the write position exist, a write is one 8-byte read-modify-write. For a
	// stream of writes, mh::bit_writer in bit_stream.hpp is faster, because it keeps the pending bits in a register.
	template<typename TDst>
	void bit_write_dynamic(TDst* dst, std::uint64_t value, size_t bits_to_write, size_t dst_offset, size_t dst_size_bytes)
	{
		static_assert(std::is_same_v<TDst, std::byte> || std::is_unsigned_v<TDst>);
		static_assert(std::endian::native == std::endian::little, "mh::bit_write_dynamic needs a little-endian target");
		if (bits_to_write > 64)
			throw std::invalid_argument("mh::bit_write_dynamic: bits_to_write is wider than 64");
		if (bits_to_write == 0)
			return;

		std::byte* p = reinterpret_cast<std::byte*>(dst) + dst_offset / 8;
		const size_t o = dst_offset % 8;
		const std::uint64_t mask = bits_to_write == 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits_to_write) - 1;
		value &= mask;
		if (dst_offset / 8 + 9 <= dst_size_bytes)
		{
			const std::uint64_t w = detail::bits_hpp::load_le<std::uint64_t>(p);
			detail::bits_hpp::store_le<std::uint64_t>(p, (w & ~(mask << o)) | (value << o));
			if (o + bits_to_write > 64)
			{
				const std::uint64_t hm = mask >> (64 - o);
				const std::uint64_t b = detail::bits_hpp::load_le<std::uint8_t>(p + 8);
				detail::bits_hpp::store_le<std::uint8_t>(p + 8, (b & ~hm) | (value >> (64 - o)));
			}
			return;
		}
		detail::bits_hpp::word_bit_copy_dynamic(reinterpret_cast<std::byte*>(dst), reinterpret_cast<const std::byte*>(&value),
			bits_to_write, 0, dst_offset, true);
	}
}
