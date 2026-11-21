#include "mh/error/exception_details.hpp"

#include <catch2/catch_all.hpp>

#include <atomic>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
	// Exception types deliberately NOT derived from std::exception, so they take
	// the catch (...) path and can only be explained by a registered handler.
	struct custom_error
	{
		int m_Code;
	};

	struct toggled_error_a {};
	struct toggled_error_b {};

	template<typename T>
	class typed_handler final : public mh::exception_details_handler
	{
	public:
		explicit typed_handler(std::string prefix) : m_Prefix(std::move(prefix)) {}

		bool try_handle(const std::exception_ptr& e, mh::exception_details& details) const noexcept override
		{
			try
			{
				std::rethrow_exception(e);
			}
			catch (const T&)
			{
				details.m_Type = &typeid(T);
				details.m_Message = m_Prefix;
				return true;
			}
			catch (...)
			{
			}

			return false;
		}

	private:
		std::string m_Prefix;
	};

	class custom_error_handler final : public mh::exception_details_handler
	{
	public:
		bool try_handle(const std::exception_ptr& e, mh::exception_details& details) const noexcept override
		{
			try
			{
				std::rethrow_exception(e);
			}
			catch (const custom_error& ce)
			{
				details.m_Type = &typeid(custom_error);
				details.m_Message = "custom_error #" + std::to_string(ce.m_Code);
				return true;
			}
			catch (...)
			{
			}

			return false;
		}
	};

	mh::exception_details details_of_thrown_custom_error(int code)
	{
		return mh::exception_details(std::make_exception_ptr(custom_error{ code }));
	}
}

TEST_CASE("exception_details - default constructed", "[error][exception_details]")
{
	const mh::exception_details details;
	CHECK(details.m_Type == nullptr);
	CHECK(details.m_Message.empty());
	CHECK(!details.m_Nested);
	CHECK(details.type_name() == std::string_view("<NULL>"));
}

TEST_CASE("exception_details - std::exception", "[error][exception_details]")
{
	const mh::exception_details details(std::make_exception_ptr(std::runtime_error("something failed")));

	REQUIRE(details.m_Type != nullptr);
	CHECK(*details.m_Type == typeid(std::runtime_error));
	CHECK(details.m_Message == "something failed");
	CHECK(!details.m_Nested);
	CHECK(details.type_name() == std::string_view(typeid(std::runtime_error).name()));
}

TEST_CASE("exception_details - const char*", "[error][exception_details]")
{
	SECTION("non-null string")
	{
		const mh::exception_details details(std::make_exception_ptr("a c string exception"));

		REQUIRE(details.m_Type != nullptr);
		CHECK(*details.m_Type == typeid(const char*));
		CHECK(details.m_Message == "a c string exception");
		CHECK(!details.m_Nested);
	}
	SECTION("null string")
	{
		const mh::exception_details details(std::make_exception_ptr(static_cast<const char*>(nullptr)));

		REQUIRE(details.m_Type != nullptr);
		CHECK(*details.m_Type == typeid(const char*));
		CHECK(details.m_Message == "<NULL>");
	}
}

TEST_CASE("exception_details - std::string", "[error][exception_details]")
{
	// std::string is not a std::exception; it must be caught by its own clause
	const mh::exception_details details(std::make_exception_ptr(std::string("a string exception")));

	REQUIRE(details.m_Type != nullptr);
	CHECK(*details.m_Type == typeid(std::string));
	CHECK(details.m_Message == "a string exception");
	CHECK(!details.m_Nested);
}

TEST_CASE("exception_details - nested exception chain", "[error][exception_details]")
{
	std::exception_ptr outer;
	try
	{
		try
		{
			throw std::runtime_error("inner");
		}
		catch (...)
		{
			std::throw_with_nested(std::runtime_error("outer"));
		}
	}
	catch (...)
	{
		outer = std::current_exception();
	}

	const mh::exception_details outerDetails(outer);

	// The thrown object's dynamic type is an unspecified type derived from both
	// std::nested_exception and std::runtime_error; only behavior is portable.
	REQUIRE(outerDetails.m_Type != nullptr);
	CHECK(outerDetails.m_Message == "outer");
	REQUIRE(outerDetails.m_Nested);

	const mh::exception_details innerDetails(outerDetails.m_Nested);
	REQUIRE(innerDetails.m_Type != nullptr);
	CHECK(*innerDetails.m_Type == typeid(std::runtime_error));
	CHECK(innerDetails.m_Message == "inner");
	CHECK(!innerDetails.m_Nested); // the innermost exception has no further nesting
}

TEST_CASE("exception_details - unknown exception type without a handler", "[error][exception_details]")
{
	const mh::exception_details details(std::make_exception_ptr(42));

	CHECK(details.m_Type == nullptr);
	CHECK(details.m_Message == "<unknown>");
	CHECK(details.type_name() == std::string_view("<NULL>"));
}

