/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
 
 
#import "FWTracepoints.h"
 
// the controls 

#define FWLOGGING 0
#define FWASSERTS 1

///////////////////////////////////////////

#if FWLOGGING
#define FWKLOG(x) IOLog x
#else
#define FWKLOG(x) do {} while (0)
#endif

#if FWLOCALLOGGING
#define FWLOCALKLOG(x) IOLog x
#else
#define FWLOCALKLOG(x) do {} while (0)
#endif

#if FWASSERTS
#define FWKLOGASSERT(a) { if(!(a)) { IOLog( "File "__FILE__", line %d: assertion '%s' failed.\n", __LINE__, #a); } }
#else
#define FWKLOGASSERT(a) do {} while (0)
#endif

#if FWASSERTS
#define FWPANICASSERT(a) { if(!(a)) { panic( "File "__FILE__", line %d: assertion '%s' failed.\n", __LINE__, #a); } }
#else
#define FWPANICASSERT(a) do {} while (0)
#endif

#if FWASSERTS
#define FWASSERTINGATE(a) { if(!((a)->inGate())) { IOLog( "File "__FILE__", line %d: warning - workloop lock is not held.\n", __LINE__); } }
#else
#define FWASSERTINGATE(a) do {} while (0)
#endif

#if FIRELOG > 0
#   define DoErrorLog( x... ) { FireLog( "ERROR: " x ) ; }
#   define DoDebugLog( x... ) { FireLog( x ) ; }
#else
#   define DoErrorLog( x... ) { IOLog( "ERROR: " x ) ; }
#   define DoDebugLog( x... ) { IOLog( x ) ; }
#endif

#define ErrorLog(x...) 				DoErrorLog( x ) ;
#define	ErrorLogCond( x, y... )		{ if (x) ErrorLog ( y ) ; }

#if IOFIREWIREDEBUG > 0
#if FIRELOG
#	import <IOKit/firewire/FireLog.h>
#endif
#	define DebugLog(x...)			DoDebugLog( x ) ;
#	define DebugLogCond( x, y... ) 	{ if (x) DebugLog ( y ) ; }
#else
#	define DebugLog(x...)			do {} while (0)
#	define DebugLogCond( x, y... )	do {} while (0)
#endif

#define TIMEIT( doit, description ) \
{ \
	AbsoluteTime start, end; \
	IOFWGetAbsoluteTime( & start ); \
	{ \
		doit ;\
	}\
	IOFWGetAbsoluteTime( & end ); \
	SUB_ABSOLUTETIME( & end, & start ) ;\
	UInt64 nanos ;\
	absolutetime_to_nanoseconds( end, & nanos ) ;\
	DebugLog("%s duration %llu us\n", "" description, nanos/1000) ;\
}

#define InfoLog(x...) do {} while (0)


