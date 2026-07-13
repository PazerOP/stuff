#include "mh/data/bits.hpp"
#include <catch2/catch_all.hpp>
#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>

// Helper to convert std::byte to unsigned for CAPTURE (Catch2 v3.4.0 lacks StringMaker<std::byte> implementation)
template<typename T>
auto capture_value(const T& val) {
	if constexpr (std::is_same_v<T, std::byte>)
		return static_cast<unsigned>(val);
	else
		return val;
}

template<unsigned bits_to_copy, unsigned src_offset, typename TSrc = void, typename TDst = void>
static void test_bit_functions(const TSrc* src, const TDst expected)
{
	uint64_t srcVal{};
	constexpr size_t srcValSize = (bits_to_copy + src_offset + 7) / 8;
	CAPTURE(srcValSize);
	memcpy(&srcVal, src, srcValSize);
	CAPTURE(srcVal);

	CAPTURE(capture_value(*src), expected, bits_to_copy, src_offset, typeid(TSrc).name(), typeid(TDst).name());

	const auto read = +mh::bit_read<TDst, bits_to_copy, src_offset>(src);

	constexpr TDst dst_max = std::numeric_limits<TDst>::max();
	constexpr TDst dst_mask = mh::BIT_MASKS<TDst>[bits_to_copy];
	CAPTURE(dst_mask);

	TDst copied = dst_max;
	mh::bit_copy<bits_to_copy, src_offset, 0, mh::bit_clear_mode::none>(&copied, src);
	REQUIRE(copied == dst_max);

	{
		constexpr auto clear_mode = mh::bit_clear_mode::clear_bits;
		CAPTURE(clear_mode);
		copied = dst_max;
		mh::bit_copy<bits_to_copy, src_offset, 0, clear_mode>(&copied, src);
		CAPTURE(copied);
		REQUIRE((copied & dst_mask) == expected);
		REQUIRE((copied & ~dst_mask) == ~dst_mask);
	}

	copied = dst_max;
	mh::bit_copy<bits_to_copy, src_offset, 0, mh::bit_clear_mode::clear_objects>(&copied, src);
	REQUIRE(copied == read);
	REQUIRE(read == expected);
	REQUIRE(copied == expected);

	copied = {};
	mh::bit_copy<bits_to_copy, src_offset, 0, mh::bit_clear_mode::none>(&copied, src);
	REQUIRE(copied == expected);
}

TEST_CASE("bit_read - uint8_t source/dest")
{
	//uint8_t src[] = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe };
	const uint16_t src = 8;
	uint8_t dst[32]{};

	mh::bit_copy<6, 0, 5, mh::bit_clear_mode::clear_bits>(dst, &src);
	REQUIRE(dst[0] == 0);
	REQUIRE(dst[1] == 1);
}

TEST_CASE("bit_read - uint8_t source")
{
	constexpr uint8_t src_value_raw[] = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe };
	const std::byte* src_value = reinterpret_cast<const std::byte*>(src_value_raw);

	test_bit_functions<16, 0>(src_value, 0x3210U);
	test_bit_functions<16, 1>(src_value, 0x1908U);
	test_bit_functions<16, 2>(src_value, 0x0C84U);
	test_bit_functions<16, 3>(src_value, 0x8642U);
	test_bit_functions<16, 4>(src_value, 0x4321U);
	test_bit_functions<16, 5>(src_value, 0xA190U);
	test_bit_functions<16, 6>(src_value, 0x50C8U);
	test_bit_functions<16, 7>(src_value, 0xA864U);
	test_bit_functions<13, 7>(src_value, 0x0864U);

	test_bit_functions<4, 47>(src_value, 0x9U);
	test_bit_functions<3, 48>(src_value, 0x4U);
	test_bit_functions<4, 48>(src_value, 0xCU);
}

TEST_CASE("bit_read - uint64_t source")
{
	constexpr uint64_t src_value = 0xFEDCBA9876543210;

	test_bit_functions<16, 0>(&src_value, 0x3210U);
	test_bit_functions<16, 1>(&src_value, 0x1908U);
	test_bit_functions<16, 2>(&src_value, 0xC84U);
	test_bit_functions<16, 3>(&src_value, 0x8642U);
	test_bit_functions<16, 4>(&src_value, 0x4321U);
	test_bit_functions<16, 5>(&src_value, 0xA190U);
	test_bit_functions<16, 6>(&src_value, 0x50C8U);
	test_bit_functions<16, 7>(&src_value, 0xA864U);
	test_bit_functions<13, 7>(&src_value, 0x0864U);

	test_bit_functions<4, 47>(&src_value, 0x9U);
	test_bit_functions<3, 48>(&src_value, 0x4U);
	test_bit_functions<4, 48>(&src_value, 0xCU);
}

