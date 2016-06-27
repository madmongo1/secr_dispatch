#pragma once

#include <secr/dispatch/config.hpp>
#include <secr/dispatch/polymorphic_stream.hpp>

namespace secr { namespace dispatch {

    struct buffered_stream
    {
        /// Construct a new buffered stream
        buffered_stream(polymorphic_stream stream)
        : stream(polymorphic_stream(std::move(stream)))
        {}
        
        /// return an asio::const_buffers object which encapsulates all the data
        /// in the buffer
        auto data() const {
            return read_buffer.data();
        }
        
        /// consume n bytes from the buffer
        void consume(std::size_t bytes) {
            assert(read_buffer.size() >= bytes);
            read_buffer.consume(bytes);
        }
        
        template<class Handler>
        void async_read_some(Handler&& handler)
        {
            // we should ensure that all bytes have been
            auto buffer = read_buffer.prepare(4096);
            stream.async_read_some(buffer,
                                   [this, handler { std::move(handler) }]
                                   (auto& error, auto bytes) mutable // <- must be mutable so that the inner handler can be moved
                                   {
                                       this->read_buffer.commit(bytes);
                                       handler(error, read_buffer.size());
                                   });
        }
        
        template<class ConstBufferSequence, class Handler>
        void async_write_some(const ConstBufferSequence& buffers, Handler&& handler)
        {
            stream.async_write_some(buffers, std::forward<Handler>(handler));
        }
        
        void shutdown(asio::socket_base::shutdown_type type)
        {
            return stream.shutdown(type);
        }
        
        error_code shutdown(asio::socket_base::shutdown_type type, error_code& ec)
        {
            return stream.shutdown(type, ec);
        }
        
        void close() {
            return stream.close();
        }
        
        error_code close(error_code& ec) {
            return stream.close(ec);
        }
        
        asio::io_service& get_io_service() {
            return *_io_service_ptr;
        }
        
        
        polymorphic_stream stream;
        asio::io_service* _io_service_ptr = std::addressof(stream.get_io_service());
        boost::asio::streambuf read_buffer;
    };
    
    /// Read data from the stream until some condition is met. see the
    /// asio free function of the same name
    template<class...Rest>
    auto async_read_until(buffered_stream& buffered_stream, Rest&&...rest)
    {
        return boost::asio::async_read_until(buffered_stream.stream,
                                             buffered_stream.read_buffer,
                                             std::forward<Rest>(rest)...);
    }
    
    /// Make a new shared pointer to a buffered stream from an existing
    /// underlying stream object
    template<class StreamType>
    auto make_shared_buffered_stream(StreamType&& stream)
    {
        return std::make_shared<buffered_stream>(polymorphic_stream(is_owner,
                                                                    std::forward<StreamType>(stream)));
    }

}}