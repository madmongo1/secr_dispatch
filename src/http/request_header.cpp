#include <secr/dispatch/http/request_header.hpp>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <boost/algorithm/string.hpp>
#include <iterator>
#include <boost/tokenizer.hpp>
#include <boost/token_iterator.hpp>
#include <regex>
#include <iostream>
#include <boost/format.hpp>
#include <numeric>

namespace secr { namespace dispatch { namespace http {
    

    const Header* find_only_header_like(const google::protobuf::RepeatedPtrField<Header>& headers, const std::string& like)
    {
        const Header* found = nullptr;
        
        for (auto& header : headers)
        {
            if (boost::iequals(header.name(), like)) {
                if (found) {
                    throw std::runtime_error("duplicate header");
                }
                found = std::addressof(header);
            }
        }
        return found;
    }

    
    const Header& require_only_header_like(const google::protobuf::RepeatedPtrField<Header>& headers,
                                           const std::string& like)
    {
        const Header* found = find_only_header_like(headers, like);
        if (not found)
            throw std::runtime_error("not found");
        return *found;
    }

    std::vector<std::reference_wrapper<const Header>>
    find_headers_like(const google::protobuf::RepeatedPtrField<Header>& headers,
                      const std::string& like)
    {
        std::vector<std::reference_wrapper<const Header>> results;

        for (auto& header : headers)
        {
            if (boost::iequals(header.name(), like)) {
                results.push_back(header);
            }
        }
        return results;
    }
    
    
    // HttpRequest
    
    void set_unique_header(HttpRequestHeader& request, const std::string& name, const std::string& value)
    {
        auto& headers = request.headers();
        auto i = std::find_if(headers.begin(), headers.end(),
                              match_header_name(value));
        if (i == headers.end())
        {
            auto hdr = request.add_headers();
            hdr->set_name(name);
            hdr->set_value(value);
        }
        else {
            throw std::logic_error("header already exists: " + name);
        }
    }
    
    bool is_separator(char c) {
        static const std::string sep_chars = "()<>@,;:\\/[]?={} \t\"";
        if (std::find(sep_chars.begin(), sep_chars.end(), c) != sep_chars.end())
            return true;
        return false;
    }
    
    template<class Iter>
    char consume_char_if(Iter& first, Iter& last, char c) {
        if (first != last && (*first == c)) {
            ++first;
            return true;
        }
        return false;
    }

    template<class Iter>
    char test_sep(Iter first) {
        auto c = *first;
        if (is_separator(c))
            return c;
        return 0;
    }
    
    bool is_control(char c) {
        return (c < 0x20) or (c == 0x7f);
    }
    
    bool is_white(char c) {
        return c == ' ' || c == '\t';
    }
    
    template<class Iter>
    std::string consume_token(Iter& first, Iter& last) {
        std::string s;
        while (first != last) {
            auto c = *first;
            if (is_control((c)))
                break;
            if (is_separator(c))
                break;
            s.push_back(c);
            ++first;
        }
        if (not s.length())
            throw std::runtime_error("invalid separator");
        return s;
    }
    template<class Iter>
    void consume_lit(Iter& first, Iter& last, char lit)
    {
        if (first == last or *first++ != lit)
            throw std::runtime_error(std::string("missing literal: ") + lit);
    }
    
    template<class Iter>
    std::string consume_token_or_quoted(Iter& first, Iter& last)
    {
        if (consume_char_if(first, last, '"'))
        {
            std::string ret;
            // quoted string
            while(first != last) {
                if (consume_char_if(first, last, '"')) {
                    // test for double-quote
                    if (consume_char_if(first, last, '"')) {
                        ret.push_back('"');
                    }
                    else {
                        return ret;
                    }
                }
                else {
                    ret.push_back(*first++);
                }
            }
            // if we get here, there was no closing quote
            throw std::runtime_error("no closing quote on parameter");
        }
        else {
            return consume_token(first, last);
        }
    }
    
    template<class Iter>
    auto skipwhite(Iter& first, Iter& last)
    {
        while (first != last) {
            auto c = *first;
            if (is_white(c))
                ++first;
            else
                break;
        }
    }
    
    std::string to_lower(std::string s) {
        boost::algorithm::to_lower(s);
        return s;
    }
    
