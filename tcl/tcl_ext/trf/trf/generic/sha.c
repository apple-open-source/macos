/*
 * sha.c --
 *
 *	Implements and registers message digest generator SHA.
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
 * CVS: $Id: sha.c,v 1.4 2007/10/05 23:12:21 andreas_kupries Exp $
 */

#include "transformInt.h"
#include "sha/sha.h"

#ifdef WORDS_BIGENDIAN
#undef  LITTLE_ENDIAN
#else
#undef  LITTLE_ENDIAN
#define LITTLE_ENDIAN
#endif

/*
 * Generator description
 * ---------------------
 *
 * The SHA alogrithm is used to compute a cryptographically strong
 * message digest.
 */

#define DIGEST_SIZE               (SHA_DIGESTSIZE)
#define CTX_TYPE                  sha_trf_info
#define CHUNK_SIZE                256

/* We cannot use SHA_INFO directly as context cause 'sha_update' handles
 * a chunk smaller then CHUNK_SIZE bytes correct if and only if it is
 * the last chunk. This forces us to buffer the incoming bytes till a chunk
 * is complete before doing an update.
 */

typedef struct _sha_trf_info {
  SHA_INFO       s;
  unsigned short count;
  unsigned char  buf [CHUNK_SIZE]; /* SHA block */
} sha_trf_info;


/*
 * Declarations of internal procedures.
 */

static void MDsha_Start     _ANSI_ARGS_ ((VOID* context));
static void MDsha_Update    _ANSI_ARGS_ ((VOID* context, unsigned int character));
static void MDsha_UpdateBuf _ANSI_ARGS_ ((VOID* context, unsigned char* buffer, int bufLen));
static void MDsha_Final     _ANSI_ARGS_ ((VOID* context, VOID* digest));

/*
 * Generator definition.
 */

static Trf_MessageDigestDescription mdDescription = { /* THREADING: constant, read-only => safe */
  "sha",
  sizeof (CTX_TYPE),
  DIGEST_SIZE,
  MDsha_Start,
  MDsha_Update,
  MDsha_UpdateBuf,
  MDsha_Final,
  NULL
};

/*
 *------------------------------------------------------*
 *
 *	TrfInit_SHA --
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
TrfInit_SHA (interp)
Tcl_Interp* interp;
{
  return Trf_RegisterMessageDigest (interp, &mdDescription);
}

/*
 *------------------------------------------------------*
 *
 *	MDsha_Start --
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
MDsha_Start (context)
VOID* context;
{
  sha_trf_info* s = (sha_trf_info*) context;

  memset (s, '\0', sizeof (sha_trf_info));
  sha_init (&s->s);
}

/*
 *------------------------------------------------------*
 *
 *	MDsha_Update --
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
MDsha_Update (context, character)
VOID* context;
unsigned int   character;
{
  sha_trf_info* s = (sha_trf_info*) context;

  s->buf [s->count] = character;
  s->count ++;

  if (s->count == CHUNK_SIZE) {
    sha_update (&s->s, s->buf, s->count);
    s->count = 0;
  }
}

/*
 *------------------------------------------------------*
 *
 *	MDsha_UpdateBuf --
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
MDsha_UpdateBuf (context, buffer, bufLen)
VOID* context;
unsigned char* buffer;
int   bufLen;
{
  sha_trf_info* s = (sha_trf_info*) context;

  if ((s->count + bufLen) < CHUNK_SIZE) {
    /*
     * Not enough for full chunk. Remember incoming
     * data and wait for another call containing more data.
     */

    memcpy ((VOID*) (s->buf + s->count), (VOID*) buffer, bufLen);
    s->count += bufLen;
  } else {
    /*
     * Complete chunk with incoming data, update digest,
     * then use all chunks contained in the buffer. Remember
     * an incomplete chunk and wait for further calls.
     */

    int k = CHUNK_SIZE - s->count;

    if (k < CHUNK_SIZE) {
      memcpy ((VOID*) (s->buf + s->count), (VOID*) buffer, k);

      sha_update (&s->s, s->buf, CHUNK_SIZE);

      buffer += k;
      bufLen -= k;
    } /* k == CHUNK_SIZE => internal buffer was empty, so skip it entirely */

    while (bufLen > CHUNK_SIZE) {
      sha_update (&s->s, buffer, CHUNK_SIZE);

      buffer += CHUNK_SIZE;
      bufLen -= CHUNK_SIZE;
    }

    s->count = bufLen;
    if (bufLen > 0)
      memcpy ((VOID*) s->buf, (VOID*) buffer, bufLen);
  }
}

/*
 *------------------------------------------------------*
 *
 *	MDsha_Final --
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
MDsha_Final (context, digest)
VOID* context;
VOID* digest;
{
  sha_trf_info* s = (sha_trf_info*) context;

  if (s->count > 0) {
    sha_update (&s->s, s->buf, s->count);
  }

  sha_final  (&s->s);

#ifndef WORDS_BIGENDIAN
  Trf_FlipRegisterLong (s->s.digest, SHA_DIGESTSIZE);
#endif

  memcpy (digest, s->s.digest, SHA_DIGESTSIZE);
}

/*
 * External code from here on.
 *
 * To make smaller object code, but run a little slower, don't use UNROLL_LOOPS.
 * To use NIST's modified SHA of 7/11/94, define USE_MODIFIED_SHA
 */

#define UNROLL_LOOPS
#include "sha/sha.c"
