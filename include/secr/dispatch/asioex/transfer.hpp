#pragma once

#include <secr/dispatch/config.hpp>
#include <array>
#include <memory>

#include <valuelib/stdext/string_algorithm.hpp>
#include <boost/log/trivial.hpp>


namespace secr { namespace dispatch { namespace asioex {
   
    
    template<class AsyncReadStream, class AsyncWriteStream, class Handler, std::size_t BufferSize>
    struct transfer_op
    :std::enable_shared_from_this
    < transfer_op< AsyncReadStream, AsyncWriteStream, Handler, BufferSize > >
    {
        using source_stream_type = AsyncReadStream;
        using dest_stream_type = AsyncWriteStream;
        using handler_type = Handler;
        
        transfer_op(source_stream_type& source, dest_stream_type& dest,
                    handler_type handler)
        : _source(source)
        , _destination(dest)
        , _handler(std::move(handler))
        {
        }
        
        void run()
        {
            start_read();
        }
        
    private:
        
        void start_read()
        {
            asio::async_read(_source, asio::buffer(_buffer),
                             [self = this->shared_from_this()]
                             (auto& ec, auto bytes)
            {
                self->handle_read(ec, bytes);
            });
        }
        
        void handle_read(const error_code& ec, std::size_t bytes)
        {
            _read_error = ec;
            _bytes_read += bytes;
            if (bytes)
            {
                asio::async_write(_destination,
                                  asio::buffer(_buffer.data(), bytes),
                                  [self = this->shared_from_this()]
                                  (auto& ec, auto size)
                                  {
                                      self->handle_write(ec, size);
                                  });
            }
            else {
                complete();
            }
        }
        
        void handle_write(const error_code& ec, std::size_t bytes)
        {
            _write_error = ec;
            _bytes_written += bytes;
            BOOST_LOG_TRIVIAL(trace) << "transferred: " << bytes << " bytes, total="
            << _bytes_written<< ", write_error: " << ec.message()
            << value::stdext::escape(_buffer.data(), _buffer.data() + bytes);
            
            if (_read_error or _write_error) {
                complete();
            }
            else {
                start_read();
            }
        }
        
        void complete()
        {
            _handler(_read_error, _write_error, _bytes_read, _bytes_written);
        }
        
        source_stream_type& _source;
        dest_stream_type& _destination;
        handler_type _handler;

        error_code _read_error = {};
        error_code _write_error = {};
        std::size_t _bytes_read = 0;
        std::size_t _bytes_written = 0;
        std::array<char, BufferSize> _buffer;
    };
    
    /// Transfer bytes from one stream to another
    /// @param from is the AsyncReadStream to transfer from
    /// @param to is the AsyncWriteStream to transfer to
    /// @param handler is the completion handler which will
    ///         be called when the transfer is complete
    /// @note the handler will be called on the io_service of *either*
    ///         stream
    /// Handler is a model of function( const error_code& source_error,
    ///                                 const error_code& dest_error,
    ///                                 std::size_t bytes_read,
    ///                                 std::size_t bytes_written)
    ///
    template<
    class AsyncReadStream,
    class AsyncWriteStream,
    class Handler,
    std::size_t BufferSize = 4096
    >
    void transfer(AsyncReadStream& from, AsyncWriteStream& to, Handler&& handler)
    {
        using source_stream_type = std::decay_t<AsyncReadStream>;
        using dest_stream_type = std::decay_t<AsyncWriteStream>;
        using handler_type = std::decay_t<Handler>;
        
        using op_type = transfer_op<source_stream_type, dest_stream_type, handler_type, BufferSize>;
        
        auto op_ptr = std::make_shared<op_type>(from, to, std::forward<Handler>(handler));
        op_ptr->run();
    }
    
}}}