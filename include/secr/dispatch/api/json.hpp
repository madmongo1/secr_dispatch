#pragma once
#include <google/protobuf/message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/type_resolver_util.h>
#include <valuelib/stdext/unique_ptr.hpp>
#include <valuelib/tuple/algorithm.hpp>

#include <sstream>

namespace secr { namespace dispatch { namespace api {
    
    struct pretty_json_type {
        void operator()(google::protobuf::util::JsonOptions& opts) const {
            opts.add_whitespace = true;
        }
    };
    static constexpr pretty_json_type pretty_json{};
    
    struct compact_json_type {
        void operator()(google::protobuf::util::JsonOptions& opts) const {
            opts.add_whitespace = false;
        }
    };
    static constexpr compact_json_type compact_json{};

    struct include_defaults_type {
        void operator()(google::protobuf::util::JsonOptions& opts) const {
            opts.always_print_primitive_fields = true;
        }
    };
    static constexpr include_defaults_type include_defaults{};
    
    template<class...Options>
    auto json_options(Options&&...options)
    {
        google::protobuf::util::JsonOptions opts;
        using expand = int [];
        void(expand{
            0,
            ((options(opts)),0)...
        });
        return opts;
    }

    std::string as_json(const google::protobuf::Message& msg,
                        google::protobuf::util::JsonOptions opts = json_options(pretty_json,
                                                                                include_defaults));
    
    std::string as_json(const google::protobuf::Message* msg,
                        google::protobuf::util::JsonOptions opts = json_options(pretty_json,
                                                                                include_defaults));
    
}}}