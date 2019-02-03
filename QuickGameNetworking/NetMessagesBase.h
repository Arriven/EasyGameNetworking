#pragma once
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "NetObjectDescriptor.h"

//TODO: replace with some more advanced hash algorithm like crc32
size_t constexpr HashMessageID(const char* p)
{
    size_t constexpr prime = 31;
    if (*p)
    {
        return (*p) * prime + HashMessageID(++p);
    }
    return 0;
}

class INetMessage
{
public:
    virtual size_t GetMessageID() const = 0;
    virtual void Serialize(boost::archive::binary_oarchive& stream) const = 0;
    virtual void Deserialize(boost::archive::binary_iarchive& stream) = 0;
};

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
    void SetDescriptor(std::unique_ptr<NetObjectDescriptor>&& descriptor) { m_descriptor = std::move(descriptor); }
    NetObjectDescriptor const& GetDescriptor() const { return *m_descriptor; }

    virtual void SerializeData(boost::archive::binary_oarchive& stream) const = 0;
    virtual void DeserializeData(boost::archive::binary_iarchive& stream) = 0;

private:
    virtual void Serialize(boost::archive::binary_oarchive& stream) const override;
    virtual void Deserialize(boost::archive::binary_iarchive& stream) override;

private:
    std::unique_ptr<NetObjectDescriptor> m_descriptor;
};

#define DEFINE_NET_MESSAGE(NetMessageType) \
public: \
    static constexpr size_t MessageID = HashMessageID(#NetMessageType); \
    virtual size_t GetMessageID() const override { return MessageID; } \
    virtual void SerializeData(boost::archive::binary_oarchive& stream) const override { stream << *this; } \
    virtual void DeserializeData(boost::archive::binary_iarchive& stream) override { stream >> *this; } \