/*
 * crc_zlib.c --
 *
 *	Implements and registers message digest generator CRC32 (taken from zlib).
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
 * CVS: $Id: crc_zlib.c,v 1.5 2003/01/09 21:27:10 andreas_kupries Exp $
 */

#include "transformInt.h"

/*
 * Generator description
 * ---------------------
 *
 * The CRC32 algorithm (contained in library 'zlib')
 * is used to compute a message digest.
 */

#define DIGEST_SIZE               4 /* byte == 32 bit */
#define CTX_TYPE                  uLong

/*
 * Declarations of internal procedures.
 */

static void MDcrcz_Start     _ANSI_ARGS_ ((VOID* context));
static void MDcrcz_Update    _ANSI_ARGS_ ((VOID* context, unsigned int character));
static void MDcrcz_UpdateBuf _ANSI_ARGS_ ((VOID* context, unsigned char* buffer, int bufLen));
static void MDcrcz_Final     _ANSI_ARGS_ ((VOID* context, VOID* digest));
static int  MDcrcz_Check     _ANSI_ARGS_ ((Tcl_Interp* interp));

/*
 * Generator definition.
 */

static Trf_MessageDigestDescription mdDescription = {
  "crc-zlib",
  sizeof (CTX_TYPE),
  DIGEST_SIZE,
  MDcrcz_Start,
  MDcrcz_Update,
  MDcrcz_UpdateBuf,
  MDcrcz_Final,
  MDcrcz_Check
};

#define CRC (*((uLong*) context))

/*
 *------------------------------------------------------*
 *
 *	TrfInit_CRC_zlib --
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
TrfInit_CRC_ZLIB (interp)
Tcl_Interp* interp;
{
  return Trf_RegisterMessageDigest (interp, &mdDescription);
}

/*
 *------------------------------------------------------*
 *
 *	MDcrcz_Start --
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
MDcrcz_Start (context)
VOID* context;
{
  /* call md specific initialization here */

  CRC = zf.zcrc32 (0L, Z_NULL, 0);
}

/*
 *------------------------------------------------------*
 *
 *	MDcrcz_Update --
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
MDcrcz_Update (context, character)
VOID* context;
unsigned int   character;
{
  /* call md specific update here */

  unsigned char buf = character;

  CRC = zf.zcrc32 (CRC, &buf, 1);
}

/*
 *------------------------------------------------------*
 *
 *	MDcrcz_UpdateBuf --
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
MDcrcz_UpdateBuf (context, buffer, bufLen)
VOID* context;
unsigned char* buffer;
int   bufLen;
{
  /* call md specific update here */

  CRC = zf.zcrc32 (CRC, buffer, bufLen);
}

/*
 *------------------------------------------------------*
 *
 *	MDcrcz_Final --
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
MDcrcz_Final (context, digest)
VOID* context;
VOID* digest;
{
  /* call md specific finalization here */

  uLong crc = CRC;
  char*   out = (char*) digest;

  /* LITTLE ENDIAN output */
  out [3] = (char) ((crc >> 24) & 0xff);
  out [2] = (char) ((crc >> 16) & 0xff);
  out [1] = (char) ((crc >>  8) & 0xff);
  out [0] = (char) ((crc >>  0) & 0xff);
}

/*
 *------------------------------------------------------*
 *
 *	MDcrcz_Check --
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
MDcrcz_Check (interp)
Tcl_Interp* interp;
{
  return TrfLoadZlib (interp);
}

