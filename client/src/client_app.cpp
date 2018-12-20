#include "client_app.h"

#include <iostream>

#include <boost/program_options.hpp>

#include "common/appimpl.h"

#include "sender.h"

namespace Client
{

void ClientParams::Dump()
{
    LOG("Params: " << "server_addr: "<< server_addr << std::endl
    << "server_port: "<< server_port << std::endl
    << "file: "<< url << std::endl);
}

class ClientApplication
        : public Common::ApplicationImpl<ClientApplication>
        , public IClientApp
        , public ISenderEvents
{
    ClientParams params;
    ISenderPtr sender;
public:
    ClientApplication(const ClientParams& params)
        : Common::ApplicationImpl<ClientApplication>("client")
        , params(params)
    {
    }

    const ClientParams& GetParams() const  override
    {
        return params;
    }

    void AppRun() override
    {
        params.Dump();

        sender = CreateSender(params, shared_from_this());

        sender->Initialize();
    }

    void AppStop() override
    {
        if (sender)
        {
            sender->Uninitialize();
            sender.reset();
        }
    }

    std::string GetServiceName() const override
    {
        return "client";
    }

    void OnSenderStopped(ISenderPtr sender) override
    {
        Unload();
    }

};

}

int RunClientApplication(int argc, char* argv[])
{
    Client::ClientParams params;

    params.url = "test_video.mp4";

    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i],"-mode"))
        {
            if (i+1==argc)
            {
                std::cerr << "unknown mode" << std::endl;
                return 1;
            }
            else
            {
                params.mode = atoi(argv[++i]);
            }
        }
        else if (!strcmp(argv[i],"-server"))
        {
            if (i+1==argc)
            {
                std::cerr << "unknown server" << std::endl;
                return 1;
            }
            else
            {
                params.server_addr = (argv[++i]);
            }
        }
        else if (!strcmp(argv[i],"-port"))
        {
            if (i+1==argc)
            {
                std::cerr << "unknown port" << std::endl;
                return 1;
            }
            else
            {
                params.server_port = atoi(argv[++i]);
            }
        }
        else
        {
            params.url = argv[i];
        }
    }


    Common::RunApplication<Client::ClientApplication>(params);
    return 0;
}
