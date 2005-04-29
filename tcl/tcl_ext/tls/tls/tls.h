/*
 * Copyright (C) 1997-2000 Matt Newman <matt@novadigm.com>
 *
 * $Header: /cvsroot/tls/tls/tls.h,v 1.2 2000/01/20 01:59:38 aborr Exp $
 *
 * TLS (aka SSL) Channel - can be layered on any bi-directional
 * Tcl_Channel (Note: Requires Trf Core Patch)
 *
 * This was built from scratch based upon observation of OpenSSL 0.9.2B
 *
 * Addition credit is due for Andreas Kupries (a.kupries@westend.com), for
 * providing the Tcl_ReplaceChannel mechanism and working closely with me
 * to enhance it to support full fileevent semantics.
 *
 * Also work done by the follow people provided the impetus to do this "right":-
 *	tclSSL (Colin McCormack, Shared Technology)
 *	SSLtcl (Peter Antman)
 *
 */
#ifndef _TLS_H
#define _TLS_H

#include <tcl.h>	/* Internal definitions for Tcl. */

#ifdef TCL_STORAGE_CLASS
# undef TCL_STORAGE_CLASS
#endif
#ifdef BUILD_tls
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# define TCL_STORAGE_CLASS DLLIMPORT
#endif

/*
 * Forward declarations
 */

EXTERN int Tls_Init _ANSI_ARGS_ ((Tcl_Interp *));
EXTERN int Tls_SafeInit _ANSI_ARGS_ ((Tcl_Interp *));

#endif /* _TLS_H */
