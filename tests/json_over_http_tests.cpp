#include <gtest/gtest.h>

#include <secr/dispatch/api/json.hpp>
#include <secr/dispatch/http/server_connection.hpp>

using namespace secr;
using namespace std::literals;

template<class Duration>
std::size_t run_one_within(dispatch::asio::io_service& io_service, Duration limit)
{
    auto first = std::chrono::system_clock::now();
    auto latest = first;
    do
    {
        if (io_service.stopped())
            io_service.reset();
        auto spins = io_service.run_one();
        if (spins)
            return spins;
        std::this_thread::sleep_for(1ms);
        latest = std::chrono::system_clock::now();
        
    } while (latest - first < limit);

    return 0;
}

using protocol = dispatch::asio::ip::tcp;

testing::AssertionResult listen_anywhere(protocol::acceptor& acceptor)
{
    protocol::resolver resolver(acceptor.get_io_service());
    for (int port = 4000 ; port < 20000 ; ++port)
    {
        protocol::resolver::query query { "localhost", std::to_string(port) };
        auto iter = resolver.resolve(query);
        assert(iter != protocol::resolver::iterator());
        boost::system::error_code err;
        acceptor.open(iter->endpoint().protocol(), err);
        if (err) {
            continue;
        }
        acceptor.bind(*iter, err);
        if (err) {
            acceptor.close();
            continue;
        }
        acceptor.listen(5, err);
        if (err) {
            acceptor.close();
            continue;
        }
        return testing::AssertionSuccess();
    }
    return testing::AssertionFailure() << "cannot allocate a port for listening";
}


struct json_server_test : ::testing::Test
{
    dispatch::asio::io_service server_comms;
    dispatch::asio::io_service server_dispatch;

    dispatch::asio::io_service client_comms;
    
    protocol::acceptor  server_acceptor { server_comms };
    protocol::socket    server_accept_socket { server_comms };
    
    protocol::socket    client_socket { client_comms };
    

};


TEST_F(json_server_test, test1)
{
    
    
}