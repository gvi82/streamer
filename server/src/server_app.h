#pragma once

#include "common/application.h"

#include <string>

namespace Server
{

struct ServerParams
{
    int port = 8080;
    unsigned max_clients = 10;
};

struct IServerApp : public virtual Common::IApplication
{
    virtual const ServerParams& GetParams() const  = 0;
};

DECLARE_PTR_S(IServerApp)

}
