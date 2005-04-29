/*
 * adler.c --
 *
 *	Implements and registers message digest generator ADLER32.
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
 * CVS: $Id: adler.c,v 1.8 2003/01/09 21:27:09 andreas_kupries Exp $
 */

#include "transformInt.h"

/*
 * Generator description
 * ---------------------
 *
 * The ADLER32 algorithm (contained in library 'zlib')
 * is used to compute a message digest.
 */

#define DIGEST_SIZE               4 /* byte == 32 bit */
#define CTX_TYPE                  uLong

/*
 * Declarations of internal procedures.
 */

static void MDAdler_Start     _ANSI_ARGS_ ((VOID* context));
static void MDAdler_Update    _ANSI_ARGS_ ((VOID* context, unsigned int character));
static void MDAdler_UpdateBuf _ANSI_ARGS_ ((VOID* context, unsigned char* buffer, int bufLen));
static void MDAdler_Final     _ANSI_ARGS_ ((VOID* context, VOID* digest));
static int  MDAdler_Check     _ANSI_ARGS_ ((Tcl_Interp* interp));

/*
 * Generator definition.
 */

static Trf_MessageDigestDescription mdDescription = { /* THREADING: constant, read-only => safe */
  "adler",
  sizeof (CTX_TYPE),
  DIGEST_SIZE,
  MDAdler_Start,
  MDAdler_Update,
  MDAdler_UpdateBuf,
  MDAdler_Final,
  MDAdler_Check
};

#define ADLER (*((uLong*) context))

/*
 *------------------------------------------------------*
 *
 *	TrfInit_ADLER --
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
TrfInit_ADLER (interp)
Tcl_Interp* interp;
{
  return Trf_RegisterMessageDigest (interp, &mdDescription);
}

/*
 *------------------------------------------------------*
 *
 *	MDAdler_Start --
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
MDAdler_Start (context)
VOID* context;
{
  START (MDAdler_Start);
  PRINT ("Context = %p, Zf = %p\n", context, &zf);

  /* call md specific initialization here */

  ADLER = zf.zadler32 (0L, Z_NULL, 0);

  DONE (MDAdler_Start);
}

/*
 *------------------------------------------------------*
 *
 *	MDAdler_Update --
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
MDAdler_Update (context, character)
VOID* context;
unsigned int   character;
{
  /* call md specific update here */

  unsigned char buf = character;

  ADLER = zf.zadler32 (ADLER, &buf, 1);
}

/*
 *------------------------------------------------------*
 *
 *	MDAdler_UpdateBuf --
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
MDAdler_UpdateBuf (context, buffer, bufLen)
VOID* context;
unsigned char* buffer;
int   bufLen;
{
  /* call md specific update here */

  ADLER = zf.zadler32 (ADLER, buffer, bufLen);
}

/*
 *------------------------------------------------------*
 *
 *	MDAdler_Final --
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
MDAdler_Final (context, digest)
VOID* context;
VOID* digest;
{
  /* call md specific finalization here */

  uLong adler = ADLER;
  char*   out = (char*) digest;

  /* BIGENDIAN output */
  out [0] = (char) ((adler >> 24) & 0xff);
  out [1] = (char) ((adler >> 16) & 0xff);
  out [2] = (char) ((adler >>  8) & 0xff);
  out [3] = (char) ((adler >>  0) & 0xff);
}

/*
 *------------------------------------------------------*
 *
 *	MDAdler_Check --
 *
 *	------------------------------------------------*
 *	Check for existence of libz, load it.
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

static int
MDAdler_Check (interp)
Tcl_Interp* interp;
{
  int res;

  START (MDAdler_Check);

  res = TrfLoadZlib (interp);

  PRINT ("res = %d\n", res);
  DONE (MDAdler_Check);
  return res;
}

