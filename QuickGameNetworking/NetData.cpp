#include "NetData.h"
#include "NetMessagesBase.h"
#include "NetMessages.h"
#include "NetObjectDescriptor.h"

std::unique_ptr<NetDataFactory> NetDataFactory::ms_instance;

void NetDataFactory::Init()
{
    ms_instance.reset(new NetDataFactory);
    ms_instance->RegisterData();
}

void NetDataFactory::Shutdown()
{
    ms_instance.reset();
}

void NetDataFactory::RegisterData()
{
    RegisterDataContainer<SessionSetupMessage>();
    RegisterDataContainer<TextMessage>();
    RegisterDataContainer<SetMasterRequestMessage>();
    RegisterDataContainer<SetMasterMessage>();
    
    RegisterDataContainer<TextNetObject>();
}