#ifndef DAVIXCONTEXT_H
#define DAVIXCONTEXT_H

#include <string>
#include <davix_types.h>
#include <status/davixstatusrequest.hpp>
#include <davixuri.hpp>

///
/// @file davixcontext.hpp
///

namespace Davix{

class ContextInternal;
class HttpRequest;

/// @brief Main entry point for Davix
/// Each new davix context has its own session-reuse pool and set of parameters
/// Thread-safe
class Context
{
public:
    /// create a new context for Davix
    Context();
    Context(const Context & c);

    virtual ~Context();

    /// clone this instance to a new context dynamically allocated,
    /// the new context inherit of a copy of all the parent context parameters
    /// this context need to be destroyed after usage
    Context* clone();


    /// create a new Http request for direct HTTP low level feature usage
    /// this HTTP request object should be destroyed after usage
    HttpRequest* createRequest(const std::string & url, DavixError** err);
    HttpRequest* createRequest(const Uri & uri, DavixError** err);


private:
    // internal context
    ContextInternal* _intern;

    friend class DavPosix;

};

}

#endif // DAVIXCONTEXT_H