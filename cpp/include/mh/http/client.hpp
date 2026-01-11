#pragma once

#include "status_code.hpp"
#include <mh/coroutine/task.hpp>
#include <string>
#include <unordered_map>

namespace mh::http
{
	struct response
	{
		status_code status;
		std::unordered_map<std::string, std::string> headers;
		std::string body;
	};

	// Coroutine-based HTTP GET
	task<response> get(const std::string& url);
}