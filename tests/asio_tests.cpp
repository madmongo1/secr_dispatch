
#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <future>
#include <tuple>
#include <random>
#include <boost/system/system_error.hpp>
#include <boost/optional.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <google/protobuf/arena.h>
#include <google/protobuf/message.h>
#include <regex>
#include <secr/dispatch/string_view.hpp>

#include <secr/dispatch/http/server_connection.hpp>
#include "test_utils.hpp"

#include <secr/dispatch/fake_stream.hpp>
#include <valuelib/stdext/exception.hpp>
#include <secr/dispatch/api/exception.hpp>

TEST(fake_stream_tests, test1)
{
    using namespace std::literals;
    using namespace secr::dispatch;
    
    asio::io_service read_service, write_service;
    
    fake_stream frs(read_service, write_service);
    
    auto s = "Hello, World"s;
    error_code write_error;
    auto written = asio::write(frs, asio::buffer(s), write_error);
    EXPECT_EQ(12, written);
    EXPECT_EQ(error_code(), write_error);
    
    frs.set_error(asio::error::misc_errors::eof);
    
    written = asio::write(frs, asio::buffer(s), write_error);
    EXPECT_EQ(0, written);
    EXPECT_EQ(asio::error::misc_errors::eof, write_error);
    
    
    
    char read_buffer[12];
    
    error_code read_error;
    std::size_t read_bytes = 0;
    asio::async_read(frs, asio::buffer(read_buffer), [&](const error_code& ec, std::size_t bytes){
        read_bytes = bytes;
        read_error = ec;
    });
    
    EXPECT_TRUE(spins_once_within(read_service, a_moment()));
    
    EXPECT_EQ(read_bytes, 12);
    EXPECT_EQ(error_code(), read_error) << read_error.message();
    EXPECT_EQ("Hello, World"_sv, string_view(std::begin(read_buffer), 12));
    
    asio::async_read(frs, asio::buffer(read_buffer), [&](const error_code& ec, std::size_t bytes){
        read_bytes = bytes;
        read_error = ec;
    });
    
    EXPECT_TRUE(spins_once_within(read_service, a_moment()));
    EXPECT_FALSE(spins_once_within(read_service, 10ms));
    EXPECT_FALSE(spins_once_within(write_service, 10ms));
    
    EXPECT_EQ(read_bytes, 0);
    EXPECT_EQ(error_code(asio::error::misc_errors::eof), read_error) << read_error.message();
    
    std::array<std::string, 3> x {
        {
            "the quick brown fox"s,
            " jumps over the lazy dog"s,
            " while the dog munches a bone!"s
        }
    };
    
    std::vector<std::vector<char>> bits;
    auto total = std::accumulate(std::begin(x), std::end(x), 0, [](auto x, auto&s) { return x + s.size(); });
    auto bit_size = total / 6;
    for (auto to_do = total ; to_do ; )
    {
        bit_size = std::min(bit_size, to_do);
        bits.emplace_back(bit_size);
        to_do -= bit_size;
    }
    
    frs.reset();
    
    asio::streambuf rsb;
    asio::async_read_until(frs, rsb, '!', [&](auto& ec, auto size) {
        read_bytes = size;
        read_error = ec;
    });
    auto buffers_of = [](auto& x) {
        using element_type = decltype(asio::buffer(*x.begin()));
        std::vector<element_type> result;
        std::transform(std::begin(x), std::end(x), std::back_inserter(result),
                       [](auto& item) { return asio::buffer(item); });
        return result;
    };
    asio::write(frs, buffers_of(x), write_error);
    EXPECT_FALSE(write_error);
    
    EXPECT_TRUE(spins_once_within(read_service, a_moment()));
    EXPECT_FALSE(spins_once_within(read_service, 10ms));
    EXPECT_FALSE(spins_once_within(write_service, 10ms));
    
    EXPECT_EQ(total, rsb.size());
    auto join = [](auto first, auto last) {
        std::decay_t<decltype(*first)> result{};
        while (first != last)
        {
            result.insert(result.end(), first->begin(), first->end());
            ++first;
        }
        return result;
    };
    auto joined = join(x.begin(), x.end());
    std::vector<char> moov(read_bytes);
    auto dat = rsb.data();
    asio::buffer_copy(asio::buffer(moov), asio::buffer(dat));
    auto moo = std::string(moov.begin(), moov.end());
    EXPECT_EQ(joined, moo);
    
}

