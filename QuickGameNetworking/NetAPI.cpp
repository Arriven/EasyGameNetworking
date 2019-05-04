#include "NetAPI.h"
#include "NetMessages.h"
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/algorithm_ext.hpp>

size_t constexpr HEARTBEAT_INTERVAL = 500;
size_t constexpr KEEP_AVILE_TIME = 2000;

std::unique_ptr<NetObjectAPI> NetObjectAPI::ms_instance;
boost::asio::io_service io_service;

NetObjectAPI::NetObjectAPI(NetAddr const& hostAddress, bool const isHost)
    : m_isHost(isHost)
    , m_hostAddress(hostAddress)
{
    if (isHost)
    {
        m_socket = NetSocket(io_service, GetHostAddress());
    }
    else
    {
        m_socket = NetSocket(io_service);
    }
    if (!isHost)
    {
        RegisterMessageHandler<SessionSetupMessage>([this](SessionSetupMessage const& message, NetAddr const& sender)
        {
            for (auto const& connection : message.m_connections)
            {
                m_socket->Connect(connection);
            }
        });
    }
}

void NetObjectAPI::Init(NetAddr const& hostAddress, bool const isHost)
{
    NetDataFactory::Init();
    ms_instance.reset(new NetObjectAPI(hostAddress, isHost));
}

void NetObjectAPI::Shutdown()
{
    ms_instance.reset();
    NetDataFactory::Shutdown();
}

void NetObjectAPI::Update()
{
    auto const [newConnections, deadConnections] = m_socket->Update();

    if (IsHost())
    {
        for (auto const& addr : newConnections)
        {
            std::vector<NetAddr> connections;
            boost::range::push_back(connections, GetConnections() | boost::adaptors::filtered([addr](auto const& conn) { return conn != addr; }));
            SendMessage(SessionSetupMessage{ connections }, addr, ESendOptions::Reliable);
        }
    }
    else
    {
        if (!m_socket->IsConnected(GetHostAddress()))
        {
            m_socket->Connect(GetHostAddress());
        }
    }

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

void NetObjectAPI::SendMessage(INetMessage const& message, NetAddr const& recipient, ESendOptions const options)
{
    if (recipient == m_socket->GetLocalAddress())
    {
        HandleMessage(&message, recipient);
        return;
    }
    std::vector<char> buffer;
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>> > output_stream(buffer);
    boost::archive::binary_oarchive stream(output_stream, boost::archive::no_header | boost::archive::no_tracking);
    stream << message.GetTypeID();
    message.Serialize(stream);
    output_stream.flush();
    m_socket->SendMessage(buffer, recipient, options);
}

NetAddr NetObjectAPI::GetHostAddress() const
{
    return m_hostAddress;
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
    auto const msg = m_socket->RecvMessage();
    if (!msg)
    {
        return false;
    }
    auto const&[recv_buf, addr] = msg.value();
    boost::iostreams::basic_array_source<char> source(recv_buf.data(), recv_buf.size());
    boost::iostreams::stream< boost::iostreams::basic_array_source <char> > input_stream(source);
    boost::archive::binary_iarchive ia(input_stream, boost::archive::no_header | boost::archive::no_tracking);

    size_t messageID;
    ia >> messageID;
    auto message = NetDataFactory::GetInstance()->CreateDataContainer(messageID);
    if (!message)
    {
        return false;
    }
    message->Deserialize(ia);

    HandleMessage(message.get(), addr);
    return true;
}

void NetObjectAPI::HandleMessage(INetMessage const* message, NetAddr const& sender)
{
    if (auto const* netObjectMessage = dynamic_cast<NetObjectMessageBase const*>(message))
    {
        if (auto netObject = m_netObjects[netObjectMessage->GetDescriptor()])
        {
            netObject->ReceiveMessage(*netObjectMessage, sender);
        }
    }
    else 
    {
        auto handlerIt = m_handlers.find(message->GetTypeID());
        if (handlerIt != m_handlers.end())
        {
            handlerIt->second(*message, sender);
        }
    }
}
