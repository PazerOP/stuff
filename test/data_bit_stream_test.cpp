#include "mh/data/bit_stream.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

namespace
{
	// LSB-first, one bit at a time, so that it is obviously correct.
	std::uint64_t golden_get(const std::vector<std::byte>& buf, std::size_t pos, unsigned bits)
	{
		std::uint64_t v = 0;
		for (unsigned i = 0; i < bits; i++)
		{
			const std::size_t p = pos + i;
			v |= std::uint64_t((std::to_integer<unsigned>(buf[p >> 3]) >> (p & 7)) & 1) << i;
		}
		return v;
	}
	void golden_put(std::vector<std::byte>& buf, std::size_t pos, std::uint64_t value, unsigned bits)
	{
		for (unsigned i = 0; i < bits; i++)
		{
			const std::size_t p = pos + i;
			const auto bit = std::byte(1u << (p & 7));
			if ((value >> i) & 1)
				buf[p >> 3] |= bit;
			else
				buf[p >> 3] &= ~bit;
		}
	}
	std::uint64_t mask(unsigned bits) { return bits >= 64 ? ~0ull : (1ull << bits) - 1; }

	struct Field
	{
		unsigned bits;
		std::uint64_t value;
	};

	std::vector<Field> random_fields(std::size_t total_bits, std::uint64_t seed)
	{
		std::mt19937_64 rng(seed);
		std::vector<Field> f;
		std::size_t used = 0;
		for (;;)
		{
			const unsigned bits = unsigned(rng() % 64) + 1;
			if (used + bits > total_bits)
				break;
			f.push_back({ bits, rng() & mask(bits) });
			used += bits;
		}
		return f;
	}

