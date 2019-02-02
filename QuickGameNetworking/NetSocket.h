#pragma once

#include <boost/asio.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <optional>
#include <vector>
#include <chrono>


using NetAddr = boost::asio::ip::udp::endpoint;
using NetBuffer = std::vector<char>;

class NetChannel
{
private:
    std::vector<NetBuffer> m_queue;
    std::chrono::system_clock::time_point m_lastAck;
};

class NetConnection
{
public:
    NetConnection(NetAddr const& recepient);

    std::optional<NetBuffer> UpdateSend();
    std::optional<NetBuffer> UpdateRecv();

    void AddSend(NetBuffer const& packet);
    void AddRecv(NetBuffer const& packet);

    bool IsConnected() const;
    NetAddr const& GetEndPoint() const { return m_endPoint; }

private:
    bool NeedToSendHeartbeat() const;
    bool IsHeartbeat(NetBuffer const& packet) const;
    NetBuffer GetHeartbeatPacket() const;

private:
    std::vector<NetBuffer> m_sendQueue;
    std::vector<NetBuffer> m_recvQueue;
    std::chrono::system_clock::time_point m_lastSentAckTime;
    std::chrono::system_clock::time_point m_lastRecvAckTime;
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

    void SendMessage(NetBuffer message, NetAddr recipient, ESendOptions options);
    bool RecvMessage(NetBuffer& message, NetAddr& sender);

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