template<class Executor, class R, class Arg>
struct future_call
{
    future_call(Executor& exec, std::function<R(std::shared_future<Arg>)> f)
    : _executor(exec, _function(std::move(f)))
    {
    }
    
    void set_exception(std::exception_ptr e)
    {
        _promise.set_exception(e);
        dispatch();
    }
    
    template<class...Args>
    void set_values(Args&&...args)
    {
        _promise.set_value(args...);
        dispatch();
    }
    
    void dispatch()
    {
        _executor.dispatch([this]{
            std::promise<R> return_promise;
            try {
                return_promise.set_value(function(_promise.get_future()));
            }
            catch(...)
            {
                return_promise.set_exception(std::current_exception());
            }
            _completion_handler(return_promise.get_future());
        });
    }
    
    Executor& _executor;
    
    std::promise<Arg> _promise;
    
    std::function<R(std::shared_future<Arg>)> _function;
    std::function<std::future<R>> _completion_handler;
};

template<class...Calls>
struct future_call_list
{
    
    template<class...Args>
    void operator()(const boost::system::error_code& ec, Args...args)
    {
        auto& future_call = std::get<0>(_calls);
        if (ec) {
            call_item<0>(boost::system::system_error(ec));
        }
        else {
            future_call.set_value(std::forward<Args>(args)...);
        }
    }
    
    template<std::size_t N>
    void call_item(const std::exception& e)
    {
        
    }
    
    
    std::tuple<Calls...> _calls;
};


template<class T, class...Args>
auto make_array(Args&&...args) -> std::array<T, sizeof...(Args)>
{
    return std::array<T, sizeof...(Args)> {
        {T(std::forward<Args>(args))...}
    };
}

template<class T>
::testing::AssertionResult ready(const std::shared_future<T>& f)
{
    if (not f.valid())
        return ::testing::AssertionFailure() << "not valid";
    if (f.wait_for(0ms) == std::future_status::ready)
        return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure() << "not ready";
}

template<class T>
std::exception_ptr is_exception(const std::shared_future<T>& sf)
{
    try {
        sf.get();
        return {};
    }
    catch(...) {
        return std::current_exception();
    }
}

template<class T, class Exception>
std::exception_ptr is_exception(const std::shared_future<T>& sf)
{
    try {
        sf.get();
        return {};
    }
    catch(const Exception&) {
        return std::current_exception();
    }
    return {};
}

std::string to_string(secr::dispatch::asio::streambuf& streambuf)
{
    auto buffer = streambuf.data();
    auto first = secr::dispatch::asio::buffer_cast<const char*>(buffer);
    auto size = buffer_size(buffer);
    auto result = std::string(first, first + size);
    streambuf.consume(size);
    return result;
}



template<class Duration>
std::string consume_available_in(boost::asio::ip::tcp::socket& socket, Duration duration)
{
    std::string result;
    std::vector<char> vbuf(128);
    auto was_blocking = socket.non_blocking();

    socket.non_blocking(true);
    
    auto now = std::chrono::high_resolution_clock::now();
    auto limit = now + duration;
    bool first = true;
    
    boost::system::error_code ec;
    
    while (first or (now = std::chrono::high_resolution_clock::now()) < limit)
    {
        first = false;
        auto bytes = socket.read_some(boost::asio::buffer(vbuf), ec);
        if (bytes) {
            result.append(vbuf.begin(), vbuf.begin() + bytes);
        }
        if (ec && ec != make_error_code(boost::asio::error::basic_errors::would_block))
            break;
    }
    
    socket.non_blocking(was_blocking);
    return result;
}


static const auto invalid_url_strings =
make_array<std::string>("GET fooble HTTP/1.1\r\n",
                        "Accept: text/*\r\n",
                        "\r\n");


struct http_server_test : ::testing::Test
{
    using protocol = secr::dispatch::asio::ip::tcp;
    using io_service = secr::dispatch::asio::io_service;

    io_service client_service, server_service, dispatch_service;
    io_service::work server_comms_work { server_service };
    protocol::socket client_socket {client_service}, server_socket { server_service };
    
    http_server_test()
    {
    }
};


