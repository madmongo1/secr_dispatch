#pragma once
#include <contrib/http_parser/http_parser.h>

namespace secr { namespace dispatch { namespace http {
    
    using http_parser = ::http_parser;
    
    inline
    http_parser* as_pointer(http_parser& parser) {
        return std::addressof(parser);
    }
    
    inline
    http_parser* as_pointer(http_parser* parser) {
        return parser;
    }
    
    inline
    http_parser& as_reference(http_parser* parser) {
        return *parser;
    }
 
    inline
    http_errno parser_error(http_parser* parser)
    {
        return HTTP_PARSER_ERRNO(parser);
    }

    inline
    http_errno parser_error(http_parser& parser)
    {
        return parser_error(as_pointer(parser));
    }
    
}}}
