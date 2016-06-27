#pragma once

#include <google/protobuf/message.h>
#include <utility>
#include <memory>


namespace secr { namespace dispatch { namespace api {

    inline
    auto ensure_arena(std::shared_ptr<google::protobuf::Arena>& arena)
    {
        if (not arena) {
            arena = std::make_shared<google::protobuf::Arena>();
        }
        return arena.get();
    }
  
    template<
    class MessageType,
    std::enable_if_t< std::is_base_of<google::protobuf::Message, std::remove_const_t<MessageType>>::value >* = nullptr
    >
    std::shared_ptr<google::protobuf::Arena> shared_arena(const std::shared_ptr<MessageType>& msg_ptr)
    {
        if (auto arena = msg_ptr->GetArena())
        {
            return {
                msg_ptr,
                arena
            };
        }
        else {
            return std::make_shared<google::protobuf::Arena>();
        }
    }
    
    
    template<
    class MessageType,
    std::enable_if_t< std::is_base_of<google::protobuf::Message, MessageType>::value >* = nullptr
    >
    std::shared_ptr<MessageType>
    create_message(std::shared_ptr<google::protobuf::Arena> arena)
    {
        auto p = ensure_arena(arena);
        return {
            std::move(arena),
            google::protobuf::Arena::CreateMessage<MessageType>(p)
        };
    }
    
    template<class AnyType, class...Args>
    std::shared_ptr<AnyType>
    create(std::shared_ptr<google::protobuf::Arena> arena,
           Args&&...args)
    {
        auto p = ensure_arena(arena);
        return {
            std::move(arena),
            google::protobuf::Arena::Create<AnyType>(p,
                                            std::forward<Args>(args)...)
        };
    }

}}}