	// The last byte of the buffer sits right before a PROT_NONE page, so an access past the end faults.
	class GuardedBuffer
	{
	public:
		explicit GuardedBuffer(std::size_t size)
		{
			const std::size_t page = std::size_t(sysconf(_SC_PAGESIZE));
			const std::size_t pages = (size + page - 1) / page;
			m_MapSize = (pages + 1) * page;
			m_Map = static_cast<std::byte*>(mmap(nullptr, m_MapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
			REQUIRE(m_Map != MAP_FAILED);
			REQUIRE(mprotect(m_Map + pages * page, page, PROT_NONE) == 0);
			m_Data = m_Map + pages * page - size;
		}
		~GuardedBuffer() { munmap(m_Map, m_MapSize); }
		std::byte* data() { return m_Data; }

	private:
		std::byte* m_Map;
		std::byte* m_Data;
		std::size_t m_MapSize;
	};
}

TEST_CASE("bit_reader reads every width at every bit offset")
{
	for (unsigned prefix = 0; prefix < 8; prefix++)
	{
		for (unsigned bits = 1; bits <= 64; bits++)
		{
			std::mt19937_64 rng(bits * 31 + prefix);
			std::vector<std::byte> buf(64 * 9 + 16);
			std::vector<std::uint64_t> values;
			std::size_t pos = prefix;
			for (int i = 0; i < 40; i++)
			{
				values.push_back(rng() & mask(bits));
				golden_put(buf, pos, values.back(), bits);
				pos += bits;
			}

			mh::bit_reader r(buf.data(), buf.size());
			r.seek(prefix);
			for (auto v : values)
				REQUIRE(r.read(bits) == v);
		}
	}
}

TEST_CASE("bit_reader compile-time widths match run-time widths")
{
	std::vector<std::byte> buf(4096);
	std::mt19937_64 rng(9);
	for (auto& b : buf)
		b = std::byte(rng());

	mh::bit_reader a(buf.data(), buf.size());
	mh::bit_reader b(buf.data(), buf.size());
	for (int i = 0; i < 64; i++)
	{
		REQUIRE(a.read<1>() == b.read(1));
		REQUIRE(a.read<7>() == b.read(7));
		REQUIRE(a.read<13>() == b.read(13));
		REQUIRE(a.read<32>() == b.read(32));
		REQUIRE(a.read<57>() == b.read(57));
		REQUIRE(a.read<58>() == b.read(58));
		REQUIRE(a.read<64>() == b.read(64));
	}
	REQUIRE(a.position() == b.position());
}

TEST_CASE("bit_reader and bit_writer move typed values")
{
	std::vector<std::byte> buf(64);
	{
		mh::bit_writer w(buf.data(), buf.size());
		w.write_bit(true);
		w.write_as(1.5f);
		w.write_as(-2.25);
		w.write_as(std::int32_t(-7));
		w.write_as(std::uint16_t(0xBEEF));
	}
	mh::bit_reader r(buf.data(), buf.size());
	CHECK(r.read_bit());
	CHECK(r.read_as<float>() == 1.5f);
	CHECK(r.read_as<double>() == -2.25);
	CHECK(r.read_as<std::int32_t>() == -7);
	CHECK(r.read_as<std::uint16_t>() == 0xBEEF);
}

TEST_CASE("bit_writer output matches the golden decoder")
{
	for (unsigned prefix = 0; prefix < 8; prefix++)
	{
		const auto fields = random_fields(8 * 4000, prefix + 100);
		std::vector<std::byte> buf(4000, std::byte(0xFF));
		{
			mh::bit_writer w(buf.data(), buf.size());
			w.seek(prefix);
			for (const auto& f : fields)
				w.write(f.value, f.bits);
			w.flush();
			CHECK(w.position() == prefix + [&] { std::size_t n = 0; for (auto& f : fields) n += f.bits; return n; }());
		}
		// The bits before the start position keep their old value.
		CHECK(golden_get(buf, 0, prefix) == mask(prefix));
		std::size_t pos = prefix;
		for (const auto& f : fields)
		{
			REQUIRE(golden_get(buf, pos, f.bits) == f.value);
			pos += f.bits;
		}
	}
}

TEST_CASE("bit_writer compile-time widths match run-time widths")
{
	std::vector<std::byte> a(512), b(512);
	{
		mh::bit_writer wa(a.data(), a.size());
		mh::bit_writer wb(b.data(), b.size());
		std::mt19937_64 rng(4);
		for (int i = 0; i < 20; i++)
		{
			const std::uint64_t v = rng();
			wa.write<3>(v);
			wb.write(v, 3);
			wa.write<64>(v);
			wb.write(v, 64);
			wa.write<29>(v);
			wb.write(v, 29);
		}
	}
	CHECK(a == b);
}

TEST_CASE("no access leaves the buffer, and a field past the end fails")
{
	for (std::size_t bytes = 1; bytes <= 24; bytes++)
	{
		for (unsigned bits = 1; bits <= 64; bits++)
		{
			GuardedBuffer buf(bytes);
			std::memset(buf.data(), 0, bytes);
			std::mt19937_64 rng(bytes * 64 + bits);
			std::vector<std::uint64_t> values;
			{
				mh::bit_writer w(buf.data(), bytes);
				while (w.position() + bits <= bytes * 8)
				{
					values.push_back(rng() & mask(bits));
					w.write(values.back(), bits);
				}
				REQUIRE_FALSE(w.failed());
				w.write(0, bits);
				CHECK(w.failed());
				CHECK_THROWS_AS(w.throw_if_failed(), std::out_of_range);
			}

			mh::bit_reader r(buf.data(), bytes);
			for (auto v : values)
				REQUIRE(r.read(bits) == v);
			REQUIRE_FALSE(r.failed());
			CHECK(r.read(bits) == 0);
			CHECK(r.failed());
			CHECK(r.position() == bytes * 8);
			CHECK_THROWS_AS(r.throw_if_failed(), std::out_of_range);
		}
	}
}

TEST_CASE("read_n and write_n match single fields at every offset")
{
	auto check = [&]<unsigned N>()
	{
		for (unsigned prefix = 0; prefix < 8; prefix++)
		{
			std::mt19937_64 rng(N * 100 + prefix);
			std::vector<std::uint64_t> values(200);
			for (auto& v : values)
				v = rng() & mask(N);

			std::vector<std::byte> a(N * 200 / 8 + 8), b(a.size());
			{
				mh::bit_writer wa(a.data(), a.size());
				mh::bit_writer wb(b.data(), b.size());
				wa.write(1, prefix ? prefix : 1);
				wb.write(1, prefix ? prefix : 1);
				wa.write_n<N>(values.data(), values.size());
				for (auto v : values)
					wb.write<N>(v);
				REQUIRE_FALSE(wa.failed());
			}
			REQUIRE(a == b);

			mh::bit_reader r(a.data(), a.size());
			r.skip(prefix ? prefix : 1);
			std::vector<std::uint64_t> got(values.size());
			r.read_n<N>(got.data(), got.size());
			REQUIRE_FALSE(r.failed());
			REQUIRE(got == values);
		}
	};
	check.template operator()<1>();
	check.template operator()<8>();
	check.template operator()<13>();
	check.template operator()<16>();
	check.template operator()<24>();
	check.template operator()<32>();
	check.template operator()<58>();
	check.template operator()<64>();
}

TEST_CASE("read_n and write_n fail as a whole when the run does not fit")
{
	std::vector<std::byte> buf(10);
	std::vector<std::uint64_t> v(11, 0xAB);
	{
		mh::bit_writer w(buf.data(), buf.size());
		w.write_n<8>(v.data(), v.size());
		CHECK(w.failed());
		CHECK(w.position() == 0);
	}
	mh::bit_reader r(buf.data(), buf.size());
	r.read_n<8>(v.data(), v.size());
	CHECK(r.failed());
	CHECK(v == std::vector<std::uint64_t>(11, 0));
}

TEST_CASE("bad widths and seeks fail")
{
	std::vector<std::byte> buf(16);
	{
		mh::bit_reader r(buf.data(), buf.size());
		r.skip(100);
		CHECK(r.position() == 100);
		CHECK(r.remaining_bits() == 28);
		CHECK(r.size_bits() == 128);
		CHECK_FALSE(r.failed());
		CHECK_NOTHROW(r.throw_if_failed());
	}
	for (unsigned bad : { 0u, 65u })
	{
		mh::bit_reader r(buf.data(), buf.size());
		CHECK(r.read(bad) == 0);
		CHECK(r.failed());
	}
	{
		mh::bit_reader r(buf.data(), buf.size());
		r.seek(129);
		CHECK(r.failed());
		CHECK(r.position() == 128);
	}
	{
		mh::bit_writer w(buf.data(), buf.size());
		CHECK(w.size_bits() == 128);
		w.write(0, 0);
		CHECK(w.failed());
	}
	{
		mh::bit_writer w(buf.data(), buf.size());
		w.seek(129);
		CHECK(w.failed());
		CHECK(w.position() == 128);
		CHECK_THROWS_AS(w.throw_if_failed(), std::out_of_range);
	}
}
