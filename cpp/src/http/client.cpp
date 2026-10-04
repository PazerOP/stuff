#include <mh/http/client.hpp>
#include <mh/coroutine/task.hpp>
#include <mh/coroutine/thread.hpp>
#include <curl/curl.h>
#include <memory>
#include <stdexcept>

namespace mh::http
{
	namespace
	{
		struct curl_deleter
		{
			void operator()(CURL* curl) const noexcept
			{
				if (curl)
				{
					curl_easy_cleanup(curl);
				}
			}
		};

		using curl_handle = std::unique_ptr<CURL, curl_deleter>;

		curl_handle make_curl_handle()
		{
			// curl_easy_init()'s implicit global init is not thread-safe before curl
			// 7.84; do it exactly once ourselves (thread-safe magic static).
			static const CURLcode global_init = curl_global_init(CURL_GLOBAL_DEFAULT);
			if (global_init != CURLE_OK)
			{
				throw std::runtime_error("curl_global_init failed");
			}

			CURL* curl = curl_easy_init();
			if (!curl)
			{
				throw std::runtime_error("Failed to initialize curl");
			}
			return curl_handle{curl};
		}

		// Callback for writing response data
		size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output)
		{
			size_t total_size = size * nmemb;
			output->append(static_cast<char*>(contents), total_size);
			return total_size;
		}

		// Callback for writing headers
		size_t header_callback(void* contents, size_t size, size_t nmemb, std::unordered_map<std::string, std::string>* headers)
		{
			size_t total_size = size * nmemb;
			std::string header(static_cast<char*>(contents), total_size);
			
			// Parse header "Key: Value\r\n"
			auto colon_pos = header.find(':');
			if (colon_pos != std::string::npos)
			{
				std::string key = header.substr(0, colon_pos);
				std::string value = header.substr(colon_pos + 1);
				
				// Trim whitespace
				value.erase(0, value.find_first_not_of(" \t"));
				value.erase(value.find_last_not_of(" \t\r\n") + 1);
				
				(*headers)[key] = value;
			}
			
			return total_size;
		}
	}

	response get_sync(const std::string& url)
	{
		auto curl = make_curl_handle();
		response resp;
		long response_code = 0;

		// Set URL and callbacks
		curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &resp.body);
		curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, header_callback);
		curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &resp.headers);
		curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);

		// Perform the request
		CURLcode res = curl_easy_perform(curl.get());
		
		if (res == CURLE_OK)
		{
			curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
			resp.status.value = static_cast<uint16_t>(response_code);
		}
		else
		{
			throw std::runtime_error("HTTP request failed: " + std::string(curl_easy_strerror(res)));
		}

		return resp;
	}

	// url is taken by value: reference coroutine parameters are stored in the frame
	// as references, and this coroutine evaluates url on another thread after the
	// caller's full-expression (and any temporary argument) is long gone.
	task<response> get(std::string url)
	{
		// Always hop off the calling thread for the blocking curl operation.
		// co_create_background_thread() only moves off the MAIN thread, which made
		// this eagerly-started coroutine fully synchronous (up to CURLOPT_TIMEOUT)
		// when called from any worker thread.
		co_await co_create_thread();

		auto curl = make_curl_handle();
		response resp;
		long response_code = 0;

		curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &resp.body);
		curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, header_callback);
		curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &resp.headers);
		curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);

		CURLcode res = curl_easy_perform(curl.get());
		
		if (res == CURLE_OK)
		{
			curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
			resp.status.value = static_cast<uint16_t>(response_code);
		}
		else
		{
			throw std::runtime_error("HTTP request failed: " + std::string(curl_easy_strerror(res)));
		}

		co_return resp;
	}
}