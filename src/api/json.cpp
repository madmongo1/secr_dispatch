#include <secr/dispatch/api/json.hpp>



namespace secr { namespace dispatch { namespace api {

    std::string as_json(const google::protobuf::Message& msg,
                        google::protobuf::util::JsonOptions opts)
    {
        namespace pb = google::protobuf;
        namespace pbu = google::protobuf::util;
        
        auto buffer = msg.SerializeAsString();
        std::string result;
        pb::io::ArrayInputStream zistream(buffer.data(), buffer.size());
        
        auto resolver = std::unique_ptr<pbu::TypeResolver> {
            pbu::NewTypeResolverForDescriptorPool("",
                                                  pb::DescriptorPool::generated_pool())
        };
        
        auto status = google::protobuf::util::BinaryToJsonString(resolver.get(),
                                                                 "/" + msg.GetDescriptor()->full_name(),
                                                                 buffer,
                                                                 std::addressof(result),
                                                                 opts);
        if (!status.ok())
        {
            std::ostringstream ss;
            ss << status;
            throw std::runtime_error(ss.str());
        }
        return result;
    }
    
    std::string as_json(const google::protobuf::Message* msg,
                        google::protobuf::util::JsonOptions opts)
    {
        return as_json(*msg, opts);
    }


    
}}}
