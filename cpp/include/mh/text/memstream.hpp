#pragma once

#include <cassert>
#include <istream>
#include <ostream>
#include <string>
#include <string.h>

namespace mh
{
	namespace detail::memstream_hpp
	{
		template<typename T> const T& min(const T& a, const T& b) { return a < b ? a : b; }
	}

	template<typename CharT = char, typename Traits = std::char_traits<CharT>>
	class basic_memstreambuf : public std::basic_streambuf<CharT, Traits>
	{
	public:
		using base_streambuf_type = std::basic_streambuf<CharT, Traits>;
		using int_type = typename base_streambuf_type::int_type;
		using pos_type = typename base_streambuf_type::pos_type;
		using off_type = typename base_streambuf_type::off_type;

		basic_memstreambuf(CharT* buf, size_t size, size_t existingSize = 0)
		{
			if (existingSize > size)
				throw std::invalid_argument("existingSize cannot be larger than size");

			assert(buf);

			this->setp(buf + existingSize, buf + size - existingSize);
			this->setg(buf, buf, buf + existingSize);
		}

		auto view() const { return std::basic_string_view<CharT, Traits>(gcur(), gend() - gcur()); }
		auto view_full() const { return std::basic_string_view<CharT, Traits>(gbeg(), gend() - gbeg()); }

	protected:
		base_streambuf_type* setbuf(CharT* s, std::streamsize n) override
		{
			this->setp(s, s + n);
			this->setg(s, s, s);
			return this;
		}

		using base_streambuf_type::setp;
		void setp(CharT* pbeg, CharT* pcur, CharT* pend)
		{
			this->setp(pbeg, pend);
			this->pbump(pcur - pbeg);
		}

		pos_type seekpos(pos_type pos, std::ios_base::openmode which) override
		{
			if (which & std::ios_base::in)
			{
				const auto newPos = gbeg() + pos;
				[[maybe_unused]] const auto startDelta = newPos - gbeg();
				[[maybe_unused]] const auto endDelta = length_g() - pos;
				assert(startDelta >= 0);
				assert(endDelta >= 0);
				this->setg(gbeg(), newPos, gend());
			}
			if (which & std::ios_base::out)
			{
				const auto newPos = pbeg() + pos;
				[[maybe_unused]] const auto startDelta = newPos - pbeg();
				[[maybe_unused]] const auto endDelta = length_p() - pos;
				assert(startDelta >= 0);
				assert(endDelta >= 0);
				this->setp(pbeg(), newPos, pend());
			}

			return pos;
		}

		pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override
		{
			if (dir == std::ios_base::beg)
			{
				return seekpos(off, which);
			}
			else if (dir == std::ios_base::end)
			{
				if (!(which & std::ios::in) && !(which & std::ios::out))
					throw std::invalid_argument("'which' must be one or a combination of 'in' and 'out'");

				pos_type result;
				if (which & std::ios::in)
					result = seekpos((gend() - gbeg()) - off, std::ios::in);
				if (which & std::ios::out)
					result = seekpos((pend() - pbeg()) - off, std::ios::out);

				return result;
			}
			else if (dir == std::ios_base::cur)
			{
				constexpr auto both = std::ios_base::in | std::ios_base::out;
				if ((which & both) == both)
				{
					if (gcur() != pcur())
					{
						assert(false);
						throw std::runtime_error("Cannot seek relative to current position if both in and out are at different offsets!");
					}

					return seekpos((gcur() - gbeg()) + off, both);
				}
				else if (which & std::ios_base::in)
					return seekpos((gcur() - gbeg()) + off, std::ios_base::in);
				else if (which & std::ios_base::out)
					return seekpos((pcur() - pbeg()) + off, std::ios_base::out);
				else
				{
					assert(false);
					throw std::invalid_argument("Unexpected value for \"which\"");
				}
			}
			else
			{
				assert(false);
				throw std::invalid_argument("Unknown seekdir");
			}
		}

		std::streamsize xsputn(const CharT* s, std::streamsize count) override
		{
			count = detail::memstream_hpp::min<off_t>(count, remaining_p());
			auto ptr = pcur();
			for (std::streamsize i = 0; i < count; i++)
			{
				if (s[i] == Traits::eof())
				{
					count = i;
					break;
				}

				ptr[i] = s[i];
			}

			this->pbump(count);
			ptr[count] = {};

			update_get_area_size();
			return count;
		}

		int_type overflow(int_type ch = Traits::eof()) override
		{
			if (ch != Traits::eof())
			{
				if (pcur() == pend())
					return Traits::eof(); // No more room

				*pcur() = static_cast<CharT>(ch);
				this->pbump(1);
				*pcur() = 0;
				update_get_area_size();
			}

			return ch;
		}

	private:
		CharT* pbeg() const { return this->pbase(); }
		CharT* pcur() const { return this->pptr(); }
		CharT* pend() const { return this->epptr(); }
		CharT* gbeg() const { return this->eback(); }
		CharT* gcur() const { return this->gptr(); }
		CharT* gend() const { return this->egptr(); }

		off_type length_p() const { return pend() - pbeg(); }
		off_type length_g() const { return gend() - gbeg(); }
		off_type remaining_p() const { return pend() - pcur() - 1; }
		off_type remaining_g() const { return gend() - gcur() - 1; }

		void update_get_area_size()
		{
			//auto getAreaSize = gend() - gbeg();
			auto minPutAreaSize = pcur() - pbeg();
			//if (getAreaSize < minPutAreaSize)
			{
				const auto newEnd = gbeg() + minPutAreaSize;
				this->setg(gbeg(), detail::memstream_hpp::min(gcur(), newEnd), newEnd);
			}
		}
	};

	template<typename CharT = char, typename Traits = std::char_traits<CharT>>
	class basic_memstream final : public basic_memstreambuf<CharT, Traits>, public std::basic_iostream<CharT, Traits>
	{
		using iostream_type = std::basic_iostream<CharT, Traits>;
		using streambuf_type = basic_memstreambuf<CharT, Traits>;
		using int_type = typename streambuf_type::int_type;
		using pos_type = typename streambuf_type::pos_type;
		using off_type = typename streambuf_type::off_type;

	public:
		basic_memstream() : iostream_type(this) {}
		template<size_t size> basic_memstream(CharT (&buf)[size]) : basic_memstream(buf, size) {}
		basic_memstream(CharT* buf, size_t size) :
			streambuf_type(buf, size),
			iostream_type(this)
		{
			assert(buf);
		}
	};

	using memstream = basic_memstream<>;
}
