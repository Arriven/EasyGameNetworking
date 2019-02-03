#pragma once

#include <boost/asio.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/container/flat_map.hpp>
#include <optional>
#include <vector>
#include <chrono>


using NetBuffer = std::vector<char>;
using NetAddr = boost::asio::ip::udp::endpoint;

namespace boost
{
    namespace serialization
    {

        template<class Archive>
        void save(Archive& ar, NetAddr const& addr, unsigned int const version)
        {
            ar & addr.address().to_v4().to_ulong();
            ar & addr.port();
        }

        template<class Archive>
        void load(Archive& ar, NetAddr& addr, unsigned int const version)
        {
            unsigned long address;
            ar & address;
            addr.address(boost::asio::ip::address_v4(address));
            unsigned short port;
            ar & port;
            addr.port(port);
        }

    } // namespace serialization
} // namespace boost

BOOST_SERIALIZATION_SPLIT_FREE(NetAddr)

class NetChannel
{
private:
    std::vector<NetBuffer> m_queue;
    std::chrono::system_clock::time_point m_lastAck;
};

class NetConnection
{
public:
    NetConnection();

    std::optional<NetBuffer> UpdateSend();
    std::optional<NetBuffer> UpdateRecv();

    void AddSend(NetBuffer const& packet);
    void AddRecv(NetBuffer const& packet);

    bool IsConnected() const;

private:
    bool NeedToSendHeartbeat() const;
    bool IsHeartbeat(NetBuffer const& packet) const;
    NetBuffer GetHeartbeatPacket() const;

private:
    std::vector<NetBuffer> m_sendQueue;
    std::vector<NetBuffer> m_recvQueue;
    std::chrono::system_clock::time_point m_lastSentAckTime;
    std::chrono::system_clock::time_point m_lastRecvAckTime;
};

enum class ESendOptions
{
    None = 0,
    Reliable = 1,
    HighPriority = 1 >> 1
};

struct NetConnectionsUpdate
{
    std::vector<NetAddr> m_newConnections;
    std::vector<NetAddr> m_deadConnections;
};

class NetSocket
{
public:
    NetSocket(boost::asio::io_service& io_service);
    NetSocket(boost::asio::io_service& io_service, NetAddr endPoint);

    void SendMessage(NetBuffer message, NetAddr recipient, ESendOptions options);
    std::optional<std::pair<NetBuffer, NetAddr>> RecvMessage();

    void Connect(NetAddr recipient);
    bool IsConnected(NetAddr recipient) const;
    std::vector<NetAddr> GetConnections() const;

    NetConnectionsUpdate Update();

    NetAddr GetLocalAddress() const;

private:
    void ProcessMessages();
    std::vector<NetAddr> KillDeadConnections();
    std::vector<NetAddr> PollNewConnections();

    NetConnection& GetOrCreateConnection(NetAddr recipient);

private:
    boost::container::flat_map<NetAddr, NetConnection> m_connections;
    boost::asio::ip::udp::socket m_socket;
    std::vector<NetAddr> m_newConnections;
};