TEST_F(http_server_test, invalid_url)
{
    namespace asio = secr::dispatch::asio;
    
    ASSERT_TRUE(tie_sockets(client_socket, server_socket));
    
    auto server_service_result = std::async(std::launch::async, [&]{
        while (not server_service.stopped())
            server_service.run_one();
    });
    
    secr::dispatch::http::server_connection http_server(std::move(server_socket),
                                                        dispatch_service);
    
    bool stopped { false };
    bool dispatched { false };
    http_server.async_start(client_service.wrap([&](std::exception_ptr errors) {
        secr::dispatch::api::Exception elist;
        
        EXPECT_EQ("{\n \"name\": \"secr::dispatch::http::protocol_error\",\n \"what\": \"HPE_INVALID_METHOD\"\n}\n",
                  as_json(populate(elist, errors)));
        stopped = true;
    }));
    
    secr::dispatch::error_code client_error;
    write(client_socket, boost::asio::buffer(invalid_url_strings), client_error);
    ASSERT_FALSE(client_error) << client_error;
    ASSERT_NO_THROW( client_socket.shutdown(boost::asio::socket_base::shutdown_send) );
    
    http_server.async_wait_dispatch([&](const secr::dispatch::shared_future<secr::dispatch::http::dispatch_context>& result)
                                    {
                                        dispatched = true;
                                        ASSERT_TRUE(ready(result));
                                        try {
                                            result.get();
                                            FAIL() << "no exception";
                                        }
                                        catch(const secr::dispatch::http::protocol_error& e)
                                        {
                                            EXPECT_STREQ("HPE_INVALID_METHOD", e.what());
                                        }
                                        catch(...)
                                        {
                                            FAIL() << value::debug::unwrap();
                                        }
                                    });
    
    auto spin1 = spins_once_within(dispatch_service, a_moment());
    ASSERT_TRUE(spin1) << "notification of dispatch";
    ASSERT_TRUE(dispatched);
    
    auto spin2 = spins_once_within(client_service, a_moment());
    ASSERT_TRUE(spin2) << "notification of server completion";
    ASSERT_TRUE(stopped);
    
    
    
    
    
    server_service.stop();
    ASSERT_NO_THROW(server_service_result.get());
    
    dispatch_service.stop();
    client_service.stop();
}

static const auto valid_get_text = make_array<std::string>("POST /fi",
                                                           "nk HTTP/1.1\r\n",
                                                           "Accept: text/*\r\n",
                                                           "Content-Length: 10\r\n",
                                                           "Connection: keep-alive\r\n",
                                                           "\r\n",
                                                           "0123456789",
                                                           
                                                           "POST /fi",
                                                           "nk HTTP/1.1\r\n",
                                                           "Accept: text/*\r\n",
                                                           "Content-Length: 10\r\n",
                                                           "Connection: close\r\n",
                                                           "\r\n",
                                                           "0987654321");


TEST_F(http_server_test, valid_get_but_no_response)
{
    namespace asio = secr::dispatch::asio;
    ASSERT_TRUE(tie_sockets(client_socket, server_socket));
    
    auto server_service_result = std::async(std::launch::async, [&]{
        while (not server_service.stopped())
            server_service.run_one();
    });
    
    secr::dispatch::http::server_connection http_server(std::move(server_socket),
                                                        dispatch_service);
    
    int dispatched = 0;
    bool stopped { false };

    http_server.async_start(client_service.wrap([&](std::exception_ptr errors)
                                                {
                                                    secr::dispatch::api::Exception elist;
                                                    EXPECT_EQ("{\n \"name\": \"boost::system::system_error\",\n \"what\": \"End of file\"\n}\n",
                                                              as_json(populate(elist, errors)));
                                                    stopped = true;
                                                }));
    
    secr::dispatch::error_code client_error;
    write(client_socket, buffers_of(valid_get_text), client_error);
    ASSERT_FALSE(client_error) << client_error.message();
    client_socket.shutdown(boost::asio::socket_base::shutdown_send, client_error);
    ASSERT_FALSE(client_error) << client_error.message();
    
    http_server.async_wait_dispatch([&](const secr::dispatch::shared_future<secr::dispatch::http::dispatch_context>& result)
                                    {
                                        dispatched += 1;
                                        ASSERT_TRUE(ready(result));
                                        try {
                                            result.get();
                                            // note: don't respond - we want to see a generic response
                                        }
                                        catch(...)
                                        {
                                            FAIL() << value::debug::unwrap();
                                        }
                                    });
    
    auto spin1 = spins_once_within(dispatch_service, a_moment());
    ASSERT_TRUE(spin1) << "notification of dispatch 1";

    http_server.async_wait_dispatch([&](auto& future){
        dispatched += 1;
        ASSERT_TRUE(ready(future));
        try {
            future.get();
        }
        catch(...) {
            EXPECT_TRUE(false) << value::debug::unwrap();
        }
    });

    auto spin2 = spins_once_within(dispatch_service, a_moment());
    ASSERT_TRUE(spin2) << "notification of dispatch 2";
    
    //
    // expect only one response because the first exception will force the server
    // to close the connection
    //
    auto response = consume_available_in(client_socket, a_moment());
    EXPECT_EQ("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\nContent-Length: 67\r\nContent-Type: application/json\r\nX-Secr-Content-Type: protobuf-message\r\nX-Secr-Message-Type: secr.api.Exception\r\n\r\n{\n \"name\": \"std::logic_error\",\n \"what\": \"server did not respond\"\n}\n", response);
    
    

    ASSERT_FALSE(stopped);
    auto spin3 = spins_once_within(client_service, a_moment());
    ASSERT_TRUE(spin3) << "notification of server completion";
    ASSERT_TRUE(stopped);
    
    
    
    
    server_service.stop();
    ASSERT_NO_THROW(server_service_result.get());
    
    dispatch_service.stop();
    client_service.stop();


}

