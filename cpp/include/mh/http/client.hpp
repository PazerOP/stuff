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

	// Coroutine-based HTTP GET.
	// The URL is taken BY VALUE on purpose: this coroutine hops to another thread,
	// and a reference parameter would dangle once the caller's argument dies.
	task<response> get(std::string url);
}