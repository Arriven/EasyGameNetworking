#pragma once
#include "NetMessagesBase.h"
#include "NetSocket.h"
#include <boost/serialization/vector.hpp>

class SessionSetupMessage : public SingletonNetMessageBase
{
    DEFINE_NET_MESSAGE(SessionSetupMessage);

public:
    SessionSetupMessage() = default;
    SessionSetupMessage(std::vector<NetAddr> connections) : m_connections(connections) {}
    std::vector<NetAddr> m_connections;

private:
    friend class boost::serialization::access;
    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & m_connections;
    }
};

class TextMessage : public NetObjectMessageBase
{
    DEFINE_NET_MESSAGE(TextMessage);

public:
    std::string text;

private:
    friend class boost::serialization::access;
    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & text;
    }
};

class SetMasterRequestMessage : public NetObjectMessageBase
{
    DEFINE_NET_MESSAGE(SetMasterRequestMessage);

public:
    NetAddr m_address;

private:
    friend class boost::serialization::access;
    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & m_address;
    }
};

class SetMasterMessage : public NetObjectMessageBase
{
    DEFINE_NET_MESSAGE(SetMasterMessage);

private:
    friend class boost::serialization::access;
    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
    }
};
