/*
 * Configuration options for the CCache Library
 */

/*
 * Include the platform-specific configuration file
 * See CCache.config.mac.h for list of all the thigns that the platform-specific
 * configuration file has to contain
 */
 
#if defined (macintosh) || (defined(__GNUC__) && (defined(__APPLE_CPP__) || defined(__APPLE_CC__) || defined(__NEXT_CPP__)))
#include "CCache.config.mac.h"

#elif	_WIN32
#include "CCache.config.win32.h"

#endif

/*
 * Set CCache_v2_compat to 1 if you want to build the CCAPI v2 compatibility code
 */

#ifndef CCache_v2_compat
#define CCache_v2_compat					1
#endif

/*
 * Set CCache_ContainsSharedStaticData to 1 only when building the part of the CCache
 * library which contains the shared static data (e.g. the server side of an RPC
 * implementation
 */
 
#ifndef CCache_ContainsSharedStaticData
#define	CCache_ContainsSharedStaticData		0
#endif

/*
 * Set CCIMessage_Warning_ to the string you want to appear at the beginning of
 * compiler warnings specified in the code
 */

#ifndef CCIMessage_Warning_
#define CCIMessage_Warning_ "Warning: "
#endif