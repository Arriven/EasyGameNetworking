#pragma once

struct NetObjectDescriptor
{
    unsigned char m_typeID;
    unsigned short m_instanceID;

    bool operator==(NetObjectDescriptor const& other) const { return m_instanceID == other.m_instanceID && m_typeID == other.m_typeID; }
};

namespace std
{
    template<>
    class hash<NetObjectDescriptor>
    {
    public:
        size_t operator()(NetObjectDescriptor const& descr) const
        {
            size_t data = descr.m_typeID << sizeof(unsigned short) | descr.m_instanceID;
            return std::hash<size_t>()(data);
        }
    };
}
