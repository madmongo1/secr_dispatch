#pragma once
#include <secr/dispatch/config.hpp>
#include <secr/dispatch/http/server_request.hpp>
#include <secr/dispatch/asioex/transfer.hpp>
#include <secr/dispatch/asioex/errors.hpp>

namespace secr { namespace dispatch { namespace http {

    template<class SocketType>
    struct responder
    {
        static constexpr const char* classname = "responder";
        using socket_type = SocketType;
        
        responder(asio::io_service::strand& strand, socket_type& socket)
        : _strand(strand)
        , _socket(socket)
        {
            SECR_DISPATCH_TRACE_METHOD(classname, __func__);
        }
        
        void submit(std::shared_ptr<request_context> context)
        {
            SECR_DISPATCH_TRACE_METHOD(classname, __func__);
            _strand.dispatch([this, context = std::move(context)]() mutable
                             {
                                 _operations.push_back([this, context = std::move(context)]()
                                                       mutable
                                                       {
                                                           async_read_from_context(std::move(context));
                                                       });
                                 start_responding();
                             });
        }
        
        void submit_error(error_code ec)
        {
            SECR_DISPATCH_TRACE_METHOD_N(classname, __func__, ec.message());
            _strand.dispatch([this, ec]{
                _operations.push_back([this, ec](){
                    if (not _last_error) {
                        _last_error = ec;
                        error_code sink;
//                        _socket.shutdown(asio::socket_base::shutdown_send, sink);
                        response_complete();
                    }
                });
                start_responding();
            });
        }
        
        template<class F>
        void async_wait(F&& handler) {
            SECR_DISPATCH_TRACE_METHOD(classname, __func__);
            using handler_type = std::decay_t<F>;
            
            auto wrapped_handler = [this,
                                    handler = handler_type(std::forward<F>(handler))](const error_code& ec)
            mutable
            {
                SECR_DISPATCH_TRACE_METHOD(classname, "wrapper_handler");
                _strand.get_io_service().post(std::bind(std::move(handler), ec));
            };
            _strand.dispatch([this,
                              handler = std::move(wrapped_handler)]()
                             mutable
                             {
                                 SECR_DISPATCH_TRACE_METHOD(classname, "async_wait~dispatch");
                                 assert(not _completion_function);
                                 _completion_function = std::move(handler);
                                 completion_check();
                             });
        }
        
    private:
        
        void start_responding()
        {
            SECR_DISPATCH_TRACE_METHOD(classname, __func__);
            assert(_strand.running_in_this_thread());
            assert(not _operations.empty());
            if (_responding) {
                return;
            }
            
            assert(not _responding);
            _responding = true;
            auto operation = std::move(_operations.front());
            _operations.pop_front();
            operation();
        }
        
        void response_complete()
        {
            assert(_strand.running_in_this_thread());
            assert(_responding);
            SECR_DISPATCH_TRACE_METHOD(classname, __func__);
            
            if (not _operations.empty()) {
                auto operation = std::move(_operations.front());
                _operations.pop_front();
                operation();
            }
            else {
                _responding = false;
                completion_check();
            }
        }
        
        void async_read_from_context(std::shared_ptr<request_context> context)
        {
            assert(_strand.running_in_this_thread());
            assert(_responding);
            assert(context.get());
            SECR_DISPATCH_TRACE_METHOD(classname, __func__);
            if (_last_error) {
                return response_complete();
            }
            asioex::transfer(context->response_stream(),
                             _socket,
                             _strand.wrap([this, context]
                                          (auto& ecr, auto&ecw, auto sr, auto sw)
                                          {
                                              this->transfer_done(context, ecr,
                                                      ecw, sr, sw);
                                          }));
        }
        
        void transfer_done(std::shared_ptr<request_context> context,
                           const error_code& read_error,
                           const error_code& write_error,
                           std::size_t bytes_read,
                           std::size_t bytes_written)
        {
            assert(_strand.running_in_this_thread());
            assert(_responding);
            assert(context.get());
            SECR_DISPATCH_TRACE_METHOD_N(classname, __func__,
                                         read_error.message(),
                                         write_error.message(),
                                         bytes_read,
                                         bytes_written);
            
            
            if (write_error) {
                error_code sink;
//                _socket.shutdown(asio::socket_base::shutdown_send, sink);
                if (!_last_error)
                    _last_error = write_error;
                return response_complete();
            }
            else if (asioex::error_not_eof(read_error))
            {
                error_code sink;
//                _socket.shutdown(asio::socket_base::shutdown_send, sink);
                if (!_last_error)
                    _last_error = read_error;
                return response_complete();
            }
            else if (asioex::is_eof(read_error)) {
                // eof
                if (context->must_force_close_on_response())
                {
                    _last_error = asio::error::basic_errors::operation_aborted;
                    error_code sink;
//                    _socket.shutdown(asio::socket_base::shutdown_send, sink);
                    _operations.clear();
                }
                return response_complete();
            }
            else {
                async_read_from_context(context);
            }
        }
        
        bool working() const {
            assert(_strand.running_in_this_thread());
            return _responding or (not _operations.empty());
        }
        
        void completion_check()
        {
            SECR_DISPATCH_TRACE_METHOD(classname, __func__);
            assert(_strand.running_in_this_thread());
            if (_completion_function and (not working()) and _last_error)
            {
                auto f = std::move(_completion_function);
                f(_last_error);
            }
        }
        
        
        
    private:
        asio::io_service::strand& _strand;
        socket_type& _socket;
        error_code _last_error = error_code();
        std::deque<std::function<void()>> _operations;
        std::function<void(const error_code&)> _completion_function = nullptr;
        bool _responding = false;
    };

}}}