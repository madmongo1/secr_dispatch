#pragma once

#include <secr/dispatch/config.hpp>
#include <secr/dispatch/ownership.hpp>

namespace secr { namespace dispatch {


    struct polymorphic_stream
    {
        using write_handler_function = std::function<void(const error_code& ec, std::size_t bytes_transferred)>;
        using read_handler_function = std::function<void(const error_code& ec, std::size_t bytes_transferred)>;
        using completion_handler_function = std::function<void()>;
        
        struct io_completion_handler {
            virtual void run(const error_code& ec, std::size_t) = 0;
        };
        
        struct concept {
            
            virtual void async_read_some(const asio::mutable_buffers_1& buffer_sequence, read_handler_function&&) = 0;
            virtual void async_write_some(const asio::const_buffers_1& buffer,
                                          std::shared_ptr<io_completion_handler>) = 0;
            virtual error_code close(error_code& ec) = 0;
            virtual error_code cancel(error_code& ec) = 0;

            virtual void shutdown(asio::socket_base::shutdown_type) = 0;
            virtual error_code shutdown(asio::socket_base::shutdown_type, error_code&) = 0;

            virtual asio::io_service& get_io_service() = 0;
            
            virtual ~concept() = default;
        };
        using concept_ptr_type = std::unique_ptr<concept>;
        
        template<class OwnershipWrapper>
        struct model : concept {
            using wrapper_type = OwnershipWrapper;
            using stream_type = typename wrapper_type::element_type;
            
            model(wrapper_type stream_wrapper) : _stream_wrapper(std::move(stream_wrapper)) {}
            
            void async_read_some(const asio::mutable_buffers_1& buffer_sequence,
                                 read_handler_function&& read_handler) override
            {
                stream().async_read_some(buffer_sequence, std::move(read_handler));
            }
            
            void async_write_some(const asio::const_buffers_1& buffer,
                                  std::shared_ptr<io_completion_handler> handler) override
            {
                stream().async_write_some(buffer,
                                          [handler = std::move(handler)]
                                          (auto&ec, auto size) {
                                              handler->run(ec, size);
                                          });
            }
            
            error_code close(error_code& ec) override
            {
                return _stream_wrapper.get().lowest_layer().close(ec);
            }
            
            asio::io_service& get_io_service() override
            {
                return _stream_wrapper.get().get_io_service();
            }
            
            void shutdown(asio::socket_base::shutdown_type type) override
            {
                return _stream_wrapper.get().lowest_layer().shutdown(type);
            }

            error_code shutdown(asio::socket_base::shutdown_type type, error_code& ec) override
            {
                return _stream_wrapper.get().lowest_layer().shutdown(type, ec);
            }

            error_code cancel(error_code& ec) override
            {
                return _stream_wrapper.get().lowest_layer().cancel(ec);
            }
            
            
            stream_type& stream() { return _stream_wrapper.get(); }
            
            wrapper_type _stream_wrapper;
        };
        
        template<class StreamType>
        static auto create_model(is_owner_type, StreamType&& stream)
        {
            using stream_type = std::decay_t<StreamType>;
            using wrapper_type = ownership_wrapper<stream_type>;
            using model_type = model<wrapper_type>;
            return std::make_unique<model_type>(wrapper_type(std::forward<StreamType>(stream)));
        }
        
        template<class StreamType>
        static auto create_model(not_owner_type, StreamType& stream)
        {
            using stream_type = StreamType;
            using deleter = decltype(do_not_delete());
            using ptr_type = std::unique_ptr<stream_type, deleter>;
            using wrapper_type = ownership_wrapper<ptr_type>;
            using model_type = model<wrapper_type>;
            return std::make_unique<model_type>(wrapper_type(ptr_type(std::addressof(stream),
                                                                          do_not_delete())));
        }
        
