#pragma once
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "NetObjectDescriptor.h"
#include "NetData.h"

using INetMessage = INetData;

class SingletonNetMessageBase : public INetMessage
{
    virtual void SerializeData(boost::archive::binary_oarchive& stream) const = 0;
    virtual void DeserializeData(boost::archive::binary_iarchive& stream) = 0;

private:
    virtual void Serialize(boost::archive::binary_oarchive& stream) const override;
    virtual void Deserialize(boost::archive::binary_iarchive& stream) override;
};

class NetObjectMessageBase : public INetMessage
{
public:
    void SetDescriptor(NetObjectDescriptor const& descriptor) { m_descriptor = descriptor; }
    NetObjectDescriptor const& GetDescriptor() const { return m_descriptor; }

    virtual void SerializeData(boost::archive::binary_oarchive& stream) const = 0;
    virtual void DeserializeData(boost::archive::binary_iarchive& stream) = 0;

private:
    virtual void Serialize(boost::archive::binary_oarchive& stream) const override;
    virtual void Deserialize(boost::archive::binary_iarchive& stream) override;

private:
    NetObjectDescriptor m_descriptor;
};

#define DEFINE_NET_MESSAGE(NetMessageType) \
DEFINE_NET_CONTAINER(NetMessageType) \
public: \
    virtual void SerializeData(boost::archive::binary_oarchive& stream) const override { stream << *this; } \
    virtual void DeserializeData(boost::archive::binary_iarchive& stream) override { stream >> *this; } \