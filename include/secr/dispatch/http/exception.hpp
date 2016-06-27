#pragma once

#include <secr/dispatch/config.hpp>
#include <stdexcept>
#include <exception>

namespace secr { namespace dispatch { namespace http {

	struct invalid_header : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	struct invalid_url : invalid_header
	{
		using invalid_header::invalid_header;
	};

	struct transport_error : system_error
	{
		using system_error::system_error;
	};
	
}}}