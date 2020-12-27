#pragma once

#include <exception>
#include <string>
#include <typeinfo>

namespace mh
{
	struct exception_details;

	class exception_details_handler
	{
	public:
		virtual ~exception_details_handler() = default;

		virtual bool try_handle(const std::exception_ptr& e, exception_details& details) const noexcept = 0;
	};

	struct exception_details
	{
		exception_details() noexcept = default;
		exception_details(const std::exception_ptr& e);

		const std::type_info* m_Type = nullptr;
		std::string m_Message;

		const char* type_name() const noexcept;

		// Adds a new handler. Returns false if there is already a handler for this type.
		static bool add_handler(const std::type_info& type, const exception_details_handler& handler);
	};
}

#ifndef MH_COMPILE_LIBRARY
#include "exception_details.inl"
#endif
