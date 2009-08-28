/*
 * Copyright (C) 1997-2000 Matt Newman <matt@novadigm.com>
 *
 * $Header: /cvsroot/tls/tls/tlsInt.h,v 1.15 2007/06/22 21:20:38 hobbs2 Exp $
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
#ifndef _TSLINT_H
#define _TLSINT_H

#include "tls.h"
#include <errno.h>
#include <string.h>

#ifdef __WIN32__
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h> /* OpenSSL needs this on Windows */
#endif

/* Handle tcl8.3->tcl8.4 CONST changes */
#ifndef CONST84
#define CONST84
#endif

#ifdef NO_PATENTS
#define NO_IDEA
#define NO_RC2
#define NO_RC4
#define NO_RC5
#define NO_RSA
#define NO_SSL2
#endif

#ifdef BSAFE
#include <ssl.h>
#include <err.h>
#include <rand.h>
#else
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#ifdef TCL_STORAGE_CLASS
# undef TCL_STORAGE_CLASS
#endif
#ifdef BUILD_tls
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# define TCL_STORAGE_CLASS DLLIMPORT
#endif
 
#ifndef ECONNABORTED
#define ECONNABORTED	130	/* Software caused connection abort */
#endif
#ifndef ECONNRESET
#define ECONNRESET	131	/* Connection reset by peer */
#endif

#ifdef DEBUG
#define dprintf fprintf
#else
#define dprintf if (0) fprintf
#endif

#define SSL_ERROR(ssl,err)	\
    ((char*)ERR_reason_error_string((unsigned long)SSL_get_error((ssl),(err))))
/*
 * OpenSSL BIO Routines
 */
#define BIO_TYPE_TCL	(19|0x0400)

/*
 * Defines for State.flags
 */
#define TLS_TCL_ASYNC	(1<<0)	/* non-blocking mode */
#define TLS_TCL_SERVER	(1<<1)	/* Server-Side */
#define TLS_TCL_INIT	(1<<2)	/* Initializing connection */
#define TLS_TCL_DEBUG	(1<<3)	/* Show debug tracing */
#define TLS_TCL_CALLBACK	(1<<4)	/* In a callback, prevent update
					 * looping problem. [Bug 1652380] */

#define TLS_TCL_DELAY (5)

/*
 * This structure describes the per-instance state
 * of an ssl channel.
 *
 * The SSL processing context is maintained here, in the ClientData
 */
typedef struct State {
    Tcl_Channel self;	/* this socket channel */
    Tcl_TimerToken timer;

    int flags;		/* see State.flags above  */
    int watchMask;	/* current WatchProc mask */
    int mode;		/* current mode of parent channel */

    Tcl_Interp *interp;	/* interpreter in which this resides */
    Tcl_Obj *callback;	/* script called for tracing, verifying and errors */
    Tcl_Obj *password;	/* script called for certificate password */ 

    int vflags;		/* verify flags */
    SSL *ssl;		/* Struct for SSL processing */
    SSL_CTX *ctx;	/* SSL Context */
    BIO *bio;		/* Struct for SSL processing */
    BIO *p_bio;		/* Parent BIO (that is layered on Tcl_Channel) */

    char *err;
} State;

/*
 * The following definitions have to be usable for 8.2.0-8.3.1 and 8.3.2+.
 * The differences between these versions:
 *
 * 8.0-8.1:	There is no support for these in TLS 1.4 (get 1.3).  This
 *		was the version with the original patch.
 *
 * 8.2.0-	Changed semantics for Tcl_StackChannel (Tcl_ReplaceChannel).
 * 8.3.1:	Check at runtime to switch the behaviour. The patch is part
 *		of the core from now on.
 *
 * 8.3.2+:	Stacked channels rewritten for better behaviour in some
 *		situations (closing). Some new API's, semantic changes.
 *
 * The following magic was adapted from Trf 2.1 (Kupries).
 */

#define TLS_CHANNEL_VERSION_1	0x1
#define TLS_CHANNEL_VERSION_2	0x2
extern int channelTypeVersion;

#ifdef USE_TCL_STUBS
#ifndef Tcl_StackChannel
/*
 * The core we are compiling against is not patched, so supply the
 * necesssary definitions here by ourselves. The form chosen for
 * the procedure macros (reservedXXX) will notify us if the core
 * does not have these reserved locations anymore.
 *
 * !! Synchronize the procedure indices in their definitions with
 *    the patch to tcl.decls, as they have to be the same.
 */

