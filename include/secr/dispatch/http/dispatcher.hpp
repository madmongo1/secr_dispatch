#pragma once

#include <secr/dispatch/http/server_request.hpp>
#include <secr/dispatch/fake_stream.hpp>
#include <secr/dispatch/http/request_header.hpp>
#include <exception>
#include <secr/dispatch/api/exception.hpp>
#include <secr/dispatch/api/json.hpp>
#include <secr/dispatch/http/errors.hpp>


namespace secr { namespace dispatch { namespace http {
    
    struct content_length_fixed
    {
        constexpr content_length_fixed(std::size_t size)
        : _size(size)
        {}
        
        constexpr std::size_t size() const { return _size; }

    private:
        std::size_t _size;
    };
    
    struct content_length_variable {};
    
    /// A lightweight context object that is designed to be copied by the
    /// dispatcher implementation. It provides access to all state associated
    /// with the http request plus read and write streams.
    /// Note that a dispatch_context's methods work even while it is const.
    /// This is to allow handlers to copy the context without themselves needing
    /// to be mutable.
    
    /// an execution context has a number of states:
    /// * read_stream state. this is either 'open' (no error) or 'error' (including eof)
    /// * execution state: in progress, complete, complete with exception
    /// * write_stream state: open (accepting input), closed or 'error'
    ///   the write stream state will move to 'error' if the connection cancels the
    ///   request for any reason (transport error, user cancel, timeout, etc)
    ///   the final error state of the execution is the set in the following order:
    ///   - user_supplied error
    ///   - read_stream_error (unless eof)
    ///   - write_stream_error (unless eof)
    ///   - no error
    /// if the execution completes without error,
    struct dispatch_context
    {
        struct request_object
        {
            request_object(request_context& request_context)
            : _request_context(request_context)
            {}
            
            const HttpRequestHeader& header()
            {
                return _request_context.request_header();
            }
            
            const ContentType& content_type() {
                return _request_context.request_manager().content_type();
            }
            
            fake_stream_read_interface& stream()
            {
                return _read_stream;
            }
            
            request_context& _request_context;
            fake_stream_read_interface _read_stream { _request_context.request_stream() };
        };
        
        struct response_object
        {
            using this_class = response_object;

            response_object(request_context& request_context)
            : _request_context(request_context)
            {}
            
            ~response_object();
            
            HttpResponseHeader& mutable_header()
            {
                assert(not header_committed());
                return _request_context.response_header();
            }

            const HttpResponseHeader& header() const
            {
                return _request_context.response_header();
            }

            std::size_t commit_header(error_code& ec);
            std::size_t commit_header();
            
            template<class Handler>
            void async_commit_header(Handler&& handler)
            {
                assert(!_header_committed);
                auto data = to_response_buffer(header()).shared();
                _header_committed = true;
                return asio::async_write(stream(), asio::buffer(data),
                                         [handler = std::move(handler), data]
                                         (const auto& ec, auto size)
                                         {
                                             handler(ec, size);
                                         });
            }
            
            /// Write data to the response stream, committing the header if necessary.
            /// @pre header is committed and indicates a variable-length stream
            ///      or header is not comitted
            /// @post header will be committed, and stream will be variable length
            template<class ConstBufferSequence>
            std::size_t write_some(const ConstBufferSequence& buffers,
                                   error_code& ec);

            /// perform a fixed-size write if possible and then close the channel
            template<class ConstBufferSequence>
            std::size_t flush(const ConstBufferSequence& buffers,
                              error_code& ec);

            /// close the stream
            error_code close(error_code& ec);
            void close();
            
            bool header_committed() const {
                return _header_committed;
            }
            
            
            void set_content_length(content_length_fixed);
            void set_content_length(content_length_variable);
            
            void set_exception(std::exception_ptr ep);
            
        private:
            void commit_with_exception(std::exception_ptr ep);
            
            template<class ConstBufferSequence>
            std::size_t write_chunk(const ConstBufferSequence& buffers, error_code& ec);

            fake_stream_write_interface& stream()
            {
                // set response_type
                return _write_stream;
            }

        private:
            request_context& _request_context;
            fake_stream_write_interface _write_stream { _request_context.response_stream() };
            bool _header_committed = false;
            enum class response_mode {
                undecided,
                content_length,
                chunked,
                raw
            };
            response_mode _response_mode = response_mode::undecided;
            using size_type = std::size_t;
            static constexpr auto unlimited_size = std::numeric_limits<size_type>::max();
            size_type _response_size_limit = unlimited_size;
            error_code _last_error;

        };
        
