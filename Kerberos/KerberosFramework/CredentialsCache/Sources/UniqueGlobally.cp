#include "UniqueGlobally.h"
#include "CCacheData.h"
#include "ContextData.h"
#include "CredentialsData.h"

// gcc 4.0 and later need a different syntax to force instantiation

#if defined (__GNUC__) && (__GNUC__ > 3)

#if CCache_ContainsSharedStaticData
template <class T> CCISharedStaticData      <struct CCIUniqueGlobally <T>::Globals> CCIUniqueGlobally <T>::sGlobals;
#endif
template <class T> CCISharedStaticDataProxy <struct CCIUniqueGlobally <T>::Globals> CCIUniqueGlobally <T>::sGlobalsProxy = sGlobals;

#else

#if CCache_ContainsSharedStaticData
CCISharedStaticData <CCIUniqueGlobally <CCICCacheData>::Globals> CCIUniqueGlobally <CCICCacheData>::sGlobals;
CCISharedStaticData <CCIUniqueGlobally <CCIContextData>::Globals> CCIUniqueGlobally <CCIContextData>::sGlobals;
CCISharedStaticData <CCIUniqueGlobally <CCICredentialsData>::Globals> CCIUniqueGlobally <CCICredentialsData>::sGlobals;
#endif

CCISharedStaticDataProxy <CCIUniqueGlobally <CCICCacheData>::Globals> CCIUniqueGlobally <CCICCacheData>::sGlobalsProxy = CCIUniqueGlobally <CCICCacheData>::sGlobals;
CCISharedStaticDataProxy <CCIUniqueGlobally <CCIContextData>::Globals> CCIUniqueGlobally <CCIContextData>::sGlobalsProxy = CCIUniqueGlobally <CCIContextData>::sGlobals;
CCISharedStaticDataProxy <CCIUniqueGlobally <CCICredentialsData>::Globals> CCIUniqueGlobally <CCICredentialsData>::sGlobalsProxy = CCIUniqueGlobally <CCICredentialsData>::sGlobals;

#endif
