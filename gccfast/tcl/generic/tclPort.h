/*
 * tclPort.h --
 *
 *	This header file handles porting issues that occur because
 *	of differences between systems.  It reads in platform specific
 *	portability files.
 *
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclPort.h,v 1.2 2001/09/14 01:43:02 zlaski Exp $
 */

#ifndef _TCLPORT
#define _TCLPORT

#if defined(__CYGWIN__) && defined(__TCL_UNIX_VARIANT)
#	include "../unix/tclUnixPort.h"
#else
#if defined(__WIN32__) || defined(_WIN32)
#   include "../win/tclWinPort.h"
#else
#   if defined(MAC_TCL)
#	include "tclMacPort.h"
#    else
#	include "../unix/tclUnixPort.h"
#    endif
#endif
#endif

#endif /* _TCLPORT */
