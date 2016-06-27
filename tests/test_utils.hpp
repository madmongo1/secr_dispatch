#pragma once

#include <chrono>
#include <thread>
#include <future>
#include <utility>
#include <array>
#include <vector>
#include <valuelib/debug/unwrap.hpp>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

std::chrono::milliseconds a_while();
std::chrono::milliseconds a_moment();


using namespace std::literals;

template<class Duration>
testing::AssertionResult spins_once_within(boost::asio::io_service& io_service, Duration duration)
{
    auto t0 = std::chrono::system_clock::now();
    auto t_limit = t0 + duration;
    
    bool first = true;
    do {
        if (io_service.stopped()) {
            io_service.reset();
        }

        if (first) { first = false; }
        else { std::this_thread::sleep_for(1ms); }

        try {
            auto spins = io_service.poll_one();
            if (spins) {
                return testing::AssertionSuccess();
            }
        }
        catch(const std::exception& e) {
            return testing::AssertionFailure() << "exception: " << e.what();
        }
    } while(std::chrono::system_clock::now() < t_limit);
    return testing::AssertionFailure() << "timeout";
}


/// open an acceptor and stard listening on any available port
template<class AcceptorType>
testing::AssertionResult listen_anywhere(AcceptorType& acceptor)
{
    using acceptor_type = AcceptorType;
    using protocol = typename acceptor_type::protocol_type;
    using resolver_type = typename protocol::resolver;
    using query_type = typename protocol::resolver::query;
    
    resolver_type resolver(acceptor.get_io_service());
    boost::system::error_code err;
    for (int port = 4000 ; port < 20000 ; ++port)
    {
        query_type query { "localhost", std::to_string(port) };
        auto iter = resolver.resolve(query);
        assert(iter != typename resolver_type::iterator());
        if (acceptor.open(iter->endpoint().protocol(), err))
            continue;
        if (acceptor.bind(*iter, err)) {
            acceptor.close();
            continue;
        }
        if (acceptor.listen(5, err)) {
            acceptor.close();
            continue;
        }
        return testing::AssertionSuccess();
    }
    return testing::AssertionFailure() << "Cannot listen. Last error was " << err << " " << err.message();
}

template<class T>
std::exception_ptr has_error(std::future<T>& future)
{
    try {
        future.get();
        return {};
    }
    catch(...) {
        return std::current_exception();
    }
}

/// allow chaining of AssertionResults with subsequent functions
template<class F>
testing::AssertionResult operator and(testing::AssertionResult l, F&& r)
{
    if (l) {
        return r();
    }
    return l;
}

template<class SocketType>
testing::AssertionResult tie_sockets(SocketType& client_socket, SocketType& server_socket)
{
    using socket_type = SocketType;
    using protocol = typename socket_type::protocol_type;
    using acceptor_type = typename protocol::acceptor;
    
    auto acceptor = acceptor_type(server_socket.get_io_service());
    return
    listen_anywhere(acceptor)
    and [&] {
        
        auto accept_future = std::async(std::launch::async, [&]{
            acceptor.accept(server_socket);
        });
        
        auto connect_future = std::async(std::launch::async, [&]{
            client_socket.connect(acceptor.local_endpoint());
        });
        
        auto connect_failure = has_error(accept_future),
        accept_failure = has_error(connect_future);
        
        if (connect_failure or accept_failure)
        {
            return testing::AssertionFailure()
            << "connect: " << value::debug::unwrap(connect_failure)
            << "accept : " << value::debug::unwrap(accept_failure);
        }
        return testing::AssertionSuccess();
    };
}


template<class T, std::size_t N, std::size_t...Is>
auto buffers_of(std::index_sequence<Is...>, const std::array<T, N>& array) -> std::array<boost::asio::const_buffers_1, N>
{
    return {{ boost::asio::buffer(array[Is])... }};
}

template<class T, std::size_t N>
auto buffers_of(const std::array<T, N>& sources)
{
    return buffers_of(std::make_index_sequence<N>(), sources);
}

template<class Iter>
auto buffers_of(Iter first, Iter last)
{
    std::vector<boost::asio::const_buffer> result;
    result.reserve(std::distance(first, last));
    std::transform(first, last,
                   std::back_inserter(result),
                   [](const auto& v) { return boost::asio::buffer(v); });
    return result;
}

template<class F>
testing::AssertionResult no_exception(F&& f)
{
    try {
        f();
        return testing::AssertionSuccess();
    }
    catch(...)
    {
        return testing::AssertionFailure() << "exception occurred:\n" << value::debug::unwrap();
    }
}

template<class T, class F>
testing::AssertionResult equal_no_exception(T&& expected, F&& f)
{
    try {
        auto result = f();
        if (result == expected)
        {
            return testing::AssertionSuccess();
        }
        else
        {
            return testing::AssertionFailure() << "expected: " << expected << "\nreturned: " << result;
        }
    }
    catch(...)
    {
        return testing::AssertionFailure() << "exception occurred:\n" << value::debug::unwrap();
    }
}

template<class Exception, class F>
testing::AssertionResult throws(F&& f)
{
    try {
        f();
        return testing::AssertionFailure() << "no exception thrown";
    }
    catch(const Exception&)
    {
        return testing::AssertionSuccess();
    }
    catch(...)
    {
        return testing::AssertionFailure() << "wrong exception type:\n" << value::debug::unwrap();
    }
}


