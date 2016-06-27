#pragma
#include <secr/dispatch/config.hpp>

namespace secr { namespace dispatch { namespace http {


	enum class protocol_error_code
	{
		missing_status_line,
        response_mode_not_set,
	};


    const error_category& http_error_category();
    error_code make_error_code(protocol_error_code code);
    error_condition make_error_condition(protocol_error_code code);

}}}

namespace boost { namespace system {
    template<>
    struct is_error_code_enum<secr::dispatch::http::protocol_error_code>
    : std::true_type {};
    
    template<>
    struct is_error_condition_enum<secr::dispatch::http::protocol_error_code>
    : std::true_type {};
}}

