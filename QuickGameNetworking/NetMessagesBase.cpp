#include "NetMessagesBase.h"
#include "NetMessages.h"

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
    stream << m_descriptor;
    SerializeData(stream);
}

void NetObjectMessageBase::Deserialize(boost::archive::binary_iarchive& stream)
{
    stream >> m_descriptor;
    DeserializeData(stream);
}