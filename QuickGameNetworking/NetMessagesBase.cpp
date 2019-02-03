#include "NetMessagesBase.h"

void SingletonNetMessageBase::Serialize(boost::archive::binary_oarchive& stream) const
{
    SerializeData(stream);
}

void SingletonNetMessageBase::Deserialize(boost::archive::binary_iarchive& stream)
{
    DeserializeData(stream);
}

void NetObjectMessageBase::Serialize(boost::archive::binary_oarchive& stream) const
{
    stream << m_descriptor->m_typeID;
    stream << m_descriptor->m_instanceID;
    SerializeData(stream);
}

void NetObjectMessageBase::Deserialize(boost::archive::binary_iarchive& stream)
{
    m_descriptor.reset(new NetObjectDescriptor);
    stream >> m_descriptor->m_typeID;
    stream >> m_descriptor->m_instanceID;
    DeserializeData(stream);
}