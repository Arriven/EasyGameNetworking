#include "NetAPI.h"
#include "Messages.h"
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>

size_t constexpr HEARTBEAT_INTERVAL = 500;
size_t constexpr KEEP_AVILE_TIME = 2000;
size_t constexpr HOST_PORT = 8888;

bool operator==(NetObjectDescriptor const& lhs, NetObjectDescriptor const& rhs)
{
    return lhs.m_instanceID == rhs.m_instanceID && lhs.m_typeID == rhs.m_typeID;
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
    InitHeartbeatHandler();
}

NetObject::~NetObject()
{
    NetObjectAPI::GetInstance()->UnregisterNetObject(m_descriptor);
}

void NetObject::Update()
{
    if (IsMaster())
    {
        //CheckReplicas();
    }
    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_lastHeartbeat).count() > HEARTBEAT_INTERVAL)
    {
        if (!m_masterAddr)
        {
            SendDiscoveryMessage();
        }
        else if (IsMaster())
        {
          //  SendMasterBroadcast(HeartbeatMessage());
        }
        else
        {
            //SendReplicaMessage(HeartbeatMessage());
        }
    }
}

void NetObject::SendMasterBroadcast(NetMessage const& message)
{
    assert(IsMaster());
    m_lastHeartbeat = std::chrono::system_clock::now();
    auto& replicas = m_masterData->m_replicas;
    for (auto replica : replicas)
    {
        NetObjectAPI::GetInstance()->SendMessage(m_descriptor, message, replica.m_addr);
    }
}

void NetObject::SendMasterBroadcastExcluding(NetMessage const& message, NetAddr const& addr)
{
    assert(IsMaster());
    auto& replicas = m_masterData->m_replicas;
    for (auto replica : replicas)
    {
        if (replica.m_addr != addr)
        {
            NetObjectAPI::GetInstance()->SendMessage(m_descriptor, message, replica.m_addr);
        }
    }
}

void NetObject::SendMasterUnicast(NetMessage const& message, NetAddr const& addr)
{
    assert(IsMaster());
    auto& replicas = m_masterData->m_replicas;
    //assert(std::find_if(replicas.begin(), replicas.end(), [&addr](ReplicaData const& replica) { return replica.m_addr == addr; }) != replicas.end());
    NetObjectAPI::GetInstance()->SendMessage(m_descriptor, message, addr);
}

void NetObject::SendReplicaMessage(NetMessage const& message)
{
    assert(!IsMaster());
    m_lastHeartbeat = std::chrono::system_clock::now();
    //TODO: send this to master directly
    if (m_masterAddr)
    {
        NetObjectAPI::GetInstance()->SendMessage(m_descriptor, message, *m_masterAddr);
    }
}

void NetObject::SendToAuthority(NetMessage const& message)
{
    NetObjectAPI::GetInstance()->SendMessage(m_descriptor, message, NetObjectAPI::GetInstance()->GetHostAddress());
}

void NetObject::ReceiveMessage(NetMessage const& message, NetAddr const& sender)
{
    if (IsMaster() && sender != NetObjectAPI::GetInstance()->GetLocalAddress())
    {
        OnReplicaMessage(sender);
    }
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
        m_masterData->m_replicas.push_back({ addr, std::chrono::system_clock::now() });
    }
}

void NetObject::OnReplicaLeft(NetAddr const& addr)
{
    if (IsMaster() && m_masterData->m_replicaLeftCallback)
    {
        m_masterData->m_replicaLeftCallback(addr);
        m_masterData->m_replicas.erase(std::find_if(m_masterData->m_replicas.begin(), m_masterData->m_replicas.end(), [addr](auto const& replica) { return replica.m_addr == addr; }));
    }
}

void NetObject::InitMasterDiscovery()
{
    RegisterMessageHandler<SetMasterMessage>([this](NetMessage const& message, NetAddr const addr)
    {
        m_masterAddr.reset(new NetAddr());
        *m_masterAddr = addr;
    });
    if (IsMaster())
    {
        RegisterMessageHandler<SetMasterRequestMessage>([this](SetMasterRequestMessage const& message, NetAddr const addr)
        {
            if (message.m_address == 0)
            {
                SendMasterUnicast(SetMasterMessage(), addr);
            }
            else
            {
                NetAddr requester(boost::asio::ip::address_v4(message.m_address), message.m_usPort);
                OnReplicaMessage(requester);
                SendMasterUnicast(SetMasterMessage(), requester);
            }
        });
        SendToAuthority(SetMasterMessage());
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
                NetObjectAPI::GetInstance()->SendMessage(m_descriptor, forwarded, *m_masterAddr);
            }
        });
    }
}

