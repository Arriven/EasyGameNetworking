#include "NetObject.h"
#include "NetAPI.h"
#include "NetMessages.h"
#include <boost/range/adaptor/filtered.hpp>

NetObject::NetObject(bool const isMaster, NetObjectDescriptor const& descriptor)
    : m_descriptor(descriptor)
{
    NetObjectAPI::GetInstance()->RegisterNetObject(descriptor, this);
    if (isMaster)
    {
        m_masterData = std::make_unique<NetObjectMasterData>();
    }
    InitMasterDiscovery();
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
}

void NetObject::SendMasterBroadcast(NetObjectMessageBase& message)
{
    assert(IsMaster());
    SendMessageHelper(message, NetObjectAPI::GetInstance()->GetConnections());
}

void NetObject::SendMasterBroadcastExcluding(NetObjectMessageBase& message, NetAddr const& addr)
{
    assert(IsMaster());
    SendMessageHelper(message, NetObjectAPI::GetInstance()->GetConnections() | boost::adaptors::filtered([addr](auto const& conn) { return conn != addr; }));
}

void NetObject::SendMasterUnicast(NetObjectMessageBase& message, NetAddr const& addr)
{
    assert(IsMaster());
    auto const replicas = NetObjectAPI::GetInstance()->GetConnections();
    assert(std::find(replicas.begin(), replicas.end(), addr) != replicas.end());
    SendMessageHelper(message, addr);
}

void NetObject::SendReplicaMessage(NetObjectMessageBase& message)
{
    assert(!IsMaster());
    if (m_masterAddr)
    {
        SendMessageHelper(message, *m_masterAddr);
    }
}

void NetObject::SendToAuthority(NetObjectMessageBase& message)
{
    SendMessageHelper(message, NetObjectAPI::GetInstance()->GetHostAddress());
}

void NetObject::ReceiveMessage(INetMessage const& message, NetAddr const& sender)
{
    auto handlerIt = m_handlers.find(message.GetMessageID());
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

void NetObject::SendMessage(NetObjectMessageBase const& message, NetAddr const& addr)
{
    NetObjectAPI::GetInstance()->SendMessage(message, addr);
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