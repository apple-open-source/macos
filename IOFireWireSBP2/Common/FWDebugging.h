/*
 *  FWDebugging.h
 *  SBP2TestApp
 *
 *  Created by cpieper on Mon Nov 06 2000.
 *  Copyright (c) 2000 __CompanyName__. All rights reserved.
 *
 */

// the controls 

#define USEFIRELOG 0
#define FWDIAGNOSTICS 0
#define FWLOGGING 0
#define LSILOGGING 0
#define LSIALLOCLOGGING 0
#define PANIC_ON_DOUBLE_APPEND 0

///////////////////////////////////////////

#if USEFIRELOG
	#if KERNEL
	#include <IOKit/firewire/IOFireLog.h>
	#endif
#endif

#if FWLOGGING
#define FWLOG(x) printf x
#else
#define FWLOG(x) do {} while (0)
#endif

#if FWLOGGING
	#if USEFIRELOG
	#define FWKLOG(x) FireLog x
	#else
	#define FWKLOG(x) IOLog x
	#endif
#else
#define FWKLOG(x) do {} while (0)
#endif

#if FWLOGGING
#define FWKLOGASSERT(a) { if(!(a)) { IOLog( "File "__FILE__", line %d: assertion '%s' failed.\n", __LINE__, #a); } }
#else
#define FWKLOGASSERT(a) do {} while (0)
#endif

#if FWLOGGING
#define FWLOGASSERT(a) { if(!(a)) { printf( "File "__FILE__", line %d: assertion '%s' failed.\n", __LINE__, #a); } }
#else
#define FWLOGASSERT(a) do {} while (0)
#endif

#if FWLOGGING
#define FWPANICASSERT(a) { if(!(a)) { panic( "File "__FILE__", line %d: assertion '%s' failed.\n", __LINE__, #a); } }
#else
#define FWPANICASSERT(a) do {} while (0)
#endif

#if LSILOGGING
#define FWLSILOG(x) FWKLOG(x)
#else
#define FWLSILOG(x) do {} while (0)
#endif

#if LSIALLOCLOGGING
#define FWLSILOGALLOC(x) FWKLOG(x)
#else
#define FWLSILOGALLOC(x) do {} while (0)
#endif