/*
 * rs_ecc.c --
 *
 *	Implements and registers error correction with Reed-Solomon (255,249,7) block code.
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
 * CVS: $Id: rs_ecc.c,v 1.10 2009/05/07 04:57:27 andreas_kupries Exp $
 */

#include "transformInt.h"

/*
 * Forward declaractions of internally used procedures.
 */

#define MSG_LEN  (249) /* 248 bytes usable, 1 byte length information
			* (always at end of block) */
#define CODE_LEN (255)

void rsencode _ANSI_ARGS_ ((unsigned char m [MSG_LEN],
			    unsigned char c [CODE_LEN]));
void rsdecode _ANSI_ARGS_ ((unsigned char c [CODE_LEN],
			    unsigned char m [MSG_LEN], int* errcode));

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
 * Converter definition.
 */

static Trf_TypeDefinition convDefinition =
{
  "rs_ecc",
  NULL, /* filled by TrfInit_RS_ECC, THREADING: serialize initialization */
  NULL, /* not used */
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
  TRF_RATIO (248, 255)
};

/*
 * Definition of the control blocks for en- and decoder.
 */

typedef struct _EncoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  /* add conversion specific items here (RS_ECC) */

  unsigned char block [MSG_LEN];
  unsigned char charCount;

} EncoderControl;


typedef struct _DecoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  /* add conversion specific items here (RS_ECC) */

  unsigned char block [CODE_LEN];
  unsigned char charCount;

} DecoderControl;




