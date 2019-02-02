#pragma once

#include <boost/asio.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <chrono>
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>
#include "NetSocket.h"


//TODO: replace with some more advanced hash algorithm like crc32
size_t constexpr HashMessageID(const char* p)
{
    size_t constexpr prime = 31;
    if (*p)
    {
        return (*p) * prime + HashMessageID(++p);
    }
    return 0;
}

class NetMessage
{
public:
    virtual size_t GetMessageID() const = 0;
    virtual void Serialize(boost::archive::binary_oarchive& stream) const = 0;
    virtual void Deserialize(boost::archive::binary_iarchive& stream) = 0;
};

#define DEFINE_NET_MESSAGE(NetMessageType) \
public: \
    virtual size_t GetMessageID() const override { return HashMessageID(#NetMessageType); } \
    virtual void Serialize(boost::archive::binary_oarchive& stream) const override { stream << *this; } \
    virtual void Deserialize(boost::archive::binary_iarchive& stream) override { stream >> *this; } \

using NetAddr = boost::asio::ip::udp::endpoint;

struct ReplicaData
{
    NetAddr m_addr;
    std::chrono::system_clock::time_point m_lastHeartbeat;
};

struct NetObjectMasterData
{
    std::vector<ReplicaData> m_replicas;

    std::function<void(NetAddr const&)> m_replicaAddedCallback;
    std::function<void(NetAddr const&)> m_replicaLeftCallback;
};

struct NetObjectDescriptor
{
    unsigned char m_typeID;
    unsigned short m_instanceID;
};

bool operator==(NetObjectDescriptor const& lhs, NetObjectDescriptor const& rhs);

namespace std
{
    template<>
    class hash<NetObjectDescriptor>
    {
    public:
        size_t operator()(NetObjectDescriptor const& descr) const
        {
            size_t data = descr.m_typeID << sizeof(unsigned short) | descr.m_instanceID;
            return std::hash<size_t>()(data);
        }
    };
}

class NetObject
{
public:
    using MessageHandler = std::function<void(NetMessage const&, NetAddr const&)>;

    NetObject(bool const isMaster, NetObjectDescriptor const& descriptor);
    ~NetObject();
    NetObject(NetObject const& other) = delete;

    bool IsMaster() const { return m_masterData.get() != nullptr; }

    void Update();

    void SendMasterBroadcast(NetMessage const& message);
    void SendMasterBroadcastExcluding(NetMessage const& message, NetAddr const& addr);
    void SendMasterUnicast(NetMessage const& message, NetAddr const& addr);
    void SendReplicaMessage(NetMessage const& message);
    void SendToAuthority(NetMessage const& message);

    void ReceiveMessage(NetMessage const& message, NetAddr const& sender);

    template<typename T> void RegisterMessageHandler(std::function<void(T const&, NetAddr const&)> handler);

    void SetOnReplicaAddedCallback(std::function<void(NetAddr const&)> const& callback);
    void SetOnReplicaLeftCallback(std::function<void(NetAddr const&)> const& callback);

    void OnReplicaAdded(NetAddr const& addr);
    void OnReplicaLeft(NetAddr const& addr);

private:
    void InitMasterDiscovery();
    void SendDiscoveryMessage();
    void InitHeartbeatHandler();
    void CheckReplicas();
    void OnReplicaMessage(NetAddr const& addr);

private:
    std::unique_ptr<NetObjectMasterData> m_masterData;

    std::unique_ptr<NetAddr> m_masterAddr;
    std::unordered_map <size_t, MessageHandler> m_handlers;
    std::chrono::system_clock::time_point m_lastHeartbeat;

    NetObjectDescriptor m_descriptor;
};

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

    void SendMessage(NetObjectDescriptor const& descriptor, NetMessage const& message, NetAddr const& recipient);

    NetAddr GetHostAddress() const;
    NetAddr GetLocalAddress() const;

    void RegisterNetObject(NetObjectDescriptor const& descriptor, NetObject* object);
    void UnregisterNetObject(NetObjectDescriptor const& descriptor);

private:
    NetObjectAPI(bool const isHost);
    NetObjectAPI(NetObjectAPI const& other) = delete;

    void InitMessageFactory();

    void ProcessMessages();
    bool ReceiveMessage();

private:
    using NetObjectMap = std::unordered_map <NetObjectDescriptor, NetObject*>;
    NetObjectMap m_netObjects;
    std::unordered_map <size_t, std::function<std::unique_ptr<NetMessage>()>> m_messageFactory;
    NetSocket m_socket;
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


