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

void NetObject::SendMasterBroadcast(NetObjectMessage& message)
{
    assert(IsMaster());
    SendMessage(message, NetObjectAPI::GetInstance()->GetConnections());
}

void NetObject::SendMasterBroadcastExcluding(NetObjectMessage& message, NetAddr const& addr)
{
    assert(IsMaster());
    SendMessage(message, NetObjectAPI::GetInstance()->GetConnections() | boost::adaptors::filtered([addr](auto const& conn) { return conn != addr; }));
}

void NetObject::SendMasterUnicast(NetObjectMessage& message, NetAddr const& addr)
{
    assert(IsMaster());
    auto const replicas = NetObjectAPI::GetInstance()->GetConnections();
    assert(std::find(replicas.begin(), replicas.end(), addr) != replicas.end());
    SendMessage(message, addr);
}

void NetObject::SendReplicaMessage(NetObjectMessage& message)
{
    assert(!IsMaster());
    if (m_masterAddr)
    {
        SendMessage(message, *m_masterAddr);
    }
}

void NetObject::SendToAuthority(NetObjectMessage& message)
{
    SendMessage(message, NetObjectAPI::GetInstance()->GetHostAddress());
}

void NetObject::ReceiveMessage(NetMessage const& message, NetAddr const& sender)
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

void NetObject::InitMasterDiscovery()
{
    RegisterMessageHandler<SetMasterMessage>([this](NetMessage const& message, NetAddr const addr)
    {
        m_masterAddr = addr;
    });
    if (IsMaster())
    {
        RegisterMessageHandler<SetMasterRequestMessage>([this](SetMasterRequestMessage const& message, NetAddr const addr)
        {
            if (message.m_address == 0)
            {
                SetMasterMessage msg;
                SendMasterUnicast(msg, addr);
            }
            else
            {
                NetAddr requester(boost::asio::ip::address_v4(message.m_address), message.m_usPort);
                SetMasterMessage msg;
                SendMasterUnicast(msg, addr);
            }
        });
        SetMasterMessage msg;
        SendToAuthority(msg);
    }
    else if (NetObjectAPI::GetInstance()->IsHost())
    {
        RegisterMessageHandler<SetMasterRequestMessage>([this](SetMasterRequestMessage const& message, NetAddr const addr)
        {
            if (m_masterAddr)
            {
                SetMasterRequestMessage forwarded;
                forwarded.m_address = addr.address().to_v4().to_ulong();
                forwarded.m_usPort = addr.port();
                SendMessage(forwarded, *m_masterAddr);
            }
        });
    }
}

void NetObject::SendDiscoveryMessage()
{
    if (!IsMaster() && !NetObjectAPI::GetInstance()->IsHost())
    {
        SetMasterRequestMessage msg;
        SendToAuthority(msg);
    }
}