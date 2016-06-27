#pragma once

#include <secr/dispatch/config.hpp>

#include <mutex>
#include <condition_variable>
#include <numeric>
#include <typeinfo>

namespace secr { namespace dispatch {
    
    /// This is a pseudo stream. 
    /// @note   It models:
    ///         AsyncReadStream
    ///         AsyncWriteStream
    ///         SyncReadStream
    ///         SyncWriteStream
    /// plus has the following methods:
    ///     reset()
    ///     close()
    
    class fake_stream
    {
        using mutex_type = std::mutex;
        using lock_type = std::unique_lock<mutex_type>;

        struct consume_op
        {
            /// Write data from the source buffer to the correct place,
            /// @returns the number of bytes written to the receiver
            /// @note it is up to the caller to call write() again in order
            ///         to transfer any remaining bytes
            virtual std::size_t consume(const lock_type& lock, asio::const_buffer data) = 0;

            /// Inform the commit op that the stream is in error. Possibly complete
            /// any outstanding call
            virtual void set_error(const lock_type& lock, error_code ec) = 0;

            /// signal the commit op that memory transfers and error setting is
            /// complete. It should complete the outstanding read if it has not
            /// already done so
            virtual void commit(lock_type lock) = 0;

            virtual ~consume_op() = default;
        };
        
        
        
        using consume_op_ptr = std::unique_ptr<consume_op>;
        
        /// Construct a stream.
        /// @param  read_io_service is the io_service on which async_read operations
        ///         will complete.
        /// @param  write_io_service is the io_service on which async_write operations
        ///         will complete
        ///
    public:
        fake_stream(asio::io_service& read_io_service,
                    asio::io_service& write_io_service);
        
        ~fake_stream() noexcept {
            cancel();
        }

        /// Mark the stream as in error. If there is an outstanding read operation,
        /// cause it to complete with an error code.
        /// @pre ec *must not* be empty or equivalent to error_code()
        ///
        void set_error(error_code ec) {
            assert(ec);
            auto lock = get_lock();
            _error_code = ec;
            flush_to_op(std::move(lock));
        }
        
        
        // SyncWriteStream

        /// Write some data to the fake stream, possibly causing outstanding reads
        /// to complete
        /// @see http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference/SyncWriteStream.html
        /// Note that this call will always complete in a timely fashion (a small
        /// block may be involved to protect access during memory buffer tansfers)
        /// The size of the receive area within this object is more-or-less limitless
        ///
        template<class ConstBufferSequence>
        std::size_t write_some(const ConstBufferSequence& buffers, error_code& ec)
        {
            auto lock = get_lock();
            if (_error_code)
            {
                ec = _error_code;
                return 0;
            }

            auto copied = asio::buffer_copy(_bytes_recvd.prepare(asio::buffer_size(buffers)),
                                            buffers);
            _bytes_recvd.commit(copied);
            flush_to_op(std::move(lock));
            return copied;
        }

        template<class ConstBufferSequence>
        std::size_t write_some(const ConstBufferSequence& buffers)
        {
            error_code ec;
            auto s = write_some(buffers, ec);
            if (ec) throw system_error(ec);
            return s;
        }
        
    private:
        std::size_t flush_to_op(lock_type lock)
        {
            std::size_t bytes_flushed = 0;
            if (_consume_op) {
                auto data = _bytes_recvd.data();
                if (asio::buffer_size(data))
                {
                    auto consumed = _consume_op->consume(lock, data);
                    _bytes_recvd.consume(consumed);
                    auto copy = std::move(_consume_op);
                    copy->commit(std::move(lock));
                }
                else if (_error_code) {
                    _consume_op->set_error(lock, _error_code);
                    auto copy = std::move(_consume_op);
                    copy->commit(std::move(lock));
                }
            }
            return bytes_flushed;
        }
        
        // AsyncWriteStream
    public:
        asio::io_service& get_write_io_service() { return _write_io_service; }

        template<class ConstBufferSequence, class Handler>
        void async_write_some(const ConstBufferSequence& buffers, Handler&& handler)
        {
            error_code ec;
            auto written = write_some(buffers, ec);
            _write_io_service.post([ec, written, handler = std::move(handler)]() mutable{
                handler(ec, written);
            });
        }

        
    public:
        // AsyncReadStream
        
        /// Return a reference to the io_service on which async calls shall be
        /// dispatched
        /// @see http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference/AsyncReadStream.html
        ///
        asio::io_service& get_read_io_service() { return _read_io_service; }
        
        /// read some data asynchronously.
        /// @see http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference/AsyncReadStream.html
        /// for documentation of the concept that this call models
        ///
        template<class MutableBufferSequence, class AsyncReadHandler>
        void async_read_some(MutableBufferSequence&& buffers, AsyncReadHandler&& handler);
        
