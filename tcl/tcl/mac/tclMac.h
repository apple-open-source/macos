/*
 * tclMac.h --
 *
 *	Declarations of Macintosh specific public variables and procedures.
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclMac.h,v 1.1.1.5 2003/03/06 00:11:56 landonf Exp $
 */

#ifndef _TCLMAC
#define _TCLMAC

#ifndef _TCL
#   include "tcl.h"
#endif
#include <Types.h>
#include <Files.h>
#include <Events.h>

typedef int (*Tcl_MacConvertEventPtr) _ANSI_ARGS_((EventRecord *eventPtr));

#include "tclPlatDecls.h"

#endif /* _TCLMAC */
