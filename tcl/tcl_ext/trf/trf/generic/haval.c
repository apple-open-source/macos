/*
 * haval.c --
 *
 *	Implements and registers message digest generator HAVAL.
 *
 *
 * Copyright (c) 1996 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
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
 * CVS: $Id: haval.c,v 1.4 2000/08/09 19:13:17 aku Exp $
 */

#include "transformInt.h"
#include "haval.1996/haval.h"

/*
 * Generator description
 * ---------------------
 *
 * The HAVAL alogrithm is used to compute a cryptographically strong
 * message digest.
 */

#define DIGEST_SIZE               (32)
#define CTX_TYPE                  haval_state

/*
 * Declarations of internal procedures.
 */

static void MDHaval_Start     _ANSI_ARGS_ ((VOID* context));
static void MDHaval_Update    _ANSI_ARGS_ ((VOID* context, unsigned int character));
static void MDHaval_UpdateBuf _ANSI_ARGS_ ((VOID* context, unsigned char* buffer, int bufLen));
static void MDHaval_Final     _ANSI_ARGS_ ((VOID* context, VOID* digest));

/*
 * Generator definition.
 */

static Trf_MessageDigestDescription mdDescription = { /* THREADING: constant, read-only => safe */
  "haval",
  sizeof (CTX_TYPE),
  DIGEST_SIZE,
  MDHaval_Start,
  MDHaval_Update,
  MDHaval_UpdateBuf,
  MDHaval_Final,
  NULL
};

/*
 *------------------------------------------------------*
 *
 *	TrfInit_HAVAL --
 *
 *	------------------------------------------------*
 *	Register the generator implemented in this file.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of 'Trf_Register'.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

int
TrfInit_HAVAL (interp)
Tcl_Interp* interp;
{
  return Trf_RegisterMessageDigest (interp, &mdDescription);
}

/*
 *------------------------------------------------------*
 *
 *	MDHaval_Start --
 *
 *	------------------------------------------------*
 *	Initialize the internal state of the message
 *	digest generator.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called procedure.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
MDHaval_Start (context)
VOID* context;
{
  haval_start ((CTX_TYPE*) context);
}

/*
 *------------------------------------------------------*
 *
 *	MDHaval_Update --
 *
 *	------------------------------------------------*
 *	Update the internal state of the message digest
 *	generator for a single character.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called procedure.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
MDHaval_Update (context, character)
VOID* context;
unsigned int   character;
{
  unsigned char buf = character;

  haval_hash ((CTX_TYPE*) context, &buf, 1);
}

/*
 *------------------------------------------------------*
 *
 *	MDHaval_UpdateBuf --
 *
 *	------------------------------------------------*
 *	Update the internal state of the message digest
 *	generator for a character buffer.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called procedure.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
MDHaval_UpdateBuf (context, buffer, bufLen)
VOID* context;
unsigned char* buffer;
int   bufLen;
{
  haval_hash ((CTX_TYPE*) context, (unsigned char*) buffer, bufLen);
}

/*
 *------------------------------------------------------*
 *
 *	MDHaval_Final --
 *
 *	------------------------------------------------*
 *	Generate the digest from the internal state of
 *	the message digest generator.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called procedure.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
MDHaval_Final (context, digest)
VOID* context;
VOID* digest;
{
  haval_end ((CTX_TYPE*) context, (unsigned char*) digest);
}

/*
 * External code from here on.
 */

#include "haval.1996/haval.c" /* THREADING: import of one constant var, read-only => safe */
