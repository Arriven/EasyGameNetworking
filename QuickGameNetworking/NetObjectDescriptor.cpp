#include "NetObjectDescriptor.h"

std::unique_ptr<NetObjectDescriptorDataFactory> NetObjectDescriptorDataFactory::ms_instance;

void NetObjectDescriptorDataFactory::Init()
{
    ms_instance.reset(new NetObjectDescriptorDataFactory);
    ms_instance->RegisterTypes();
}

void NetObjectDescriptorDataFactory::Shutdown()
{
    ms_instance.reset();
}

std::unique_ptr<INetObjectDescriptorData> NetObjectDescriptorDataFactory::CreateData(size_t const typeID)
{
    auto factory = m_dataFactory[typeID];
    if (!factory)
    {
        return nullptr;
    }
    return factory();
}

void NetObjectDescriptorDataFactory::RegisterTypes()
{
    RegisterType<TextNetObject>();
}
