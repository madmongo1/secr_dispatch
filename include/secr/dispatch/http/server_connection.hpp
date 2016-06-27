#pragma once

#include <functional>
#include <deque>

#include <secr/dispatch/config.hpp>
#include <secr/dispatch/http/identifiers.hpp>
#include <secr/dispatch/http/exception.hpp>
#include <secr/dispatch/polymorphic_stream.hpp>
#include <secr/dispatch/buffered_stream.hpp>
#include <secr/dispatch/http/request_header.hpp>
#include <secr/dispatch/http/dispatcher.hpp>

#include <contrib/http_parser/http_parser.h>
#include <secr/dispatch/http/server_request.hpp>
#include <boost/optional.hpp>
#include <secr/dispatch/http/responder.hpp>

#include <boost/log/trivial.hpp>

namespace secr { namespace dispatch { namespace http {
    
    
    struct server_connection
    {
        using completion_arg = void;
        using completion_future = shared_future<completion_arg>;
        using completion_handler = std::function<void(completion_future)>;
        
        using dispatch_promise = promise<dispatch_context>;
        using dispatch_future = future<dispatch_context>;
        using dispatch_shared_future = shared_future<dispatch_context>;
        using dispatch_function = std::function<const void(dispatch_shared_future)&>;
        
        template<class StreamType>
        server_connection(StreamType&& stream, asio::io_service& dispatch_io_service)
        : server_connection(polymorphic_stream(is_owner,
                                               std::forward<StreamType>(stream)),
                            dispatch_io_service)
        {}
        
        /// create a server connection with an already-open polymorphic stream
        /// @pre polymorphic_stream is valid and contains an open, connected
        ///      async stream object (e.g. a socket)
        /// @post the server_connection takes ownership of the poly socket
        /// @param stream is a polymorphic stream object
        server_connection(polymorphic_stream stream, asio::io_service& dispatch_io_service);
        
        // wait for the next avaiable dispatch request
        template<class Handler>
        void async_wait_dispatch(Handler&& handler);
        
        
        /// Start the server, dispatch all requests, send responses.
        /// Once complete, call the completion handler with the last error
        /// @note the callback will happen on the socket's io_service
        /// *not* the dispatcher io service
        template<class Handler>
        void async_start(Handler&& handler);
        
        asio::io_service& get_io_service() {
            return _io_service;
        }
        
        asio::io_service::strand& get_strand() {
            return _strand;
        }
        
        
    private:
        /// second part of constuctor - required because of c api
        void init_callbacks();
        
        // parser handlers - to be called on the strand
        int handle_message_begin();
        int handle_message_url(const char* begin, std::size_t size);
        int handle_message_header_field(const char* begin, std::size_t size);
        int handle_message_header_value(const char* begin, std::size_t size);
        int handle_message_headers_complete();
        int handle_message_body(const char* begin, std::size_t size);
        void end_of_message_check();
        void handle_message_fully_read();
        
        
        // pause the consumption of data
        void pause();
        
        // resume the consumption of data
        void unpause();
        
        bool collect_more_data();
        void handle_read(const error_code& ec, std::size_t bytes_available);
        
        void handle_transport_error(const error_code& ec);
        void handle_protocol_error(http_errno err);
        void handle_protocol_error(std::exception_ptr);
        // void handle_end_of_stream();
        //        bool check_for_completion();
        //        bool check_for_finished();
        
        /// creates a new receiver
        void new_receiver();
        
        /// aborts a receiver before it gets to the stage where it can be dispatched
        /// @note this will cause an async_dispatch to complete with the given error code
        void receiver_abort(std::exception_ptr exception);
        
        /// indicate to the receiver's read stream that the receiver will get no more data
        /// ec would normally be eof unless there is an error on the stream
        void receiver_end_request(error_code ec);
        
        /// notify the receiver that it is available for dispatching
        /// @note will cause an async_dispatch to complete
        void receiver_available_for_dispatch();
        
        /// notify the receiver that it can receive responses and forward them back to the client
        void receiver_available_for_response();
        
        /// if there is a dispatch request pending and there is a receiver avaiable for
        /// dispatch, match the two and initiate the dispatch
        void attempt_dispatch();
        
        
        void push_work();
        void pop_work();
        template<class F>
        void with_work(server_connection* self, F&& f)
        {
            self->push_work();
            try {
                f();
            }
            catch(...) {
                self->pop_work();
                throw;
            }
            self->pop_work();
        }
        
        connection_id _connection_id { connection_id::generate };
        http_parser_settings* parser_settings() { return std::addressof(_parser_settings); }
        http_parser* parser() { return std::addressof(_parser); }
        
        polymorphic_stream _connection;
        std::array<char, 4096> _read_buffer;
        
        asio::io_service& _io_service { _connection.get_io_service() };
        asio::io_service::strand _strand { _io_service };
        asio::io_service& _dispatch_service;
        
