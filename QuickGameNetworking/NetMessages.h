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

class MementoUpdateMessage : public NetObjectMessageBase
{
    DEFINE_NET_CONTAINER(MementoUpdateMessage);
public:
    MementoUpdateMessage() = default;
    MementoUpdateMessage(std::unique_ptr<INetData>&& data)
        : m_data(std::move(data))
    {
    }
    MementoUpdateMessage(MementoUpdateMessage const& other)
        : NetObjectMessageBase(other)
        , m_data(other.m_data->Clone())
    {
    }

    INetData const* GetData() const { return m_data.get(); }

private:
    virtual void SerializeData(boost::archive::binary_oarchive& stream) const override
    {
        stream << m_data->GetTypeID();
        m_data->Serialize(stream);
    }
    virtual void DeserializeData(boost::archive::binary_iarchive& stream) override
    {
        size_t typeId;
        stream >> typeId;
        m_data = NetDataFactory::GetInstance()->CreateDataContainer(typeId);
        assert(m_data);
        if (!m_data)
        {
            return;
        }
        m_data->Deserialize(stream);
    }

    std::unique_ptr<INetData> m_data;
};

class TextMemento : public INetData
{
    DEFINE_NET_CONTAINER(TextMemento);

public:
    std::string text;

private:
    virtual void Serialize(boost::archive::binary_oarchive& stream) const override { stream << text; }
    virtual void Deserialize(boost::archive::binary_iarchive& stream) override { stream >> text; }
};
