#pragma once

#include "object.h"
#include "ptr.h"
#include <thread>
#include <functional>

namespace boost { namespace asio {
    class io_context;
    typedef io_context io_service;
}}

namespace Common {

struct IApplication : public IObject
{
    virtual void Run() = 0;
    virtual boost::asio::io_service& GetIOService() = 0;
    virtual std::thread::id GetThreadId() = 0;
    virtual void Post(std::function<void()>) = 0;
    virtual void BlockedCall(std::function<void()>) = 0;
    virtual bool Unloading() = 0;
    virtual void Unload() = 0;
};

DECLARE_PTR_S(IApplication)

}

