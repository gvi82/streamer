#include "appimpl.h"
#include "common.h"
#include <boost/asio.hpp>
#include <boost/stacktrace.hpp>

#include <iostream>

#include <execinfo.h>

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

namespace
{
void handler(int sig) {
  boost::stacktrace::stacktrace st;
  std::ostringstream sstr;
  sstr << "Got signal " << sig << std::endl << st;

  std::cout << sstr.str();

  LOGE(sstr.str());

  exit(1);
}

std::weak_ptr<Common::IApplication> gApp;

void int_handler(int sig) {
    auto app = gApp.lock();
    if (app)
    {
        if (app->Unloading())
        {
            LOGW("Force stop by SIGINT");
            exit(1);
        }
        else
            app->Unload();
    }
}

}

namespace Common
{

class ApplicationBase::Private
{
public:
    boost::asio::io_service ioService;
    std::thread::id threadId;
    boost::asio::deadline_timer timer;
    int sec_to_unload = -1;
    ApplicationBase& q;
    std::string logfile;

    Private(ApplicationBase& qIn, const std::string& logrfileIn)
        : timer(ioService), q(qIn), logfile(logrfileIn)
    {}

    void Run()
    {
        if (logfile.empty())
            logfile = q.GetServiceName();

        Common::initialize_log(logfile);
        Common::register_current_thread(q.GetServiceName());

        signal(SIGSEGV, handler);
        signal(SIGABRT, handler);
        signal(SIGINT, int_handler);

        LOGI("***** " << q.GetServiceName() << " rev:" << APP_VERSION << " *****");

        threadId = std::this_thread::get_id();

        TRY
        {
            q.AppRun();
            DumpObjectCount();
            LOG("Main service runing");
            ioService.run();
        }
        CATCH_ERR("Application run error: ");
    }

    void DumpObjectCount()
    {
        timer.expires_from_now(boost::posix_time::seconds(300));
        timer.async_wait([this](const boost::system::error_code& error){
            if (!error)
                DumpObjectCount();
        });

        Common::dump_objects_count();
    }

    void Post(std::function<void()> f)
    {
        ioService.post(f);
    }

    void BlockedCall(std::function<void()> f)
    {
        std::mutex m;
        std::condition_variable cond_var;

        bool notified = false;
        std::unique_lock<std::mutex> lock(m);

        ioService.post([&notified, &f,&m,&cond_var](){
            f();
            std::unique_lock<std::mutex> lock(m);
            notified = true;
            cond_var.notify_one();
        });

        while (!notified) {  // loop to avoid spurious wakeups
            cond_var.wait(lock);
        }
    }

    bool Unloading()
    {
        return sec_to_unload >= 0;
    }

    void Stop()
    {
        if (Unloading())
            return;

        LOG("Unload app");
        ioService.post([this](){
            ProcessStop();
        });
    }

    void ProcessStop()
    {
        if (gApp.use_count() == 1 || !sec_to_unload)
        {
            timer.cancel();

            if (sec_to_unload)
                LOGI("Application successfully stopped");
            else
            {
                LOGW("Force stopped");
                ioService.stop();
            }

            return;
        }

        if (sec_to_unload < 0)
        {
            sec_to_unload = 10;
            LOG("Stop initiate");

            q.AppStop();

            timer.cancel();
        }

        if (sec_to_unload)
        {
             LOG("Procees unloading..." << sec_to_unload << " use_count " << gApp.use_count());
             dump_objects_count(1);

            --sec_to_unload;
            timer.expires_from_now(boost::posix_time::seconds(1));
            timer.async_wait([this](const boost::system::error_code& error){
                if (error)
                    LOGW("timer error " << error);
                if (!error)
                    ProcessStop();
            });
        }

    }
};

ApplicationBase::ApplicationBase(const std::string& logrfile)
    :d(*new Private(*this, logrfile))
{

}

ApplicationBase::~ApplicationBase()
{
    delete &d;
}

void ApplicationBase::Run()
{
    d.Run();
}

boost::asio::io_service& ApplicationBase::GetIOService()
{
    return d.ioService;
}

std::thread::id ApplicationBase::GetThreadId()
{
    return d.threadId;
}

void ApplicationBase::Post(std::function<void()> f)
{
    d.Post(f);
}

void ApplicationBase::BlockedCall(std::function<void()> f)
{
    d.BlockedCall(f);
}

bool ApplicationBase::Unloading()
{
    return d.Unloading();
}

void ApplicationBase::Unload()
{
    d.Stop();
}

void ApplicationBase::SetSelfPtr(IApplicationPtr ptr)
{
    gApp = ptr;
}

}