/* 281 */
typedef Tcl_Channel (tls_StackChannel) _ANSI_ARGS_((Tcl_Interp* interp,
						    Tcl_ChannelType* typePtr,
						    ClientData instanceData,
						    int mask,
						    Tcl_Channel prevChan));
/* 282 */
typedef void (tls_UnstackChannel) _ANSI_ARGS_((Tcl_Interp* interp,
					       Tcl_Channel chan));

#define Tcl_StackChannel     ((tls_StackChannel*) tclStubsPtr->reserved281)
#define Tcl_UnstackChannel ((tls_UnstackChannel*) tclStubsPtr->reserved282)

#endif /* Tcl_StackChannel */

#ifndef Tcl_GetStackedChannel
/*
 * Separate definition, available in 8.2, but not 8.1 and before !
 */

/* 283 */
typedef Tcl_Channel (tls_GetStackedChannel) _ANSI_ARGS_((Tcl_Channel chan));

#define Tcl_GetStackedChannel ((tls_GetStackedChannel*) tclStubsPtr->reserved283)

#endif /* Tcl_GetStackedChannel */


#ifndef TCL_CHANNEL_VERSION_2
/*
 * Core is older than 8.3.2.  Supply the missing definitions for
 * the new API's in 8.3.2.
 */
#define EMULATE_CHANNEL_VERSION_2

typedef struct TlsChannelTypeVersion_* TlsChannelTypeVersion;
#define TCL_CHANNEL_VERSION_2	((TlsChannelTypeVersion) 0x2)

typedef int (TlsDriverHandlerProc) _ANSI_ARGS_((ClientData instanceData,
					int interestMask));
/* 394 */
typedef int (tls_ReadRaw)  _ANSI_ARGS_((Tcl_Channel chan, char *dst,
					int bytesToRead));
/* 395 */
typedef int (tls_WriteRaw) _ANSI_ARGS_((Tcl_Channel chan, char *src,
					int srcLen));
/* 397 */
typedef int (tls_GetTopChannel) _ANSI_ARGS_((Tcl_Channel chan));

/*
 * Generating code for accessing these parts of the stub table when
 * compiling against a core older than 8.3.2 is a hassle because even
 * the 'reservedXXX' fields of the structure are not defined yet. So
 * we have to write up some macros hiding some very hackish pointer
 * arithmetics to get at these fields. We assume that pointer to
 * functions are always of the same size.
 */

#define STUB_BASE   ((char*)(&(tclStubsPtr->tcl_UtfNcasecmp))) /* field 370 */
#define procPtrSize (sizeof (Tcl_DriverBlockModeProc *))
#define IDX(n)      (((n)-370) * procPtrSize)
#define SLOT(n)     (STUB_BASE + IDX(n))

#define Tcl_ReadRaw		(*((tls_ReadRaw**)	(SLOT(394))))
#define Tcl_WriteRaw		(*((tls_WriteRaw**)	(SLOT(395))))
#define Tcl_GetTopChannel	(*((tls_GetTopChannel**)(SLOT(396))))

/*
 * Required, easy emulation.
 */
#define Tcl_ChannelGetOptionProc(chanDriver) ((chanDriver)->getOptionProc)

#endif /* TCL_CHANNEL_VERSION_2 */

#endif /* USE_TCL_STUBS */

/*
 * Forward declarations
 */

EXTERN Tcl_ChannelType *Tls_ChannelType _ANSI_ARGS_((void));
EXTERN Tcl_Channel	Tls_GetParent _ANSI_ARGS_((State *statePtr));

EXTERN Tcl_Obj*		Tls_NewX509Obj _ANSI_ARGS_ (( Tcl_Interp *interp, X509 *cert));
EXTERN void		Tls_Error _ANSI_ARGS_ ((State *statePtr, char *msg));
EXTERN void		Tls_Free _ANSI_ARGS_ ((char *blockPtr));
EXTERN void		Tls_Clean _ANSI_ARGS_ ((State *statePtr));
EXTERN int		Tls_WaitForConnect _ANSI_ARGS_(( State *statePtr,
							int *errorCodePtr));

EXTERN BIO_METHOD *	BIO_s_tcl _ANSI_ARGS_((void));
EXTERN BIO *		BIO_new_tcl _ANSI_ARGS_((State* statePtr, int flags));

#endif /* _TLSINT_H */
