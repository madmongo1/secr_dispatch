#pragma once

#include <secr/dispatch/http/execution_promise.hpp>

namespace secr { namespace dispatch { namespace http {

    struct execution_context
    {
        dispatch_promise& promise();
        http_read_stream& read_stream();
        http_write_stream& write_stream();
    };
    
    struct dispatcher
    {
        using dispatch_function = std::function<void(http_execution_context&)>;
        
    };

}}}