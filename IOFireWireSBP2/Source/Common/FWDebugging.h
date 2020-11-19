/*
 *  FWDebugging.h
 *  SBP2TestApp
 *
 *  Created by cpieper on Mon Nov 06 2000.
 *  Copyright (c) 2000 __CompanyName__. All rights reserved.
 *
 */

// the controls 

#define FWLOGGING 0
#define FWASSERTS 0

#define FWDIAGNOSTICS 0
#define LSILOGGING 0
#define LSIALLOCLOGGING 0
#define PANIC_ON_DOUBLE_APPEND 0

///////////////////////////////////////////

#if FWLOGGING
#define FWLOG(x) printf x
#else
#define FWLOG(x) do {} while (0)
#endif

#if FWLOGGING
#define FWKLOG(x) IOLog x
#else
#define FWKLOG(x) do {} while (0)
#endif

#if FWASSERTS
#define FWKLOGASSERT(a) { if(!(a)) { IOLog( "File %s, line %d: assertion '%s' failed.\n", __FILE__, __LINE__, #a); } }
#else
#define FWKLOGASSERT(a) do {} while (0)
#endif

#if FWASSERTS
#define FWLOGASSERT(a) { if(!(a)) { printf( "File %s, line %d: assertion '%s' failed.\n", __FILE__, __LINE__, #a); } }
#else
#define FWLOGASSERT(a) do {} while (0)
#endif

#if FWASSERTS
#define FWPANICASSERT(a) { if(!(a)) { panic( "File %s, line %d: assertion '%s' failed.\n", __FILE__, __LINE__, #a); } }
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
