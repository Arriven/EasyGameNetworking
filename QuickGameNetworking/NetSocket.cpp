#include "NetSocket.h"
#include <iostream>

size_t constexpr HEARTBEAT_INTERVAL = 5000;
size_t constexpr KEEP_AVILE_TIME = 2000000;
size_t constexpr MAX_READ_SIZE = 1024;

NetConnection::NetConnection(NetAddr const& recepient)
    : m_endPoint(recepient)
{
    m_lastRecvAckTime = std::chrono::system_clock::now();
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
        m_lastRecvAckTime = std::chrono::system_clock::now();
        NetBuffer recv = m_recvQueue.front();
        m_recvQueue.erase(m_recvQueue.begin());
        if (!IsHeartbeat(recv))
        {
            return recv;
        }
    }
    return {};
}

void NetConnection::AddSend(NetBuffer const& packet)
{
    m_sendQueue.emplace_back(packet);
}

void NetConnection::AddRecv(NetBuffer const& packet)
{
    m_recvQueue.emplace_back(packet);
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

bool NetSocket::RecvMessage(NetBuffer& message, NetAddr& sender)
{
    for (auto& connection : m_connections)
    {
        auto recv = connection.UpdateRecv();
        if (recv)
        {
            message = recv.value();
            sender = connection.GetEndPoint();
            return true;
        }
    }
    return false;
}

void NetSocket::Update()
{
    ProcessMessages();
    KillDeadConnections();
    for (auto& connection : m_connections)
    {
        while (auto send = connection.UpdateSend())
        {
            boost::system::error_code ignored_error;
            m_socket.send_to(boost::asio::buffer(send.value()),
                connection.GetEndPoint(), 0, ignored_error);
            assert(!ignored_error);
        }
    }
}

NetAddr NetSocket::GetLocalAddress() const
{
    return m_socket.local_endpoint();
}

void NetSocket::SetOnConnectionAddedCallback(std::function<void(NetAddr)> callback)
{
    m_onConnectionAdded = callback;
}

void NetSocket::SetOnConnectionRemovedCallback(std::function<void(NetAddr)> callback)
{
    m_onConnectionRemoved = callback;
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
            return;
        }
        recv_buf.resize(bytes);
        auto& conn = GetOrCreateConnection(sender);
        conn.AddRecv(recv_buf);
    }
}

void NetSocket::KillDeadConnections()
{
    for (auto const& connection : m_connections)
    {
        if (m_onConnectionRemoved && !connection.IsConnected())
        {
            m_onConnectionRemoved(connection.GetEndPoint());
        }
    }

    m_connections.erase(
        std::remove_if(
            m_connections.begin(),
            m_connections.end(),
            [](NetConnection const& connection) { return !connection.IsConnected(); }),
        m_connections.end());
}

NetConnection& NetSocket::GetOrCreateConnection(NetAddr recipient)
{
    auto connIt = std::find_if(m_connections.begin(), m_connections.end(), [recipient](NetConnection const& connection) { return connection.GetEndPoint() == recipient; });
    if (connIt == m_connections.end())
    {
        connIt = m_connections.insert(connIt, NetConnection(recipient));
        if (m_onConnectionAdded)
        {
            m_onConnectionAdded(recipient);
        }
    }
    return *connIt;
}
