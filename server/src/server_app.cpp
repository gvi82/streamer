#include "server_app.h"

#include "common/appimpl.h"

#include "stream_svc.h"

namespace Server
{

class ServerApplication
        : public Common::ApplicationImpl<ServerApplication>
        , public IServerApp
{
    ServerParams params;
    IStreamServicePtr stream_svc;
public:
    ServerApplication(const ServerParams& params)
        : Common::ApplicationImpl<ServerApplication>("server")
        , params(params)
    {
    }

    const ServerParams& GetParams() const  override
    {
        return params;
    }

    void AppRun() override
    {
        stream_svc = CreateStreamService(shared_from_this());
        stream_svc->Initialize();
    }

    void AppStop() override
    {
        if (stream_svc)
        {
            stream_svc->Uninitialize();
            stream_svc.reset();
        }
    }

    std::string GetServiceName() const override
    {
        return "server";
    }

};

}

int RunServerApplication(int argc, char* argv[])
{
    Server::ServerParams params;
    Common::RunApplication<Server::ServerApplication>(params);
    return 0;
}