void NetObject::SendDiscoveryMessage()
{
    m_lastHeartbeat = std::chrono::system_clock::now();
    if (!IsMaster() && !NetObjectAPI::GetInstance()->IsHost())
    {
        SendToAuthority(SetMasterRequestMessage());
    }
}

void NetObject::InitHeartbeatHandler()
{
    RegisterMessageHandler<HeartbeatMessage>([this](HeartbeatMessage const& message, NetAddr const& addr) {});
}

void NetObject::CheckReplicas()
{
    assert(IsMaster());

    auto& replicas = m_masterData->m_replicas;
    auto replicasToRemoveIt = std::partition(replicas.begin(), replicas.end(), [](ReplicaData const& replica)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - replica.m_lastHeartbeat).count() < KEEP_AVILE_TIME;
    });
    std::vector<ReplicaData> removedReplicas(replicasToRemoveIt, replicas.end());
    replicas.erase(replicasToRemoveIt, replicas.end());
    if (m_masterData->m_replicaLeftCallback)
    {
        for (auto const& replica : removedReplicas)
        {
            m_masterData->m_replicaLeftCallback(replica.m_addr);
        }
    }
}

void NetObject::OnReplicaMessage(NetAddr const& addr)
{
    auto& replicas = m_masterData->m_replicas;
    auto replica = std::find_if(replicas.begin(), replicas.end(), [addr](ReplicaData const& element) { return addr == element.m_addr; });
    if (replica != replicas.end())
    {
        replica->m_lastHeartbeat = std::chrono::system_clock::now();
    }
    else
    {
        replicas.push_back({ addr, std::chrono::system_clock::now() });
        if (m_masterData->m_replicaAddedCallback)
        {
            m_masterData->m_replicaAddedCallback(addr);
        }
    }
}

std::unique_ptr<NetObjectAPI> NetObjectAPI::ms_instance;
boost::asio::io_service io_service;

NetObjectAPI::NetObjectAPI(bool const isHost)
    : m_isHost(isHost)
    , m_socket(io_service)
{
    if (isHost)
    {
        m_socket.Bind(GetHostAddress());
    }
    InitMessageFactory();
    m_socket.SetOnConnectionAddedCallback(
        [this](NetAddr address) 
    {
        for (auto const& netObj : m_netObjects)
        {
            netObj.second->OnReplicaAdded(address);
        }
    });
    m_socket.SetOnConnectionRemovedCallback(
        [this](NetAddr address)
    {
        for (auto const& netObj : m_netObjects)
        {
            netObj.second->OnReplicaLeft(address);
        }
    });
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
    m_socket.Update();
    ProcessMessages();
    for (auto& it : m_netObjects)
    {
        auto netObject = it.second;
        if (netObject)
        {
            netObject->Update();
        }
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
    RegisterMessage<HeartbeatMessage>();
    RegisterMessage<TextMessage>();
    RegisterMessage<SetMasterRequestMessage>();
    RegisterMessage<SetMasterMessage>();
}

void NetObjectAPI::SendMessage(NetObjectDescriptor const& descriptor, NetMessage const& message, NetAddr const& recipient)
{
    std::vector<char> buffer;
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>> > output_stream(buffer);
    boost::archive::binary_oarchive stream(output_stream, boost::archive::no_header | boost::archive::no_tracking);
    stream << descriptor.m_typeID;
    stream << descriptor.m_instanceID;
    stream << message.GetMessageID();
    message.Serialize(stream);
    output_stream.flush();
    m_socket.SendMessage(buffer, recipient, ESendOptions::None);
}

NetAddr NetObjectAPI::GetHostAddress() const
{
    boost::asio::ip::udp::endpoint remote_endpoint(boost::asio::ip::address::from_string("127.0.0.1"), HOST_PORT);
    return remote_endpoint;
}

NetAddr NetObjectAPI::GetLocalAddress() const
{
    return m_socket.GetLocalAddress();
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
    if (!m_socket.RecvMessage(recv_buf, addr))
    {
        return false;
    }
    boost::iostreams::basic_array_source<char> source(recv_buf.data(), recv_buf.size());
    boost::iostreams::stream< boost::iostreams::basic_array_source <char> > input_stream(source);
    boost::archive::binary_iarchive ia(input_stream, boost::archive::no_header | boost::archive::no_tracking);
    NetObjectDescriptor descriptor;
    ia >> descriptor.m_typeID;
    ia >> descriptor.m_instanceID;

    auto netObject = m_netObjects[descriptor];
    if (netObject)
    {
        size_t messageID;
        ia >> messageID;
        auto factory = m_messageFactory[messageID];
        if (!factory)
        {
            return false;
        }
        auto message = factory();
        message->Deserialize(ia);

        netObject->ReceiveMessage(*message, addr);
    }
    return true;
}
