#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
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

	namespace detail { namespace bits_hpp
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

		template<typename TFunc> constexpr void debug(const TFunc& f)
		{
#if (__cpp_lib_is_constant_evaluated >= 201811)
			//if (!std::is_constant_evaluated())
			//	f();
#endif
		}
	} }

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

	template<unsigned bits, int_for_bits_mode mode = int_for_bits_mode::exact> using uint_for_bits_t =
		decltype(detail::bits_hpp::uint_for_bits_helper<bits, mode>());
	template<unsigned bits, int_for_bits_mode mode = int_for_bits_mode::exact> using sint_for_bits_t =
		std::make_signed_t<uint_for_bits_t<bits, mode>>;
	template<bool signed_, unsigned bits, int_for_bits_mode mode = int_for_bits_mode::exact> using int_for_bits_t =
		std::conditional_t<signed_, sint_for_bits_t<bits, mode>, uint_for_bits_t<bits, mode>>;

	enum class bit_clear_mode
	{
		// Doesn't clear anything. Only OR's stuff into dst. Fastest.
		none,

		// Clears only the bits being written, leaving others untouched.
		clear_bits,

		// Clears all touched TDst objects. Likely slower, requires additional branching
		clear_objects,

		// Objects are pre-zeroed. Use the fastest code path for writing into dst.
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

	template<unsigned shift_left, unsigned shift_right, typename T = void>
	[[nodiscard]] constexpr T shift_both(T value)
	{
		if constexpr (shift_left >= shift_right)
			return T(value << (shift_left - shift_right));
		else
			return T(value >> (shift_right - shift_left));
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

	template<size_t byte_count, typename TSrc, typename TDst>
	constexpr void byte_copy(TDst* MH_RESTRICT dst, const TSrc* MH_RESTRICT src)
	{
		using namespace detail::bits_hpp;

		if constexpr (byte_count > 0)
		{
#if (__cpp_lib_is_constant_evaluated >= 201811)
			if (!std::is_constant_evaluated())
			{
				std::memcpy(dst, src, byte_count);
			}
			else
#endif
			{
				for (size_t i = 0; i < byte_count; i++)
					byte_write(dst, i, byte_read(src, i));
			}
		}
	}

#if false
	template<size_t bits_to_copy>
	constexpr uint_fast16_t bit_copy_mask_helper()
	{
		using namespace detail::bits_hpp;

		constexpr auto mask = BIT_MASKS<uint_fast16_t>[min<size_t>(bits_to_copy, BITS_PER_BYTE)]
				<< dst_offset_bits;

		constexpr auto mask = BIT_MASKS<uint_fast16_t>[min<size_t>(bits_to_copy, BITS_PER_BYTE)]
			>> (BITS_PER_BYTE - ((dst_offset_bits + bits_to_copy) % BITS_PER_BYTE));
	}
#endif

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

		uint_fast16_t src_buf = byte_read<uint_fast16_t>(src, src_offset_bytes + index);
		if constexpr (src_multibyte)
			src_buf |= byte_read<uint_fast16_t>(src, src_offset_bytes + index + 1) << BITS_PER_BYTE;

		src_buf = shift_both<dst_offset_bits, src_offset_bits>(src_buf);

		//constexpr auto mask = BIT_MASKS<uint_fast16_t>[min<size_t>(bits_to_copy, BITS_PER_BYTE)]
		//	>> (BITS_PER_BYTE - ((dst_offset_bits + bits_to_copy) % BITS_PER_BYTE));

		src_buf &= mask;

		uint_fast16_t dst_buf = byte_read<uint_fast16_t>(dst, dst_offset_bytes + index);
		//constexpr bool dst_multibyte = (bits_to_copy + dst_offset_bits) > BITS_PER_BYTE;
		if constexpr (dst_multibyte)
		{
			dst_buf |= byte_read<uint_fast16_t>(dst, dst_offset_bytes + index + 1)
				<< BITS_PER_BYTE;
		}

		if constexpr (clear_mode == bit_clear_mode::clear_bits)
			dst_buf &= ~mask;

		dst_buf |= src_buf;

		byte_write(dst, dst_offset_bytes + index, std::byte(dst_buf));
		if constexpr (dst_multibyte)
		{
			byte_write(dst, dst_offset_bytes + index + 1,
				std::byte(dst_buf >> BITS_PER_BYTE));
		}

		debug([&]{ std::cerr << "bit_copy byte " << index << " (mask " << std::hex << mask << ")\n"; });
	}

	template<size_t bits_to_copy,
		size_t src_offset = 0, size_t dst_offset = 0,
		bit_clear_mode clear_mode = bit_clear_mode::none,
		typename TSrc = void, typename TDst = void>
	constexpr void bit_copy(TDst* MH_RESTRICT dst, const TSrc* MH_RESTRICT src)
	{
		using namespace detail::bits_hpp;

		static_assert(std::is_same_v<TSrc, std::byte> || std::is_unsigned_v<TSrc>);
		static_assert(std::is_same_v<TDst, std::byte> || std::is_unsigned_v<TDst>);

		using ubyte_t = std::underlying_type_t<std::byte>;
		using TSrcR = std::conditional_t<std::is_same_v<TSrc, std::byte>, ubyte_t, TSrc>;
		using TDstR = std::conditional_t<std::is_same_v<TDst, std::byte>, ubyte_t, TDst>;

		constexpr size_t bits_per_src = sizeof(TSrcR) * BITS_PER_BYTE;
		constexpr size_t bits_per_dst = sizeof(TDstR) * BITS_PER_BYTE;

		constexpr size_t src_offset_bits = src_offset % bits_per_src;
		constexpr size_t src_offset_bytes = src_offset / BITS_PER_BYTE;
		//constexpr size_t src_offset_obj = src_offset / bits_per_src;
		constexpr size_t dst_offset_bits = dst_offset % bits_per_dst;
		constexpr size_t dst_offset_bytes = dst_offset / BITS_PER_BYTE;
		constexpr size_t dst_offset_obj = dst_offset / bits_per_dst;

		constexpr size_t dst_touched_bits = dst_offset_bits + bits_to_copy;
		constexpr size_t src_touched_bits = src_offset_bits + bits_to_copy;

		if constexpr (clear_mode == bit_clear_mode::clear_objects)
		{
			constexpr size_t dst_count_count =
				(dst_offset_bits + bits_to_copy + (bits_per_dst - 1)) / bits_per_dst;
			for (size_t i = 0; i < dst_count_count; i++)
				dst[dst_offset_obj] = {};
		}

// Known working speedups
#if false
#if (__cpp_lib_is_constant_evaluated >= 201811)
		if (!std::is_constant_evaluated())
		{
			constexpr size_t dst_touched_bytes = (dst_touched_bits + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
			constexpr size_t src_touched_bytes = (src_touched_bits + BITS_PER_BYTE - 1) / BITS_PER_BYTE;

			if constexpr ((clear_mode == bit_clear_mode::clear_objects ||
				clear_mode == bit_clear_mode::contents_zero) && (src_touched_bytes == dst_touched_bytes))
			{
				if constexpr (src_touched_bytes == 2)
				{
					auto src_value = *reinterpret_cast<const uint16_t*>(&src[src_offset_obj]);
					src_value >>= src_offset_bits;
					src_value &= BIT_MASKS<uint16_t>[bits_to_copy];
					src_value <<= dst_offset_bits;
					*reinterpret_cast<uint16_t*>(&dst[dst_offset_obj]) = src_value;
					return;
				}
				if constexpr (src_touched_bytes == 4)
				{
					auto src_value = *reinterpret_cast<const uint32_t*>(&src[src_offset_obj]);
					src_value >>= src_offset_bits;
					src_value &= BIT_MASKS<uint32_t>[bits_to_copy];
					src_value <<= dst_offset_bits;
					*reinterpret_cast<uint32_t*>(&dst[dst_offset_obj]) = src_value;
					return;
				}
				if constexpr (src_touched_bytes == 8)
				{
					auto src_value = *reinterpret_cast<const uint64_t*>(&src[src_offset_obj]);
					src_value >>= src_offset_bits;
					src_value &= BIT_MASKS<uint64_t>[bits_to_copy];
					src_value <<= dst_offset_bits;
					*reinterpret_cast<uint64_t*>(&dst[dst_offset_obj]) = src_value;
					return;
				}
			}
		}
#endif

		if constexpr (src_offset_bits == 0 && dst_offset_bits == 0 &&
			(bits_to_copy % BITS_PER_BYTE) == 0)
		{
			byte_copy<bits_to_copy / BITS_PER_BYTE>(&dst[dst_offset_obj], &src[src_offset_obj]);
			return;
		}

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
#endif

		debug([]{ std::cerr << "\nslow path\n"; });
		constexpr size_t src_whole_bytes = src_touched_bits / BITS_PER_BYTE;
		constexpr size_t dst_whole_bytes = dst_touched_bits / BITS_PER_BYTE;
		constexpr size_t whole_bytes = min(src_whole_bytes, dst_whole_bytes);

		const auto read_uint16 = [&](const auto* ptr, size_t startByte)
		{
			const auto value = uint_fast16_t(byte_read(ptr, startByte)) |
				(uint_fast16_t(byte_read(ptr, startByte + 1)) << BITS_PER_BYTE);

			debug([&]{ std::cerr << "read_uint16: " << std::hex << value << '\n'; });
			return value;
		};
		const auto write_uint16 = [&](auto* ptr, size_t startByte, uint_fast16_t value)
		{
			debug([&]{ std::cerr << "write_uint16: " << std::hex << value << '\n'; });
			byte_write(ptr, startByte, std::byte(value));
			byte_write(ptr, startByte + 1, std::byte(value >> BITS_PER_BYTE));
		};

		constexpr size_t dst_bytes_test = (bits_to_copy + dst_offset_bits) / BITS_PER_BYTE;
		debug([&]{ std::cerr
			<< "src_offset_bytes: " << src_offset_bytes << '\n'
			<< "src_offset_bits: " << src_offset_bits << '\n'
			<< "dst_offset_bytes: " << dst_offset_bytes << '\n'
			<< "dst_offset_bits: " << dst_offset_bits << '\n'
			<< "dst_bytes_test: " << dst_bytes_test << '\n'
			; });

		if constexpr (bits_to_copy > 0)
		{
			constexpr bool src_multibyte = ((src_offset_bits + bits_to_copy) > BITS_PER_BYTE);
			constexpr bool dst_multibyte = ((bits_to_copy + dst_offset_bits) > BITS_PER_BYTE);
			constexpr uint_fast16_t mask = BIT_MASKS<uint_fast16_t>[min<size_t>(bits_to_copy, BITS_PER_BYTE)]
				<< dst_offset_bits;
			bit_copy_helper<src_offset, dst_offset, src_multibyte, dst_multibyte, clear_mode, mask>(dst, src, 0);
#if false
			uint_fast16_t src_buf = byte_read<uint_fast16_t>(src, src_offset_bytes);
			if constexpr ((src_offset_bits + bits_to_copy) > BITS_PER_BYTE)
				src_buf |= byte_read<uint_fast16_t>(src, src_offset_bytes + 1) << BITS_PER_BYTE;

			src_buf = shift_both<dst_offset_bits, src_offset_bits>(src_buf);

			constexpr auto mask = BIT_MASKS<uint_fast16_t>[min<size_t>(bits_to_copy, BITS_PER_BYTE)]
				<< dst_offset_bits;

			src_buf &= mask;

			uint_fast16_t dst_buf = byte_read<uint_fast16_t>(dst, dst_offset_bytes);
			constexpr bool dst_multibyte = (bits_to_copy + dst_offset_bits) > BITS_PER_BYTE;
			if constexpr (dst_multibyte)
				dst_buf |= byte_read<uint_fast16_t>(dst, dst_offset_bytes + 1) << BITS_PER_BYTE;

			if constexpr (clear_mode == bit_clear_mode::clear_bits)
				dst_buf &= ~mask;

			dst_buf |= src_buf;

			byte_write(dst, dst_offset_bytes, std::byte(dst_buf));
			if constexpr (dst_multibyte)
				byte_write(dst, dst_offset_bytes + 1, std::byte(dst_buf >> BITS_PER_BYTE));

			debug([&]{ std::cerr << "bit_copy byte 0 (mask " << std::hex << mask << ")\n"; });
#endif
		}

		//if constexpr (dst_bytes_test > 1)
		{
			constexpr bool src_multibyte = src_offset_bits > 0;
			constexpr bool dst_multibyte = dst_offset_bits > 0;
			constexpr uint_fast16_t mask = BIT_MASKS<uint_fast16_t>[BITS_PER_BYTE] << dst_offset_bits;
			for (size_t b = 1; b < dst_bytes_test; b++)
			{
				bit_copy_helper<src_offset, dst_offset, src_multibyte, dst_multibyte, clear_mode, mask>(dst, src, b);
#if false
				debug([&]{ std::cerr << "bit_copy loop[" << b << "]\n"; });
				uint_fast16_t src_buf = read_uint16(src, src_offset_bytes + b);
				src_buf = shift_both<dst_offset_bits, src_offset_bits>(src_buf);

				constexpr auto mask = BIT_MASKS<uint_fast16_t>[BITS_PER_BYTE] << dst_offset_bits;
				src_buf &= mask;

				uint_fast16_t dst_buf = read_uint16(dst, dst_offset_bytes + b);
				if constexpr (clear_mode == bit_clear_mode::clear_bits)
					dst_buf &= ~mask;

				dst_buf |= src_buf;

				write_uint16(dst, dst_offset_bytes + b, dst_buf);
#endif
			}
		}

		if constexpr (constexpr bool dst_multibyte = ((dst_offset_bits + bits_to_copy) % BITS_PER_BYTE))
		{
			constexpr bool src_multibyte = ((src_offset_bits + bits_to_copy) % BITS_PER_BYTE);
			constexpr uint_fast16_t mask =  BIT_MASKS<uint_fast16_t>[min<size_t>(bits_to_copy, BITS_PER_BYTE)]
				>> (BITS_PER_BYTE - ((dst_offset_bits + bits_to_copy) % BITS_PER_BYTE));

			bit_copy_helper<src_offset, dst_offset, src_multibyte, dst_multibyte, clear_mode, mask>(
				dst, src, dst_bytes_test);
#if false
			uint_fast16_t src_buf = byte_read<uint_fast16_t>(src, src_offset_bytes + dst_bytes_test);
			if constexpr ((src_offset_bits + bits_to_copy) % BITS_PER_BYTE)
				src_buf |= byte_read<uint_fast16_t>(src, src_offset_bytes + dst_bytes_test + 1) << BITS_PER_BYTE;

			src_buf = shift_both<dst_offset_bits, src_offset_bits>(src_buf);

			constexpr auto mask = BIT_MASKS<uint_fast16_t>[min<size_t>(bits_to_copy, BITS_PER_BYTE)]
				>> (BITS_PER_BYTE - ((dst_offset_bits + bits_to_copy) % BITS_PER_BYTE));

			src_buf &= mask;

			uint_fast16_t dst_buf = byte_read<uint_fast16_t>(dst, dst_offset_bytes + dst_bytes_test);
			if constexpr (dst_multibyte)
			{
				dst_buf |= byte_read<uint_fast16_t>(dst, dst_offset_bytes + dst_bytes_test + 1)
					<< BITS_PER_BYTE;
			}

			if constexpr (clear_mode == bit_clear_mode::clear_bits)
				dst_buf &= ~mask;

			dst_buf |= src_buf;

			byte_write(dst, dst_offset_bytes + dst_bytes_test, std::byte(dst_buf));
			if constexpr (dst_multibyte)
			{
				byte_write(dst, dst_offset_bytes + dst_bytes_test + 1,
					std::byte(dst_buf >> BITS_PER_BYTE));
			}

			debug([&]{ std::cerr << "bit_copy byte " << dst_bytes_test << " (mask " << std::hex << mask << ")\n"; });
#endif
		}

#if false
		for (size_t b = 0; b < bits_to_copy; )
		{
			const size_t dst_byte = (b + dst_offset) / bits_per_dst;
			const size_t dst_bit = (b + dst_offset) % bits_per_dst;
			const size_t max_dst_writebits = bits_per_dst - dst_bit;

			const size_t src_byte = (b + src_offset) / bits_per_src;
			const size_t src_bit = (b + src_offset) % bits_per_src;
			const size_t max_src_readbits = bits_per_src - src_bit;

			const size_t loop_bits = min(max_dst_writebits, max_src_readbits, bits_to_copy - b);
			//debug([&]{ std::cerr << "loop_bits: " << loop_bits << '\n'; });

			TDstR existing_dst_value = TDstR(dst[dst_byte]);

			if constexpr (clear_mode == bit_clear_mode::clear_objects)
			{
				if (dst_bit == 0)
					existing_dst_value = {};
			}
			else if constexpr (clear_mode == bit_clear_mode::clear_bits)
			{
				existing_dst_value &= ~(BIT_MASKS<TDstR>[loop_bits] << dst_bit);
			}

			const TDstR new_dst_value = bit_read<TDstR>(TSrcR(src[src_byte]), loop_bits, src_bit, dst_bit);

			dst[dst_byte] = TDst(existing_dst_value | new_dst_value);
			b += loop_bits;
		}
#endif
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
}
