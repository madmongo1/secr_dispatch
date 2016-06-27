#include <secr/dispatch/http/server_connection.hpp>
#include <secr/dispatch/http/server_request.hpp>
#include <secr/dispatch/http/parse.hpp>


namespace secr { namespace dispatch { namespace http {
    
    server_connection::server_connection(polymorphic_stream polystream,
                                         asio::io_service& dispatch_io_service)
    : _connection(std::move(polystream))
    , _dispatch_service(dispatch_io_service)
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection", __func__);
        http_parser_init(parser(), HTTP_REQUEST);
        parser()->data = this;
        init_callbacks();
    }
    
    namespace {
        server_connection* to_this(http_parser* p) {
            return reinterpret_cast<server_connection*>(p->data);
        }
    }
    
    void server_connection::init_callbacks()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection", __func__);
        auto settings = parser_settings();
        http_parser_settings_init(settings);
        
        settings->on_body = [](http_parser* p, const char* data,
                               std::size_t length) {
            return to_this(p)->handle_message_body(data, length);
        };
        
        settings->on_url = [](http_parser* p, const char* data,
                              std::size_t length) {
            return to_this(p)->handle_message_url(data, length);
        };

//        settings->on_status;
//        settings->on_chunk_complete;
//        settings->on_chunk_header;
        settings->on_message_begin = [](http_parser* p) {
            return to_this(p)->handle_message_begin();
        };
        settings->on_header_field = [](http_parser* p, const char* begin,
                                       std::size_t length) {
            return to_this(p)->handle_message_header_field(begin, length);
        };
        settings->on_header_value = [](http_parser* p, const char* begin,
                                       std::size_t length) {
            return to_this(p)->handle_message_header_value(begin, length);
        };
        settings->on_headers_complete = [](http_parser* p) {
            return to_this(p)->handle_message_headers_complete();
        };
    }
    
    bool server_connection::collect_more_data()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection", __func__);
        assert(_strand.running_in_this_thread());
        if (_error or _pause_count) return false;