    template<class Iter>
    void parse(ContentType& ct, Iter first, Iter last)
    {
        ct.mutable_type()->append(to_lower(consume_token(first, last)));
        consume_lit(first, last, '/');
        ct.mutable_subtype()->append(to_lower(consume_token(first, last)));
        while (first != last) {
            skipwhite(first, last);
            if (consume_char_if(first, last, ';'))
            {
                auto p = ct.add_parameters();
                skipwhite(first, last);
                p->set_name(to_lower(consume_token(first, last)));
                skipwhite(first, last);
                if (consume_char_if(first, last, '='))
                {
                    skipwhite(first, last);
                    p->set_value(consume_token_or_quoted(first, last));
                }
                else {
                    // no value on this parameter
                }
            }
            else if (consume_char_if(first, last, ','))
            {
                // normally we should get the next value-token here but in the case of Content-Type
                // we must barf
                throw std::runtime_error("invalid use of ; in Content-Type");
            }
        }
    }
    
    ContentType& populate(ContentType& ct, const std::string& header_value)
    {
        parse(ct, header_value.begin(), header_value.end());
        return ct;
    }
    
    ContentType& populate(ContentType& ct, const HttpRequestHeader& hdr)
    {
        if(auto p = find_only_header_like(hdr.headers(), "Content-Type"))
        {
            populate(ct, p->value());
        }
        return ct;
    }
    
    
    const Header* find_only_header_like(const HttpRequestHeader& request,
                                        const std::string& like)
    {
        return find_only_header_like(request.headers(), like);
    }
    
    const Header& require_only_header_like(const HttpRequestHeader& request,
                                   const std::string& like)
    {
        return require_only_header_like(request.headers(), like);
    }

    std::vector<std::reference_wrapper<const Header>>
    find_headers_like(const HttpRequestHeader& request,
                      const std::string& like)
    {
        return find_headers_like(request.headers(), like);
    }
    
    void set_status(HttpResponseHeader* rmsg, int code,
                    const std::string& message)
    {
        set_status(*rmsg, code, message);
    }
    
    void set_status(HttpResponseHeader& rmsg, int code,
                    const std::string& message)
    {
        auto status = rmsg.mutable_status();
        status->set_code(code);
        status->set_message(message);
    }
    
    void add_header(HttpResponseHeader* rmsg, const std::string& name,
                    const std::string& value)
    {
        add_header(*rmsg, name, value);
    }
    
    void add_header(HttpResponseHeader& rmsg, const std::string& name, 
                    const std::string& value)
    {
        auto hdr = rmsg.add_headers();
        hdr->set_name(name);
        hdr->set_value(value);
    }
    
    Header& set_header(HttpResponseHeader& response,
                       const std::string& name,
                       const std::string& value)
    {
        auto& headers = *response.mutable_headers();
        auto i = std::find_if(headers.begin(), headers.end(),
                              match_header_name(name));
        if (i == headers.end()) {
            auto pheader = response.add_headers();
            pheader->set_name(name);
            pheader->set_value(value);
            return *pheader;
        }

        i->set_value(value);
        auto first_rest = std::next(i);
        auto last_rest = std::end(headers);

        headers.erase(std::remove_if(first_rest, last_rest,
                                     match_header_name(name)),
                      last_rest);
        
        return *i;
    }
    


    
    struct add_representation_length
    {
        std::size_t operator()(std::size_t x, const Header& hdr) const
        {
            return x + hdr.name().length() + 2 + hdr.value().length() + 2;
        }
    };
    
    arena_byte_buffer to_response_buffer(const HttpResponseHeader& rmsg)
    {
        assert(rmsg.has_status());
        assert(not rmsg.status().message().empty());

        static const char colon[2] = { ':', ' ' };
        static const char crlf[2] = { '\r', '\n' };
        
        auto status_line = (boost::format("HTTP/%1%.%2% %3% %4%\r\n")
        % rmsg.version_major()
        % rmsg.version_minor()
        % rmsg.status().code()
        % rmsg.status().message()).str();
        
        auto response_length = status_line.size()
        + std::accumulate(std::begin(rmsg.headers()),
                          std::end(rmsg.headers()),
                          std::size_t(0),
                          add_representation_length())
        + 2;
        
        auto buffer = arena_byte_buffer(rmsg.GetArena(), response_length);
        auto i = buffer.begin();
        i = std::copy(status_line.begin(), status_line.end(), i);
        for (auto& header : rmsg.headers())
        {
            i = std::copy(header.name().begin(), header.name().end(), i);
            i = std::copy(std::begin(colon), std::end(colon), i);
            i = std::copy(header.value().begin(), header.value().end(), i);
            i = std::copy(std::begin(crlf), std::end(crlf), i);
        }
        i = std::copy(std::begin(crlf), std::end(crlf), i);
        assert(i == std::end(buffer));
        return buffer;
    }

    arena_byte_buffer to_response_buffer(const HttpResponseHeader* rmsg)
    {
        return to_response_buffer(*rmsg);
    }



    
    
}}}