/*
 *------------------------------------------------------*
 *
 *	TrfInit_RS_ECC --
 *
 *	------------------------------------------------*
 *	Register the ECC algorithm implemented in this file.
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
TrfInit_RS_ECC (interp)
Tcl_Interp* interp;
{
  TrfLock; /* THREADING: serialize initialization */
  convDefinition.options = Trf_ConverterOptions ();
  TrfUnlock;

  return Trf_Register (interp, &convDefinition);
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
  EncoderControl* c;

  c = (EncoderControl*) ckalloc (sizeof (EncoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;

  /* initialize conversion specific items here (RS_ECC) */

  memset (c->block, '\0', MSG_LEN);
  c->charCount = 0;

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

  /* release conversion specific items here (RS_ECC) */

  ckfree ((char*) c);
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
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (RS_ECC) */

  c->block [c->charCount] = character;
  c->charCount ++;

  if (c->charCount == (MSG_LEN-1)) {
    char out [CODE_LEN];

    c->block [MSG_LEN-1] = c->charCount; /* == MSG_LEN-1 */

    rsencode (c->block, (unsigned char*) out);

    /* not really required: memset (c->block, '\0', MSG_LEN); */
    c->charCount = 0;

    return c->write (c->writeClientData, (unsigned char*) out, CODE_LEN, interp);
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
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (RS_ECC) */

  char out [CODE_LEN], oldchar;
  int res;

  /*
   * Complete chunk with incoming data, generate EC-information,
   * then use all chunks contained in the buffer. Remember
   * an incomplete chunk and wait for further calls.
   */

  int k = (MSG_LEN-1) - c->charCount;

  if (k > bufLen) {
    /*
     * We got less information than required to form a full block.
     * Extend the internal buffer and wait for more.
     */
    memcpy ((VOID*) (c->block + c->charCount), (VOID*) buffer, bufLen);
    c->charCount += bufLen;
    return TCL_OK;
  }

  if (k < (MSG_LEN-1)) {
    memcpy ((VOID*) (c->block + c->charCount), (VOID*) buffer, k);

    c->block [MSG_LEN-1] = c->charCount; /* == MSG_LEN-1 */

    rsencode (c->block, (unsigned char*) out);

    c->charCount = 0;

    res = c->write (c->writeClientData, (unsigned char*) out, CODE_LEN, interp);

    buffer += k;
    bufLen -= k;
    
    if (res != TCL_OK)
      return res;
  } /* k == (MSG_LEN-1) => internal buffer was empty, so skip it entirely */



  while (bufLen > (MSG_LEN-1)) {
    /*
     * We are directly manipulating the buffer to get the charCount
     * of the individual chunks into it. To avoid memory overuns we
     * query for '>' at the begin of the loop and handle the special
     * case of 'bufLen == MSG_LEN-1' afterward.
     */

    oldchar = buffer [MSG_LEN-1];
    buffer [MSG_LEN-1] = (unsigned char) (MSG_LEN-1);

    rsencode (buffer, (unsigned char*) out);

    buffer [MSG_LEN-1] = oldchar;

    res = c->write (c->writeClientData, (unsigned char*) out, CODE_LEN, interp);

    buffer += MSG_LEN-1;
    bufLen -= MSG_LEN-1;

    if (res != TCL_OK)
      return res;
  }

  memcpy ((VOID*) c->block, (VOID*) buffer, bufLen);
  c->charCount = bufLen;

  if (bufLen == (MSG_LEN-1)) {
    c->block [MSG_LEN-1] = c->charCount; /* == MSG_LEN-1 */

    rsencode (c->block, (unsigned char*) out);

    c->charCount = 0;

    return c->write (c->writeClientData, (unsigned char*) out, CODE_LEN, interp);
  } /* else: nothing more to do to remember incomplete data */

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
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (RS_ECC) */

  if (c->charCount > 0) {
    char out [CODE_LEN];

    c->block [MSG_LEN-1] = c->charCount; /* < (MSG_LEN-1) */

    rsencode (c->block, (unsigned char*) out);

    return c->write (c->writeClientData, (unsigned char*) out, CODE_LEN, interp);
  }

  return TCL_OK;
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
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (RS_ECC) */

  memset (c->block, '\0', MSG_LEN);
  c->charCount = 0;
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
  DecoderControl* c;

  c = (DecoderControl*) ckalloc (sizeof (DecoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;

  /* initialize conversion specific items here (RS_ECC) */

  memset (c->block, '\0', CODE_LEN);
  c->charCount = 0;

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
ClientData       clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* release conversion specific items here (RS_ECC) */

  ckfree ((char*) c);
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
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (RS_ECC) */

  c->block [c->charCount] = character;
  c->charCount ++;

  if (c->charCount == CODE_LEN) {
    unsigned char msg [MSG_LEN];
    int  err;
    int  length;

    rsdecode (c->block, msg, &err);
    /* should check decoder result */

    /* not really required: memset (c->block, '\0', CODE_LEN); */
    c->charCount = 0;

    length = msg [MSG_LEN-1]; /* <= (MSG_LEN-1) */

    /* restrict length to legal range.
     * unrecoverable errors could have destroyed this information too!
     */

    if (length > (MSG_LEN-1))
      length = MSG_LEN-1;

    return c->write (c->writeClientData, msg, length, interp);
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
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (RS_ECC) */

  unsigned char msg [MSG_LEN];
  int  err, length, res;

  /*
   * Complete chunk with incoming data, generate EC-information,
   * then use all chunks contained in the buffer. Remember
   * an incomplete chunk and wait for further calls.
   */

  int k = (CODE_LEN-1) - c->charCount;

  if (k > bufLen) {
    /*
     * We got less information than required to form a full block.
     * Extend the internal buffer and wait for more.
     */
    memcpy ((VOID*) (c->block + c->charCount), (VOID*) buffer, bufLen);
    c->charCount += bufLen;
    return TCL_OK;
  }

  if (k < (CODE_LEN-1)) {
    memcpy ((VOID*) (c->block + c->charCount), (VOID*) buffer, k);

    rsdecode (c->block, msg, &err);

    length = msg [MSG_LEN-1]; /* <= (MSG_LEN-1) */

    /* restrict length to legal range.
     * unrecoverable errors could have destroyed this information too!
     */

    if (length > (MSG_LEN-1))
      length = MSG_LEN-1;

    res = c->write (c->writeClientData, msg, length, interp);

    c->charCount = 0;

    buffer += k;
    bufLen -= k;
    
    if (res != TCL_OK)
      return res;
  } /* k == (MSG_LEN-1) => internal buffer was empty, so skip it entirely */



  while (bufLen >= CODE_LEN) {

    rsdecode (buffer, msg, &err);
    /* should check decoder result */

    length = msg [MSG_LEN-1]; /* <= (MSG_LEN-1) */

    /* restrict length to legal range.
     * unrecoverable errors could have destroyed this information too!
     */

    if (length > (MSG_LEN-1))
      length = MSG_LEN-1;

    res = c->write (c->writeClientData, msg, length, interp);

    buffer += CODE_LEN;
    bufLen -= CODE_LEN;

    if (res != TCL_OK)
      return res;
  }


  if (bufLen > 0) {
    memcpy ((VOID*) c->block, (VOID*) buffer, bufLen);
    c->charCount = bufLen;
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
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (RS_ECC) */

  if (c->charCount > 0) {
    if (interp != NULL) {
      Tcl_AppendResult (interp, "can not decode incomplete block at end of input", (char*) NULL);
    }

    return TCL_ERROR;
  }

  return TCL_OK;
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
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (RS_ECC) */

  memset (c->block, '\0', CODE_LEN);
  c->charCount = 0;
}

/*
 * External code from here on.
 */

/* #include rs-ecc / *.c: THREADING: import of three constant vars, read-only => safe */
#include "rs-ecc/gflib.c"
#include "rs-ecc/rslib.c"
