#pragma once

#include <deque>

namespace Server
{
class PortsPool
{
    static constexpr uint16_t start_port = 35000;
    std::deque<uint16_t> free_ports;
public:
    PortsPool(unsigned max_clients)
    {
        for (unsigned i = 0; i < max_clients; ++i)
        {
            free_ports.push_back(start_port + i*4);
            free_ports.push_back(free_ports.back()+2);
        }
    }

    uint16_t pop()
    {
        if (free_ports.empty())
            return 0;
        int port = free_ports.front();
        free_ports.pop_front();
        return port;
    }

    void return_port(int port)
    {
        free_ports.push_front(port);
    }
};
}

