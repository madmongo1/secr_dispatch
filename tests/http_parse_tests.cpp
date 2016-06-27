#define BOOST_SPIRIT_DEBUG
#include <boost/config/warning_disable.hpp>
#include <gtest/gtest.h>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/fusion/include/adapted.hpp>

#include <utility>
#include <vector>
#include <string>
#include <iostream>


using token_pair = std::pair<std::string, std::string>;

struct parameter {
    std::string name;
    std::string value;
    bool has_value;
};

struct media_type {
    token_pair type_subtype;
    std::vector<parameter> params;
};


BOOST_FUSION_ADAPT_STRUCT(parameter, name, value, has_value)
BOOST_FUSION_ADAPT_STRUCT(media_type, type_subtype, params)

namespace qi = boost::spirit::qi;
namespace phoenix = boost::phoenix;
using namespace std::literals;

template<class Iterator>
struct components
{
    
    components()
    {
        using qi::ascii::char_;
        spaces        = char_(" \t");
        token         = +~char_( "()<>@,;:\\\"/[]?={} \t");
        token_pair_rule = token >> '/' >> token;
        token_pair_only = token_pair_rule >> qi::skip(spaces)[qi::eoi];
        quoted_string = '"' >> *('\\' >> char_ | ~char_('"')) >> '"';
        value         = quoted_string | token;
        
        name_only         = token >> qi::attr("") >> qi::attr(false);
        nvp               = token >> '=' >> value >> qi::attr(true);
        any_parameter     = ';' >> (nvp | name_only);
        some_parameters   = +any_parameter;
        parameters        = *any_parameter;
        
        qi::on_error<qi::fail>(
                               token,
                               this->report_error(qi::_1, qi::_2, qi::_3, qi::_4)
                               );
        
        BOOST_SPIRIT_DEBUG_NODES((token)
                                 (quoted_string)
                                 (value)
                                 (name_only)
                                 (nvp)
                                 (any_parameter)
                                 (parameters)
                                 (token_pair_rule)
                                 (token_pair_only)
                                 )
    }
    
protected:
    using Skipper = qi::space_type;
    Skipper spaces;
    qi::rule<Iterator, std::string()>        quoted_string, token, value;
    qi::rule<Iterator, parameter(), Skipper> nvp, name_only, any_parameter;
    qi::rule<Iterator, std::vector<parameter>(), Skipper> parameters, some_parameters;
    qi::rule<Iterator, token_pair()>        token_pair_rule, token_pair_only;
    
public:
    std::string error_message;
    
protected:
    struct ReportError {
        // the result type must be explicit for Phoenix
        template<typename, typename, typename, typename>
        struct result { typedef void type; };
        
        ReportError(std::string& error_message)
        : error_message(error_message) {}
        
        // contract the string to the surrounding new-line characters
        template<typename Iter>
        void operator()(Iter first, Iter last,
                        Iter error, const qi::info& what) const
        {
            using namespace std::string_literals;
            std::ostringstream ss;
            ss << "Error! Expecting "
            << what
            << " in header value: " << std::quoted(std::string(first, last))
            << " at position: " << error - first;
            error_message = ss.str();
        }
        std::string& error_message;
    };
    
    const phoenix::function<ReportError> report_error = ReportError(error_message);
};

template<class Iterator>
struct token_grammar
: components<Iterator>
, qi::grammar<Iterator, media_type()>
{
    
    token_grammar() : token_grammar::base_type(media_type_rule)
    {
        
        media_type_with_parameters = qi::eps > token_pair_rule >> qi::skip(spaces)[some_parameters];
        media_type_no_parameters = qi::eps > token_pair_rule >> qi::attr(std::vector<parameter>()) >> qi::skip(spaces)[qi::eoi];
        media_type_rule = qi::eps > (qi::hold[media_type_no_parameters]
                                     | qi::hold[media_type_with_parameters]);
        
        BOOST_SPIRIT_DEBUG_NODES((media_type_with_parameters)
                                 (media_type_no_parameters)
                                 (media_type_rule))
        
        qi::on_error<qi::fail>(
                               media_type_rule,
                               this->media_type_error(phoenix::construct<failed_media_type>(),
                                                      qi::_1, qi::_2, qi::_3, qi::_4)
                               );
        
        qi::on_error<qi::fail>(
        media_type_no_parameters,
                               this->media_type_error(phoenix::construct<failed_media_type_no_parameters>(),
                                                  qi::_1, qi::_2, qi::_3, qi::_4)
        );
        qi::on_error<qi::fail>(
                               media_type_with_parameters,
                               this->media_type_error(phoenix::construct<failed_media_type_with_parameters>(),
                                                  qi::_1, qi::_2, qi::_3, qi::_4)
                               );
        
    }

    struct failed_media_type_with_parameters {};
    struct failed_media_type_no_parameters {};
    struct failed_media_type {};
    struct MediaTypeError {
        // the result type must be explicit for Phoenix
        template<typename, typename, typename, typename, typename>
        struct result { typedef void type; };
        
        // contract the string to the surrounding new-line characters
        template<typename Iter>
        void operator()(failed_media_type_with_parameters,
                        Iter first, Iter last,
                        Iter error, const qi::info& what) const
        {
            using namespace std::string_literals;
            std::ostringstream ss;
            ss << "Error! Expecting "
            << what
            << " in header value: " << std::quoted(std::string(first, last))
            << " at position: " << error - first;
            std::cerr << std::endl << ss.str() << std::endl;
            //            error_message = ss.str();
        }
        template<typename Iter>
        void operator()(failed_media_type_no_parameters,
                        Iter first, Iter last,
                        Iter error, const qi::info& what) const
        {
            using namespace std::string_literals;
            std::ostringstream ss;
            ss << "Error! Expecting "
            << what
            << " in header value: " << std::quoted(std::string(first, last))
            << " at position: " << error - first;
            //            error_message = ss.str();
            std::cerr << std::endl << ss.str() << std::endl;
        }

        template<typename Iter>
        void operator()(failed_media_type,
                        Iter first, Iter last,
                        Iter error, const qi::info& what) const
        {
            using namespace std::string_literals;
            std::ostringstream ss;
            ss << "Error! Expecting "
            << what
            << " in header value: " << std::quoted(std::string(first, last))
            << " at position: " << error - first;
            //            error_message = ss.str();
            std::cerr << std::endl << ss.str() << std::endl;
        }
    };
    
    const phoenix::function<MediaTypeError> media_type_error = MediaTypeError();

private:
    using Skipper = typename token_grammar::components::Skipper;
    using token_grammar::components::spaces;
    using token_grammar::components::token;
    using token_grammar::components::token_pair_rule;
    using token_grammar::components::value;
    using token_grammar::components::any_parameter;
    using token_grammar::components::parameters;
    using token_grammar::components::some_parameters;
    
public:
    qi::rule<Iterator, media_type()>        media_type_no_parameters, media_type_with_parameters, media_type_rule;
};



