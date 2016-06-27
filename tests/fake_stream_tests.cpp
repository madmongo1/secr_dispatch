#include <gtest/gtest.h>
#include "test_utils.hpp"
#include <secr/dispatch/fake_stream.hpp>
#include <vector>
#include <thread>
#include <future>
#include <random>
#include <algorithm>
#include <iterator>


TEST(fake_stream_tests, mass_transport)
{
    using char_buffer = std::vector<char>;
    using char_buffers = std::vector<char_buffer>;
    
    char_buffers vv;
    
    std::random_device rd;
    std::default_random_engine eng(rd());
    std::uniform_int_distribution<int> length(40, 80);
    std::uniform_int_distribution<char> chars('A', 'Z');
    
    for (int i = 0 ; i < 100 ; ++i)
    {
        char_buffer v;
        std::generate_n(std::back_inserter(v),
                        length(eng),
                        [&eng, &chars] { return chars(eng); });
        vv.push_back(std::move(v));
    }
    
    boost::asio::io_service s, d;
    
    secr::dispatch::fake_stream fs(s, d);
    auto sread = secr::dispatch::fake_stream_read_interface(fs);
    auto swrite = secr::dispatch::fake_stream_write_interface(fs);
    
    
    auto frecv = std::async(std::launch::async,
                            [&]
    {
        std::string result;
        char buf [30];
        secr::dispatch::error_code ec;
        while (not ec) {
            auto size = secr::dispatch::asio::read(sread, secr::dispatch::asio::buffer(buf), ec);
            result.append(buf, size);
        }
        return result;
    });
    
    std::string sent;
    for (int i = 0 ; i < vv.size() ; )
    {
        auto remaining = vv.size() - i;
        auto to_send = std::uniform_int_distribution<std::size_t>(1,5)(eng);
        to_send = std::min(remaining, to_send);
        auto buffers = buffers_of(std::begin(vv) + i , std::begin(vv) + i + to_send);
        secr::dispatch::error_code ec;
        secr::dispatch::asio::write(swrite, buffers, ec);
        for (int x = i ; x < i + to_send ; ++x) {
            sent.append(vv[x].begin(), vv[x].end());
        }
        i += to_send;
    }
    swrite.close();
    
    frecv.wait_for(a_moment());
    std::string srecv;
    EXPECT_NO_THROW(srecv = frecv.get());
    EXPECT_EQ(sent, srecv);
}