TEST_CASE("exception_details - custom handler explains unknown exception types", "[error][exception_details]")
{
	const custom_error_handler handlerImpl;

	{
		const auto registration = mh::exception_details::add_handler(typeid(custom_error), handlerImpl);

		const auto details = details_of_thrown_custom_error(42);
		REQUIRE(details.m_Type != nullptr);
		CHECK(*details.m_Type == typeid(custom_error));
		CHECK(details.m_Message == "custom_error #42");
	}

	// registration destroyed -> handler removed -> back to <unknown>
	const auto details = details_of_thrown_custom_error(42);
	CHECK(details.m_Type == nullptr);
	CHECK(details.m_Message == "<unknown>");
}

TEST_CASE("exception_details - duplicate handler registration fails without stealing", "[error][exception_details]")
{
	const custom_error_handler first;
	const custom_error_handler second;

	const auto firstRegistration = mh::exception_details::add_handler(typeid(custom_error), first);

	{
		// Registering a second handler for the same type must fail; destroying
		// the returned (inactive) handler must NOT unregister the first one.
		const auto secondRegistration = mh::exception_details::add_handler(typeid(custom_error), second);
	}

	const auto details = details_of_thrown_custom_error(7);
	REQUIRE(details.m_Type != nullptr);
	CHECK(details.m_Message == "custom_error #7");
}

TEST_CASE("exception_details - handler move constructor transfers ownership", "[error][exception_details]")
{
	const custom_error_handler handlerImpl;

	{
		mh::exception_details::handler outerRegistration;

		{
			auto innerRegistration = mh::exception_details::add_handler(typeid(custom_error), handlerImpl);
			outerRegistration = std::move(innerRegistration);
			// innerRegistration destroyed here: it gave up ownership, so the
			// handler must remain registered
		}

		CHECK(details_of_thrown_custom_error(1).m_Message == "custom_error #1");

		mh::exception_details::handler movedTo(std::move(outerRegistration));
		CHECK(details_of_thrown_custom_error(2).m_Message == "custom_error #2");
	}

	// the final owner went out of scope -> unregistered
	CHECK(details_of_thrown_custom_error(3).m_Message == "<unknown>");
}

TEST_CASE("exception_details - handler move assignment releases the previous registration", "[error][exception_details]")
{
	const typed_handler<toggled_error_a> handlerA("handler A");
	const typed_handler<toggled_error_b> handlerB("handler B");

	auto registrationA = mh::exception_details::add_handler(typeid(toggled_error_a), handlerA);
	auto registrationB = mh::exception_details::add_handler(typeid(toggled_error_b), handlerB);

	CHECK(mh::exception_details(std::make_exception_ptr(toggled_error_a{})).m_Message == "handler A");
	CHECK(mh::exception_details(std::make_exception_ptr(toggled_error_b{})).m_Message == "handler B");

	// B's registration is replaced by A's: B must unregister immediately
	registrationB = std::move(registrationA);

	CHECK(mh::exception_details(std::make_exception_ptr(toggled_error_a{})).m_Message == "handler A");
	CHECK(mh::exception_details(std::make_exception_ptr(toggled_error_b{})).m_Message == "<unknown>");
}

TEST_CASE("exception_details - handler registry thread safety smoke", "[error][exception_details]")
{
	const custom_error_handler handlerImpl;
	const auto registration = mh::exception_details::add_handler(typeid(custom_error), handlerImpl);

	const typed_handler<toggled_error_a> handlerA("handler A");
	const typed_handler<toggled_error_b> handlerB("handler B");

	std::atomic<bool> stop = false;

	// Two threads register/unregister their own handler types while this thread
	// keeps formatting exceptions through the registry.
	std::thread toggleA([&]
		{
			while (!stop)
				const auto reg = mh::exception_details::add_handler(typeid(toggled_error_a), handlerA);
		});
	std::thread toggleB([&]
		{
			while (!stop)
				const auto reg = mh::exception_details::add_handler(typeid(toggled_error_b), handlerB);
		});

	bool allCorrect = true;
	for (int i = 0; i < 500; i++)
	{
		// permanently registered: must be handled no matter what the togglers do
		if (details_of_thrown_custom_error(i).m_Message != ("custom_error #" + std::to_string(i)))
			allCorrect = false;

		// toggled: handled or unknown depending on timing, never anything else
		const auto toggled = mh::exception_details(std::make_exception_ptr(toggled_error_a{})).m_Message;
		if (toggled != "handler A" && toggled != "<unknown>")
			allCorrect = false;
	}

	stop = true;
	toggleA.join();
	toggleB.join();

	CHECK(allCorrect);
}
