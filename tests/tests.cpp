// tests.cpp

#include <gtest/gtest.h>

#include <mutex>

#include "common/common.h"

#include "client/src/sender.h"
#include "client/src/client_app.h"

#include "server/src/ports_pull.hpp"

TEST(ServerTest, PortsPool)
{
    Server::PortsPool pool(10);

    int port1 = pool.pop();
    int port2 = pool.pop();

    ASSERT_EQ(port1, 35000);
    ASSERT_EQ(port2, 35002);
}

using namespace Client;

class SenderHandler : public ISenderEvents
{
public:
    std::set<ISenderPtr> senders;
    std::set<ISenderPtr> senders_for_remove;
    std::mutex mx;

    SenderHandler()
    {}

    void OnSenderStopped(ISenderPtr sender)
    {
        std::lock_guard<std::mutex> lock(mx);
        senders_for_remove.insert(sender);
    }

    bool CanUnload()
    {
        bool can_unload = false;
        {
            std::lock_guard<std::mutex> lock(mx);
            for (auto& rm_sender : senders_for_remove)
                senders.erase(rm_sender);
            can_unload = senders.empty();
        }
        return can_unload;
    }
};

TEST(ClientServerTest, FirstClient)
{
    ClientParams params;
    params.url = "test_video.mp4";

    auto handler = std::make_shared<SenderHandler>();

    for (size_t i = 0; i < 5; ++i)
    {
        auto client = CreateSender(params, handler);
        handler->senders.insert(client);
        client->Initialize();
    }

    while(!handler->CanUnload())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    Common::initialize_log("test_client");
    return RUN_ALL_TESTS();
}
