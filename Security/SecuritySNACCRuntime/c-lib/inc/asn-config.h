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
 * asn_config.h - configures the ANSI/non ansi, defines
 *                decoder alloc routines and buffer routines
 *
 * MS 91
 * Copyright (C) 1992 Michael Sample and the University of British Columbia
 *
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * If you modify this file, you must clearly indicate your changes.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/inc/asn-config.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-config.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:22  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:20  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.6  1997/03/13 09:15:16  wan
 * Improved dependency generation for stupid makedepends.
 * Corrected PeekTag to peek into buffer only as far as necessary.
 * Added installable error handler.
 * Fixed small glitch in idl-code generator (Markku Savela <msa@msa.tte.vtt.fi>).
 *
 * Revision 1.5  1995/07/24 21:01:11  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.4  1995/02/13  14:47:33  rj
 * settings for IEEE_REAL_FMT/IEEE_REAL_LIB moved from {c_lib,c++_lib}/inc/asn_config.h to acconfig.h.
 *
 * Revision 1.3  1994/10/08  04:46:20  rj
 * config.h -> snacc.h, which now is the toplevel config file.
 *
 * Revision 1.2  1994/08/31  23:53:05  rj
 * redundant code moved into ../../config.h.bot
 *
 * Revision 1.1  1994/08/28  09:21:25  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#ifndef _asn_config_h_
#define _asn_config_h_

#include <stdio.h>
#include <setjmp.h> /* for jmp_buf type, setjmp and longjmp */

/* for pow() used in asn_real.c - must include to avoid casting err on pow */
#include <math.h>

#include "snacc.h"


/* used to test if optionals are present */
#define NOT_NULL( ptr)			((ptr) != NULL)


/*
 * Asn1Error (char *str) - configure error handler
 */
void Asn1Error PROTO ((char* str));


/*
 * Asn1Warning (char *str) - configure warning mechanism
 * (currently never called)
 */
void Asn1Warning PROTO ((char* str));

/*
 * Asn1ErrorHandler - procedure to call upon Asn1Warning (severity 0)
 * and Asn1Error (severity 1).
 */
typedef void (*Asn1ErrorHandler) PROTO ((char* str, int severity));

/*
 * Asn1InstallErrorHandler - installs new error handler, returns former one
 */
Asn1ErrorHandler Asn1InstallErrorHandler PROTO ((Asn1ErrorHandler handler));

/*
 * configure memory scheme used by decoder to allocate memory
 * for the decoded value.
 * The Asn1Free will be called in the optionally generated
 * hierachical free routines.
 *
 * nibble_alloc allocs from a single buffer and EVERYTHING
 * is freed by a single fcn call. Individual elmts cannot be freed
 */

#ifndef USE_NIBBLE_MEMORY
#define USE_NIBBLE_MEMORY		1
#endif

#if USE_NIBBLE_MEMORY

#include "nibble-alloc.h"

#define Asn1Alloc( size)		NibbleAlloc (size)
#define Asn1Free( ptr)			/* empty */
#define CheckAsn1Alloc( ptr, env)	\
	if ((ptr) == NULL)\
	  longjmp (env, -27)

#else /* !USE_NIBBLE_MEMORY */

#include "mem.h"

#define Asn1Alloc( size)		Malloc (size)
#define Asn1Free( ptr)			Free (ptr)
#define CheckAsn1Alloc( ptr, env)	\
	if ((ptr) == NULL)\
	  longjmp (env, -27)

#endif /* USE_NIBBLE_MEMORY */

#define ENV_TYPE jmp_buf

/*
 * configure buffer routines that the encoders (write)
 * and decoders (read) use.  This config technique kind
 * of bites but is allows efficient macro calls.  The
 * Generated code & lib routines  call/use the "Buf????"
 * version of the macro - you define their meaning here.
 */
#ifdef USE_EXP_BUF

#include "exp-buf.h"

