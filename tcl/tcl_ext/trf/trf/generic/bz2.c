/*
 * bz2.c --
 *
 *	Implements and registers compressor based on bzip2.
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
 * CVS: $Id: bz2.c,v 1.8 2009/05/07 04:57:27 andreas_kupries Exp $
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

static void Bz2libError _ANSI_ARGS_ ((Tcl_Interp* interp,
				    bz_stream*  state,
				    int         errcode,
				    CONST char* prefix));


/*
 * Converter definition.
 */

static Trf_TypeDefinition convDefinition =
{
  "bz2",
  NULL, /* client data not used       */
  NULL, /* filled by TrfInit_ZB2, THREADING: serialize initialization */
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

  /* add conversion specific items here (BZ2) */

  bz_stream state;	/* compressor state */

  char* output_buffer;

} EncoderControl;


typedef struct _DecoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  /* add conversion specific items here (BZ2) */

  bz_stream state;	/* decompressor state */

  char* output_buffer;

  int lastRes;

} DecoderControl;

#define KILO     (1024)
#define OUT_SIZE (32 * KILO)


/*
 *------------------------------------------------------*
 *
 *	TrfInit_BZ2 --
 *
 *	------------------------------------------------*
 *	Register the compressor implemented in this file.
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
TrfInit_BZ2 (interp)
Tcl_Interp* interp;
{
  TrfLock; /* THREADING: serialize initialization */
  convDefinition.options = TrfBZ2Options ();
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
ClientData     writeClientData;
Trf_WriteProc* fun;
Trf_Options    optInfo;
Tcl_Interp*    interp;
ClientData     clientData;
{
  EncoderControl*    c;
  TrfBz2OptionBlock* o = (TrfBz2OptionBlock*) optInfo;
  int res;

  c = (EncoderControl*) ckalloc (sizeof (EncoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;

  /* initialize conversion specific items here (BZ2) */

  c->state.bzalloc = NULL;
  c->state.bzfree  = NULL;
  c->state.opaque = NULL;

  c->output_buffer = (char*) ckalloc (OUT_SIZE);

  if (c->output_buffer == (char*) NULL) {
    ckfree ((VOID*) c);
    return (ClientData) NULL;
  }

  res = bz.bcompressInit (&c->state, o->level, 0, 0);

  if (res != BZ_OK) {
    if (interp) {
      Bz2libError (interp, &c->state, res, "compressor/init");
    }

    ckfree ((VOID*) c->output_buffer);
    ckfree ((VOID*) c);
    return (ClientData) NULL;
  }

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
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* release conversion specific items here (BZ2) */

  bz.bcompressEnd (&c->state);
  ckfree ((char*) c->output_buffer);
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
unsigned int character;
Tcl_Interp* interp;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (BZ2) */

  char in;
  int res;

  in = character;

  c->state.next_in   = (unsigned char*) (Bytef*) &in;
  c->state.avail_in  = 1;

  for (;;) {
    c->state.next_out  = (unsigned char*) (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    res = bz.bcompress (&c->state, BZ_RUN);

    if (res < BZ_OK) {
      if (interp) {
	Bz2libError (interp, &c->state, res, "compressor");
      }
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	return res;
      }
    }

    if (c->state.avail_in > 0)
      continue;

    if ((c->state.avail_out == 0) && (res == BZ_OK))
      continue;

    break;
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
int         bufLen;
Tcl_Interp* interp;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (BZ2) */
  int res;

  c->state.next_in   = (unsigned char*) (Bytef*) buffer;
  c->state.avail_in  = bufLen;

  for (;;) {
    c->state.next_out  = (unsigned char*) (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    res = bz.bcompress (&c->state, BZ_RUN);

    if (res < BZ_OK) {
      if (interp) {
	Bz2libError (interp, &c->state, res, "compressor");
      }
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	return res;
      }
    }

    if (c->state.avail_in > 0)
      continue;

    if ((c->state.avail_out == 0) && (res == BZ_OK))
      continue;

    break;
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
Tcl_Interp* interp;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (BZ2) */
  int res;

  c->state.next_in   = (unsigned char*) (Bytef*) NULL;
  c->state.avail_in  = 0;

  for (;;) {
    c->state.next_out  = (unsigned char*) (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    res = bz.bcompress (&c->state, BZ_FINISH);

    if (res < BZ_OK) {
      if (interp) {
	Bz2libError (interp, &c->state, res, "compressor/flush");
      }
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	return res;
      }
    }

    if ((c->state.avail_out == 0) && (res == BZ_OK))
      continue;

    break;
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
ClientData clientData;
{
  /* EncoderControl* c = (EncoderControl*) ctrlBlock; */

  /* execute conversion specific code here (BZ2) */

  /* bz.bcompressReset (&c->state); */
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
ClientData     writeClientData;
Trf_WriteProc* fun;
Trf_Options    optInfo;
Tcl_Interp*    interp;
ClientData     clientData;
{
  DecoderControl*    c;
  int res;

  c = (DecoderControl*) ckalloc (sizeof (DecoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;

  /* initialize conversion specific items here (BZ2) */

  c->state.bzalloc = NULL;
  c->state.bzfree  = NULL;
  c->state.opaque = NULL;

  c->output_buffer = (char*) ckalloc (OUT_SIZE);

  if (c->output_buffer == (char*) NULL) {
    ckfree ((VOID*) c);
    return (ClientData) NULL;
  }

  res = bz.bdecompressInit (&c->state, 0, 0);

  if (res != BZ_OK) {
    if (interp) {
      Bz2libError (interp, &c->state, res, "decompressor/init");
    }

    ckfree ((VOID*) c->output_buffer);
    ckfree ((VOID*) c);
    return (ClientData) NULL;
  }

  c->lastRes = res;

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

  /* release conversion specific items here (BZ2) */

  bz.bdecompressEnd (&c->state);

  ckfree ((char*) c->output_buffer);
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
unsigned int character;
Tcl_Interp* interp;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (BZ2) */
  char in;
  int res;

  in = character;

  c->state.next_in   = (unsigned char*) (Bytef*) &in;
  c->state.avail_in  = 1;

  for (;;) {
    c->state.next_out  = (unsigned char*) (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    res = bz.bdecompress (&c->state);
    c->lastRes = res;

    if ((res < BZ_OK) && (res != BZ_STREAM_END)) {
      if (interp) {
	Bz2libError (interp, &c->state, res, "decompressor");
      }
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	return res;
      }
    }

    if (c->state.avail_in > 0)
      continue;

    if ((c->state.avail_out == 0) && (res == BZ_OK))
      continue;

    break;
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
unsigned char* buffer;
int bufLen;
Tcl_Interp* interp;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (BZ2) */
  int res;

  c->state.next_in   = (unsigned char*) (Bytef*) buffer;
  c->state.avail_in  = bufLen;

  for (;;) {
    c->state.next_out  = (unsigned char*) (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    res = bz.bdecompress (&c->state);
    c->lastRes = res;

    if ((res < BZ_OK) && (res != BZ_STREAM_END)) {
      if (interp) {
	Bz2libError (interp, &c->state, res, "decompressor");
      }
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	return res;
      }
    }

    if (c->state.avail_in > 0)
      continue;

    if ((c->state.avail_out == 0) && (res == BZ_OK))
      continue;

    break;
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
Tcl_Interp* interp;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (BZ2) */
  int res;

  if (c->lastRes == BZ_STREAM_END) {
    /* Essentially already flushed ! */
    return TCL_OK;
  }

  c->state.next_in  = (unsigned char*) (Bytef*) c->output_buffer; /* fake out
								   * 'inflate'
								   */
  c->state.avail_in = 0;

  for (;;) {
    c->state.next_out  = (unsigned char*) (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    res = bz.bdecompress (&c->state);

    if ((res < BZ_OK) && (res != BZ_STREAM_END)) {
      if (interp) {
	Bz2libError (interp, &c->state, res, "decompressor/flush");
      }
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	return res;
      }
    }

    if ((c->state.avail_out == 0) && (res == BZ_OK))
      continue;

    break;
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
ClientData clientData;
{
  /*  DecoderControl* c = (DecoderControl*) ctrlBlock; */

  /* execute conversion specific code here (BZ2) */

  /* bz.bdecompressReset (&c->state); */
}

/*
 *------------------------------------------------------*
 *
 *	Bz2libError --
 *
 *	------------------------------------------------*
 *	Append error message from bzlib-state to interpreter
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
Bz2libError (interp, state, errcode, prefix)
Tcl_Interp* interp;
bz_stream*   state;
int         errcode;
CONST char* prefix;
{
    CONST char* msg;

    /*
     * A table-lookup might have been nicer, but this
     * is more secure against changes of the codevalues
     * used by bzlib.
     */
    switch (errcode) {
    case BZ_MEM_ERROR:
      msg =  "not enough memory available";
      break;

    case BZ_SEQUENCE_ERROR:
      msg =  "sequence error";
      break;

    case BZ_PARAM_ERROR:
      msg =  "param error";
      break;

    case BZ_DATA_ERROR:
      msg = "incoming data corrupted";
      break;

    case BZ_DATA_ERROR_MAGIC:
      msg = "magic number corrupted";
      break;

    case BZ_IO_ERROR:
      msg = "io error";
      break;

    case BZ_UNEXPECTED_EOF:
      msg = "unexpected eof";
      break;

    case BZ_OUTBUFF_FULL:
      msg = "output buffer full";
      break;

    default:
      msg = "?";
      break;
    }

  Tcl_AppendResult (interp, "bz2lib error (", (char*) NULL);
  Tcl_AppendResult (interp, prefix, (char*) NULL);
  Tcl_AppendResult (interp, "): ", (char*) NULL);
  Tcl_AppendResult (interp, msg, (char*) NULL);
}
