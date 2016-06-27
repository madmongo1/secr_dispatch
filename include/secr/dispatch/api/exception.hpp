#pragma once

#include <secr/dispatch/exception.pb.h>
#include <secr/dispatch/api/shared_ptr.hpp>

#include <utility>
#include <memory>

namespace secr { namespace dispatch { namespace api {
    

    
    Exception& populate (Exception& emsg, const std::exception& e);
    Exception& populate (Exception& emsg, const std::exception_ptr& ep);

    Exception* populate (Exception* emsg, const std::exception& e);
    Exception* populate (Exception* emsg, const std::exception_ptr& ep);
    Exception* populate (Exception* emsg, const char* text);
	
    inline
    auto create_message(const std::exception_ptr& ep,
                        std::shared_ptr<google::protobuf::Arena> arena = nullptr)
    {
        auto msg = create_message<Exception>(arena);
        populate(msg.get(), ep);
        return msg;
    }
    
    ExceptionList& populate (ExceptionList& emsg, const std::vector<std::exception_ptr>& es);
    ExceptionList* populate (ExceptionList* emsg, const std::vector<std::exception_ptr>& es);
    
    
}}}