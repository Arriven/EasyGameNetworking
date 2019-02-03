#include "NetMessagesBase.h"

void NetObjectMessage::Serialize(boost::archive::binary_oarchive& stream) const
{
    stream << m_descriptor->m_typeID;
    stream << m_descriptor->m_instanceID;
    SerializeData(stream);
}

void NetObjectMessage::Deserialize(boost::archive::binary_iarchive& stream)
{
    m_descriptor.reset(new NetObjectDescriptor);
    stream >> m_descriptor->m_typeID;
    stream >> m_descriptor->m_instanceID;
    DeserializeData(stream);
}