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


std::unique_ptr<NetMessageFactory> NetMessageFactory::ms_instance;

void NetMessageFactory::Init()
{
    ms_instance.reset(new NetMessageFactory);
    ms_instance->RegisterMessages();
}

void NetMessageFactory::Shutdown()
{
    ms_instance.reset();
}

std::unique_ptr<INetMessage> NetMessageFactory::CreateMessage(size_t const messageID)
{
    auto factory = m_messageFactory[messageID];
    if (!factory)
    {
        return nullptr;
    }
    return factory();
}

void NetMessageFactory::RegisterMessages()
{
    RegisterMessage<SessionSetupMessage>();
    RegisterMessage<TextMessage>();
    RegisterMessage<SetMasterRequestMessage>();
    RegisterMessage<SetMasterMessage>();
}
