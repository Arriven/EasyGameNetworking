#include "NetSocket.h"
#include <iostream>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>

#ifdef _DEBUG
size_t constexpr HEARTBEAT_INTERVAL = 5000;
size_t constexpr KEEP_AVILE_TIME = 2000000;
#else
size_t constexpr HEARTBEAT_INTERVAL = 100;
size_t constexpr KEEP_AVILE_TIME = 2000;
#endif
size_t constexpr MAX_READ_SIZE = 1024;

NetConnection::NetConnection()
    : m_lastRecvAckTime(std::chrono::system_clock::now())
{
}

std::optional<NetBuffer> NetConnection::UpdateSend()
{
    if (!m_sendQueue.empty())
    {
        m_lastSentAckTime = std::chrono::system_clock::now();
        NetBuffer send = m_sendQueue.front();
        m_sendQueue.erase(m_sendQueue.begin());
        return send;
    }
    else if (NeedToSendHeartbeat())
    {
        m_lastSentAckTime = std::chrono::system_clock::now();
        return GetHeartbeatPacket();
    }
    return {};
}

std::optional<NetBuffer> NetConnection::UpdateRecv()
{
    if (!m_recvQueue.empty())
    {
        NetBuffer recv = m_recvQueue.front();
        m_recvQueue.erase(m_recvQueue.begin());
        return recv;
    }
    return {};
}

void NetConnection::AddSend(NetBuffer const& packet)
{
    m_sendQueue.emplace_back(packet);
}

void NetConnection::AddRecv(NetBuffer const& packet)
{
    m_lastRecvAckTime = std::chrono::system_clock::now();
    if (!IsHeartbeat(packet))
    {
        m_recvQueue.emplace_back(packet);
    }
}

bool NetConnection::IsConnected() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_lastRecvAckTime).count() < KEEP_AVILE_TIME;
}

bool NetConnection::NeedToSendHeartbeat() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_lastSentAckTime).count() >= HEARTBEAT_INTERVAL;
}

bool NetConnection::IsHeartbeat(NetBuffer const& packet) const
{
    return packet.empty();
}

NetBuffer NetConnection::GetHeartbeatPacket() const
{
    return NetBuffer();
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

void NetSocket::SendMessage(NetBuffer message, NetAddr recipient, ESendOptions options)
{
    auto& conn = GetOrCreateConnection(recipient);
    conn.AddSend(message);
}

std::optional<std::pair<NetBuffer, NetAddr>> NetSocket::RecvMessage()
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
        NetBuffer recv_buf;
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
