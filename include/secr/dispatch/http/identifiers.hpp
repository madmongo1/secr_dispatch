#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

namespace secr { namespace dispatch { namespace http {
   
    struct self_generating_uuid : boost::uuids::uuid
    {
        struct generate_type {};
        static constexpr auto generate = generate_type {};

        // hijack the constructor
        explicit self_generating_uuid(generate_type)
        : boost::uuids::uuid(boost::uuids::random_generator()())
        {
            
        }
        
    };

    struct request_id : self_generating_uuid
    {
        using self_generating_uuid::self_generating_uuid;
    };

    struct connection_id : self_generating_uuid
    {
        using self_generating_uuid::self_generating_uuid;
    };


}}}