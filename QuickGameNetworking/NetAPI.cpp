#include "NetAPI.h"
#include "NetMessages.h"
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/range/adaptor/filtered.hpp>

size_t constexpr HEARTBEAT_INTERVAL = 500;
size_t constexpr KEEP_AVILE_TIME = 2000;
size_t constexpr HOST_PORT = 8888;

bool operator==(NetObjectDescriptor const& lhs, NetObjectDescriptor const& rhs)
{
    return lhs.m_instanceID == rhs.m_instanceID && lhs.m_typeID == rhs.m_typeID;
}

void NetObjectMessage::Serialize(boost::archive::binary_oarchive& stream) const
{
    stream << m_descriptor->m_typeID;
    stream << m_descriptor->m_instanceID;
    SerializeData(stream);
}

void NetObjectMessage::Deserialize(boost::archive::binary_iarchive& stream)
{
    m_descriptor.reset(new NetObjectDescriptor);
    stream >> m_descriptor->m_typeID;
    stream >> m_descriptor->m_instanceID;
    DeserializeData(stream);
}

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

std::unique_ptr<NetObjectAPI> NetObjectAPI::ms_instance;
boost::asio::io_service io_service;

NetObjectAPI::NetObjectAPI(bool const isHost)
    : m_isHost(isHost)
{
    if (isHost)
    {
        m_socket = NetSocket(io_service, GetHostAddress());
    }
    else
    {
        m_socket = NetSocket(io_service);
    }
    InitMessageFactory();
}

void NetObjectAPI::Init(bool const isHost)
{
    ms_instance.reset(new NetObjectAPI(isHost));
}

void NetObjectAPI::Shutdown()
{
    ms_instance.reset();
}

void NetObjectAPI::Update()
{
    auto const [newConnections, deadConnections] = m_socket->Update();

    ProcessMessages();

    for (auto&[descriptor, netObject] : m_netObjects)
    {
        for (auto const& deadConnection : deadConnections)
        {
            netObject->OnReplicaLeft(deadConnection);
        }
        for (auto const& newConnection : newConnections)
        {
            netObject->OnReplicaAdded(newConnection);
        }
        netObject->Update();
    }
}

std::unique_ptr<NetObject> NetObjectAPI::CreateMasterNetObject(NetObjectDescriptor const& descriptor)
{
    return std::make_unique<NetObject>(true, descriptor);
}

std::unique_ptr<NetObject> NetObjectAPI::CreateReplicaNetObject(NetObjectDescriptor const& descriptor)
{
    return std::make_unique<NetObject>(false, descriptor);
}

std::unique_ptr<NetObject> NetObjectAPI::CreateThirdPartyNetObject(NetObjectDescriptor const& descriptor)
{
    return IsHost() ? CreateMasterNetObject(descriptor) : CreateReplicaNetObject(descriptor);
}

void NetObjectAPI::RegisterNetObject(NetObjectDescriptor const& descriptor, NetObject* object)
{
    m_netObjects[descriptor] = object;
}

void NetObjectAPI::UnregisterNetObject(NetObjectDescriptor const& descriptor)
{
    m_netObjects[descriptor] = nullptr;
}

void NetObjectAPI::InitMessageFactory()
{
    RegisterMessage<TextMessage>();
    RegisterMessage<SetMasterRequestMessage>();
    RegisterMessage<SetMasterMessage>();
}

void NetObjectAPI::SendMessage(NetMessage const& message, NetAddr const& recipient)
{
    if (recipient == m_socket->GetLocalAddress())
    {
        HandleMessage(&message, recipient);
        return;
    }
    std::vector<char> buffer;
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>> > output_stream(buffer);
    boost::archive::binary_oarchive stream(output_stream, boost::archive::no_header | boost::archive::no_tracking);
    stream << message.GetMessageID();
    message.Serialize(stream);
    output_stream.flush();
    m_socket->SendMessage(buffer, recipient, ESendOptions::None);
}

NetAddr NetObjectAPI::GetHostAddress() const
{
    boost::asio::ip::udp::endpoint remote_endpoint(boost::asio::ip::address::from_string("127.0.0.1"), HOST_PORT);
    return remote_endpoint;
}

NetAddr NetObjectAPI::GetLocalAddress() const
{
    return m_socket->GetLocalAddress();
}

std::vector<NetAddr> NetObjectAPI::GetConnections() const
{
    return m_socket->GetConnections();
}

void NetObjectAPI::ProcessMessages()
{
    while (ReceiveMessage());
}

bool NetObjectAPI::ReceiveMessage()
{
    std::vector<char> recv_buf;
    boost::asio::ip::udp::endpoint addr;
    boost::system::error_code error;
    if (!m_socket->RecvMessage(recv_buf, addr))
    {
        return false;
    }
    boost::iostreams::basic_array_source<char> source(recv_buf.data(), recv_buf.size());
    boost::iostreams::stream< boost::iostreams::basic_array_source <char> > input_stream(source);
    boost::archive::binary_iarchive ia(input_stream, boost::archive::no_header | boost::archive::no_tracking);

    size_t messageID;
    ia >> messageID;
    auto factory = m_messageFactory[messageID];
    if (!factory)
    {
        return false;
    }
    auto message = factory();
    message->Deserialize(ia);

    HandleMessage(message.get(), addr);
    return true;
}

void NetObjectAPI::HandleMessage(NetMessage const* message, NetAddr const& sender)
{
    if (auto const* netObjectMessage = dynamic_cast<NetObjectMessage const*>(message))
    {
        if (auto netObject = m_netObjects[netObjectMessage->GetDescriptor()])
        {
            netObject->ReceiveMessage(*netObjectMessage, sender);
        }
    }
}
