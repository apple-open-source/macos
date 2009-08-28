/*
 * digest.c --
 *
 *	Implements and registers code common to all message digests.
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
 * CVS: $Id: digest.c,v 1.16 2001/08/21 05:51:33 tcl Exp $
 */

#include "transformInt.h"


/*
 * Declarations of internal procedures.
 */

static Trf_ControlBlock CreateEncoder  _ANSI_ARGS_ ((ClientData writeClientData,
						     Trf_WriteProc *fun,
						     Trf_Options optInfo,
						     Tcl_Interp*   interp,
						     ClientData clientData));
static void             DeleteEncoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));
static int              Encode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     unsigned int character,
						     Tcl_Interp* interp,
						     ClientData clientData));
static int              EncodeBuffer   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     unsigned char* buffer, int bufLen,
						     Tcl_Interp* interp,
						     ClientData clientData));
static int              FlushEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     Tcl_Interp* interp,
						     ClientData clientData));
static void             ClearEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));


static Trf_ControlBlock CreateDecoder  _ANSI_ARGS_ ((ClientData writeClientData,
						     Trf_WriteProc *fun,
						     Trf_Options optInfo,
						     Tcl_Interp*   interp,
						     ClientData clientData));
static void             DeleteDecoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));
static int              Decode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     unsigned int character,
						     Tcl_Interp* interp,
						     ClientData clientData));
static int              DecodeBuffer   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     unsigned char* buffer, int bufLen,
						     Tcl_Interp* interp,
						     ClientData clientData));
static int              FlushDecoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     Tcl_Interp* interp,
						     ClientData clientData));
static void             ClearDecoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));


/*
 * Generator definition.
 */

static Trf_TypeDefinition mdDefinition = /* THREADING: constant, read-only => safe */
{
  NULL, /* filled later by Trf_RegisterMessageDigest (in a copy) */
  NULL, /* filled later by Trf_RegisterMessageDigest (in a copy) */
  NULL, /* filled later by Trf_RegisterMessageDigest (in a copy) */
  {
    CreateEncoder,
    DeleteEncoder,
    Encode,
    EncodeBuffer,
    FlushEncoder,
    ClearEncoder,
    NULL /* no MaxRead */
  }, {
    CreateDecoder,
    DeleteDecoder,
    Decode,
    DecodeBuffer,
    FlushDecoder,
    ClearDecoder,
    NULL /* no MaxRead */
  },
  TRF_UNSEEKABLE
};

/*
 * Definition of the control blocks for en- and decoder.
 */

typedef struct _EncoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  int            operation_mode;

  char*          destHandle;	/* Name of target for ATTACH_WRITE */
  Tcl_Channel    dest;		/* Channel target for ATTACH_WRITE. possibly NULL */
  Tcl_Interp*    vInterp;	/* Interpreter containing 'destHandle', if name of variable */

  /* Possible combinations:
   * destHandle != NULL, vInterp != NULL, dest == NULL  /ATTACH_WRITE, to variable
   * destHandle == NULL, vInterp == NULL, dest != NULL  /ATTACH_WRITE, to channel
   * destHandle == NULL, vInterp == NULL, dest == NULL  /ATTACH_ABSORB, or IMMEDIATE
   *
   * TRF_TRANSPARENT <=> TRF_WRITE_HASH
   */

  VOID*          context;


} EncoderControl;

#define IMMEDIATE     (0)
#define ATTACH_ABSORB (1)
#define ATTACH_WRITE  (2)
#define ATTACH_TRANS  (3)

typedef struct _DecoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  int            operation_mode;

  char*          destHandle;	/* Name of target for ATTACH_WRITE */
  Tcl_Channel    dest;		/* Channel target for ATTACH_WRITE. possibly NULL */
  Tcl_Interp*    vInterp;	/* Interpreter containing 'destHandle', if name of variable */

  /* Possible combinations:
   * destHandle != NULL, dest == NULL	/ATTACH_WRITE, to variable
   * destHandle == NULL, dest != NULL	/ATTACH_WRITE, to channel
   * destHandle == NULL, dest == NULL	/ATTACH_ABSORB, or IMMEDIATE
   * vInterp always set, because of 'matchFlag'.
   *
   * TRF_TRANSPARENT <=> TRF_WRITE_HASH
   */

  VOID*          context;
  char*          matchFlag;      /* target for ATTACH_ABSORB */

  unsigned char* digest_buffer;
  short          buffer_pos;
  unsigned short charCount;

} DecoderControl;


