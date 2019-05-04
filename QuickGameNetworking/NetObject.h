#pragma once
#include "NetMessagesBase.h"
#include "NetSocket.h"
#include <optional>
#include <functional>


struct NetObjectMasterData
{
    std::function<void(NetAddr const&)> m_replicaAddedCallback;
    std::function<void(NetAddr const&)> m_replicaLeftCallback;
};

class NetObject
{
public:
    using MessageHandler = std::function<void(INetMessage const&, NetAddr const&)>;

    NetObject(bool const isMaster, NetObjectDescriptor const& descriptor);
    ~NetObject();
    NetObject(NetObject const& other) = delete;

    bool IsMaster() const { return m_masterData.get() != nullptr; }

    void Update();

    void SendMasterBroadcast(NetObjectMessageBase& message);
    void SendMasterBroadcastExcluding(NetObjectMessageBase& message, NetAddr const& addr);
    void SendMasterUnicast(NetObjectMessageBase& message, NetAddr const& addr);
    void SendReplicaMessage(NetObjectMessageBase& message);
    void SendToAuthority(NetObjectMessageBase& message);

    void ReceiveMessage(INetMessage const& message, NetAddr const& sender);

    template<typename T> void RegisterMessageHandler(std::function<void(T const&, NetAddr const&)> handler);

    void SetOnReplicaAddedCallback(std::function<void(NetAddr const&)> const& callback);
    void SetOnReplicaLeftCallback(std::function<void(NetAddr const&)> const& callback);

    void OnReplicaAdded(NetAddr const& addr);
    void OnReplicaLeft(NetAddr const& addr);

private:
    template<typename ReceiversT>
    void SendMessageHelper(NetObjectMessageBase& message, ReceiversT const& receivers);
    void SendMessage(NetObjectMessageBase const& message, NetAddr const& addr);

    void InitMasterDiscovery();
    void SendDiscoveryMessage();

private:
    std::unique_ptr<NetObjectMasterData> m_masterData;

    std::optional<NetAddr> m_masterAddr;
    std::unordered_map <size_t, MessageHandler> m_handlers;

    NetObjectDescriptor m_descriptor;
};

template<typename T>
void NetObject::RegisterMessageHandler(std::function<void(T const&, NetAddr const&)> handler)
{
    static_assert(std::is_base_of<NetObjectMessageBase, T>::value, "Can be called only for classes derived from NetMessage");
    m_handlers[T::TypeID] = [handler](INetMessage const& message, NetAddr const& addr)
    {
        assert(dynamic_cast<T const*>(&message));
        handler(static_cast<T const&>(message), addr);
    };
}

template<typename ReceiversT>
void NetObject::SendMessageHelper(NetObjectMessageBase& message, ReceiversT const& receivers)
{
    message.SetDescriptor(m_descriptor);
    for (auto const& addr : receivers)
    {
        SendMessage(message, addr);
    }
}

template<>
void NetObject::SendMessageHelper<NetAddr>(NetObjectMessageBase& message, NetAddr const& addr);
