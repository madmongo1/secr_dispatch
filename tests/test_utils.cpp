#include "test_utils.hpp"
#include <chrono>

static constexpr bool debugging = true;

using namespace std::literals;

std::chrono::milliseconds a_while()
{
    
	return debugging ? 600000ms : 5000ms;
}

std::chrono::milliseconds a_moment()
{
	return debugging ? 600000ms : 5ms;
}

