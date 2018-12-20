#include "stream_svc.h"

#include "ports_pull.hpp"

#include <deque>
#include <set>
#include <fstream>

#include <boost/asio.hpp>

#include "common/common.h"
#include "common/messages.h"

#include "receiver.h"
#include "server_app.h"

using boost::asio::ip::tcp;

namespace Server
{

DECLARE_PTR(Session)

struct IStreamServiceInternal : public IStreamService
{
    virtual uint16_t PopPort() = 0;
    virtual void ReturnPort(uint16_t port) = 0;
    virtual uint16_t StopSession(const SessionPtr& session) = 0;
    virtual void Post(const std::function<void()>& f) = 0;
};

DECLARE_PTR_S(IStreamServiceInternal)

class Session : public std::enable_shared_from_this<Session>
        , public Common::ObjectCounter<Session>
        , public IReceiverCallback
{
    tcp::socket socket;
    static constexpr unsigned max_length = 4096;
    char data[max_length];
    IStreamServiceInternalPtr svc;

    uint16_t port1 = 0;
    uint16_t port2 = 0;

    std::vector<std::uint8_t> write_buffer;

    IReceiverPtr receiver;

    const int id;

    enum class States
    {
        WaitPortQuery,
        WaitSDP,
        WaitReseiverAnswer,
        ReseverStoped,
        ReseverProcessed,
    } state = States::WaitPortQuery;

public:
    Session(const IStreamServiceInternalPtr& svc, tcp::socket socket, int id) : socket(std::move(socket)), svc(svc), id(id)
    {
        LOG("Session CONSTRUCT " << this);
    }

    ~Session()
    {
        LOG("Session DESTROY " << this);
    }

    void Start()
    {
        DoRead();
    }

    void Stop()
    {
        LOG("Stop");

        if (receiver)
        {
            receiver->Uninitialize();
            receiver.reset();
        }

        socket.close();

        // return ports to pool
        svc->ReturnPort(port1);
        svc->ReturnPort(port2);

        svc->StopSession(shared_from_this());
    }

private:
    void DoRead()
    {
        if (state == States::ReseverStoped)
        {
            Stop();
            return;
        }

        if (state == States::ReseverProcessed)
            return;

        auto self(shared_from_this());
        socket.async_read_some(boost::asio::buffer(data, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (ec)
                {
                    LOG("DoRead got error " << ec);
                    Stop();
                    return;
                }

                LOG("Got message " << (int)data[0]);

                if (state == States::WaitPortQuery)
                {
                    ProcessPortReserve(length);
                    return;
                }

                if (state == States::WaitSDP)
                {
                    ProcessStartReceiving(length);
                    return;
                }
            });
    }

    void DoWrite()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket, boost::asio::buffer(write_buffer),
          [this, self](boost::system::error_code ec, std::size_t )
          {
              if (!ec)
              {
                  if (state == States::WaitPortQuery)
                  {
                      state = States::WaitSDP;
                      LOG("Switch state to WaitSDP");
                  }

                  DoRead();
              }
              else
              {
                  Stop();
              }
          });
    }

    void ProcessPortReserve(std::size_t length)
    {
        if (data[0] != Common::Messages::RESERVE_TWO_PORTS)
        {
            LOGW("Unexpected message type " << (int)data[0]);
            return;
        }

        port1 = svc->PopPort();
        port2 = svc->PopPort();

        if (!port1 || !port2)
        {
            LOGW("There are no free ports on server");
        }

        LOG("Reserver " << port1 << " " << port2);

        uint16_t nport1 = htons(port1);
        uint16_t nport2 = htons(port2);

        write_buffer.resize(4);

        memcpy(&write_buffer[0], &nport1, 2);
        memcpy(&write_buffer[2], &nport2, 2);

        DoWrite();
    }

    void ProcessStartReceiving(std::size_t length)
    {
        if (data[0] != Common::Messages::START_STREAM)
        {
            LOGW("Unexpected message type " << (int)data[0]);
            return;
        }

        std::string sdp(data+1, length - 1);

        LOGI("Got sdp " << sdp);

        std::ostringstream fns;
        fns << "sdp" << id << ".sdp";
        std::ofstream sdpf(fns.str());
        sdpf << sdp;
        sdpf.close();

        receiver = CreateReceiver(shared_from_this(), fns.str(), id);
        receiver->Initialize();

        state = States::WaitReseiverAnswer;
    }

    void OnReceiverStarted() override
    {
        svc->Post([this]()
        {
            ProcessReceiverStarted();
        });
    }

    void OnReceiverFailed() override
    {
        svc->Post([this]()
        {
            ProcessReceiverFailed();
        });
    }

    void ProcessReceiverStarted()
    {
        if (state == States::WaitReseiverAnswer)
        {
            write_buffer.resize(2);

            write_buffer[0] = 'O';
            write_buffer[1] = 'K';

            state = States::ReseverProcessed;
            DoWrite();
        }
        else
        {
            Stop();
        }

    }

    void ProcessReceiverFailed()
    {
        if (state == States::WaitReseiverAnswer)
        {
            write_buffer.resize(4);

            write_buffer[0] = 'F';
            write_buffer[1] = 'A';
            write_buffer[2] = 'I';
            write_buffer[3] = 'L';

            state = States::ReseverStoped;
            DoWrite();
        }
        else
        {
            Stop();
        }
    }

};


class StreamService : public IStreamServiceInternal
        , public std::enable_shared_from_this<StreamService>
        , public Common::ObjectCounter<StreamService>
{
    const IServerAppPtr app;

    tcp::acceptor acceptor;
    tcp::socket socket;

    PortsPool ports_pool;

    int session_ids = 100;

    std::set<SessionPtr> sessions;

public:
    StreamService(const IServerAppPtr& app)
        : app(app)
        , acceptor(app->GetIOService(), tcp::endpoint(tcp::v4(), app->GetParams().port))
        , socket(app->GetIOService())
        , ports_pool(app->GetParams().max_clients)
    {
    }

    void Initialize() override
    {
        DoAccept();
    }

    void Uninitialize() override
    {
        auto removing_sessions = sessions;
        for(auto session : removing_sessions)
            session->Stop();
        acceptor.close();
    }

    void DoAccept()
    {
        auto self(shared_from_this());
        acceptor.async_accept(socket,
            [this,self](boost::system::error_code ec)
            {
                if (!ec)
                {
                    auto session = std::make_shared<Session>(self, std::move(socket), ++session_ids);
                    sessions.insert(session);
                    session->Start();
                }
                else
                    return;

                DoAccept();
            });
    }

    uint16_t PopPort() override
    {
        return ports_pool.pop();
    }

    void ReturnPort(uint16_t port) override
    {
        if (port)
            ports_pool.return_port(port);
    }

    uint16_t StopSession(const SessionPtr& session) override
    {
        sessions.erase(session);
    }

    void Post(const std::function<void()>& f) override
    {
        app->Post(f);
    }

};

IStreamServicePtr CreateStreamService(const IServerAppPtr& app)
{
    return std::make_shared<StreamService>(app);
}

}
