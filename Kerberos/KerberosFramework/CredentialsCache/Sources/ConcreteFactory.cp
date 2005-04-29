#include "CCache.config.h"

#include "ConcreteFactory.h"

#if TARGET_RT_MAC_MACHO

#include "ContextDataMachIPCStubs.h"
#include "CCacheDataMachIPCStubs.h"
#include "CredsDataMachIPCStubs.h"

CCIAbstractFactory*		CCIAbstractFactory::sTheFactory =
	new CCIConcreteFactory <CCIContextDataMachIPCStub, CCICCacheDataMachIPCStub, CCICredentialsDataMachIPCStub, CCICompatCredentials> ();

#elif TARGET_RT_MAC_CFM && CCacheUsesAppleEvents

#include "ContextDataAEStubs.h"
#include "CCacheDataAEStubs.h"
#include "CredentialsDataAEStubs.h"

CCIAbstractFactory*		CCIAbstractFactory::sTheFactory = 
	new CCIConcreteFactory <CCIContextDataAEStub, CCICCacheDataAEStub, CCICredentialsDataAEStub, CCICompatCredentials> ();

#elif TARGET_RT_MAC_CFM

#include "ContextDataCallStubs.h"
#include "CCacheDataCallStubs.h"
#include "CredentialsDataCallStubs.h"

CCIAbstractFactory*		CCIAbstractFactory::sTheFactory =
	new CCIConcreteFactory <CCIContextDataCallStub, CCICCacheDataCallStub, CCICredentialsDataCallStub, CCICompatCredentials> ();

#else

#error Unknown target type

#endif