#include "NetSocket.h"
#include <iostream>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>

#ifdef _DEBUG
size_t constexpr HEARTBEAT_INTERVAL = 5000;
size_t constexpr KEEP_AVILE_TIME = 2000000;
#else
size_t constexpr HEARTBEAT_INTERVAL = 100;
size_t constexpr KEEP_AVILE_TIME = 2000;
#endif
size_t constexpr RESEND_INTERVAL = 200;
size_t constexpr HIHG_PRIORITY_RESEND_INTERVAL = 10;
size_t constexpr MAX_READ_SIZE = 1024;


NetData NetPacket::Serialize() const
{
    NetData buffer;
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>> > output_stream(buffer);
    boost::archive::binary_oarchive stream(output_stream, boost::archive::no_header | boost::archive::no_tracking);
    stream << m_options;
    stream << m_ack;
    stream << m_data;
    output_stream.flush();
    return buffer;
}

NetPacket NetPacket::Deserialize(NetData const& data)
{
    boost::iostreams::basic_array_source<char> source(data.data(), data.size());
    boost::iostreams::stream< boost::iostreams::basic_array_source <char> > input_stream(source);
    boost::archive::binary_iarchive stream(input_stream, boost::archive::no_header | boost::archive::no_tracking);
    NetPacket packet;
    stream >> packet.m_options;
    stream >> packet.m_ack;
    stream >> packet.m_data;
    return packet;
}

bool NetPacket::NeedsResend() const
{
    auto const lastSendTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_lastSentTime).count();
    return lastSendTime >= RESEND_INTERVAL ||
        ((m_options & ESendOptions::HighPriority) != ESendOptions::None && lastSendTime >= HIHG_PRIORITY_RESEND_INTERVAL);
}

void NetPacket::UpdateSendTime()
{
    m_lastSentTime = std::chrono::system_clock::now();
}

bool PacketHelpers::IsHeartbeat(NetData const& packet)
{
    return packet.empty();
}

NetData PacketHelpers::GetHeartbeatPacket()
{
    return NetData();
}

bool PacketHelpers::IsAck(NetData const& packet)
{
    return packet.size() == sizeof(size_t);
}

NetData PacketHelpers::GetAckPacket(size_t const ack)
{
    NetData buffer;
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>> > output_stream(buffer);
    boost::archive::binary_oarchive stream(output_stream, boost::archive::no_header | boost::archive::no_tracking);
    stream << ack;
    output_stream.flush();
    assert(buffer.size() == sizeof(size_t));
    return buffer;
}

size_t PacketHelpers::GetAck(NetData const& packet)
{
    boost::iostreams::basic_array_source<char> source(packet.data(), packet.size());
    boost::iostreams::stream< boost::iostreams::basic_array_source <char> > input_stream(source);
    boost::archive::binary_iarchive stream(input_stream, boost::archive::no_header | boost::archive::no_tracking);
    size_t ack;;
    stream >> ack;
    return ack;
}

std::optional<NetData> UnreliableChannel::UpdateSend()
{
    if (!m_sendQueue.empty())
    {
        NetPacket const& packet = m_sendQueue.front();
        NetData const send = packet.Serialize();
        m_sendQueue.erase(m_sendQueue.begin());
        return send;
    }
    return {};
}


std::optional<NetData> UnreliableChannel::UpdateRecv()
{
    if (!m_recvQueue.empty())
    {
        NetPacket const& packet = m_recvQueue.front();
        NetData const recv = packet.m_data;
        m_recvQueue.erase(m_recvQueue.begin());
        return recv;
    }
    return {};
}

void UnreliableChannel::AddSend(NetData const& data, ESendOptions const options)
{
    assert((options & ESendOptions::Reliable) == ESendOptions::None);
    m_sendQueue.emplace_back(data, options, ++m_lastSendAck);
}

void UnreliableChannel::AddRecv(NetPacket const& packet)
{
    assert((packet.m_options & ESendOptions::Reliable) == ESendOptions::None);
    if (packet.m_ack > m_lastRecvAck)
    {
        m_lastRecvAck++;
        m_recvQueue.emplace_back(packet);
    }
}

std::optional<NetData> ReliableChannel::UpdateSend()
{
    if (!m_ackQueue.empty())
    {
        NetData const send = m_ackQueue.front();
        m_ackQueue.erase(m_ackQueue.begin());
        return send;
    }

    auto it = boost::find_if(m_sendQueue, [](NetPacket const& packet) { return packet.NeedsResend(); });
    if (it != m_sendQueue.end())
    {
        NetPacket& packet = *it;
        NetData send = packet.Serialize();
        assert((packet.m_options & ESendOptions::Reliable) != ESendOptions::None);
        packet.UpdateSendTime();
        return send;
    }

    return {};
}

std::optional<NetData> ReliableChannel::UpdateRecv()
{
    if (!m_recvQueue.empty())
    {
        NetPacket recv = *m_recvQueue.begin();
        assert((recv.m_options & ESendOptions::Reliable) != ESendOptions::None);
        if (recv.m_ack == m_lastRecvAck)
        {
            m_lastRecvAck++;
            m_recvQueue.erase(m_recvQueue.begin());
            return recv.m_data;
        }
    }
    return {};
}

void ReliableChannel::AddSend(NetData const& data, ESendOptions const options)
{
    assert((options & ESendOptions::Reliable) != ESendOptions::None);
    m_sendQueue.emplace_back(data, options, ++m_lastSendAck);
}

void ReliableChannel::AddRecv(NetPacket const& packet)
{
    assert((packet.m_options & ESendOptions::Reliable) != ESendOptions::None);
    m_ackQueue.emplace_back(PacketHelpers::GetAckPacket(packet.m_ack));
    m_recvQueue.insert(packet);
}

