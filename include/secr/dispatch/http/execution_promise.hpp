#pragma once
#include <exception>

namespace secr { namespace dispatch { namespace http {
    
    struct request_context;
    
    struct execution_promise
    {
        void complete_with_error(std::exception_ptr);
        void complete_success();
        
        request_context* _request_context;
    };
    
}}}
