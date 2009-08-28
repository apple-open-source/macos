#ifndef MEMCHAN_H
/*
 * memchanInt.h --
 *
 *	Internal definitions.
 *
 * Copyright (C) 1996-1999 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 * SOFTWARE AND ITS DOCUMENTATION, EVEN IF I HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * I SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND
 * I HAVE NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * CVS: $Id: memchanInt.h,v 1.20 2008/10/03 21:46:30 andreas_kupries Exp $
 */


#include <errno.h>
#include <string.h>
#define USE_NON_CONST
#include <tcl.h>

/*
 * Make sure that both EAGAIN and EWOULDBLOCK are defined. This does not
 * compile on systems where neither is defined. We want both defined so
 * that we can test safely for both. In the code we still have to test for
 * both because there may be systems on which both are defined and have
 * different values.
 *
 * Taken from tcl/generic/tclIO.h
 * Might be better if the 'tclPort' headers were public.
 */

#if ((!defined(EWOULDBLOCK)) && (defined(EAGAIN)))
#   define EWOULDBLOCK EAGAIN
#endif
#if ((!defined(EAGAIN)) && (defined(EWOULDBLOCK)))
#   define EAGAIN EWOULDBLOCK
#endif
#if ((!defined(EAGAIN)) && (!defined(EWOULDBLOCK)))
error one of EWOULDBLOCK or EAGAIN must be defined
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Number of bytes used to extend a storage area found to small.
 */

#define INCREMENT (512)

/*
 * Number of milliseconds to wait between polls of channel state,
 * e.g. generation of readable/writable events.
 *
 * Relevant for only Tcl 8.0 and beyond.
 */

#define DELAY (5)

/* Detect Tcl 8.1 and beyond => Stubs, panic <-> Tcl_Panic
 */

#define GT81 ((TCL_MAJOR_VERSION > 8) || \
((TCL_MAJOR_VERSION == 8) && \
 (TCL_MINOR_VERSION >= 1)))

/* Detect Tcl 8.4 and beyond => API CONSTification
 */

#define GT84 ((TCL_MAJOR_VERSION > 8) || \
((TCL_MAJOR_VERSION == 8) && \
 (TCL_MINOR_VERSION >= 4)))

/* There are currently two cases to consider
 *
 * 1. An API function called with a const string, which was non-const
 *    in the relevant argument before 8.4 and is now const in that
 *    argument. This meanst that before 8.4 the actual parameter
 *    required a cast to unconst the value and doesn't require the
 *    cast for 8.4 and beyond.
 *
 *    This is solved by the macro MC_UNCONSTB84
 *    = MemChan unCONST Before 8.4
 *
 * 2. The result of an API function was non-const before 8.4 and is
 *    now const, and is assinged to a non-const string pointer.
 */

#if GT84
#define MC_UNCONSTB84
#else
#define MC_UNCONSTB84   (char*)
#endif /* GT84 */

#ifndef CONST84
#define CONST84
#endif

/*
 * Pre-8.3 the Tcl_ChannelTypeVersion was not defined.
 */
#if ((TCL_MAJOR_VERSION >= 8) && (TCL_MINOR_VERSION < 3))
typedef Tcl_DriverBlockModeProc* Tcl_ChannelTypeVersion;
#endif

#if ! (GT81)
/* Enable use of procedure internal to tcl. Necessary only
 * for versions of tcl below 8.1.
 */

EXTERN void
panic _ANSI_ARGS_ (TCL_VARARGS(char*, format));

#undef  Tcl_Panic
#define Tcl_Panic panic
#endif

#undef HAVE_LTOA /* Forcing 'sprintf'. HP ltoa function signature may diverge */
#ifdef HAVE_LTOA
#define LTOA(x,str) ltoa (x, str, 10)
#else
#define LTOA(x,str) sprintf (str, "%lu", (unsigned long) (x))
#endif


/* Internal command visible to other parts of the package.
 */

extern int
MemchanCmd _ANSI_ARGS_ ((ClientData notUsed,
			 Tcl_Interp* interp,
			 int objc, Tcl_Obj*CONST objv[]));

extern int
MemchanFifoCmd _ANSI_ARGS_ ((ClientData notUsed,
			     Tcl_Interp* interp,
			     int objc, Tcl_Obj*CONST objv[]));

extern int
MemchanFifo2Cmd _ANSI_ARGS_ ((ClientData notUsed,
			      Tcl_Interp* interp,
			      int objc, Tcl_Obj*CONST objv[]));

extern int
MemchanNullCmd _ANSI_ARGS_ ((ClientData notUsed,
			     Tcl_Interp* interp,
			     int objc, Tcl_Obj*CONST objv[]));

extern int
MemchanRandomCmd _ANSI_ARGS_ ((ClientData notUsed,
                  Tcl_Interp* interp,
			      int objc, Tcl_Obj*CONST objv[]));

extern int
MemchanZeroCmd _ANSI_ARGS_ ((ClientData notUsed,
                  Tcl_Interp* interp,
			      int objc, Tcl_Obj*CONST objv[]));

/* Generator procedure for handles. Handles mutex issues for a thread
 * enabled version of tcl.
 */

extern Tcl_Obj*
MemchanGenHandle _ANSI_ARGS_ ((CONST char* prefix));

#ifdef __cplusplus
}
#endif /* C++ */

/*
 * Exported functionality.
 */

#include "memchan.h"

#endif /* MEMCHAN_H */
