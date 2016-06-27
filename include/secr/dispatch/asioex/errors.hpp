#pragma once
#include <openssl/err.h>

namespace secr { namespace dispatch { namespace asioex {
   
    inline
    bool is_eof(const error_code& ec) {
        static const auto short_read = ERR_PACK(ERR_LIB_SSL, 0, SSL_R_SHORT_READ);
        // 0x140DB000

        return ec == asio::error::misc_errors::eof
        or (ec.category() == asio::error::ssl_category
            and ec.value() == short_read);
        // 0x140000DB
        
    }

    inline
    bool error_not_eof(const error_code& ec)
    {
        return ec and (not is_eof(ec));
    }
}}}