static int
WriteDigest _ANSI_ARGS_ ((Tcl_Interp* interp, char* destHandle,
			  Tcl_Channel dest,   char* digest,
			  Trf_MessageDigestDescription* md));


/*
 *------------------------------------------------------*
 *
 *	Trf_RegisterMessageDigest --
 *
 *	------------------------------------------------*
 *	Register the specified generator as transformer.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Allocates memory. As of 'Trf_Register'.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

int
Trf_RegisterMessageDigest (interp, md_desc)
Tcl_Interp*                         interp;
CONST Trf_MessageDigestDescription* md_desc;
{
  Trf_TypeDefinition* md;
  int                res;

  START (Trf_RegisterMessageDigest);

  /* THREADING: read-only access => safe */
  md = (Trf_TypeDefinition*) Tcl_Alloc (sizeof (Trf_TypeDefinition));

  memcpy ((VOID*) md, (VOID*) &mdDefinition, sizeof (Trf_TypeDefinition));

  md->name       = md_desc->name;
  md->clientData = (ClientData) md_desc;
  md->options    = TrfMDOptions ();

  PRINT ("MD_Desc %p\n", md_desc); FL; IN;

  PRINT ("Name:      %s\n", md_desc->name);
  PRINT ("Context:   %d\n", md_desc->context_size);
  PRINT ("Digest:    %d\n", md_desc->digest_size);
  PRINT ("Start:     %p\n", md_desc->startProc);
  PRINT ("Update:    %p\n", md_desc->updateProc);
  PRINT ("UpdateBuf: %p\n", md_desc->updateBufProc);
  PRINT ("Final      %p\n", md_desc->finalProc);
  PRINT ("Check:     %p\n", md_desc->checkProc);

  OT;


  res = Trf_Register (interp, md);

  /* 'md' is a memory leak, it will never be released.
   */

  DONE (Trf_RegisterMessageDigest);
  return res;
}

/*
 *------------------------------------------------------*
 *
 *	CreateEncoder --
 *
 *	------------------------------------------------*
 *	Allocate and initialize the control block of a
 *	data encoder.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Allocates memory.
 *
 *	Result:
 *		An opaque reference to the control block.
 *
 *------------------------------------------------------*
 */