        struct shared_state
        {
            using this_class = shared_state;
            shared_state(std::shared_ptr<request_context> request_context);
            
            auto request() -> request_object& { return _request; }
            auto response() -> response_object& { return _response; }
            
            std::shared_ptr<request_context> _request_context;
            request_object _request { *_request_context };
            response_object _response { *_request_context };
            
            void set_exception(std::exception_ptr ep);
            
            ~shared_state() noexcept;
            
        };
        
        //
        // request() - return a reference to the request object, which begins with an open stream
        //             [async]_read_some
        //
        // response() - return a reference to the response object, which begins with an empty header
        //
        // response : stock_reply() - emit a stock reply, after which the response is complete()
        //            set_status()
        //            set_header()
        //            set_response_type(fixed_length(length)) - set content-length
        //            set_response_type(chunked())
        //            set_response_type(until_end_of_stream())
        //            [async_]write_some(data) - in chunked mode, send a chunk.
        //                      fixed-length mode, limit to remaining length (eof error for remainder),
        //                      in until-end mode, send all.
        //
        
        dispatch_context(std::shared_ptr<request_context> request)
        : _shared_state(std::make_shared<shared_state>(std::move(request)))
        {
        }
        
        auto request() const -> request_object& { return _shared_state->request(); }
        auto response() const -> response_object& { return _shared_state->response(); }
        
        template<class F>
        void action(F&& f)
        {
            try {
                f();
            }
            catch(...) {
                _shared_state->set_exception(std::current_exception());
            }
        }
        
    private:
        
        std::shared_ptr<shared_state> _shared_state;
        
        /// emit debug info
        friend std::ostream& operator<<(std::ostream&os, const dispatch_context& context);

    };
    
    
    
    
    //
    // IMPLEMENTATION OF response_object
    //

    template<class ConstBufferSequence>
    std::size_t
    dispatch_context::response_object
    ::flush(const ConstBufferSequence& buffers, error_code& ec)
    {
        switch (_response_mode)
        {
            case response_mode::chunked:
            case response_mode::content_length:
            case response_mode::raw:
                break;
                
            case response_mode::undecided:
                set_content_length(content_length_fixed(asio::buffer_size(buffers)));
                break;
        }
        if (not header_committed())
            commit_header(ec);
        auto written = write_some(buffers, ec);
        if (not ec)
            close(ec);
        return written;
    }

    template<class ConstBufferSequence>
    std::size_t
    dispatch_context::response_object
    ::write_some(const ConstBufferSequence& buffers, error_code& ec)
    {
        ec.clear();
        if (_response_mode == response_mode::undecided) {
            set_content_length(content_length_variable());
        }

        if (not header_committed()) {
            if (not header().has_status()) {
                auto stat = mutable_header().mutable_status();
                stat->set_code(200);
                stat->set_message("OK");
            }
            commit_header(ec);
        }

        switch(_response_mode)
        {
            case response_mode::undecided:
                assert(false);
                ec = make_error_code(protocol_error_code::response_mode_not_set);
                return 0;
                
            case response_mode::chunked:
                return write_chunk(buffers, ec);
                
            case response_mode::content_length:
            case response_mode::raw:
                break;
        }
        
        
        // here we're going to write as much data as possible
        std::size_t total_written = 0;
        auto total_to_write = this->_response_size_limit;
        for (auto buffer : buffers)
        {
            while (total_to_write and asio::buffer_size(buffer) and not ec)
            {
                if (total_to_write < asio::buffer_size(buffer))
                    buffer = asio::buffer(buffer, total_to_write);
                
                auto written = stream().write_some(asio::const_buffers_1(buffer), ec);
                buffer = buffer + written;
                if (total_to_write != unlimited_size)
                    total_to_write -= written;
                total_written += written;
            }
        }
        return total_written;
    }
    
    template<class ConstBufferSequence>
    std::size_t
    dispatch_context::response_object::
    write_chunk(const ConstBufferSequence& buffers, error_code& ec)
    {
        static const std::string crlf = "\r\n";
        auto total_size = asio::buffer_size(buffers);
        std::ostringstream ss;
        ss << std::hex << total_size;
        auto chunk_header = ss.str() + crlf;
        void(asio::write(stream(), asio::buffer(chunk_header), ec));
        auto total_written = 0;
        if (not ec) {
            total_written += asio::write(stream(), buffers, ec);
        }
        if (not ec) {
            void(asio::write(stream(), asio::buffer(crlf), ec));
        }
        return total_written;
    }

    
    

    
    
}}}