        http_parser _parser;
        http_parser_settings _parser_settings;
        
        // building requests
        
        using request_ptr = std::shared_ptr<request_context>;
        /// the request curently being built or sent data
        request_ptr _current_receiver;
        
        /// the queue of all requests waiting to be dispatched
        /// @note   requests will be added to the queue the moment they have a complete
        ///         header.
        /// @note   each async_dispatch call will pop one off the queue.
        std::deque<request_ptr> _requests_pending_dispatch;
        
        struct dispatch_op {
            dispatch_op() = default;
            dispatch_op(dispatch_op&&) = default;
            dispatch_op& operator=(dispatch_op&&) = default;
            virtual ~dispatch_op() = default;
            
            void complete(std::exception_ptr pe) {
                _promise.set_exception(std::move(pe));
                call(_promise.get_future().share());
            }
            
            void complete(dispatch_context context)
            {
                _promise.set_value(std::move(context));
                call(_promise.get_future().share());
            }
            
        private:
            virtual void call(dispatch_shared_future f) = 0;
            
            dispatch_promise _promise;
        };
        
        struct server_finished_op
        {
            server_finished_op() = default;
            server_finished_op(const server_finished_op&) = default;
            server_finished_op(server_finished_op&&) = default;
            server_finished_op& operator=(const server_finished_op&) = default;
            server_finished_op& operator=(server_finished_op&&) = default;
            virtual ~server_finished_op() = default;
            
            virtual void complete(std::exception_ptr errorr) = 0;
        };
        
        /// the current oustanding wait_dispatch request
        std::shared_ptr<dispatch_op> _pending_dispatch;
        
        /// The operation to execute when the http server has finished all tasks
        std::unique_ptr<server_finished_op> _pending_finished;
        
        // feeding request streams
        
        std::size_t _pause_count = 1;
        
        // the first error collected.
        // if there is an error, the service must stop and no more
        // dispatches may happen
        std::exception_ptr _error;
        
        
        /// the number of async operations currently either queued or executing
        /// when this number reaches zero, we are eligible for completion
        std::size_t _work_count { 0 };
        
        responder<polymorphic_stream> _responder { _strand, _connection };
        bool _responder_complete = false;
        
        connection_id _id { connection_id::generate };
        
    };
    
    // wait for the next avaiable dispatch request
    template<class Handler>
    void server_connection::async_wait_dispatch(Handler&& handler)
    {
        SECR_DISPATCH_TRACE_METHOD("server_connection", __func__);
        using handler_type = std::decay_t<Handler>;
        
        struct my_dispatch_op : dispatch_op
        {
            my_dispatch_op(asio::io_service& io_service,
                           handler_type&& handler)
            : _io_service(io_service)
            , _handler(std::move(handler))
            {}
            
            void call(dispatch_shared_future f) {
                SECR_DISPATCH_TRACE_METHOD("server_connection::async_wait_dispatch::my_dispatch_op", __func__);

                _io_service.post([handler = std::move(_handler), f = std::move(f)]
                                 {
                                     handler(f);
                                 });
            }
            asio::io_service& _io_service;
            handler_type _handler;
        };
        
        assert(!_pending_dispatch);
        _strand.dispatch([this,
                          handler = std::move(handler)]() mutable {
            with_work(this, [this, handler = std::move(handler)]() mutable{
                assert(!_pending_dispatch);
                _pending_dispatch = std::make_shared<my_dispatch_op>(_dispatch_service,
                                                                     std::move(handler));
                attempt_dispatch();
            });
        });
    }
    
    
    template<class Handler>
    void server_connection::async_start(Handler&& handler)
    {
        using std::declval;
        
        using handler_type = std::decay_t<Handler>;
        using wrapped_type = decltype(declval<asio::io_service>().wrap(declval<handler_type>()));
        
        struct finish_handler : server_finished_op
        {
            finish_handler(wrapped_type handler)
            : _handler(std::move(handler))
            {}
            
            void complete(std::exception_ptr error)
            {
                _handler(std::move(error));
            }
            
            wrapped_type _handler;
        };
        
        _strand.dispatch([this,
                          handler = finish_handler(_io_service.wrap(std::forward<Handler>(handler)))]() mutable
                         {
                             push_work();
                             _responder.async_wait(_strand.wrap([this](const error_code& ec) {
                                 // when the responder is finished, cause any read ops
                                 // on the stream to cancel
                                 error_code sink;
                                 this->_connection.cancel(sink);
                                 _responder_complete = true;
                                 pop_work();
                             }));
                             
                             with_work(this,
                                       [this, handler = std::move(handler)]() mutable
                                       {
                                           assert(!_error);
                                           assert(_pause_count == 1);
                                           
                                           _pending_finished = std::make_unique<finish_handler>(std::move(handler));
                                           
                                           unpause();
                                       });
                         });
    }
    
    
    
    
}}}