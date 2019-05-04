#pragma once
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

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


class INetData
{
public:
    virtual size_t GetTypeID() const = 0;
    virtual void Serialize(boost::archive::binary_oarchive& stream) const = 0;
    virtual void Deserialize(boost::archive::binary_iarchive& stream) = 0;
    virtual std::unique_ptr<INetData> Clone() const = 0;
    virtual void CopyFrom(INetData const* other) = 0;
};

#define DEFINE_NET_CONTAINER(Type) \
public: \
    static constexpr size_t TypeID = HashTypeID(#Type); \
    virtual size_t GetTypeID() const override { return TypeID; } \
    virtual std::unique_ptr<INetData> Clone() const { return std::make_unique<Type>(*this);} \
    virtual void CopyFrom(INetData const* other) {assert(GetTypeID() == other->GetTypeID()); this->~Type(); new (this) Type(*static_cast<Type const*>(other));}

class NetDataFactory
{
public:
    static void Init();
    static void Shutdown();
    static NetDataFactory* GetInstance() { return ms_instance.get(); }

    template<typename T> void RegisterDataContainer();
    template<typename T> bool IsDataContainerRegistered();

    template<typename T = INetData>
    std::unique_ptr<T> CreateDataContainer(size_t const typeID);

private:
    NetDataFactory() = default;
    NetDataFactory(NetDataFactory const& other) = delete;

    void RegisterData();

    std::unordered_map <size_t, std::function<std::unique_ptr<INetData>()>> m_typeFactory;

    static std::unique_ptr<NetDataFactory> ms_instance;
};

template<typename T>
void NetDataFactory::RegisterDataContainer()
{
    static_assert(std::is_base_of<INetData, T>::value, "Can be called only for classes derived from NetMessage");
    m_typeFactory[T::TypeID] = []() { return std::make_unique<T>(); };
}

template<typename T>
bool NetDataFactory::IsDataContainerRegistered()
{
    static_assert(std::is_base_of<INetData, T>::value, "Can be called only for classes derived from NetMessage");

    return m_typeFactory.find(T::TypeID) != m_typeFactory.end();
}

template<typename T>
std::unique_ptr<T> NetDataFactory::CreateDataContainer(size_t const typeID)
{
    auto factory = m_typeFactory[typeID];
    if (!factory)
    {
        return nullptr;
    }
    return std::unique_ptr<T>(static_cast<T*>(factory().release()));
}