#pragma once
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/split_member.hpp>

//TODO: replace with some more advanced hash algorithm like crc32
size_t constexpr HashTypeID(const char* p)
{
    size_t constexpr prime = 31;
    if (*p)
    {
        return (*p) * prime + HashTypeID(++p);
    }
    return 0;
}

class INetObjectDescriptorData
{
public:
    virtual size_t GetTypeID() const = 0;
    virtual void Serialize(boost::archive::binary_oarchive& stream) const = 0;
    virtual void Deserialize(boost::archive::binary_iarchive& stream) = 0;
    virtual std::unique_ptr<INetObjectDescriptorData> Clone() const = 0;
    virtual bool operator==(INetObjectDescriptorData const& other) = 0;
};

#define DEFINE_NET_DESCRIPTOR_DATA(NetDescriptorDataType) \
public: \
    static constexpr size_t TypeID = HashTypeID(#NetDescriptorDataType); \
    virtual size_t GetTypeID() const override { return TypeID; } \
    virtual void Serialize(boost::archive::binary_oarchive& stream) const override { stream << *this; } \
    virtual void Deserialize(boost::archive::binary_iarchive& stream) override { stream >> *this; } \
    virtual std::unique_ptr<INetObjectDescriptorData> Clone() const { return std::make_unique<NetDescriptorDataType>(*this);}

#define DEFINE_EMPTY_NET_DESCRIPTOR_DATA(NetDescriptorDataType) \
class NetDescriptorDataType : public INetObjectDescriptorData \
{ \
    DEFINE_NET_DESCRIPTOR_DATA(NetDescriptorDataType); \
public: \
    virtual bool operator==(INetObjectDescriptorData const& other) { return true; }; \
private: \
    friend class boost::serialization::access;\
    template<class Archive> void serialize(Archive & ar, const unsigned int version) {} \
}; \

DEFINE_EMPTY_NET_DESCRIPTOR_DATA(EmptyDescriptorData);

class NetObjectDescriptorDataFactory
{
public:
    static void Init();
    static void Shutdown();
    static NetObjectDescriptorDataFactory* GetInstance() { return ms_instance.get(); }

    template<typename T> void RegisterType();
    template<typename T> bool IsTypeRegistered();

    std::unique_ptr<INetObjectDescriptorData> CreateData(size_t const typeID);

private:
    NetObjectDescriptorDataFactory() = default;
    NetObjectDescriptorDataFactory(NetObjectDescriptorDataFactory const& other) = delete;

    void RegisterTypes();

    std::unordered_map <size_t, std::function<std::unique_ptr<INetObjectDescriptorData>()>> m_dataFactory;

    static std::unique_ptr<NetObjectDescriptorDataFactory> ms_instance;
};

template<typename T>
void NetObjectDescriptorDataFactory::RegisterType()
{
    static_assert(std::is_base_of<INetObjectDescriptorData, T>::value, "Can be called only for classes derived from INetObjectDescriptorData");
    m_dataFactory[T::TypeID] = []() { return std::make_unique<T>(); };
}

template<typename T>
bool NetObjectDescriptorDataFactory::IsTypeRegistered()
{
    static_assert(std::is_base_of<INetObjectDescriptorData, T>::value, "Can be called only for classes derived from INetObjectDescriptorData");

    return m_dataFactory.find(T::MessageID) != m_dataFactory.end();
}

class NetObjectDescriptor
{
public:
    NetObjectDescriptor() = default; //Invalid state
    NetObjectDescriptor(std::unique_ptr<INetObjectDescriptorData>&& data) : m_data(std::move(data)) {}
    NetObjectDescriptor(NetObjectDescriptor const& other) : m_data(other.m_data->Clone()) {}
    NetObjectDescriptor& operator=(NetObjectDescriptor const& other) { m_data = other.m_data->Clone(); return *this; }

    template<typename NetObjectDescriptorDataType, typename... ARGS>
    static NetObjectDescriptor Create(ARGS... args);

    size_t GetTypeID() const { return m_data->GetTypeID(); }

    bool operator==(NetObjectDescriptor const& other) const { return GetTypeID() == other.GetTypeID() && *m_data == *other.m_data; }

private:
    std::unique_ptr<INetObjectDescriptorData> m_data;

    friend class boost::serialization::access;

    template<class Archive>
    void save(Archive& ar, unsigned int const version) const
    {
        ar << m_data->GetTypeID();
        m_data->Serialize(ar);
    }

    template<class Archive>
    void load(Archive& ar, unsigned int const version)
    {
        size_t typeID;
        ar >> typeID;
        m_data = NetObjectDescriptorDataFactory::GetInstance()->CreateData(typeID);
        m_data->Deserialize(ar);
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

template<typename NetObjectDescriptorDataType, typename... ARGS>
NetObjectDescriptor NetObjectDescriptor::Create(ARGS... args)
{
    return NetObjectDescriptor(std::make_unique<NetObjectDescriptorDataType>(std::forward<ARGS>(args)...));
}

namespace std
{
    template<>
    class hash<NetObjectDescriptor>
    {
    public:
        size_t operator()(NetObjectDescriptor const& descr) const
        {
            size_t data = descr.GetTypeID();
            return std::hash<size_t>()(data);
        }
    };
}
