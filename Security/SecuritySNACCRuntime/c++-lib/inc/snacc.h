/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * file: snacc.h
 *
 *
 * ------------------------------------------------------------------------
 * -  J.G. Van Dyke & Associates, Inc. Modification History of SNACC 1.3  -
 * ------------------------------------------------------------------------
 * 
 * All modification are relative to the v1.3 of SNACC.  We used SunOS 4.1.3's 
 * SCCS.  The revision #'s start at 1.1,  which is the original version from 
 * SNACC 1.3.
 * 
 * 
 * ../SCCS/s.snacc.h:
 * 
 * D 1.3	98/04/24 22:30:19 pleonber	3 2	00002/00000/00157
 * added INSERT_VDA_COMMENTS comment for script that adds SCCS history.
 * 
 * D 1.2	97/11/07 08:01:08 pleonber	2 1	00004/00000/00153
 * added #ifndef _gVDADER_RULES extern int gVDADER_RULES #endif
 * 
 * D 1.1	97/10/30 13:09:44 cmmaster	1 0	00153/00000/00000
 * date and time created 97/10/30 13:09:44 by cmmaster
 * 
 * ----------------------- End of VDA Modifications ---------------------------
 * 
 * 
 * 
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/inc/snacc.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
 * $Log: snacc.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:06  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:18  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1  2000/06/15 18:47:21  dmitch
 * Files duplicated or copied from elsewhere. See Apple_README for gory details.
 *
 * Revision 1.1.1.1  2000/03/09 01:00:04  rmurphy
 * Base Fortissimo Tree
 *
 * Revision 1.1  1999/02/25 05:32:31  mb
 * Added public headers.
 *
 * Revision 1.7  1997/04/07 13:13:18  wan
 * Made more C++ readable (credits to Steve Walker)
 *
 * Revision 1.6  1997/02/28 13:39:35  wan
 * Modifications collected for new version 1.3: Bug fixes, tk4.2.
 *
 * Revision 1.5  1997/02/15 20:38:48  rj
 * In member functions, return *this after calling abort() for stupid compilers that don't seem to know about volatile abort() (they would otherwise abort with an error).
 *
 * Revision 1.4  1995/07/24  15:06:52  rj
 * configure checks for mem* functions. define replacements using b* functions, if necessary.
 *
 */

#ifndef _SNACC_H_
#define _SNACC_H_

#define GLASS	1
#define KHO	1

#include "config.h"

#if STDC_HEADERS
#include <stdlib.h>
#endif

#ifndef NULL
#define NULL	0
#endif

#if HAVE_MEMCMP /* memcmp(3) returns <0, 0 and 0, bcmp(3) returns only 0 and !0 */
#define memcmpeq( a, b, len)	memcmp (a, b, len)
#else
#define memcmpeq( a, b, len)	bcmp (a, b, len)
#endif
#if HAVE_MEMSET
#define memzero( p, len)	memset (p, 0, len)
#else
#define memzero( p, len)	bzero (p, len)
#endif
#if !HAVE_MEMCPY
#define memcpy( dst, src, len)	bcopy (src, dst, len)
#endif

#ifdef __cplusplus

#ifdef VOLATILE_RETRUN
#  define RETURN_THIS_FOR_COMPILERS_WITHOUT_VOLATILE_FUNCTIONS	return *this;
#else
#  define RETURN_THIS_FOR_COMPILERS_WITHOUT_VOLATILE_FUNCTIONS
#endif

#if !BOOL_BUILTIN
#ifndef true
// enum bool { false, true };
// the above looks elegant, but leads to anachronisms (<, ==, !=, ... return value of type int, not enum bool), therefore:
typedef int bool;
enum { false, true };
#endif
#endif

#else /* !__cplusplus */

#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

#endif /* __cplusplus */

/*
 *  Inspired by gdb 4.0, for better or worse...
 *  (grabbed from Barry Brachman - MS)
 *
 *  These macros munge C routine declarations such
 *  that they work for ANSI or non-ANSI C compilers
 */
#ifdef __USE_ANSI_C__

#define PROTO( X)			X
#define PARAMS( arglist, args)  	(args)
#define NOPARAMS()	        	(void)
#define _AND_				,
#define DOTS				, ...

#else /* !__USE_ANSI_C__ */

#define PROTO( X)			()
#define PARAMS( arglist, args)	 	arglist args;
#define NOPARAMS()	        	()
#define _AND_				;
#define DOTS
#define void                            char

#endif /* __USE_ANSI_C__ */

#include "policy.h"

#if COMPILER
#define TCL	(HAVE_TCL && !NO_TCL)
#define META	(TCL && !NO_META)
#endif

#if MAKEDEPEND
#if !NO_META
#ifdef META
#undef META
#endif
#define META	1
#endif
#if !NO_TCL
#ifdef TCL
#undef TCL
#endif
#define TCL	1
#endif
#endif

#if TCL
#ifdef META
#undef META
#endif
#define META	1
#endif

#define COMMA			,

#ifdef _IBM_ENC_
#define if_IBM_ENC( code)	code
#else
#define if_IBM_ENC( code)
#endif

#if META
#define if_META( code)		code
#else
#define if_META( code)
#endif

#if TCL && META
#define if_TCL( code)		code
#else
#define if_TCL( code)
#endif

#ifndef _gVDADER_RULES
extern int gVDADER_RULES;
#endif

#endif /* _SNACC_H_ */
