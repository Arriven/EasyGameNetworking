#include "NetObject.h"
#include "NetAPI.h"
#include "NetMessages.h"
#include <boost/range/adaptor/filtered.hpp>
#include <chrono>

NetObject::NetObject(bool const isMaster, NetObjectDescriptor const& descriptor)
    : m_descriptor(descriptor)
{
    NetObjectAPI::GetInstance()->RegisterNetObject(descriptor, this);
    if (isMaster)
    {
        m_masterData = std::make_unique<NetObjectMasterData>();
    }
    InitMasterDiscovery();
    RegisterMessageHandler<MementoUpdateMessage>([this](MementoUpdateMessage const& message, NetAddr const& addr) { OnMementoUpdateMessage(message, addr); });
}

NetObject::~NetObject()
{
    NetObjectAPI::GetInstance()->UnregisterNetObject(m_descriptor);
}

void NetObject::Update()
{
    if (!m_masterAddr)
    {
        SendDiscoveryMessage();
    }
    if (IsMaster())
    {
        for (auto&[typeId, memento] : m_mementoes)
        {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - memento.m_lastUpdateTime).count() > memento.m_updateInterval)
            {
                MementoUpdateMessage update(memento.m_data->Clone());
                memento.m_lastUpdateTime = std::chrono::system_clock::now();
                SendMasterBroadcast(update);
            }
        }
    }
}

void NetObject::SendMasterBroadcast(NetObjectMessageBase& message, ESendOptions const options)
{
    assert(IsMaster());
    SendMessageHelper(message, NetObjectAPI::GetInstance()->GetConnections(), options);
}

void NetObject::SendMasterBroadcastExcluding(NetObjectMessageBase& message, NetAddr const& addr, ESendOptions const options)
{
    assert(IsMaster());
    SendMessageHelper(message, NetObjectAPI::GetInstance()->GetConnections() | boost::adaptors::filtered([addr](auto const& conn) { return conn != addr; }), options);
}

void NetObject::SendMasterUnicast(NetObjectMessageBase& message, NetAddr const& addr, ESendOptions const options)
{
    assert(IsMaster());
    auto const replicas = NetObjectAPI::GetInstance()->GetConnections();
    assert(std::find(replicas.begin(), replicas.end(), addr) != replicas.end());
    SendMessageHelper(message, addr, options);
}

void NetObject::SendReplicaMessage(NetObjectMessageBase& message, ESendOptions const options)
{
    assert(!IsMaster());
    if (m_masterAddr)
    {
        SendMessageHelper(message, *m_masterAddr, options);
    }
}

void NetObject::SendToAuthority(NetObjectMessageBase& message, ESendOptions const options)
{
    SendMessageHelper(message, NetObjectAPI::GetInstance()->GetHostAddress(), options);
}

void NetObject::ReceiveMessage(INetMessage const& message, NetAddr const& sender)
{
    auto handlerIt = m_handlers.find(message.GetTypeID());
    if (handlerIt != m_handlers.end())
    {
        handlerIt->second(message, sender);
    }
}

void NetObject::SetOnReplicaAddedCallback(std::function<void(NetAddr const&)> const& callback)
{
    assert(IsMaster());
    m_masterData->m_replicaAddedCallback = callback;
}

void NetObject::SetOnReplicaLeftCallback(std::function<void(NetAddr const&)> const& callback)
{
    assert(IsMaster());
    m_masterData->m_replicaLeftCallback = callback;
}


void NetObject::OnReplicaAdded(NetAddr const& addr)
{
    if (IsMaster() && m_masterData->m_replicaAddedCallback)
    {
        m_masterData->m_replicaAddedCallback(addr);
    }
}

void NetObject::OnReplicaLeft(NetAddr const& addr)
{
    if (IsMaster() && m_masterData->m_replicaLeftCallback)
    {
        m_masterData->m_replicaLeftCallback(addr);
    }
}

void NetObject::SendMessage(NetObjectMessageBase const& message, NetAddr const& addr, ESendOptions const options)
{
    NetObjectAPI::GetInstance()->SendMessage(message, addr, options);
}

void NetObject::InitMasterDiscovery()
{
    if (IsMaster())
    {
        RegisterMessageHandler<SetMasterRequestMessage>([this](SetMasterRequestMessage const& message, NetAddr const addr)
        {
            SetMasterMessage msg;
            SendMasterUnicast(msg, addr);
        });
    }
    else
    {
        RegisterMessageHandler<SetMasterMessage>([this](INetMessage const& message, NetAddr const addr)
        {
            m_masterAddr = addr;
        });
    }
}

void NetObject::SendDiscoveryMessage()
{
    if (!IsMaster())
    {
        SetMasterRequestMessage msg;
        SendMessageHelper(msg, NetObjectAPI::GetInstance()->GetConnections());
    }
}

void NetObject::OnMementoUpdateMessage(MementoUpdateMessage const& message, NetAddr const& addr)
{
    size_t const typeId = message.GetData()->GetTypeID();
    NetObjectMemento& memento = m_mementoes[typeId];
    memento.m_data->CopyFrom(message.GetData());
}

template<>
void NetObject::SendMessageHelper(NetObjectMessageBase& message, NetAddr const& addr, ESendOptions const options)
{
    message.SetDescriptor(m_descriptor);
    SendMessage(message, addr, options);
}
