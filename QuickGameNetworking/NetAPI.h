#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <memory>
#include <xtr1common>
#include "NetSocket.h"
#include "NetMessagesBase.h"
#include "NetObject.h"




class NetObjectAPI
{
public:
    static void Init(bool const isHost);
    static void Shutdown();
    static NetObjectAPI* GetInstance() { return ms_instance.get(); }

    bool IsHost() const { return m_isHost; }
    void Update();

    std::unique_ptr<NetObject> CreateMasterNetObject(NetObjectDescriptor const& descriptor);
    std::unique_ptr<NetObject> CreateReplicaNetObject(NetObjectDescriptor const& descriptor);
    std::unique_ptr<NetObject> CreateThirdPartyNetObject(NetObjectDescriptor const& descriptor);

    template<typename T> void RegisterMessage();
    template<typename T> bool IsMessageRegistered();

    void SendMessage(NetMessage const& message, NetAddr const& recipient);

    NetAddr GetHostAddress() const;
    NetAddr GetLocalAddress() const;
    std::vector<NetAddr> GetConnections() const;

    void RegisterNetObject(NetObjectDescriptor const& descriptor, NetObject* object);
    void UnregisterNetObject(NetObjectDescriptor const& descriptor);

private:
    NetObjectAPI(bool const isHost);
    NetObjectAPI(NetObjectAPI const& other) = delete;

    void InitMessageFactory();

    void ProcessMessages();
    bool ReceiveMessage();
    void HandleMessage(NetMessage const* message, NetAddr const& sender);

private:
    using NetObjectMap = std::unordered_map <NetObjectDescriptor, NetObject*>;
    NetObjectMap m_netObjects;
    std::unordered_map <size_t, std::function<std::unique_ptr<NetMessage>()>> m_messageFactory;
    std::optional<NetSocket> m_socket;
    bool const m_isHost;

    static std::unique_ptr<NetObjectAPI> ms_instance;
};

template<typename T>
void NetObject::RegisterMessageHandler(std::function<void(T const&, NetAddr const&)> handler)
{
    static_assert(std::is_base_of<NetMessage, T>::value, "Can be called only for classes derived from NetMessage");
    assert(NetObjectAPI::GetInstance()->IsMessageRegistered<T>());
    m_handlers[T().GetMessageID()] = [handler](NetMessage const& message, NetAddr const& addr)
    {
        assert(dynamic_cast<T const*>(&message));
        handler(static_cast<T const&>(message), addr);
    };
}

template<typename ReceiversT>
void NetObject::SendMessage(NetObjectMessage& message, ReceiversT const& receivers)
{
    message.SetDescriptor(std::make_unique<NetObjectDescriptor>(m_descriptor));
    if constexpr (std::is_same_v<ReceiversT, NetAddr>)
    {
        NetObjectAPI::GetInstance()->SendMessage(message, receivers);
    }
    else
    {
        for (auto const& addr : receivers)
        {
            NetObjectAPI::GetInstance()->SendMessage(message, addr);
        }
    }
}

template<typename T>
void NetObjectAPI::RegisterMessage()
{
    static_assert(std::is_base_of<NetMessage, T>::value, "Can be called only for classes derived from NetMessage");
    m_messageFactory[T().GetMessageID()] = []() { return std::make_unique<T>(); };
}

template<typename T>
bool NetObjectAPI::IsMessageRegistered()
{
    static_assert(std::is_base_of<NetMessage, T>::value, "Can be called only for classes derived from NetMessage");

    return m_messageFactory.find(T().GetMessageID()) != m_messageFactory.end();
}