namespace
{
	// Bit-by-bit reference implementation of bit_copy's clear_bits semantics:
	// little-endian bit addressing over the underlying bytes (bit i lives in
	// byte i/8, position i%8), exactly matching byte_read/byte_write. The bits
	// in [dst_offset, dst_offset + bits) become an exact copy of the bits in
	// [src_offset, src_offset + bits); every other destination bit is untouched.
	bool ref_get_bit(const unsigned char* p, size_t bit)
	{
		return (p[bit / 8] >> (bit % 8)) & 1;
	}

	void ref_set_bit(unsigned char* p, size_t bit, bool value)
	{
		if (value)
			p[bit / 8] = static_cast<unsigned char>(p[bit / 8] | (1u << (bit % 8)));
		else
			p[bit / 8] = static_cast<unsigned char>(p[bit / 8] & ~(1u << (bit % 8)));
	}

	void ref_bit_copy(unsigned char* dst, const unsigned char* src, size_t bits, size_t src_offset, size_t dst_offset)
	{
		for (size_t i = 0; i < bits; i++)
			ref_set_bit(dst, dst_offset + i, ref_get_bit(src, src_offset + i));
	}

	struct bit_copy_failure_log
	{
		int fail_count = 0;
		std::string details; // first few failing configs, for the assertion message

		void record(const char* kind, size_t bits, size_t src_offset, size_t dst_offset)
		{
			fail_count++;
			if (fail_count <= 25)
			{
				details += kind;
				details += ": bits=" + std::to_string(bits);
				details += " src_offset=" + std::to_string(src_offset);
				details += " dst_offset=" + std::to_string(dst_offset);
				details += '\n';
			}
		}
	};

	// Deterministic non-trivial fill patterns (no RNG needed).
	void fill_src_pattern(unsigned char* p, size_t size)
	{
		for (size_t i = 0; i < size; i++)
			p[i] = static_cast<unsigned char>(0x37 + i * 0x51);
	}
	void fill_dst_pattern(unsigned char* p, size_t size)
	{
		for (size_t i = 0; i < size; i++)
			p[i] = static_cast<unsigned char>(0xA5 ^ (i * 0x33));
	}

	template<size_t bits, size_t src_offset, size_t dst_offset>
	void check_bit_copy_bytes(bit_copy_failure_log& log)
	{
		unsigned char src_raw[16], dst_ref[16], dst_mh[16];
		fill_src_pattern(src_raw, sizeof(src_raw));
		fill_dst_pattern(dst_ref, sizeof(dst_ref));
		memcpy(dst_mh, dst_ref, sizeof(dst_ref));

		ref_bit_copy(dst_ref, src_raw, bits, src_offset, dst_offset);
		mh::bit_copy<bits, src_offset, dst_offset, mh::bit_clear_mode::clear_bits>(
			reinterpret_cast<std::byte*>(dst_mh), reinterpret_cast<const std::byte*>(src_raw));

		if (memcmp(dst_ref, dst_mh, sizeof(dst_ref)) != 0)
			log.record("byte buffers", bits, src_offset, dst_offset);
	}

	template<size_t bits, size_t src_offset, size_t dst_offset>
	void check_bit_copy_u32(bit_copy_failure_log& log)
	{
		// Multi-byte objects: offsets are relative to the uint32_t objects, so
		// offset % 32 can be >= 8 while the byte-level offsets stay small.
		uint32_t src32[4], ref32[4], mh32[4];
		for (uint32_t i = 0; i < 4; i++)
			src32[i] = 0x12345678u * (i + 3) + 0x9E3779B9u;
		for (uint32_t i = 0; i < 4; i++)
			ref32[i] = 0xCAFEBABEu ^ (0x01010101u * i);
		memcpy(mh32, ref32, sizeof(ref32));

		unsigned char ref_bytes[sizeof(ref32)], src_bytes[sizeof(src32)];
		memcpy(ref_bytes, ref32, sizeof(ref32));
		memcpy(src_bytes, src32, sizeof(src32));
		ref_bit_copy(ref_bytes, src_bytes, bits, src_offset, dst_offset);

		mh::bit_copy<bits, src_offset, dst_offset, mh::bit_clear_mode::clear_bits>(
			mh32, static_cast<const uint32_t*>(src32));

		if (memcmp(ref_bytes, mh32, sizeof(ref32)) != 0)
			log.record("uint32_t objects", bits, src_offset, dst_offset);
	}