#define BUF_TYPE			ExpBuf **
#define BufGetByte( b)			ExpBufGetByte (b)
#define BufGetSeg( b, lenPtr)		ExpBufGetSeg (b, lenPtr)
#define BufCopy( dst, b, len)		ExpBufCopy (dst, b, len)
#define BufSkip( b, len)		ExpBufSkip (b, len)
#define BufPeekByte( b)			ExpBufPeekByte (b)
#define BufPutByteRvs( b, byte)		ExpBufPutByteRvs (b, byte)
#define BufPutSegRvs( b, data, len)	ExpBufPutSegRvs (b, data, len)
#define BufReadError( b)		ExpBufReadError (b)
#define BufWriteError( b)		ExpBufWriteError (b)

#else /* !USE_EXP_BUF */

#ifdef USE_MIN_BUF

#include "min-buf.h"

#define BUF_TYPE			char **
#define BufGetByte( b)			MinBufGetByte (b)
#define BufGetSeg( b, lenPtr)		MinBufGetSeg (b, lenPtr)
#define BufCopy( dst, b, len)		MinBufCopy (dst, b, len)
#define BufSkip( b, len)		MinBufSkip (b, len)
#define BufPeekByte( b)			MinBufPeekByte (b)
#define BufPutByteRvs( b, byte)		MinBufPutByteRvs (b, byte)
#define BufPutSegRvs( b, data, len)	MinBufPutSegRvs (b, data, len)
#define BufReadError( b)		MinBufReadError (b)
#define BufWriteError( b)		MinBufWriteError (b)

#else /* !USE_EXP_BUF && !USE_MIN_BUF */

#ifdef USE_SBUF

#include "sbuf.h"

#define BUF_TYPE			SBuf *
#define BufGetByte( b)			SBufGetByte (b)
#define BufGetSeg( b, lenPtr)		SBufGetSeg (b, lenPtr)
#define BufCopy( dst, b, len)		SBufCopy (dst, b, len)
#define BufSkip( b, len)		SBufSkip (b, len)
#define BufPeekByte( b)			SBufPeekByte (b)
#define BufPutByteRvs( b, byte)		SBufPutByteRvs (b, byte)
#define BufPutSegRvs( b, data, len)	SBufPutSegRvs (b, data, len)
#define BufReadError( b)		SBufReadError (b)
#define BufWriteError( b)		SBufWriteError (b)

#else /* !USE_EXP_BUF && !USE_MIN_BUF && !USE_SBUF*/

#ifdef USE_GEN_BUF

/*
 * NOTE: for use with tables, I defined the (slower)
 *  GenBuf type that is more flexible (à la ISODE and XDR).
 *  This allows the encode/decode libs to support other
 *  buffer types dynamically instead of having different
 *  libs for each buffer type.
 *  The GenBufs are not provided for the compiled code
 *  (ie the c_lib directory) but could easily be added
 *  (I don't have time, tho).  Tables tools are
 *  around 4x slower than the compiled version so a
 *  the GenBufs aren't such a big performance hit for table stuff.
 *
 */
#include "gen-buf.h"

#define BUF_TYPE			GenBuf *
#define BufGetByte( b)			GenBufGetByte (b)
#define BufGetSeg( b, lenPtr)		GenBufGetSeg (b, lenPtr)
#define BufCopy( dst, b, len)		GenBufCopy (dst, b, len)
#define BufSkip( b, len)		GenBufSkip (b, len)
#define BufPeekByte( b)			GenBufPeekByte (b)
#define BufPeekSeg( b, lenPtr)		GenBufPeekSeg (b, lenPtr)
#define BufPeekCopy( dst, b, len)	GenBufPeekCopy (dst, b, len)
#define BufPutByteRvs( b, byte)		GenBufPutByteRvs (b, byte)
#define BufPutSegRvs( b, data, len)	GenBufPutSegRvs (b, data, len)
#define BufReadError( b)		GenBufReadError (b)
#define BufWriteError( b)		GenBufWriteError (b)

#else /* none?! */

#ifndef MAKEDEPEND
  #error "don't know what buffer type to use!"
#endif

#endif /* USE_GEN_BUF */
#endif /* USE_SBUF */
#endif /* USE_MIN_BUF */
#endif /* USE_EXP_BUF */

#include "print.h"  /* for printing set up */

#endif /* conditional include */
