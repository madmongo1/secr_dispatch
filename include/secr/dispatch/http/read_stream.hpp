#pragma once
#include <secr/dispatch/config.hpp>
#include <limits>
#include <secr/dispatch/secr_dispatch_http.pb.h>
#include <deque>
#include <vector>
#include <stack>

namespace secr { namespace dispatch { namespace http {

    struct request_context;
    /*
    struct read_stream_impl
    {
        using read_handler = std::function<void(const error_code& ec, std::size_t bytes_available)>;
        read_stream_impl(request_context* request_context);
        
        void push_data(const char* data, std::size_t length);
        void async_read(std::size_t bytes_required, read_handler handler);

        void notify_transport_error(error_code ec)
        {
            _stream_error = ec;
        }
        
        void notify_eof()
        {
            _stream_error = asio::error::misc_errors::eof;
        }
        
        std::size_t remaining_capacity() const
        {
            return remaining_capacity_impl();
        }
        
        /// @pre there must not be an async_read pending
        /// @pre must not be called from the connection's strand
        asio::const_buffer data() const {
            
            assert(!_read_completion_pending);
            return _read_buffer.data();
        }

        /// @pre there must not be an async_read pending
        /// @pre must not be called from the connection's strand
        void consume(std::size_t bytes) {
            assert(!_read_completion_pending);
            return _read_buffer.consume(bytes);
        }

    protected:
        virtual void push_data_impl(std::vector<char>&& new_data);
        virtual std::size_t remaining_capacity_impl() const;
        
        void check_read_completion();
        
        asio::io_service& dispatcher_io_service();
        

        void check_pending_read();
        
        request_context* request() const {
            return _request_context;
        }
        
        request_context* _request_context;
        
        asio::streambuf _read_buffer;
        /// a list of buffers containing data that is available to be read on the
        /// next async_read_some call.
        /// it will be transferred into the read buffer before the call
        /// completes
        std::deque<std::vector<char>> _receive_buffer;
        
        
        std::function<void(error_code, std::size_t)> _read_completion_pending = nullptr;
        std::size_t _bytes_required = 0;  ///! the number of bytes required by the pending completion
        
        // if true then the stream is eof on next read when buffer empty
        error_code _stream_error = error_code();
    };
    
    struct read_to_end_stream : read_stream_impl
    {
        read_to_end_stream(request_context* request)
        : read_stream_impl(request)
        {}

    };
    
    struct read_chunked_stream : read_stream_impl
    {
        read_chunked_stream(request_context* request)
        : read_stream_impl(request)
        {}

    };
    
    struct read_fixed_length_stream : read_stream_impl
    {
        read_fixed_length_stream(request_context* context, std::size_t length)
        : read_stream_impl(context)
        , _content_length(length)
        {}
        

        std::size_t remaining_capacity_impl() const override;
        
    private:

        void push_data_impl(std::vector<char>&& data) override;

        std::size_t _content_length;
        std::size_t _acquired = 0;
    };
    
    
    /// The interface presented to the client
    struct read_stream
    {
        read_stream(read_stream_impl& impl) : _impl(impl) {};
        /// get a reference to the executor's io service. stream callbacks
        /// happen on this.
        asio::io_service& get_io_service();
        
        /// Get a reference to the message header
        const MessageHeader& request_header();

        /// make more data available internally. If there is unconsumed data
        /// already in the stream, return that immediately
        template<class ReadHandler>
        void async_read_some(ReadHandler&& handler) //  const error_code& ec, std::size_t bytes_available
        {
            _impl.async_read(0, std::forward<ReadHandler>(handler));
        }
        
        /// keep reading until there is at least this much data in the buffer
        template<class ReadHandler>
        void async_read(std::size_t needed, ReadHandler&& handler) //const error_code& ec, std::size_t bytes_available
        {
            _impl.async_read(needed, std::forward<ReadHandler>(handler));
        }
        
        /// return a buffer representing the data available
        asio::const_buffer data() { return _impl.data(); }
        
        /// remove this much data from the stream
        void consume(std::size_t bytes) { _impl.consume(bytes); }
        
    private:
        read_stream_impl& _impl;
        
    };
    */


}}}
