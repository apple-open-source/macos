#include "ConcreteFactory.h"

CCIAbstractFactory*		CCIAbstractFactory::sTheFactory = NULL;

CCIAbstractFactory* CCIAbstractFactory::GetTheFactory ()
{
	if (sTheFactory == NULL) {
		sTheFactory = MakeFactory ();
	} 
	
	return sTheFactory;
}


#if TARGET_RT_MAC_MACHO

#include "ContextDataMachIPCStubs.h"
#include "CCacheDataMachIPCStubs.h"
#include "CredsDataMachIPCStubs.h"

CCIAbstractFactory*		CCIAbstractFactory::MakeFactory ()
{
	return new CCIConcreteFactory <CCIContextDataMachIPCStub, CCICCacheDataMachIPCStub, CCICredentialsDataMachIPCStub, CCICompatCredentials> ();
}

#elif TARGET_RT_MAC_CFM && CCacheMacOSClassicImplementation

#include "ContextDataClassicStubs.h"
#include "CCacheDataClassicStubs.h"
#include "CredsDataClassicStubs.h"

CCIAbstractFactory*		CCIAbstractFactory::MakeFactory ()
{
	return new CCIConcreteFactory <CCIContextDataClassicStub, CCICCacheDataClassicStub, CCICredentialsDataClassicStub, CCICompatCredentials> ();
}

#elif TARGET_RT_MAC_CFM

#include "ContextDataCallStubs.h"
#include "CCacheDataCallStubs.h"
#include "CredentialsDataCallStubs.h"

CCIAbstractFactory*		CCIAbstractFactory::MakeFactory ()
{
	return new CCIConcreteFactory <CCIContextDataCallStub, CCICCacheDataCallStub, CCICredentialsDataCallStub, CCICompatCredentials> ();
}

#else

#error Unknown target type

#endif