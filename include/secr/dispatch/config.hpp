#pragma once
#include <valuelib/debug/trace.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <future>

namespace secr { namespace dispatch {
  
    namespace asio = boost::asio;
    using error_code = boost::system::error_code;
    using error_condition = boost::system::error_condition;
    using error_category = boost::system::error_category;
    using system_error = boost::system::system_error;
    namespace errc = boost::system::errc;
    
    template<class Type> using future = std::future<Type>;
    template<class Type> using shared_future = std::shared_future<Type>;
    template<class Type> using promise = std::promise<Type>;
    
    
    
}}

#define SECR_DISPATCH_TRACE 0

#if SECR_DISPATCH_TRACE
#define SECR_DISPATCH_TRACE_METHOD_N(CLASS,METHOD,...) value::debug::tracer _secr_dispatch_local_tracer(std::clog, value::debug::classname(CLASS), value::debug::method(METHOD), __VA_ARGS__)

#define SECR_DISPATCH_TRACE_METHOD(CLASS,METHOD) value::debug::tracer _secr_dispatch_local_tracer(std::clog, value::debug::classname(CLASS), value::debug::method(METHOD))


#else
#define SECR_DISPATCH_TRACE_METHOD_N(CLASS,METHOD,...)
#define SECR_DISPATCH_TRACE_METHOD(class,method)

#endif
