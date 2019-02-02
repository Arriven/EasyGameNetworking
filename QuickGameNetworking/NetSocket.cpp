#include "NetSocket.h"
#include <iostream>

size_t constexpr HEARTBEAT_INTERVAL = 5000;
size_t constexpr KEEP_AVILE_TIME = 2000000;

NetConnection::NetConnection(NetAddr const& recepient)
    : m_endPoint(recepient)
{
    m_lastRecvAck = std::chrono::system_clock::now();
}

std::optional<NetPacket> NetConnection::UpdateSend()
{
    if (!m_sendQueue.empty())
    {
        m_lastSentAck = std::chrono::system_clock::now();
        NetPacket send = m_sendQueue.front();
        m_sendQueue.erase(m_sendQueue.begin());
        return send;
    }
    else if (NeedToSendHeartbeat())
    {
        m_lastSentAck = std::chrono::system_clock::now();
        return GetHeartbeatPacket();
    }
    return {};
}

std::optional<NetPacket> NetConnection::UpdateRecv()
{
    if (!m_recvQueue.empty())
    {
        m_lastRecvAck = std::chrono::system_clock::now();
        NetPacket recv = m_recvQueue.front();
        m_recvQueue.erase(m_recvQueue.begin());
        if (!IsHeartbeat(recv))
        {
            return recv;
        }
    }
    return {};
}

void NetConnection::AddSend(NetPacket const& packet)
{
    m_sendQueue.emplace_back(packet);
}

void NetConnection::AddRecv(NetPacket const& packet)
{
    m_recvQueue.emplace_back(packet);
}

bool NetConnection::IsConnected() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_lastRecvAck).count() < KEEP_AVILE_TIME;
}

bool NetConnection::NeedToSendHeartbeat() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_lastSentAck).count() >= HEARTBEAT_INTERVAL;
}

bool NetConnection::IsHeartbeat(NetPacket const& packet) const
{
    return packet.empty();
}

NetPacket NetConnection::GetHeartbeatPacket() const
{
    return NetPacket();
}

NetSocket::NetSocket(boost::asio::io_service& io_service)
    : m_socket(io_service, boost::asio::ip::udp::v4())
{
    m_socket.non_blocking(true);
}

NetSocket::NetSocket(boost::asio::io_service& io_service, NetAddr endPoint)
    : NetSocket(io_service)
{
    m_socket.bind(endPoint);
}

void NetSocket::Bind(NetAddr endPoint)
{
    m_socket.bind(endPoint);
}

void NetSocket::SendMessage(NetPacket message, NetAddr recipient, ESendOptions options)
{
    auto& conn = GetOrCreateConnection(recipient);
    conn.AddSend(message);
}

bool NetSocket::RecvMessage(NetPacket& message, NetAddr& sender)
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
            std::cout << send.value().size() << std::endl;
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
        NetPacket recv_buf;
        boost::asio::ip::udp::endpoint sender;
        boost::system::error_code error;
        size_t bytes = m_socket.receive_from(boost::asio::buffer(recv_buf),
            sender, 0, error);
        if (error)
        {
            return;
        }
        std::cout << bytes << " " << recv_buf.size() << std::endl;
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