        pause();
        push_work();
        _connection.async_read_some(asio::buffer(_read_buffer),
                                    _strand.wrap([this]
                                                 (auto& ec, auto bytes)
                                                 {
                                                     this->handle_read(ec, bytes);
                                                     this->unpause();
                                                     this->pop_work();
                                                 }));
        return true;
    }
    
    void server_connection::pause()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection", __func__);
        assert(_strand.running_in_this_thread());
        ++_pause_count;
    }
    
    void server_connection::unpause()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection", __func__);
        assert(_strand.running_in_this_thread());
        if (--_pause_count == 0) {
            collect_more_data();
        }
    }
    
    void server_connection::handle_read(const error_code &ec,
                                        std::size_t bytes_available)
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection", __func__,
                                     ec.message(), bytes_available);
        assert (_strand.running_in_this_thread());
        
        if (bytes_available)
        {
            http_parser_execute(parser(),
                                parser_settings(),
                                _read_buffer.data(),
                                bytes_available);
        }
        handle_protocol_error(parser_error(parser()));
        if (ec) {
            handle_transport_error(ec);
        }
    }
    
    int server_connection::handle_message_begin()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection", __func__);
        assert(_strand.running_in_this_thread());
        receiver_end_request(asio::error::misc_errors::eof);
        assert(!_current_receiver);
        new_receiver();
        return 0;
    }
    
    int server_connection::handle_message_url(const char* begin, std::size_t size)
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection", __func__,
                                     static_cast<const void*>(begin), size);
        assert(_strand.running_in_this_thread());
        assert(_current_receiver);
        try {
            _current_receiver->append_uri(begin, size);
        }
        catch(...)
        {
            handle_protocol_error(std::current_exception());
            return 1;
        }
        return 0;
    }
    
    int server_connection::handle_message_header_field(const char* begin,
                                                       std::size_t size)
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection", __func__,
                                     static_cast<const void*>(begin), size);
        assert(_strand.running_in_this_thread());
        assert(_current_receiver);
        try {
            _current_receiver->append_header_field(begin, size);
        }
        catch(...)
        {
            handle_protocol_error(std::current_exception());
            return 1;
        }
        return 0;
    }
    int server_connection::handle_message_header_value(const char* begin,
                                                       std::size_t size)
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection", __func__,
                                     static_cast<const void*>(begin), size);
        assert(_strand.running_in_this_thread());
        assert(_current_receiver);
        try {
            _current_receiver->append_header_value(begin, size);
        }
        catch(...) {
            handle_protocol_error(std::current_exception());
            return 1;
        }
        return 0;
    }
    
    int server_connection::handle_message_headers_complete()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection", __func__);
        assert(_strand.running_in_this_thread());
        assert(_current_receiver);
        try {
            _current_receiver->finalise_header(parser());
            receiver_available_for_dispatch();
        }
        catch(...)
        {
            handle_protocol_error(std::current_exception());
            return 3;
        }
        return 0;
    }
    
    int server_connection::handle_message_body(const char* data,
                                               std::size_t size)
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection", __func__,
                                     static_cast<const void*>(data), size);
        assert(_strand.running_in_this_thread());
        try {
            _current_receiver->consume_body(data, size);
            if (_parser.content_length == 0) {
                receiver_end_request(asio::error::misc_errors::eof);
            }
            return 0;
        }
        catch(...) {
            handle_protocol_error(std::current_exception());
            return 1;
        }
    }
    
    void server_connection::handle_transport_error(const error_code &ec)
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection", __func__, ec.message());
        assert(_strand.running_in_this_thread());
        if (ec && !_error) {
            pause(); // note - not matched with an unpause. prevents any more reading from stream
            receiver_end_request(ec);
            if (not asioex::is_eof(ec)) {
                _requests_pending_dispatch.clear();
            }
            _responder.submit_error(ec);
            _error = std::make_exception_ptr(system_error(ec));
        }
    }
    
    void server_connection::handle_protocol_error(http_errno err)
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection", __func__,
                                     http_errno_name(err));
        assert(_strand.running_in_this_thread());
        if (err and (err != HPE_PAUSED))
        {
            auto error = std::make_exception_ptr(protocol_error(err));
            handle_protocol_error(error);
        }
    }
    
    void server_connection::handle_protocol_error(std::exception_ptr ep)
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection",__func__);
        assert(_strand.running_in_this_thread());
        if (ep && !_error)
        {
            pause();
            
            receiver_end_request(asio::error::basic_errors::operation_aborted);
            _requests_pending_dispatch.clear();
            _error = ep;
            error_code ec;
            _connection.shutdown(asio::socket_base::shutdown_receive, ec);
            _responder.submit_error(asio::error::basic_errors::operation_aborted);
            attempt_dispatch();
            _current_receiver.reset();
        }
    }

    void server_connection::new_receiver()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection",__func__);
        assert(_strand.running_in_this_thread());
        assert(not _current_receiver);
        _current_receiver = std::make_shared<request_context>(_connection_id,
                                                              _strand.get_io_service(),
                                                              _dispatch_service);
    }

    void server_connection::receiver_end_request(error_code ec)
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection",__func__, ec.message());
        assert(_strand.running_in_this_thread());
        if (_current_receiver) {
            _current_receiver->request_stream().set_error(ec);
            _current_receiver.reset();
        }
    }
    
    void server_connection::receiver_available_for_dispatch()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection",__func__);
        assert(_strand.running_in_this_thread());
        if (_current_receiver) {
            _requests_pending_dispatch.push_back(_current_receiver);
            attempt_dispatch();
            receiver_available_for_response();
        }
    }
    
    void server_connection::receiver_available_for_response()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection",__func__);
        assert (_strand.running_in_this_thread());
        assert (_current_receiver);
        _responder.submit(_current_receiver);
    }
    
    void server_connection::attempt_dispatch()
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection",__func__);
        assert(_strand.running_in_this_thread());
        if (_pending_dispatch)
        {
            if (not _requests_pending_dispatch.empty())
            {
                auto op_ptr = std::move(_pending_dispatch);
                auto context = std::move(_requests_pending_dispatch.front());
                _requests_pending_dispatch.pop_front();
                op_ptr->complete(context);
            }
            else if (_error) {
                auto op_ptr = std::move(_pending_dispatch);
                op_ptr->complete(_error);
            }
        }
    }
    
    void server_connection::push_work()
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection", __func__, _work_count);
        assert(_strand.running_in_this_thread());
        ++_work_count;
    }

    void server_connection::pop_work()
    {
        SECR_DISPATCH_TRACE_METHOD_N("server_connection", __func__, _work_count);
        assert(_strand.running_in_this_thread());
        if (--_work_count == 0) {
            if (_pending_finished and _responder_complete and _requests_pending_dispatch.empty())
            {
                auto pf = std::move(_pending_finished);
                pf->complete(_error);
            }
        }
    }
    
    
}}}