#include <secr/dispatch/http/read_stream.hpp>
#include <secr/dispatch/http/server_request.hpp>
#include <secr/dispatch/http/dispatcher.hpp>


namespace secr { namespace dispatch { namespace http {
/*
    read_stream_impl::read_stream_impl(request_context* request_context)
    : _request_context(request_context)
    {
        
    }

    void read_stream_impl::push_data(const char* data, std::size_t length)
    {
        assert(request());
        assert(request()->get_strand().running_in_this_thread());
        push_data_impl(std::vector<char>(data, data + length));
        check_read_completion();
    }
    
    void read_stream_impl::check_read_completion()
    {
        if (_read_completion_pending)
        {
            // commit new received data to the read buffer
            while (!_receive_buffer.empty())
            {
                auto block = std::move(_receive_buffer.front());
                _receive_buffer.pop_front();
                auto size = block.size();
                auto buffer = _read_buffer.prepare(size);
                std::copy(std::begin(block),
                          std::end(block),
                          asio::buffer_cast<char*>(buffer));
                _read_buffer.commit(size);
            }
            auto bytes_available = asio::buffer_size(_read_buffer.data());
            auto bytes_needed = _bytes_required ? _bytes_required : 1;
            
            auto error = error_code();
            bool will_complete = false;

            if (bytes_needed > bytes_available + remaining_capacity()) {
                error = make_error_code(asio::error::misc_errors::eof);
                will_complete = true;
            }
            
            if (_stream_error && bytes_needed > bytes_available) {
                error = _stream_error;
                will_complete = true;
            }
            
            if (bytes_available >= bytes_needed)
            {
                error = error_code();
                will_complete = true;
            }
            
            if (will_complete)
            {
                dispatcher_io_service().post([this,
                                              f = std::move(_read_completion_pending),
                                              error,
                                              bytes_available]() mutable
                                             {
                                                 f(error, bytes_available);
                                             });
            }
        }
    }
    
    void read_stream_impl::async_read(std::size_t bytes_required, read_handler handler)
    {
        _request_context->get_strand().dispatch([this,
                                                 bytes_required,
                                                 handler = std::move(handler)] ()
                                                mutable
                                                {
                                                    assert(!_read_completion_pending);
                                                    // if required > capacity, error
                                                    _bytes_required = bytes_required;
                                                    _read_completion_pending = std::move(handler);
                                                    check_read_completion();
                                                });
        
        
    }
    
    asio::io_service& read_stream_impl::dispatcher_io_service()
    {
        assert(request());
        return request()->get_dispatcher().get_io_service();
    }

    

    void read_stream_impl::push_data_impl(std::vector<char>&& new_data)
    {
        _receive_buffer.push_back(std::move(new_data));
        
    }


    std::size_t read_stream_impl::remaining_capacity_impl() const {
        assert(request());
        assert(request()->get_strand().running_in_this_thread());
        if (_stream_error)
            return 0;
        else
            return std::numeric_limits<std::size_t>::max();
    }
    
    std::size_t read_fixed_length_stream::remaining_capacity_impl() const
    {
        assert(_acquired <= _content_length);
        return std::min(_content_length - _acquired,
                        read_stream_impl::remaining_capacity_impl());
    }
    
    
    void read_fixed_length_stream::push_data_impl(std::vector<char>&& data)
    {
        assert(remaining_capacity() >= data.size());
        _acquired += data.size();
        read_stream_impl::push_data_impl(std::move(data));
    }
*/
}}}