        template<class StreamType>
        polymorphic_stream(is_owner_type is_owner, StreamType&& stream)
        : _impl { create_model(is_owner, std::forward<StreamType>(stream)) }
        {
        }
        
        template<class StreamType>
        polymorphic_stream(not_owner_type not_owner, StreamType& stream)
        : _impl { create_model(not_owner, stream) }
        {
        }
        
        template<
        class StreamType,
        std::enable_if_t<not std::is_base_of<polymorphic_stream, StreamType>::value>* = nullptr>
        polymorphic_stream(StreamType&& stream)
        : polymorphic_stream { is_owner, std::forward<StreamType>(stream) }
        {
        }
        
        template<class ReadHandler>
        void async_read_some(const asio::mutable_buffers_1& buffer, ReadHandler&& handler)
        {
            _impl->async_read_some(buffer, read_handler_function(std::move(handler)));
        }
        
        template<class ConstBufferSequence, class WriteHandler>
        void async_write_some(ConstBufferSequence&& buffers, WriteHandler&& final_handler)
        {
            using buffer_sequence_type = std::decay_t<ConstBufferSequence>;
            using final_handler_type = std::decay_t<WriteHandler>;
            
            struct write_op
            : io_completion_handler
            , std::enable_shared_from_this<write_op>
            {

                write_op(buffer_sequence_type buffers, final_handler_type handler,
                         concept_ptr_type& impl)
                : _buffer_sequence(std::move(buffers))
                , _handler(std::move(handler))
                , _impl(impl)
                {
                }
                
                void run() {
                    // if the first and last buffers are the same, we should do a 0-length
                    // write in order for the read handler to fire
                    _impl->async_write_some(asio::const_buffers_1(*_first),
                                            this->shared_from_this());
                }
                
                void run(const error_code& ec, std::size_t size) override
                {
                    _total_written += size;
                    _partial_buffer = _partial_buffer + size;
                    if (asio::buffer_size(_partial_buffer) == 0) {
                        ++_first;
                    }
                    finalisation_check(ec);
                }
                
                void finalisation_check(const error_code& ec) {
                    if (ec or (_first == _last)) {
                        _handler(ec, _total_written);
                    }
                    else {
                        if (asio::buffer_size(_partial_buffer)) {
                            _partial_buffer = *_first;
                        }
                        _impl->async_write_some(asio::const_buffers_1(_partial_buffer),
                                                this->shared_from_this());
                    }
                }
                
                buffer_sequence_type _buffer_sequence;
                final_handler_type _handler;
                
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
                using iterator = typename buffer_sequence_type::const_iterator;
                using buffer_type = typename asio::const_buffer;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
                iterator _first = iterator(_buffer_sequence.begin());
                iterator _last = iterator(_buffer_sequence.end());
                concept_ptr_type& _impl;
                buffer_type _partial_buffer { _first == _last ? buffer_type("", 0) : *_first };
                std::size_t _total_written = 0;
            };
            auto op = std::make_shared<write_op>(std::forward<ConstBufferSequence>(buffers),
                                                 std::forward<WriteHandler>(final_handler),
                                                 _impl);
            op->run();
        }

        void shutdown(asio::socket_base::shutdown_type type)
        {
            return _impl->shutdown(type);
        }
        
        error_code shutdown(asio::socket_base::shutdown_type type, error_code& ec)
        {
            return _impl->shutdown(type, ec);
        }

        void close() {
            error_code ec;
            if(close(ec))
                throw system_error(ec, "close");
        }
        
        error_code close(error_code& ec) {
            return _impl->close(ec);
        }
        
        error_code cancel(error_code& ec) {
            return _impl->cancel(ec);
        }
        
        void cancel() {
            error_code ec;
            if(cancel(ec))
                throw system_error(ec, "cancel");
        }
        
        asio::io_service& get_io_service() {
            return _impl->get_io_service();
        }
        
    private:
        concept_ptr_type _impl;
    };
    
    

}}