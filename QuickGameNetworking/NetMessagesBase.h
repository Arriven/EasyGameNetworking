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
public: \
    static constexpr size_t MessageID = HashMessageID(#NetMessageType); \
    virtual size_t GetMessageID() const override { return MessageID; } \
    virtual void SerializeData(boost::archive::binary_oarchive& stream) const override { stream << *this; } \
    virtual void DeserializeData(boost::archive::binary_iarchive& stream) override { stream >> *this; } \

class NetMessageFactory
{
public:
    static void Init();
    static void Shutdown();
    static NetMessageFactory* GetInstance() { return ms_instance.get(); }

    template<typename T> void RegisterMessage();
    template<typename T> bool IsMessageRegistered();

    std::unique_ptr<INetMessage> CreateMessage(size_t const messageID);

private:
    NetMessageFactory() = default;
    NetMessageFactory(NetMessageFactory const& other) = delete;

    void RegisterMessages();

    std::unordered_map <size_t, std::function<std::unique_ptr<INetMessage>()>> m_messageFactory;

    static std::unique_ptr<NetMessageFactory> ms_instance;
};

template<typename T>
void NetMessageFactory::RegisterMessage()
{
    static_assert(std::is_base_of<INetMessage, T>::value, "Can be called only for classes derived from NetMessage");
    m_messageFactory[T::MessageID] = []() { return std::make_unique<T>(); };
}

template<typename T>
bool NetMessageFactory::IsMessageRegistered()
{
    static_assert(std::is_base_of<INetMessage, T>::value, "Can be called only for classes derived from NetMessage");

    return m_messageFactory.find(T::MessageID) != m_messageFactory.end();
}