        // SyncReadStream
        
        template<class MutableBufferSequence>
        std::size_t read_some(MutableBufferSequence&& buffers, error_code& ec);

        template<class MutableBufferSequence>
        std::size_t read_some(MutableBufferSequence&& buffers)
        {
            error_code ec;
            std::size_t s = read_some(std::forward<MutableBufferSequence>(buffers), ec);
            if (ec) throw system_error(ec);
            return s;
        }
        
        /// Cancel all pending I/O operations
        /// @note cancels all I/O operations - including blocked synchronous
        /// reads
        void cancel() noexcept
        {
            try {
                auto lock = get_lock();
                if (_consume_op) {
                    _consume_op->set_error(lock, asio::error::basic_errors::operation_aborted);
                    auto copy = std::move(_consume_op);
                    copy->commit(std::move(lock));
                }
            }
            catch(...) {
                // ignore
            }
        }
        
        /// `Closes` the fake socket
        
        
        void close()
        {
            set_error(asio::error::misc_errors::eof);
        }
        
        /// Reset the stream, causing any readers to see the stream as EOF,
        /// remove all data and then re-open the stream for new input
        void reset()
        {
            auto lock = get_lock();
            _error_code = error_code();
            _bytes_recvd.consume(asio::buffer_size(_bytes_recvd.data()));
            if (_consume_op) {
                _consume_op->set_error(lock, asio::error::basic_errors::operation_aborted);
                auto copy = std::move(_consume_op);
                copy->commit(std::move(lock));
            }
        }
        
    private:
        template<class Handler, class MutableBufferSequence>
        struct async_read_op;
        template<class Handler, class MutableBufferSequence>
        friend struct async_read_op;

        consume_op_ptr set_consume_op(consume_op_ptr ptr) { std::swap(ptr, _consume_op); return ptr; }

    private:
        lock_type get_lock() { return lock_type(_mutex); }
        
        asio::io_service& _read_io_service; ///! The io_service on which reads will complete
        asio::io_service& _write_io_service; ///! The io_service on which write will complete
        error_code _error_code;
        asio::streambuf _bytes_recvd;
        
        std::unique_ptr<consume_op> _consume_op;
        
        std::mutex _mutex;
    };
    
    template<class MutableBufferSequence>
    struct transfer_to_buffers_op
    {
        using buffer_sequence_type = MutableBufferSequence;
        using sequence_iterator = decltype(std::declval<buffer_sequence_type>().begin());

        transfer_to_buffers_op(MutableBufferSequence target_buffers)
        : _target_buffers(std::move(target_buffers))
        {}
        
        std::size_t transfer(asio::const_buffer data)
        {
            std::size_t transferred = 0;
            while (!complete() && asio::buffer_size(data))
            {
                auto target_buffer = *_first + _pos;
                if (asio::buffer_size(target_buffer))
                {
                    auto copied = asio::buffer_copy(target_buffer, data);
                    _pos += copied;
                    target_buffer = target_buffer + copied;
                    data = data + copied;
                    transferred += copied;
                }
                if (asio::buffer_size(target_buffer) == 0)
                {
                    ++_first;
                    _pos = 0;
                }
            }
            _count += transferred;
            return transferred;
        }
        
        bool complete() const {
            return _first == _last;
        }
        
        std::size_t count() const {
            return _count;
        }

        buffer_sequence_type _target_buffers;
        sequence_iterator _first = _target_buffers.begin();
        sequence_iterator _last = _target_buffers.end();
        std::size_t _pos = 0;
        std::size_t _count = 0;
    };
    
    template<class Handler, class MutableBufferSequence>
    struct fake_stream::async_read_op : fake_stream::consume_op
    {
        using buffer_sequence_type = MutableBufferSequence;
        using sequence_iterator = decltype(std::declval<buffer_sequence_type>().begin());
        using handler_type = Handler;
        
        async_read_op(buffer_sequence_type target_buffers,
                      handler_type handler)
        : consume_op()
        , _transfer_op(std::move(target_buffers))
        , _handler(std::move(handler))
        {
            
        }
        
        std::size_t consume(const lock_type& lock,
                            asio::const_buffer data) override
        {
            auto transferred = _transfer_op.transfer(data);
            return transferred;
        }
        
        void set_error(const lock_type& lock, error_code ec) override
        {
            _error_code = ec;
        }
        
        void commit(lock_type lock) override
        {
            _handler(std::move(lock), _error_code, _transfer_op.count());
        }
        
        transfer_to_buffers_op<buffer_sequence_type> _transfer_op;
        handler_type _handler;
        error_code _error_code = error_code();
    };
    
