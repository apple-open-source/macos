/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
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
#   define DoErrorLog( x... ) { FireLog ( "%s %u: ", __FILE__, __LINE__ ); FireLog( "ERROR: " x ) ; }
#   define DoDebugLog( x... ) { FireLog ( "%s %u: ", __FILE__, __LINE__ ); FireLog( x ) ; }
#else
#   define DoErrorLog( x... ) { IOLog ( "%s %u: ", __FILE__, __LINE__ ); IOLog( "ERROR: " x ) ; }
#   define DoDebugLog( x... ) { IOLog ( "%s %u: ", __FILE__, __LINE__ ); IOLog( x ) ; }
#endif

#define ErrorLog(x...) 				DoErrorLog( x ) ;
#define	ErrorLogCond( x, y... )		{ if (x) ErrorLog ( y ) ; }

#if IOFIREWIREDEBUG > 0
#if FIRELOG
#	import <IOKit/firewire/IOFireLog.h>
#endif
#	define DebugLog(x...)			DoDebugLog( x ) ;
#	define DebugLogCond( x, y... ) 	{ if (x) DebugLog ( y ) ; }
#else
#	define DebugLog(x...)
#	define DebugLogCond( x, y... )
#endif

#define TIMEIT( doit, description ) \
{ \
	AbsoluteTime start, end; \
	clock_get_uptime( & start ); \
	{ \
		doit ;\
	}\
	clock_get_uptime( & end ); \
	SUB_ABSOLUTETIME( & end, & start ) ;\
	UInt64 nanos ;\
	absolutetime_to_nanoseconds( end, & nanos ) ;\
	DebugLog("%s duration %llu us\n", "" description, nanos/1000) ;\
}

#define InfoLog(x...) {}

