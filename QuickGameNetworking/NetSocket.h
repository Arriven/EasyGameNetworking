#pragma once

#include <boost/asio.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/detail/bitmask.hpp>
#include <optional>
#include <vector>
#include <chrono>


using NetData = std::vector<char>;
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

BOOST_SERIALIZATION_SPLIT_FREE(NetAddr);

enum class ESendOptions
{
    None = 0,
    Reliable = 1,
};

BOOST_BITMASK(ESendOptions);

struct NetPacket
{
    NetPacket() = default;
    NetPacket(NetData const& data, ESendOptions const options, size_t const ack) : m_data(data), m_options(options), m_ack(ack) {}

    NetData Serialize() const;
    static NetPacket Deserialize(NetData const& data);

    bool NeedsResend() const;
    void UpdateSendTime();

    NetData m_data;
    ESendOptions m_options;
    size_t m_ack;
    std::chrono::system_clock::time_point m_lastSentTime;
};

class NetConnection
{
public:
    NetConnection();

    std::optional<NetData> UpdateSend();
    std::optional<NetData> UpdateRecv();

    void AddSend(NetData const& data, ESendOptions const options);
    void AddRecv(NetData const& data);

    bool IsConnected() const;

private:
    bool NeedToSendHeartbeat() const;
    bool IsHeartbeat(NetData const& packet) const;
    NetData GetHeartbeatPacket() const;
    bool IsAck(NetData const& packet) const;
    NetData GetAckPacket(size_t const ack) const;
    size_t GetAck(NetData const& packet) const;

private:
    std::vector<NetPacket> m_sendQueue;
    std::vector<NetPacket> m_recvQueue;
    std::vector<NetData> m_ackQueue;
    std::chrono::system_clock::time_point m_lastSendTime;
    std::chrono::system_clock::time_point m_lastRecvTime;
    size_t m_lastSendAck = 0;
    size_t m_lastRecvAck = 1;
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

    void SendMessage(NetData message, NetAddr recipient, ESendOptions options);
    std::optional<std::pair<NetData, NetAddr>> RecvMessage();

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