    template<class MutableBufferSequence, class AsyncReadHandler>
    void fake_stream::async_read_some(MutableBufferSequence&& buffers,
                                           AsyncReadHandler&& handler)
    {
        using buffer_sequence_type = std::decay_t<MutableBufferSequence>;
        using handler_type = std::decay_t<AsyncReadHandler>;
        
        auto lock = get_lock();
        assert(not _consume_op);
        
        auto dispatch_handler = [this,
                                 handler = handler_type(std::forward<AsyncReadHandler>(handler))]
        (auto lock, auto& ec, auto size) mutable
        {
            _read_io_service.post([handler = std::move(handler), ec, size]() mutable
            {
                handler(ec, size);
            });
        };
        
        using final_handler_type = decltype(dispatch_handler);
        
        using op_type = async_read_op<final_handler_type, buffer_sequence_type>;
        
        _consume_op = std::make_unique<op_type>(std::forward<MutableBufferSequence>(buffers),
                                                std::move(dispatch_handler));
        flush_to_op(std::move(lock));
    }
    
    template<class MutableBufferSequence>
    std::size_t fake_stream::read_some(MutableBufferSequence&& buffers,
                                           error_code& ec)
    {
        using buffer_sequence_type = std::decay_t<MutableBufferSequence>;
        
        auto lock = get_lock();
        auto available_data = _bytes_recvd.data();

        if (asio::buffer_size(available_data))
        {
            transfer_to_buffers_op<buffer_sequence_type> transfer(std::forward<MutableBufferSequence>(buffers));
            auto transferred = transfer.transfer(available_data);
            _bytes_recvd.consume(transferred);
            ec = error_code();
            return transferred;
        }
        else if (_error_code) {
            ec = _error_code;
            return 0;
        }
        else {
            bool completed = false;
            std::size_t bytes_transferred = 0;
            std::condition_variable cv;
            auto handler = [&](lock_type lock, const error_code& _ec, std::size_t _bytes_transferred) {
                completed = true;
                bytes_transferred = _bytes_transferred;
                ec = _ec;
                lock.unlock();
                cv.notify_one();
            };
            using op_type = async_read_op<decltype(handler), buffer_sequence_type>;
            _consume_op = std::make_unique<op_type>(std::forward<MutableBufferSequence>(buffers),
                                                    std::move(handler));
            cv.wait(lock, [&] { return completed; });
            return bytes_transferred;
        }
    }
    
    struct fake_stream_read_interface
    {
        fake_stream_read_interface(fake_stream& stream)
        : _stream(stream)
        {}
        
        // AsyncReadStream
        
        /// Return a reference to the io_service on which async calls shall be
        /// dispatched
        /// @see http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference/AsyncReadStream.html
        ///
        asio::io_service& get_io_service() { return _stream.get_read_io_service(); }
        
        /// read some data asynchronously.
        /// @see http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference/AsyncReadStream.html
        /// for documentation of the concept that this call models
        ///
        template<class MutableBufferSequence, class AsyncReadHandler>
        void async_read_some(MutableBufferSequence&& buffers, AsyncReadHandler&& handler)
        {
            return _stream.async_read_some(std::forward<MutableBufferSequence>(buffers),
                                           std::forward<AsyncReadHandler>(handler));
        }
        
        // SyncReadStream
        
        template<class MutableBufferSequence>
        std::size_t read_some(MutableBufferSequence&& buffers, error_code& ec)
        {
            return _stream.read_some(std::forward<MutableBufferSequence>(buffers),
                                     ec);
        }

        template<class MutableBufferSequence>
        std::size_t read_some(MutableBufferSequence&& buffers)
        {
            return _stream.read_some(std::forward<MutableBufferSequence>(buffers));
        }
        
        /// Cancel all pending I/O operations
        /// @note cancels all I/O operations - including blocked synchronous
        /// reads
        void cancel() noexcept
        {
            return _stream.cancel();
        }
        
        void close() {
            _stream.close();
        }


    private:
        fake_stream& _stream;
    };

    struct fake_stream_write_interface
    {
        fake_stream_write_interface(fake_stream& stream)
        : _stream(stream)
        {}
        
        // SyncWriteStream
        
        /// Write some data to the fake stream, possibly causing outstanding reads
        /// to complete
        /// @see http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference/SyncWriteStream.html
        /// Note that this call will always complete in a timely fashion (a small
        /// block may be involved to protect access during memory buffer tansfers)
        /// The size of the receive area within this object is more-or-less limitless
        ///
        template<class ConstBufferSequence>
        std::size_t write_some(const ConstBufferSequence& buffers, error_code& ec)
        {
            return _stream.write_some(buffers, ec);
        }
        
        template<class ConstBufferSequence>
        std::size_t write_some(const ConstBufferSequence& buffers)
        {
            return _stream.write_some(buffers);
        }

        void close() {
            _stream.close();
        }
        
    private:
        fake_stream& _stream;
    };

}}