TEST(spirit_test, test1)
{
    token_grammar<std::string::const_iterator> grammar{};
    
    auto test = R"__test(application/json )__test"s;
    auto ct = media_type {};
    bool r = parse(test.cbegin(), test.cend(), grammar, ct);
    EXPECT_EQ("application", ct.type_subtype.first);
    EXPECT_EQ("json", ct.type_subtype.second);
    EXPECT_EQ(0, ct.params.size());
    
    ct = {};
    test = R"__test(text/html ; charset = "ISO-8859-5")__test"s;
    parse(test.cbegin(), test.cend(), grammar, ct);
    EXPECT_EQ("text", ct.type_subtype.first);
    EXPECT_EQ("html", ct.type_subtype.second);
    ASSERT_EQ(1, ct.params.size());
    EXPECT_TRUE(ct.params[0].has_value);
    EXPECT_EQ("charset", ct.params[0].name);
    EXPECT_EQ("ISO-8859-5", ct.params[0].value);
    
    auto mt = media_type {};
    parse(test.cbegin(), test.cend(), grammar.media_type_rule, mt);
    EXPECT_EQ("text", mt.type_subtype.first);
    EXPECT_EQ("html", mt.type_subtype.second);
    EXPECT_EQ(1, mt.params.size());
    
    //
    // Introduce a failure case
    //

    mt = media_type {};
    test = R"__test(text/html garbage ; charset = "ISO-8859-5")__test"s;
    r = parse(test.cbegin(), test.cend(), grammar.media_type_rule, mt);
    EXPECT_FALSE(r);
    EXPECT_EQ("", grammar.error_message);
}


#include <valuelib/debug/demangle.hpp>
#include "test_utils.hpp"
#include <secr/dispatch/http/request_header.hpp>
#include <secr/dispatch/api/json.hpp>

TEST(http_header_analise, test1)
{
    secr::dispatch::http::HttpRequestHeader rh;
    rh.set_method("GET");
    rh.set_uri("/moo");
    rh.mutable_query()->set_path("/moo");
    rh.set_version_major(1);
    rh.set_version_minor(1);
    
    set_unique_header(rh, "Accept", "text/plain; q=0.5, text/html, text/x-dvi; q=0.8, text/x-c");
    
    //    analyse(rh);
    
    //    ASSERT_EQ(4, rh.meta_data().accept().media_ranges().size());
    
    
    /*
     text/html;level=1         = 1
     text/html                 = 0.7
     text/plain                = 0.3
     image/jpeg                = 0.5
     text/html;level=2         = 0.4
     text/html;level=3         = 0.7
     */
    
    
}

TEST(http_parse_tests, content_type)
{
    secr::dispatch::http::ContentType ct;
    EXPECT_TRUE(no_exception([&]{secr::dispatch::http::populate(ct, "text/html; charset=ISO-8859-4");}));
    auto json = secr::dispatch::api::as_json(ct);
    EXPECT_EQ("{\n \"type\": \"text\",\n \"subtype\": \"html\",\n \"parameters\": [\n  {\n   \"name\": \"charset\",\n   \"value\": \"ISO-8859-4\"\n  }\n ]\n}\n", json);
    
    
    ct.Clear();
    EXPECT_TRUE(no_exception([&]{secr::dispatch::http::populate(ct, "text/html; charset= \"ISO-8859-4\"");}));
    json = secr::dispatch::api::as_json(ct);
    EXPECT_EQ("{\n \"type\": \"text\",\n \"subtype\": \"html\",\n \"parameters\": [\n  {\n   \"name\": \"charset\",\n   \"value\": \"ISO-8859-4\"\n  }\n ]\n}\n", json);
    
    ct.Clear();
    EXPECT_TRUE(no_exception([&]{secr::dispatch::http::populate(ct, "teXt/hTml  ;  cHarset  = \"ISO-8859-4\"   ");}));
    json = secr::dispatch::api::as_json(ct);
    EXPECT_EQ("{\n \"type\": \"text\",\n \"subtype\": \"html\",\n \"parameters\": [\n  {\n   \"name\": \"charset\",\n   \"value\": \"ISO-8859-4\"\n  }\n ]\n}\n", json);
    
}


