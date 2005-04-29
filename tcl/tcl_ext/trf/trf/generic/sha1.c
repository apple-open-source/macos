/*
 * sha1.c --
 *
 *	Implements and registers message digest generator SHA1.
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
 * CVS: $Id: sha1.c,v 1.3 2000/08/09 19:13:18 aku Exp $
 */

#include "loadman.h"

/*
 * Generator description
 * ---------------------
 *
 * The SHA1 alogrithm is used to compute a cryptographically strong
 * message digest.
 */

#ifndef OTP
#define DIGEST_SIZE               (SHA_DIGEST_LENGTH)
#else
#define DIGEST_SIZE               (8)
#endif
#define CTX_TYPE                  SHA_CTX

/*
 * Declarations of internal procedures.
 */

static void MDsha1_Start     _ANSI_ARGS_ ((VOID* context));
static void MDsha1_Update    _ANSI_ARGS_ ((VOID* context, unsigned int character));
static void MDsha1_UpdateBuf _ANSI_ARGS_ ((VOID* context, unsigned char* buffer, int bufLen));
static void MDsha1_Final     _ANSI_ARGS_ ((VOID* context, VOID* digest));
static int  MDsha1_Check     _ANSI_ARGS_ ((Tcl_Interp* interp));

/*
 * Generator definition.
 */

static Trf_MessageDigestDescription mdDescription = { /* THREADING: constant, read-only => safe */
#ifndef OTP
  "sha1",
#else
  "otp_sha1",
#endif
  sizeof (CTX_TYPE),
  DIGEST_SIZE,
  MDsha1_Start,
  MDsha1_Update,
  MDsha1_UpdateBuf,
  MDsha1_Final,
  MDsha1_Check
};

/*
 *------------------------------------------------------*
 *
 *	TrfInit_SHA1 --
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
#ifndef OTP
TrfInit_SHA1 (interp)
#else
TrfInit_OTP_SHA1 (interp)
#endif
Tcl_Interp* interp;
{
  return Trf_RegisterMessageDigest (interp, &mdDescription);
}

/*
 *------------------------------------------------------*
 *
 *	MDsha1_Start --
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
MDsha1_Start (context)
VOID* context;
{
  sha1f.init ((SHA_CTX*) context);
}

/*
 *------------------------------------------------------*
 *
 *	MDsha1_Update --
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
MDsha1_Update (context, character)
VOID* context;
unsigned int   character;
{
  unsigned char buf = character;

  sha1f.update ((SHA_CTX*) context, &buf, 1);
}

/*
 *------------------------------------------------------*
 *
 *	MDsha1_UpdateBuf --
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
MDsha1_UpdateBuf (context, buffer, bufLen)
VOID* context;
unsigned char* buffer;
int   bufLen;
{
  sha1f.update ((SHA_CTX*) context, (unsigned char*) buffer, bufLen);
}

/*
 *------------------------------------------------------*
 *
 *	MDsha1_Final --
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
MDsha1_Final (context, digest)
VOID* context;
VOID* digest;
{
#ifndef OTP
  sha1f.final ((unsigned char*) digest, (SHA_CTX*) context);
#else
    unsigned int result[SHA_DIGEST_LENGTH / sizeof (char)];

    sha1f.final ((unsigned char*) result, (SHA_CTX*) context);

    result[0] ^= result[2];
    result[1] ^= result[3];
    result[0] ^= result[4];

    Trf_FlipRegisterLong ((VOID*) result, DIGEST_SIZE);
    memcpy ((VOID *) digest, (VOID *) result, DIGEST_SIZE);
#endif
}

/*
 *------------------------------------------------------*
 *
 *	MDsha1_Check --
 *
 *	------------------------------------------------*
 *	Do global one-time initializations of the message
 *	digest generator.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Loads the shared library containing the
 *		SHA1 functionality
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
MDsha1_Check (interp)
Tcl_Interp* interp;
{
  return TrfLoadSHA1 (interp);
}