TEST_F(http_server_test, valid_get_with_response)
{
    namespace asio = secr::dispatch::asio;
    ASSERT_TRUE(tie_sockets(client_socket, server_socket));
    
    auto server_service_result = std::async(std::launch::async, [&]{
        while (not server_service.stopped())
            server_service.run_one();
    });
    
    secr::dispatch::http::server_connection http_server(std::move(server_socket),
                                                        dispatch_service);
    
    int dispatched = 0;
    bool stopped { false };
    
    http_server.async_start(client_service.wrap([&](std::exception_ptr errors)
                                                {
                                                    secr::dispatch::api::Exception elist;
                                                    EXPECT_EQ("{\n \"name\": \"boost::system::system_error\",\n \"what\": \"End of file\"\n}\n",
                                                              as_json(populate(elist, errors)));
                                                    stopped = true;
                                                }));
    
    secr::dispatch::error_code client_error;
    write(client_socket, buffers_of(valid_get_text), client_error);
    ASSERT_FALSE(client_error) << client_error.message();
    client_socket.shutdown(boost::asio::socket_base::shutdown_send, client_error);
    ASSERT_FALSE(client_error) << client_error.message();
    
    auto reverse_all = [&](auto& future)
    {
        dispatched += 1;
        ASSERT_TRUE(ready(future));
        try {
            auto context = future.get();
            secr::dispatch::error_code ec;
            secr::dispatch::asio::streambuf streambuf;
            auto bytes_read = asio::read(context.request().stream(), streambuf, ec);
            EXPECT_EQ(10, bytes_read);
            auto data = streambuf.data();
            std::string s(asio::buffer_cast<const char*>(data),
                          asio::buffer_size(data));
            std::reverse(s.begin(), s.end());
            if (dispatched == 1) {
                auto size = context.response().flush(asio::const_buffers_1(asio::buffer(s)), ec);
                EXPECT_EQ(s.size(), size);
            }
            else {
                auto size = asio::write(context.response(), asio::buffer(s), ec);
                EXPECT_EQ(s.size(), size);
                EXPECT_FALSE(ec) << ec.message();
                context.response().close(ec);
            }
            EXPECT_FALSE(ec) << ec.message();
        }
        catch(...)
        {
            FAIL() << value::debug::unwrap();
        }
    };
    
    http_server.async_wait_dispatch(reverse_all);
    auto spin1 = spins_once_within(dispatch_service, a_moment());
    ASSERT_TRUE(spin1) << "notification of dispatch 1";

    http_server.async_wait_dispatch(reverse_all);
    auto spin2 = spins_once_within(dispatch_service, a_moment());
    ASSERT_TRUE(spin2) << "notification of dispatch 2";
    
    //
    // expect only one response because the first exception will force the server
    // to close the connection
    //
    auto response = consume_available_in(client_socket, a_moment());
    EXPECT_EQ("HTTP/1.1 200 OK\r\nContent-Length: 10\r\nConnection: keep-alive\r\n\r\n9876543210HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n10\r\n1234567890\r\n",
              response);
    
    
    
    ASSERT_FALSE(stopped);
    auto spin3 = spins_once_within(client_service, a_moment());
    ASSERT_TRUE(spin3) << "notification of server completion";
    ASSERT_TRUE(stopped);
    
    
    
    
    server_service.stop();
    ASSERT_NO_THROW(server_service_result.get());
    
    dispatch_service.stop();
    client_service.stop();
    
    
}