static Trf_ControlBlock
CreateEncoder (writeClientData, fun, optInfo, interp, clientData)
ClientData    writeClientData;
Trf_WriteProc *fun;
Trf_Options   optInfo;
Tcl_Interp*   interp;
ClientData    clientData;
{
  EncoderControl*                c;
  TrfMDOptionBlock*              o = (TrfMDOptionBlock*) optInfo;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;

  START (digest.CreateEncoder);
  PRINT ("%p: %s\n", md, md->name);

  c = (EncoderControl*) Tcl_Alloc (sizeof (EncoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;

  PRINT ("Setting up state\n"); FL;

  if ((o->behaviour == TRF_IMMEDIATE) || (o->mode == TRF_ABSORB_HASH)) {
    c->operation_mode = (o->behaviour == TRF_IMMEDIATE) ? IMMEDIATE : ATTACH_ABSORB;
    c->vInterp        = (Tcl_Interp*) NULL;
    c->destHandle     = (char*)       NULL;
    c->dest           = (Tcl_Channel) NULL;
  } else {
    if (o->mode == TRF_WRITE_HASH)
      c->operation_mode = ATTACH_WRITE;
    else
      c->operation_mode = ATTACH_TRANS;

    if (o->wdIsChannel) {
      c->vInterp        = (Tcl_Interp*) NULL;
      c->destHandle     = (char*)       NULL;
      c->dest           = o->wdChannel;
    } else {
      c->vInterp        = o->vInterp;
      c->destHandle     = o->writeDestination;
      c->dest           = (Tcl_Channel) NULL;

      o->writeDestination = (char*) NULL; /* ownership moved, prevent early free */
    }
  }

  /*
   * Create and initialize the context.
   */

  PRINT ("Setting up context (%d bytes)\n", md->context_size); FL;

  c->context = (VOID*) Tcl_Alloc (md->context_size);
  (*md->startProc) (c->context);

  DONE (digest.CreateEncoder);

  return (ClientData) c;
}

/*
 *------------------------------------------------------*
 *
 *	DeleteEncoder --
 *
 *	------------------------------------------------*
 *	Destroy the control block of an encoder.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Releases the memory allocated by 'CreateEncoder'
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
DeleteEncoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData       clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  Tcl_Free ((char*) c->context);
  Tcl_Free ((char*) c);
}

/*
 *------------------------------------------------------*
 *
 *	Encode --
 *
 *	------------------------------------------------*
 *	Encode the given character and write the result.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called WriteFun.
 *
 *	Result:
 *		Generated bytes implicitly via WriteFun.
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
Encode (ctrlBlock, character, interp, clientData)
Trf_ControlBlock ctrlBlock;
unsigned int     character;
Tcl_Interp*      interp;
ClientData       clientData;
{
  EncoderControl*                c = (EncoderControl*) ctrlBlock;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;
  unsigned char                buf;

  buf = character;
  (*md->updateProc) (c->context, character);

  if ((c->operation_mode == ATTACH_ABSORB) ||
      (c->operation_mode == ATTACH_TRANS)) {
    /*
     * absorption/transparent mode: incoming characters flow
     * unchanged through transformation.
     */

    return c->write (c->writeClientData, &buf, 1, interp);
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	EncodeBuffer --
 *
 *	------------------------------------------------*
 *	Encode the given buffer and write the result.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called WriteFun.
 *
 *	Result:
 *		Generated bytes implicitly via WriteFun.
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
EncodeBuffer (ctrlBlock, buffer, bufLen, interp, clientData)
Trf_ControlBlock ctrlBlock;
unsigned char*   buffer;
int              bufLen;
Tcl_Interp*      interp;
ClientData       clientData;
{
  EncoderControl*                c = (EncoderControl*) ctrlBlock;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;

  if (*md->updateBufProc != (Trf_MDUpdateBuf*) NULL) {
    (*md->updateBufProc) (c->context, buffer, bufLen);
  } else {
    unsigned int character, i;

    for (i=0; i < ((unsigned int) bufLen); i++) {
      character = buffer [i];
      (*md->updateProc) (c->context, character);
    }
  }

  if ((c->operation_mode == ATTACH_ABSORB) ||
      (c->operation_mode == ATTACH_TRANS)) {
    /*
     * absorption/transparent mode: incoming characters flow
     * unchanged through transformation.
     */

    return c->write (c->writeClientData, buffer, bufLen, interp);
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	FlushEncoder --
 *
 *	------------------------------------------------*
 *	Encode an incomplete character sequence (if possible).
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called WriteFun.
 *
 *	Result:
 *		Generated bytes implicitly via WriteFun.
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
FlushEncoder (ctrlBlock, interp, clientData)
Trf_ControlBlock ctrlBlock;
Tcl_Interp*      interp;
ClientData       clientData;
{
  EncoderControl*                c = (EncoderControl*) ctrlBlock;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;
  char*                     digest;
  int                          res = TCL_OK;

  /*
   * Get a bit more, for a trailing \0 in 7.6, see 'WriteDigest' too
   */
  digest = (char*) Tcl_Alloc (2 + md->digest_size);
  (*md->finalProc) (c->context, digest);

  if ((c->operation_mode == ATTACH_WRITE) ||
      (c->operation_mode == ATTACH_TRANS)) {
    res = WriteDigest (c->vInterp, c->destHandle, c->dest, digest, md);
  } else {
    /*
     * Immediate execution or attached channel absorbing the checksum.
     */

    /* -W- check wether digest can be declared 'uchar*', or if this has
     * -W- other sideeffects, see lines 636, 653, 82 too.
     */
    res = c->write (c->writeClientData, (unsigned char*) digest, md->digest_size, interp);
  }

  Tcl_Free (digest);
  return res;
}

/*
 *------------------------------------------------------*
 *
 *	ClearEncoder --
 *
 *	------------------------------------------------*
 *	Discard an incomplete character sequence.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ClearEncoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData       clientData;
{
  EncoderControl*                c = (EncoderControl*) ctrlBlock;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;

  (*md->startProc) (c->context);
}

/*
 *------------------------------------------------------*
 *
 *	CreateDecoder --
 *
 *	------------------------------------------------*
 *	Allocate and initialize the control block of a
 *	data decoder.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Allocates memory.
 *
 *	Result:
 *		An opaque reference to the control block.
 *
 *------------------------------------------------------*
 */

static Trf_ControlBlock
CreateDecoder (writeClientData, fun, optInfo, interp, clientData)
ClientData    writeClientData;
Trf_WriteProc *fun;
Trf_Options   optInfo;
Tcl_Interp*   interp;
ClientData    clientData;
{
  DecoderControl*                c;
  TrfMDOptionBlock*              o = (TrfMDOptionBlock*) optInfo;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;

  c = (DecoderControl*) Tcl_Alloc (sizeof (DecoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;

  c->matchFlag  = o->matchFlag;
  c->vInterp    = o->vInterp;
  o->matchFlag  = NULL;

  /* impossible: (o->behaviour == TRF_IMMEDIATE) */

  if (o->mode == TRF_ABSORB_HASH) {
    c->operation_mode = ATTACH_ABSORB;
    c->destHandle     = (char*)       NULL;
    c->dest           = (Tcl_Channel) NULL;
  } else {
    if (o->mode == TRF_WRITE_HASH)
      c->operation_mode = ATTACH_WRITE;
    else
      c->operation_mode = ATTACH_TRANS;

    if (o->rdIsChannel) {
      c->destHandle     = (char*)       NULL;
      c->dest           = o->rdChannel;
    } else {
      c->destHandle     = o->readDestination;
      c->dest           = (Tcl_Channel) NULL;

      o->readDestination = (char*) NULL; /* ownership moved, prevent early free */
    }  
  }

  c->buffer_pos = 0;
  c->charCount  = 0;

  c->context = (VOID*) Tcl_Alloc (md->context_size);
  (*md->startProc) (c->context);

  c->digest_buffer = (unsigned char*) Tcl_Alloc (md->digest_size);
  memset (c->digest_buffer, '\0', md->digest_size);

  return (ClientData) c;
}

/*
 *------------------------------------------------------*
 *
 *	DeleteDecoder --
 *
 *	------------------------------------------------*
 *	Destroy the control block of an decoder.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Releases the memory allocated by 'CreateDecoder'
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
DeleteDecoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  Tcl_Free ((char*) c->digest_buffer);
  Tcl_Free ((char*) c->context);
  Tcl_Free ((char*) c);
}

/*
 *------------------------------------------------------*
 *
 *	Decode --
 *
 *	------------------------------------------------*
 *	Decode the given character and write the result.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called WriteFun.
 *
 *	Result:
 *		Generated bytes implicitly via WriteFun.
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
Decode (ctrlBlock, character, interp, clientData)
Trf_ControlBlock ctrlBlock;
unsigned int     character;
Tcl_Interp*      interp;
ClientData       clientData;
{
  DecoderControl*                c = (DecoderControl*) ctrlBlock;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;
  char                         buf;

  if (c->operation_mode == ATTACH_WRITE) {
    buf = character;
    (*md->updateProc) (c->context, character);

  } else if (c->operation_mode == ATTACH_TRANS) {
    buf = character;
    (*md->updateProc) (c->context, character);

    return c->write (c->writeClientData, (unsigned char*) &buf, 1, interp);
  } else {
    if (c->charCount == md->digest_size) {
      /*
       * ringbuffer full, forward oldest character
       * and replace with new one.
       */

      buf = c->digest_buffer [c->buffer_pos];

      c->digest_buffer [c->buffer_pos] = character;
      c->buffer_pos ++;
      c->buffer_pos %= md->digest_size;

      character = buf;
      (*md->updateProc) (c->context, character);

      return c->write (c->writeClientData, (unsigned char*) &buf, 1, interp);
    } else {
      /*
       * Fill ringbuffer.
       */

      c->digest_buffer [c->buffer_pos] = character;

      c->buffer_pos ++;
      c->charCount  ++;
    }
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	DecodeBuffer --
 *
 *	------------------------------------------------*
 *	Decode the given buffer and write the result.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called WriteFun.
 *
 *	Result:
 *		Generated bytes implicitly via WriteFun.
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
DecodeBuffer (ctrlBlock, buffer, bufLen, interp, clientData)
Trf_ControlBlock ctrlBlock;
unsigned char*   buffer;
int              bufLen;
Tcl_Interp*      interp;
ClientData       clientData;
{
  DecoderControl*                c = (DecoderControl*) ctrlBlock;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;

  if (c->operation_mode == ATTACH_WRITE) {
    if (*md->updateBufProc != (Trf_MDUpdateBuf*) NULL) {
      (*md->updateBufProc) (c->context, buffer, bufLen);
    } else {
      int character, i;

      for (i=0; i < bufLen; i++) {
	character = buffer [i];
	(*md->updateProc) (c->context, character);
      }
    }

  } else if (c->operation_mode == ATTACH_TRANS) {
    if (*md->updateBufProc != (Trf_MDUpdateBuf*) NULL) {
      (*md->updateBufProc) (c->context, buffer, bufLen);
    } else {
      int character, i;

      for (i=0; i < bufLen; i++) {
	character = buffer [i];
	(*md->updateProc) (c->context, character);
      }
    }

    return c->write (c->writeClientData, buffer, bufLen, interp);

  } else {
    /* try to use more than character at a time. */

    if (*md->updateBufProc != NULL) {

      /*
       * 2 cases:
       *
       * - Incoming buffer and data stored from previous calls are less
       *   or just enough to fill the ringbuffer. Copy the incoming bytes
       *   into the buffer and return.
       *
       * - Incoming data + ringbuffer contain more than reqired for a
       * digest. Concatenate both and use the oldest bytes to update the
       * hash context. Place the remaining 'digest_size' bytes into the
       * ringbuffer again.
       *
       * Both cases assume the ringbuffer data to be starting at index '0'.
       */

      if ((c->charCount + bufLen) <= md->digest_size) {
	/* extend ring buffer */

	memcpy ( (VOID*) (c->digest_buffer + c->charCount), (VOID*) buffer, bufLen);
	c->charCount += bufLen;
      } else {
	/*
	 * n contains the number of bytes we are allowed to hash into the context.
	 */

	int n = c->charCount + bufLen - md->digest_size;
	int res;

	if (c->charCount > 0) {
	  if (n <= c->charCount) {
	    /*
	     * update context, then shift down the remaining bytes
	     */

	    (*md->updateBufProc) (c->context, c->digest_buffer, n);

	    res = c->write (c->writeClientData, c->digest_buffer, n, interp);

	    memmove ((VOID*) c->digest_buffer,
		     (VOID*) (c->digest_buffer + n), c->charCount - n);
	    c->charCount -= n;
	    n             = 0;
	  } else {
	    /*
	     * Hash everything inside the buffer.
	     */

	    (*md->updateBufProc) (c->context, c->digest_buffer, c->charCount);

	    res = c->write (c->writeClientData, c->digest_buffer, c->charCount, interp);

	    n -= c->charCount;
	    c->charCount = 0;
	  }

	  if (res != TCL_OK)
	    return res;
	}

	if (n > 0) {
	  (*md->updateBufProc) (c->context, buffer, n);

	  res = c->write (c->writeClientData, buffer, n, interp);

	  memcpy ((VOID*) (c->digest_buffer + c->charCount),
		  (VOID*) (buffer + n),
		  (bufLen - n));
	  c->charCount = md->digest_size; /* <=> 'c->charCount += bufLen - n;' */

	  if (res != TCL_OK)
	    return res;
	}
      }
    } else {
      /*
       * no 'updateBufProc', simulate it using the
       * underlying single character routine
       */

      int character, i, res;
      char         buf;

      for (i=0; i < bufLen; i++) {
	buf       = c->digest_buffer [c->buffer_pos];
	character = buffer [i];

	if (c->charCount == md->digest_size) {
	  /*
	   * ringbuffer full, forward oldest character
	   * and replace with new one.
	   */

	  c->digest_buffer [c->buffer_pos] = character;
	  c->buffer_pos ++;
	  c->buffer_pos %= md->digest_size;

	  character = buf;
	  (*md->updateProc) (c->context, character);

	  res = c->write (c->writeClientData, (unsigned char*) &buf, 1, interp);
	  if (res != TCL_OK)
	    return res;
	} else {
	  /*
	   * Fill ringbuffer.
	   */
	
	  c->digest_buffer [c->buffer_pos] = character;

	  c->buffer_pos ++;
	  c->charCount  ++;
	}
      } /* for */
    }
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	FlushDecoder --
 *
 *	------------------------------------------------*
 *	Decode an incomplete character sequence (if possible).
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the called WriteFun.
 *
 *	Result:
 *		Generated bytes implicitly via WriteFun.
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
FlushDecoder (ctrlBlock, interp, clientData)
Trf_ControlBlock ctrlBlock;
Tcl_Interp*      interp;
ClientData       clientData;
{
  DecoderControl*                c = (DecoderControl*) ctrlBlock;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;
  char* digest;
  int res= TCL_OK;

  /*
   * Get a bit more, for a trailing \0 in 7.6, see 'WriteDigest' too
   */
  digest = (char*) Tcl_Alloc (2 + md->digest_size);
  (*md->finalProc) (c->context, digest);

  if ((c->operation_mode == ATTACH_WRITE) ||
      (c->operation_mode == ATTACH_TRANS)) {
    res = WriteDigest (c->vInterp, c->destHandle, c->dest, digest, md);
  } else if (c->charCount < md->digest_size) {
    /*
     * ATTACH_ABSORB, not enough data in input!
     */

    if (interp) {
      Tcl_AppendResult (interp, "not enough bytes in input", (char*) NULL);
    }

    res = TCL_ERROR;
  } else {
    char* result_text;

    if (c->buffer_pos > 0) {
      /*
       * Reorder bytes in ringbuffer to form the correct digest.
       */

      char* temp;
      int i,j;

      temp = (char*) Tcl_Alloc (md->digest_size);

      for (i= c->buffer_pos, j=0;
	   j < md->digest_size;
	   i = (i+1) % md->digest_size, j++) {
	temp [j] = c->digest_buffer [i];
      }

      memcpy ((VOID*) c->digest_buffer, (VOID*) temp, md->digest_size);
      Tcl_Free (temp);
    }

    /*
     * Compare computed and transmitted checksums.
     */

    result_text = (0 == memcmp ((VOID*) digest, (VOID*) c->digest_buffer, md->digest_size) ?
		   "ok" : "failed");

    Tcl_SetVar (c->vInterp, c->matchFlag, result_text, TCL_GLOBAL_ONLY);
  }

  Tcl_Free (digest);
  return res;
}

/*
 *------------------------------------------------------*
 *
 *	ClearDecoder --
 *
 *	------------------------------------------------*
 *	Discard an incomplete character sequence.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ClearDecoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData       clientData;
{
  DecoderControl*                c = (DecoderControl*) ctrlBlock;
  Trf_MessageDigestDescription* md = (Trf_MessageDigestDescription*) clientData;

  c->buffer_pos = 0;
  c->charCount  = 0;

  (*md->startProc) (c->context);
  memset (c->digest_buffer, '\0', md->digest_size);
}

/*
 *------------------------------------------------------*
 *
 *	WriteDigest --
 *
 *	------------------------------------------------*
 *	Writes the generated digest into the destination
 *	variable, or channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		May leave an error message in the
 *		interpreter result area.
 *
 *	Result:
 *		A standard Tcl error message.
 *
 *------------------------------------------------------*
 */

static int
WriteDigest (interp, destHandle, dest, digest, md)
Tcl_Interp*                   interp;
char*                         destHandle;
Tcl_Channel                   dest;
char*                         digest;
Trf_MessageDigestDescription* md;
{
  if (destHandle != (char*) NULL) {

#if GT81
    Tcl_Obj* digestObj = Tcl_NewByteArrayObj (digest, md->digest_size);
#else
    Tcl_Obj* digestObj = Tcl_NewStringObj    (digest, md->digest_size);
#endif
    Tcl_Obj* result;

    /*#if GT81
      / * 8.1 and beyond * /
      Tcl_IncrRefCount(digestObj);

      result = Tcl_SetObjVar2 (interp, destHandle, (char*) NULL, digestObj,
      TCL_LEAVE_ERR_MSG | TCL_GLOBAL_ONLY);
      #else*/
    /* 8.0 section */
    Tcl_Obj* varName = Tcl_NewStringObj (destHandle, strlen (destHandle));

    Tcl_IncrRefCount(varName);
    Tcl_IncrRefCount(digestObj);

    result = Tcl_ObjSetVar2 (interp, varName, (Tcl_Obj*) NULL, digestObj,
			     TCL_LEAVE_ERR_MSG | TCL_GLOBAL_ONLY |
			     TCL_PARSE_PART1);
    Tcl_DecrRefCount(varName);
    /*#endif / * GT81 */

    Tcl_DecrRefCount(digestObj);

    if (result == (Tcl_Obj*) NULL) {
      return TCL_ERROR;
    }
  } else if (dest != (Tcl_Channel) NULL) {
    int res = Tcl_Write (dest, digest, md->digest_size);

    if (res < 0) {
      if (interp) {
	Tcl_AppendResult (interp,
			  "error writing \"", Tcl_GetChannelName (dest),
			  "\": ", Tcl_PosixError (interp), (char*) NULL);
      }
      return TCL_ERROR;
    }
  }

  return TCL_OK;
}
