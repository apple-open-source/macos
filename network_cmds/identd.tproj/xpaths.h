/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
**	$Id: xpaths.h,v 1.1.1.2 2000/01/11 01:48:48 wsanchez Exp $
**
** paths.h		Common path definitions for the in.identd daemon
**
** Last update: 11 Dec 1992
**
** Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
*/

#include <paths.h>

#ifdef SEQUENT
#  define _PATH_UNIX "/dynix"
#endif

#if defined(MIPS) || defined(IRIX)
#  define _PATH_UNIX "/unix"
#endif

#if defined(hpux) || defined(__hpux)
#  define _PATH_UNIX "/hp-ux"
#endif

#ifdef SOLARIS
#  define _PATH_UNIX "/dev/ksyms"
#else
#  ifdef SVR4
#    define _PATH_UNIX "/stand/unix"
#  endif
#endif

#ifdef BSD43
#  define _PATH_SWAP "/dev/drum"
#  define _PATH_MEM  "/dev/mem"
#endif

#ifdef _AUX_SOURCE
#  define _PATH_UNIX "/unix"
#endif

#ifdef _CRAY
#  define _PATH_UNIX "/unicos"
#  define _PATH_MEM  "/dev/mem"
#endif

#ifdef __APPLE__
#  define _PATH_UNIX "/mach"
#endif


/*
 * Some defaults...
 */
#ifndef _PATH_KMEM
#  define _PATH_KMEM "/dev/kmem"
#endif

#ifndef _PATH_UNIX
#  define _PATH_UNIX "/vmunix"
#endif


#ifndef PATH_CONFIG
#  define PATH_CONFIG "/etc/identd.conf"
#endif
