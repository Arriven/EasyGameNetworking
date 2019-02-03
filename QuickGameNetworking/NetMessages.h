#pragma once
#include "NetAPI.h"

class TextMessage : public NetObjectMessage
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

class SetMasterRequestMessage : public NetObjectMessage
{
    DEFINE_NET_MESSAGE(SetMasterRequestMessage);

public:
    unsigned long m_address;
    unsigned short m_usPort;

private:
    friend class boost::serialization::access;
    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & m_address;
        ar & m_usPort;
    }
};

class SetMasterMessage : public NetObjectMessage
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
