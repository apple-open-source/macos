#include "UniqueGlobally.h"
#include "CCacheData.h"
#include "ContextData.h"
#include "CredentialsData.h"

#if CCache_ContainsSharedStaticData
CCISharedStaticData <CCIUniqueGlobally <CCICCacheData>::Globals>	CCIUniqueGlobally <CCICCacheData>::sGlobals;
CCISharedStaticData <CCIUniqueGlobally <CCIContextData>::Globals>	CCIUniqueGlobally <CCIContextData>::sGlobals;
CCISharedStaticData <CCIUniqueGlobally <CCICredentialsData>::Globals>	CCIUniqueGlobally <CCICredentialsData>::sGlobals;
#endif

CCISharedStaticDataProxy <CCIUniqueGlobally <CCICCacheData>::Globals>	CCIUniqueGlobally <CCICCacheData>::sGlobalsProxy = CCIUniqueGlobally <CCICCacheData>::sGlobals;
CCISharedStaticDataProxy <CCIUniqueGlobally <CCIContextData>::Globals>	CCIUniqueGlobally <CCIContextData>::sGlobalsProxy = CCIUniqueGlobally <CCIContextData>::sGlobals;
CCISharedStaticDataProxy <CCIUniqueGlobally <CCICredentialsData>::Globals>	CCIUniqueGlobally <CCICredentialsData>::sGlobalsProxy = CCIUniqueGlobally <CCICredentialsData>::sGlobals;
