#include <secr/dispatch/fake_stream.hpp>

namespace secr { namespace dispatch {


    // consume_op implementation
    
    // fake_stream implementation
    
    fake_stream::fake_stream(asio::io_service& read_io_service,
                             asio::io_service& write_io_service)
    : _read_io_service(read_io_service)
    , _write_io_service(write_io_service)
    , _consume_op { nullptr }
    {
    }



}}