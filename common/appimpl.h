#pragma once
#include "application.h"
#include "common.h"

namespace Common
{

class ApplicationBase : public virtual IApplication
{
    class Private;
    Private& d;
public:
    ApplicationBase(const std::string& logrfile);
    ~ApplicationBase();

    // start your services here
    virtual void AppRun() = 0;

    // stop your services here
    virtual void AppStop() = 0;

    virtual std::string GetServiceName() const = 0;

protected:
    void Run() override;
    boost::asio::io_service& GetIOService() override;
    std::thread::id GetThreadId() override;
    void Post(std::function<void()>) override;
    void BlockedCall(std::function<void()>) override;
    bool Unloading() override;
    void Unload() override;

    void SetSelfPtr(IApplicationPtr ptr);
};

template <typename App>
class ApplicationImpl : public ApplicationBase
        , public std::enable_shared_from_this<App>
        , ObjectCounter<App>
{
protected:
    ApplicationImpl(const std::string& logrfile)
        : ApplicationBase(logrfile)
    {
    }

    void Run() override
    {
        SetSelfPtr(this->shared_from_this());
        ApplicationBase::Run();
    }
};
template<typename App, typename... Args>
void RunApplication(Args...args)
{
    std::shared_ptr<IApplication> app = std::make_shared<App>(args...);
    app->Run();
    app.reset();
    dump_objects_count(1);
}
}
