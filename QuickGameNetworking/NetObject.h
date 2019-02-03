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
    using MessageHandler = std::function<void(NetMessage const&, NetAddr const&)>;

    NetObject(bool const isMaster, NetObjectDescriptor const& descriptor);
    ~NetObject();
    NetObject(NetObject const& other) = delete;

    bool IsMaster() const { return m_masterData.get() != nullptr; }

    void Update();

    void SendMasterBroadcast(NetObjectMessage& message);
    void SendMasterBroadcastExcluding(NetObjectMessage& message, NetAddr const& addr);
    void SendMasterUnicast(NetObjectMessage& message, NetAddr const& addr);
    void SendReplicaMessage(NetObjectMessage& message);
    void SendToAuthority(NetObjectMessage& message);

    void ReceiveMessage(NetMessage const& message, NetAddr const& sender);

    template<typename T> void RegisterMessageHandler(std::function<void(T const&, NetAddr const&)> handler);

    void SetOnReplicaAddedCallback(std::function<void(NetAddr const&)> const& callback);
    void SetOnReplicaLeftCallback(std::function<void(NetAddr const&)> const& callback);

    void OnReplicaAdded(NetAddr const& addr);
    void OnReplicaLeft(NetAddr const& addr);

private:
    template<typename ReceiversT>
    void SendMessage(NetObjectMessage& message, ReceiversT const& receivers);

    void InitMasterDiscovery();
    void SendDiscoveryMessage();

private:
    std::unique_ptr<NetObjectMasterData> m_masterData;

    std::optional<NetAddr> m_masterAddr;
    std::unordered_map <size_t, MessageHandler> m_handlers;

    NetObjectDescriptor m_descriptor;
};
