#pragma once

#include <cstdint>
#include <compare>

namespace mh::http
{
	struct status_code
	{
	private:
		enum class code : uint16_t
		{
			// 2xx Success
			ok = 200,
			created = 201,
			no_content = 204,
			
			// 3xx Redirection
			moved_permanently = 301,
			found = 302,
			not_modified = 304,
			
			// 4xx Client Error
			bad_request = 400,
			unauthorized = 401,
			forbidden = 403,
			not_found = 404,
			conflict = 409,
			
			// 5xx Server Error
			internal_server_error = 500,
			bad_gateway = 502,
			service_unavailable = 503
		};

	public:
		uint16_t value = 0;

		// Using declarations to bring enum values into struct scope
		using enum code;

		// Constructors
		constexpr status_code() = default;
		explicit constexpr status_code(uint16_t v) : value(v) {}
		constexpr status_code(code c) : value(static_cast<uint16_t>(c)) {}

		// Member functions
		constexpr bool is_informational() const noexcept
		{
			return value >= 100 && value < 200;
		}

		constexpr bool is_success() const noexcept
		{
			return value >= 200 && value < 300;
		}

		constexpr bool is_redirection() const noexcept
		{
			return value >= 300 && value < 400;
		}

		constexpr bool is_client_error() const noexcept
		{
			return value >= 400 && value < 500;
		}

		constexpr bool is_server_error() const noexcept
		{
			return value >= 500 && value < 600;
		}

		constexpr bool is_error() const noexcept
		{
			return is_client_error() || is_server_error();
		}

		// Comparison operators
		constexpr bool operator==(const status_code& other) const noexcept = default;
		constexpr auto operator<=>(const status_code& other) const noexcept = default;
	};
}