void ReliableChannel::OnAck(size_t const ack)
{
    auto it = boost::find_if(m_sendQueue, [ack](NetPacket const& packet) { return packet.m_ack == ack; });
    m_sendQueue.erase(it);
}

NetConnection::NetConnection()
    : m_lastRecvTime(std::chrono::system_clock::now())
{
}

std::optional<NetData> NetConnection::UpdateSend()
{
    std::optional<NetData> ret;

    if (auto send = m_reliableChannel.UpdateSend())
    {
        ret = send;
    }
    else if (auto send = m_unreliableChannel.UpdateSend())
    {
        ret = send;
    }
    else if (NeedToSendHeartbeat())
    {
        ret = PacketHelpers::GetHeartbeatPacket();
    }

    if (ret)
    {
        m_lastSendTime = std::chrono::system_clock::now();
    }
    return ret;
}

std::optional<NetData> NetConnection::UpdateRecv()
{
    if (auto recv = m_reliableChannel.UpdateRecv())
    {
        return recv;
    }
    if (auto recv = m_unreliableChannel.UpdateRecv())
    {
        return recv;
    }
    return {};
}

void NetConnection::AddSend(NetData const& data, ESendOptions const options)
{
    if ((options & ESendOptions::Reliable) != ESendOptions::None)
    {
        m_reliableChannel.AddSend(data, options);
    }
    else
    {
        m_unreliableChannel.AddSend(data, options);
    }
}

void NetConnection::AddRecv(NetData const& data)
{
    m_lastRecvTime = std::chrono::system_clock::now();
    if (PacketHelpers::IsHeartbeat(data))
    {
        return;
    }
    else if (PacketHelpers::IsAck(data))
    {
        size_t const ack = PacketHelpers::GetAck(data);
        m_reliableChannel.OnAck(ack);
        return;
    }

    NetPacket const packet = NetPacket::Deserialize(data);
    if ((packet.m_options & ESendOptions::Reliable) != ESendOptions::None)
    {
        m_reliableChannel.AddRecv(packet);
    }
    else
    {
        m_unreliableChannel.AddRecv(packet);
    }
}

bool NetConnection::IsConnected() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_lastRecvTime).count() < KEEP_AVILE_TIME;
}

bool NetConnection::NeedToSendHeartbeat() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_lastSendTime).count() >= HEARTBEAT_INTERVAL;
}

NetSocket::NetSocket(boost::asio::io_service& io_service)
    : NetSocket(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0))
{
}

NetSocket::NetSocket(boost::asio::io_service& io_service, NetAddr endPoint)
    : m_socket(io_service, endPoint)
{
    m_socket.non_blocking(true);
}

void NetSocket::SendMessage(NetData message, NetAddr recipient, ESendOptions options)
{
    auto& conn = GetOrCreateConnection(recipient);
    conn.AddSend(message, options);
}

std::optional<std::pair<NetData, NetAddr>> NetSocket::RecvMessage()
{
    for (auto& [endPoint, connection] : m_connections)
    {
        auto recv = connection.UpdateRecv();
        if (recv)
        {
            return { std::pair(recv.value(), endPoint) };
        }
    }
    return {};
}

void NetSocket::Connect(NetAddr recipient)
{
    GetOrCreateConnection(recipient);
}

bool NetSocket::IsConnected(NetAddr recipient) const
{
    return m_connections.find(recipient) != m_connections.end();
}

std::vector<NetAddr> NetSocket::GetConnections() const
{
    std::vector<NetAddr> connections;
    boost::range::push_back(connections, m_connections | boost::adaptors::map_keys);
    return connections;
}

NetConnectionsUpdate NetSocket::Update()
{
    for (auto& [endPoint, connection] : m_connections)
    {
        while (auto send = connection.UpdateSend())
        {
            boost::system::error_code ignored_error;
            m_socket.send_to(boost::asio::buffer(send.value()),
                endPoint, 0, ignored_error);
            assert(!ignored_error);
        }
    }
    ProcessMessages();
    return {
        PollNewConnections(),
        KillDeadConnections()
    };
}

NetAddr NetSocket::GetLocalAddress() const
{
    return m_socket.local_endpoint();
}

void NetSocket::ProcessMessages()
{
    while (true)
    {
        NetData recv_buf;
        recv_buf.resize(MAX_READ_SIZE);
        boost::asio::ip::udp::endpoint sender;
        boost::system::error_code error;
        size_t bytes = m_socket.receive_from(boost::asio::buffer(recv_buf),
            sender, 0, error);
        if (error)
        {
            break;
        }
        recv_buf.resize(bytes);
        auto& conn = GetOrCreateConnection(sender);
        conn.AddRecv(recv_buf);
    }
}

std::vector<NetAddr> NetSocket::KillDeadConnections()
{
    auto connectionIsDead = [](auto const& connection) { return !connection.second.IsConnected(); };
    std::vector<NetAddr> deadConnections;
    boost::range::push_back(deadConnections, m_connections | boost::adaptors::filtered(connectionIsDead) | boost::adaptors::map_keys);
    m_connections.erase(boost::remove_if(m_connections, connectionIsDead), m_connections.end());
    return deadConnections;
}

std::vector<NetAddr> NetSocket::PollNewConnections()
{
    auto const newConnections = m_newConnections;
    m_newConnections.clear();
    return newConnections;
}

NetConnection& NetSocket::GetOrCreateConnection(NetAddr recipient)
{
    if (!IsConnected(recipient))
    {
        m_newConnections.push_back(recipient);
    }
    return m_connections[recipient];
}