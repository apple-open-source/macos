/*
 * md2.c --
 *
 *	Implements and registers message digest generator MD2.
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
 * CVS: $Id: md2.c,v 1.3 2000/08/09 19:13:17 aku Exp $
 */

#include "loadman.h"

/*
 * Generator description
 * ---------------------
 *
 * The MD2 alogrithm is used to compute a cryptographically strong
 * message digest.
 */

#define DIGEST_SIZE               (MD2_DIGEST_LENGTH)
#define CTX_TYPE                  MD2_CTX

/*
 * Declarations of internal procedures.
 */

static void MDmd2_Start     _ANSI_ARGS_ ((VOID* context));
static void MDmd2_Update    _ANSI_ARGS_ ((VOID* context, unsigned int character));
static void MDmd2_UpdateBuf _ANSI_ARGS_ ((VOID* context, unsigned char* buffer, int bufLen));
static void MDmd2_Final     _ANSI_ARGS_ ((VOID* context, VOID* digest));
static int  MDmd2_Check     _ANSI_ARGS_ ((Tcl_Interp* interp));

/*
 * Generator definition.
 */

static Trf_MessageDigestDescription mdDescription = { /* THREADING: constant, read-only => safe */
  "md2",
  sizeof (CTX_TYPE),
  DIGEST_SIZE,
  MDmd2_Start,
  MDmd2_Update,
  MDmd2_UpdateBuf,
  MDmd2_Final,
  MDmd2_Check
};

/*
 *------------------------------------------------------*
 *
 *	TrfInit_MD2 --
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
TrfInit_MD2 (interp)
Tcl_Interp* interp;
{
  return Trf_RegisterMessageDigest (interp, &mdDescription);
}

/*
 *------------------------------------------------------*
 *
 *	MDmd2_Start --
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
MDmd2_Start (context)
VOID* context;
{
  md2f.init ((MD2_CTX*) context);
}

/*
 *------------------------------------------------------*
 *
 *	MDmd2_Update --
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
MDmd2_Update (context, character)
VOID* context;
unsigned int   character;
{
  unsigned char buf = character;

  md2f.update ((MD2_CTX*) context, &buf, 1);
}

/*
 *------------------------------------------------------*
 *
 *	MDmd2_UpdateBuf --
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
MDmd2_UpdateBuf (context, buffer, bufLen)
VOID* context;
unsigned char* buffer;
int   bufLen;
{
  md2f.update ((MD2_CTX*) context, (unsigned char*) buffer, bufLen);
}

/*
 *------------------------------------------------------*
 *
 *	MDmd2_Final --
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
MDmd2_Final (context, digest)
VOID* context;
VOID* digest;
{
  md2f.final ((unsigned char*) digest, (MD2_CTX*) context);
}

/*
 *------------------------------------------------------*
 *
 *	MDmd2_Check --
 *
 *	------------------------------------------------*
 *	Do global one-time initializations of the message
 *	digest generator.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Loads the shared library containing the
 *		MD2 functionality
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
MDmd2_Check (interp)
Tcl_Interp* interp;
{
  return TrfLoadMD2 (interp);
}
