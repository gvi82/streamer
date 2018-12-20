#pragma once

#include "common/application.h"

#include <string>

namespace Client
{

struct ClientParams
{
    enum Modes
    {
        server_mode = 0,
        single_mode = 1,
        file_mode = 2,
    };

    std::string server_addr = "127.0.0.1";
    int server_port = 8080;
    std::string url;
    int mode = server_mode;

    void Dump();
};

struct IClientApp : public virtual Common::IApplication
{
    virtual const ClientParams& GetParams() const  = 0;
};

DECLARE_PTR_S(IClientApp)

}
