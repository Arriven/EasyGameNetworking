#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <memory>
#ifdef _MSC_VER
#include <xtr1common>
#endif
#include "NetSocket.h"
#include "NetMessagesBase.h"
#include "NetObject.h"

class NetObjectAPI
{
public:
    static void Init(NetAddr const& hostAddress, bool const isHost);
    static void Shutdown();
    static NetObjectAPI* GetInstance() { return ms_instance.get(); }

    bool IsHost() const { return m_isHost; }
    void Update();

    std::unique_ptr<NetObject> CreateMasterNetObject(NetObjectDescriptor const& descriptor);
    std::unique_ptr<NetObject> CreateReplicaNetObject(NetObjectDescriptor const& descriptor);
    std::unique_ptr<NetObject> CreateThirdPartyNetObject(NetObjectDescriptor const& descriptor);

    void SendMessage(INetMessage const& message, NetAddr const& recipient, ESendOptions const options = ESendOptions::None);

    NetAddr GetHostAddress() const;
    NetAddr GetLocalAddress() const;
    std::vector<NetAddr> GetConnections() const;

    void RegisterNetObject(NetObjectDescriptor const& descriptor, NetObject* object);
    void UnregisterNetObject(NetObjectDescriptor const& descriptor);

    template<typename T> void RegisterMessageHandler(std::function<void(T const&, NetAddr const&)> handler);
    template<typename T> void UnregisterMessageHandler();

private:
    NetObjectAPI(NetAddr const& hostAddress, bool const isHost);
    NetObjectAPI(NetObjectAPI const& other) = delete;

    void ProcessMessages();
    bool ReceiveMessage();
    void HandleMessage(INetMessage const* message, NetAddr const& sender);

private:
    using NetObjectMap = std::unordered_map <NetObjectDescriptor, NetObject*>;
    NetObjectMap m_netObjects;
    std::unordered_map <size_t, std::function<std::unique_ptr<INetMessage>()>> m_messageFactory;
    std::optional<NetSocket> m_socket;
    NetAddr m_hostAddress;
    bool const m_isHost;

    using MessageHandler = std::function<void(INetMessage const&, NetAddr const&)>;
    std::unordered_map <size_t, MessageHandler> m_handlers;

    static std::unique_ptr<NetObjectAPI> ms_instance;
};

template<typename T>
void NetObjectAPI::RegisterMessageHandler(std::function<void(T const&, NetAddr const&)> handler)
{
    static_assert(std::is_base_of<INetMessage, T>::value, "Can be called only for classes derived from NetMessage");
    assert(NetMessageFactory::GetInstance()->IsMessageRegistered<T>());
    m_handlers[T::MessageID] = [handler](INetMessage const& message, NetAddr const& addr)
    {
        assert(dynamic_cast<T const*>(&message));
        handler(static_cast<T const&>(message), addr);
    };
}

template<typename T>
void NetObjectAPI::UnregisterMessageHandler()
{
    m_handlers.erase(T::MessageID);
}

