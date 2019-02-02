#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <vector>
#include "NetAPI.h"
#include "Messages.h"

class MTQueue
{
public:
    void PushMessage(std::string const& message)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(message);
    }

    std::string PopMessage()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_queue.empty())
        {
            std::string const result = m_queue.front();
            m_queue.erase(m_queue.begin());
            return result;
        }
        return "";
    }

private:
    std::vector<std::string> m_queue;

    std::mutex m_mutex;
};

MTQueue queue;


void server_main()
{
    NetObjectDescriptor descr{ 0,0 };
    auto netObj = NetObjectAPI::GetInstance()->CreateMasterNetObject(descr);
    netObj->RegisterMessageHandler<TextMessage>([&netObj](TextMessage const& message, NetAddr const& addr)
    {
        TextMessage chatMessage;
        std::stringstream ss;
        ss << "User" << addr.port() << ": " << message.text;
        chatMessage.text = ss.str();
        netObj->SendMasterBroadcastExcluding(chatMessage, addr);
        std::cout << ss.str() << std::endl;
    });
    netObj->SetOnReplicaAddedCallback([&netObj](NetAddr const& addr)
    {
        TextMessage welcomeMessage;
        welcomeMessage.text = "System: Welcome to the chat!";
        netObj->SendMasterUnicast(welcomeMessage, addr);
        TextMessage chatMessage;
        std::stringstream ss;
        ss << "System: User" << addr.port() << " has joined!";
        chatMessage.text = ss.str();
        netObj->SendMasterBroadcastExcluding(chatMessage, addr);
        std::cout << ss.str() << std::endl;
    });
    netObj->SetOnReplicaLeftCallback([&netObj](NetAddr const& addr)
    {
        TextMessage chatMessage;
        std::stringstream ss;
        ss << "System: User" << addr.port() << " has left!";
        chatMessage.text = ss.str();
        netObj->SendMasterBroadcastExcluding(chatMessage, addr);
        std::cout << ss.str() << std::endl;
    });
    while (1)
    {
        NetObjectAPI::GetInstance()->Update();
    }
}
void client_main()
{
    NetObjectDescriptor descr{ 0,0 };
    auto netObj = NetObjectAPI::GetInstance()->CreateReplicaNetObject(descr);
    netObj->RegisterMessageHandler<TextMessage>([](TextMessage const& message, NetAddr const& addr)
    {
        std::cout << message.text << std::endl;
    });
    while (1)
    {
        std::string input = queue.PopMessage();
        if (!input.empty())
        {
            if (input.find("Bye", 0) == 0)
            {
                return;
            }
            TextMessage message;
            message.text = input;
            netObj->SendReplicaMessage(message);
        }
        NetObjectAPI::GetInstance()->Update();
    }
}

int main()
{
    std::string input;
    std::cout << "Host?" << std::endl;
    std::getline(std::cin, input);
    bool isHost = (input.find("y", 0) == 0 || input.find("Y", 0) == 0);
    std::cout << "Server?" << std::endl;
    std::getline(std::cin, input);
    bool isServer = (input.find("y", 0) == 0 || input.find("Y", 0) == 0);
    NetObjectAPI::Init(isHost);
    if (isServer)
    {
        server_main();
    }
    else
    {
        std::thread client_thread(client_main);
        while (1)
        {
            std::getline(std::cin, input);
            queue.PushMessage(input);
        }
        client_thread.join();
    }

    NetObjectAPI::Shutdown();

    return 0;
}
