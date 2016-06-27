#include <secr/dispatch/http/dispatcher.hpp>
#include <stdexcept>
#include <exception>
#include <secr/dispatch/http/server_request.hpp>
#include <boost/log/trivial.hpp>
#include <valuelib/stdext/exception.hpp>

namespace secr { namespace dispatch { namespace http {
    
    dispatch_context::shared_state::
    shared_state(std::shared_ptr<request_context> request_context)
    : _request_context(std::move(request_context))
    {}
    
    dispatch_context::shared_state::~shared_state() noexcept
    {
        _request_context->request_stream().set_error(asio::error::misc_errors::eof);
    }
    
    
    // response object
    
    //
    // IMPLEMENTATION OF response_object
    //
    
    dispatch_context::response_object::~response_object()
    {
        auto ep = std::current_exception();
        try {
            if (not header_committed())
            {
                if (not ep) {
                    ep = std::make_exception_ptr(std::logic_error("server did not respond"));
                }
                set_exception(ep);
            }
        }
        catch(...)
        {
            BOOST_LOG_TRIVIAL(warning)
            << value::debug::demangle<this_class>() << "::" << __func__
            << " : exception : " << value::debug::unwrap();
        }
    }
    void dispatch_context::response_object::set_exception(std::exception_ptr ep)
    {
        if (ep and not header_committed())
        {
            commit_with_exception(ep);
        }
        else {
            _request_context.response_stream().set_error(_last_error = asio::error::basic_errors::operation_aborted);
        }
    }
    
    void
    dispatch_context::response_object::
    set_content_length(secr::dispatch::http::content_length_fixed len)
    {
        assert(_response_mode == response_mode::undecided);
        set_header(mutable_header(), "Content-Length", std::to_string(len.size()));
        
        // determine whether we must close
        
        auto ifind = find_only_header_like(_request_context.request_header(),
                                           "Connection");
        if (not ifind
            or boost::iequals(ifind->value(), "close")
            or (http_version(_request_context.request_header()) < std::make_tuple(1,1)
                and not boost::iequals(ifind->value(), "keep-alive")))
        {
            set_header(mutable_header(), "Connection", "close");
        }
        else {
            set_header(mutable_header(), "Connection", "keep-alive");
        }
        _response_mode = response_mode::content_length;
    }
    
    namespace {
        static const std::string Transfer_Encoding = "Transfer-Encoding";
        static const std::string _chunked = "chunked";
        static const std::string Content_Length = "Content-Length";
        static const std::string Connection = "Connection";
        static const std::string _keep_alive = "keep-alive";
        static const std::string _close = "close";
        
        bool supports_chunked(const HttpRequestHeader& request)
        {
            if (request.version_major() < 1)
                return false;
            if (request.version_major() == 1)
            {
                return request.version_minor() > 0;
            }
            return true;
        }
        
        bool demanding_close(const HttpRequestHeader& request)
        {
            auto first = request.headers().begin();
            auto last = request.headers().begin();
            auto ifind = std::find_if(first,
                                      last,
                                      match_header_name(Connection));
            if (std::make_tuple(request.version_major(), request.version_minor()) < std::make_tuple(1,1))
            {
                if (ifind == last or not boost::iequals(ifind->value(), _keep_alive))
                    return true;
                return false;
            }
            else {
                if (ifind == last or boost::iequals(ifind->value(), _keep_alive))
                {
                    return false;
                }
            }
            return true;
        }
    }
    
    void
    dispatch_context::response_object::
    set_content_length(secr::dispatch::http::content_length_variable)
    {
        assert(_response_mode == response_mode::undecided);
        
        auto headers = *mutable_header().mutable_headers();
        headers.erase(headers.begin(), std::remove_if(headers.begin(),
                                                      headers.end(),
                                                      match_header_name(Content_Length)));
        
        auto& request_header = _request_context.request_header();
        if (supports_chunked(request_header))
        {
            add_header(mutable_header(), Transfer_Encoding, _chunked);
            if (demanding_close(request_header))
            {
                add_header(mutable_header(), Connection, _close);
            }
            else {
                add_header(mutable_header(), Connection, _keep_alive);
            }
            _response_mode = response_mode::chunked;
        }
        else {
            add_header(mutable_header(), Connection, _close);
            _response_mode = response_mode::raw;
        }
    }
    
    auto dispatch_context::response_object::close(error_code& ec)
    -> error_code
    {
        
        if (not _last_error) {
            ec.clear();
            switch(_response_mode)
            {
                case response_mode::chunked: {
                    static const char last_chunk[] = { '0', '\r', '\n', '\r', '\n' };
                    asio::write(stream(), asio::buffer(last_chunk), ec);
                    if (not ec) {
                        stream().close();
                    }
                    _last_error = asio::error::misc_errors::eof;
                } break;
                    
                case response_mode::raw:
                case response_mode::content_length: {
                    _last_error = asio::error::misc_errors::eof;
                    stream().close();
                } break;
                    
                case response_mode::undecided: {
                    auto ep = std::current_exception();
                    if (not ep) {
                        ep = std::make_exception_ptr(std::logic_error("server did not respond"));
                        commit_with_exception(ep);
                        _last_error = asio::error::misc_errors::eof;
                    }
                }
            }
            return ec;
        }
        else {
            ec = _last_error;
            return ec;
        }
    }
    
    std::size_t dispatch_context::response_object::commit_header(error_code& ec)
    {
        assert(!_header_committed);
        
        if (not header().has_status())
        {
            ec = protocol_error_code::missing_status_line;
            return 0;
        }
        
        auto data = to_response_buffer(header());
        _header_committed = true;
        return asio::write(stream(), asio::buffer(data), ec);
    }

    std::size_t dispatch_context::response_object::commit_header()
    {
        error_code ec;
        auto s = commit_header(ec);
        if (ec) throw(system_error(ec));
        return s;
    }

    
    void
    dispatch_context::response_object::
    commit_with_exception(std::exception_ptr ep)
    {
        assert(!header_committed());
        assert(ep);
        auto& header = _request_context.response_header();
        auto arena = header.GetArena();
        auto emsg = google::protobuf::Arena::CreateMessage<secr::dispatch::api::Exception>(arena);
        populate(emsg, ep);
        auto body = api::as_json(emsg);
        
        set_status(header, 500, "Internal Server Error");
        header.set_version_major(1);
        header.set_version_minor(1);
        set_header(header, "Connection", "close");
        set_header(header, "Content-Length", std::to_string(body.size()));
        set_header(header, "Content-Type", "application/json");
        set_header(header, "X-Secr-Content-Type", "protobuf-message");
        set_header(header, "X-Secr-Message-Type", emsg->GetDescriptor()->full_name());
        try {
            commit_header();
            asio::write(stream(), asio::buffer(body));
            stream().close();
        }
        catch(...) {
            BOOST_LOG_TRIVIAL(warning)
            << value::debug::demangle<this_class>() << "::" << __func__
            << " : exception : " << value::debug::unwrap();
        }
    }
    
    std::ostream& operator<<(std::ostream&os, const dispatch_context& context)
    {
        auto& request = *(context._shared_state->_request_context);
        return os << "request: " << request.get_request_id()
        << ", connection: " << request.get_connection_id();
    }

    
    
    
    
}}}