	template<size_t bits, size_t src_offset, size_t... dst_offsets>
	void sweep_bit_copy_dst(bit_copy_failure_log& log, std::index_sequence<dst_offsets...>)
	{
		(check_bit_copy_bytes<bits, src_offset, dst_offsets>(log), ...);
	}
	template<size_t bits, size_t... src_offsets>
	void sweep_bit_copy_src(bit_copy_failure_log& log, std::index_sequence<src_offsets...>)
	{
		(sweep_bit_copy_dst<bits, src_offsets>(log, std::make_index_sequence<10>{}), ...);
	}
	template<size_t... bits>
	void sweep_bit_copy_bits(bit_copy_failure_log& log, std::index_sequence<bits...>)
	{
		(sweep_bit_copy_src<bits + 1>(log, std::make_index_sequence<10>{}), ...);
	}
}

TEST_CASE("bit_copy - arbitrary src/dst bit offsets match bit-by-bit reference")
{
	bit_copy_failure_log log;

	// Full grid: bits 1..20 x src_offset 0..9 x dst_offset 0..9 over byte buffers.
	// This includes every combination where dst_offset % 8 != 0, which the
	// single-object fast path cannot handle (they all go through the slow path).
	sweep_bit_copy_bits(log, std::make_index_sequence<20>{});

	// Multi-byte source/destination objects with object-relative offsets >= 8.
	check_bit_copy_u32<24, 0, 12>(log);
	check_bit_copy_u32<20, 0, 20>(log);
	check_bit_copy_u32<16, 12, 0>(log);
	check_bit_copy_u32<16, 20, 16>(log);
	check_bit_copy_u32<9, 0, 9>(log);
	check_bit_copy_u32<40, 0, 12>(log);

	INFO(log.details);
	REQUIRE(log.fail_count == 0);
}

TEST_CASE("bit_copy - non-byte-aligned destination writes exact expected bytes")
{
	// Hand-computed anchor values, independent of the reference implementation.

	{
		// Copy src bits [0, 8) to dst bits [4, 12).
		// src = 0xC3 -> dst[0] high nibble := 0x3, dst[1] low nibble := 0xC.
		// Everything outside dst bits 4..11 must be preserved (clear_bits mode).
		uint8_t dst[4] = {0xAA, 0x55, 0xEE, 0xEE};
		const uint8_t src[2] = {0xC3, 0x7F}; // second byte must never be read
		mh::bit_copy<8, 0, 4, mh::bit_clear_mode::clear_bits>(dst, static_cast<const uint8_t*>(src));

		REQUIRE(dst[0] == 0x3A);
		REQUIRE(dst[1] == 0x5C);
		REQUIRE(dst[2] == 0xEE); // outside the destination region: untouched
		REQUIRE(dst[3] == 0xEE);
	}

	{
		// Copy src bits [0, 24) to dst bits [12, 36) across two uint32_t objects.
		// src bits 0..23 = 0xABCDEF.
		uint32_t dst[2] = {0x00000000, 0xFFFFFFFF};
		const uint32_t src[1] = {0x89ABCDEF};
		mh::bit_copy<24, 0, 12, mh::bit_clear_mode::clear_bits>(dst, static_cast<const uint32_t*>(src));

		REQUIRE(dst[0] == 0xBCDEF000u);
		REQUIRE(dst[1] == 0xFFFFFFFAu);
	}

	{
		// The repo's original uint16_t-source configuration (see
		// "bit_read - uint8_t source/dest" above), with a sentinel-filled
		// destination: copy src bits [0, 6) to dst bits [5, 11).
		// src = 8 -> only dst bit 8 is set within the region.
		uint8_t dst[4] = {0xFF, 0xFF, 0xEE, 0xEE};
		const uint16_t src = 8;
		mh::bit_copy<6, 0, 5, mh::bit_clear_mode::clear_bits>(dst, &src);

		REQUIRE(dst[0] == 0x1F);
		REQUIRE(dst[1] == 0xF9);
		REQUIRE(dst[2] == 0xEE);
		REQUIRE(dst[3] == 0xEE);
	}
}

TEST_CASE("bit_read - out-of-range bit counts throw")
{
	// runtime preconditions of the dynamic-count overload: the requested bits
	// must fit both the source and the destination type
	REQUIRE_THROWS(mh::bit_read<uint32_t>(uint8_t(0xFF), 9));    // 9 bits > 8-bit source
	REQUIRE_THROWS(mh::bit_read<uint8_t>(uint16_t(0xFFFF), 12)); // 12 bits > 8-bit destination

	// exactly at the boundary is still fine
	REQUIRE(+mh::bit_read<uint8_t>(uint8_t(0xAB), 8) == 0xAB);
}
