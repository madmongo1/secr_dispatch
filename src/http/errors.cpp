#include <secr/dispatch/http/errors.hpp>

namespace secr { namespace dispatch { namespace http {

    namespace {

        struct _protocol_error_category : error_category
        {
            const char *     name() const noexcept override {
                return "secr::dispatch::http::protocol_error";
            }
            
            std::string message( int ev ) const override
            {
                switch (static_cast<protocol_error_code>(ev))
                {
                    case protocol_error_code::missing_status_line:
                        return "missing status line";
                        
                    case protocol_error_code::response_mode_not_set:
                        return "response mode not set";
                        
                    default:
                        return "unknown error: " + std::to_string(ev);
                }
            }
        };
    }
    
    const error_category& http_error_category()
    {
        static const _protocol_error_category _ {};
        return _;
    };
    
    error_code make_error_code(protocol_error_code code)
    {
        return error_code(static_cast<int>(code), http_error_category());
    }
    
    error_condition make_error_condition(protocol_error_code code)
    {
        return error_condition(static_cast<int>(code), http_error_category());
    }
    
    
}}}
