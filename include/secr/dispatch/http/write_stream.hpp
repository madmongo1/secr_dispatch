#pragma once

namespace secr { namespace dispatch { namespace http {
    
    struct write_stream
    {
        MessageHeader& response_header();
        void async_write();
    };

}}}
