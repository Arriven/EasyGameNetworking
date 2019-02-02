#pragma once

#include <boost/asio.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <optional>
#include <vector>
#include <chrono>


using NetAddr = boost::asio::ip::udp::endpoint;
using NetPacket = std::vector<char>;

class NetChannel
{
private:
    std::vector<NetPacket> m_queue;
    std::chrono::system_clock::time_point m_lastAck;
};

class NetConnection
{
public:
    NetConnection(NetAddr const& recepient);

    std::optional<NetPacket> UpdateSend();
    std::optional<NetPacket> UpdateRecv();

    void AddSend(NetPacket const& packet);
    void AddRecv(NetPacket const& packet);

    bool IsConnected() const;
    NetAddr const& GetEndPoint() const { return m_endPoint; }

private:
    bool NeedToSendHeartbeat() const;
    bool IsHeartbeat(NetPacket const& packet) const;
    NetPacket GetHeartbeatPacket() const;

private:
    std::vector<NetPacket> m_sendQueue;
    std::vector<NetPacket> m_recvQueue;
    std::chrono::system_clock::time_point m_lastSentAck;
    std::chrono::system_clock::time_point m_lastRecvAck;
    NetAddr m_endPoint;
};

enum class ESendOptions
{
    None = 0,
    Reliable = 1,
    HighPriority = 1 >> 1
};

class NetSocket
{
public:
    NetSocket(boost::asio::io_service& io_service);
    NetSocket(boost::asio::io_service& io_service, NetAddr endPoint);
    void Bind(NetAddr endPoint);

    void SendMessage(NetPacket message, NetAddr recipient, ESendOptions options);
    bool RecvMessage(NetPacket& message, NetAddr& sender);

    void Update();

    NetAddr GetLocalAddress() const;

    void SetOnConnectionAddedCallback(std::function<void(NetAddr)> callback);
    void SetOnConnectionRemovedCallback(std::function<void(NetAddr)> callback);

private:
    void ProcessMessages();
    void KillDeadConnections();

    NetConnection& GetOrCreateConnection(NetAddr recipient);

private:
    std::vector<NetConnection> m_connections;
    boost::asio::ip::udp::socket m_socket;

    std::function<void(NetAddr)> m_onConnectionAdded;
    std::function<void(NetAddr)> m_onConnectionRemoved;
};