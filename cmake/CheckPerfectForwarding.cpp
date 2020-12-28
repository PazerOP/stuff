#include <string>
#include <utility>

struct MyType
{
	MyType(int&& i, std::string&& s, float&& f) :
		intArg(std::move(i)), stringArg(std::move(s)), floatArg(std::move(f))
	{
	}

	int intArg;
	std::string stringArg;
	float floatArg;
};

template<typename... TArgs>
static void TestForwarding(TArgs&&... args)
{
	MyType type(std::forward<TArgs>(args)...);
}

int main(int argc, char** argv)
{
	TestForwarding(3, "hello world", 42.1234f);
}
