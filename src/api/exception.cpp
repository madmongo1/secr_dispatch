#include <secr/dispatch/api/exception.hpp>
#include <valuelib/debug/demangle.hpp>

#include <regex>
#include <string>

#include <boost/variant.hpp>

namespace
{
    std::string remove_nested(std::string demangled)
    {
#if _LIBCPP_VERSION
        static const std::regex re("^std::__nested<(.*)>$");
#elif __GLIBCXX__
        static const std::regex re("^std::_Nested_exception<(.*)>$");
#endif
        std::smatch match;
        if (std::regex_match(demangled, match, re))
        {
            demangled = match[1].str();
        }
        return demangled;
    }
}

namespace secr { namespace dispatch { namespace api {
    
    Exception* populate(Exception* emsg, const char* text)
    {
        emsg->set_what(text);
        emsg->set_name("text");
        return emsg;
    }
    
    Exception* populate_unknown(Exception* emsg)
    {
        emsg->set_what("unknown error");
        emsg->set_name("unknown");
        return emsg;
    }
    
    Exception* populate(Exception* emsg, const std::exception& e)
    {
        emsg->set_what(e.what());
        emsg->set_name(remove_nested(value::debug::demangle(typeid(e))));
        try {
            std::rethrow_if_nested(e);
        }
        catch(std::exception& e)
        {
            populate(emsg->mutable_nested(), e);
        }
        catch(const char* text)
        {
            populate(emsg->mutable_nested(), text);
        }
        catch(...) {
            populate_unknown(emsg->mutable_nested());
        }
        return emsg;
    }
    
    Exception& populate(Exception& emsg, const std::exception& e)
    {
        return *populate(std::addressof(emsg), e);
    }
    
    
    Exception* populate(Exception* emsg, const std::exception_ptr& ep)
    {
        try {
            std::rethrow_exception(ep);
            emsg->set_name("none");
            emsg->set_what("no exception");
            emsg->clear_nested();
        }
        catch(const std::exception& e)
        {
            populate(emsg, e);
        }
        catch(const char* text)
        {
            populate(emsg, text);
        }
        catch(...)
        {
            populate_unknown(emsg);
        }
        return emsg;
    }
    
    Exception& populate(Exception& emsg, const std::exception_ptr& ep)
    {
        return *populate(std::addressof(emsg), ep);
    }
    
    ExceptionList& populate (ExceptionList& emsg, const std::vector<std::exception_ptr>& es)
    {
        return *populate(std::addressof(emsg), es);
    }
    
    ExceptionList* populate (ExceptionList* emsg, const std::vector<std::exception_ptr>& es)
    {
        for (auto& e : es) {
            populate(emsg->add_exceptions(), e);
        }
        return emsg;
    }

    
}}}

