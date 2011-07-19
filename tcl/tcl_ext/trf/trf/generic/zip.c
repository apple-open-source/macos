/*
 * zip.c --
 *
 *	Implements and registers compressor based on deflate (LZ77 variant).
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
 * CVS: $Id: zip.c,v 1.21 2009/05/07 04:57:27 andreas_kupries Exp $
 */

#include "transformInt.h"

/*
 * Declarations of internal procedures.
 */

static Trf_ControlBlock
CreateEncoder  _ANSI_ARGS_ ((ClientData     writeClientData,
			     Trf_WriteProc* fun,
			     Trf_Options    optInfo,
			     Tcl_Interp*    interp,
			     ClientData     clientData));
static void
DeleteEncoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData       clientData));
static int
Encode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     unsigned int     character,
			     Tcl_Interp*      interp,
			     ClientData       clientData));
static int
EncodeBuffer   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     unsigned char*   buffer,
			     int              bufLen,
			     Tcl_Interp*      interp,
			     ClientData       clientData));
static int
FlushEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     Tcl_Interp*      interp,
			     ClientData       clientData));
static void
ClearEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData clientData));

static Trf_ControlBlock
CreateDecoder  _ANSI_ARGS_ ((ClientData     writeClientData,
			     Trf_WriteProc* fun,
			     Trf_Options    optInfo,
			     Tcl_Interp*    interp,
			     ClientData     clientData));
static void
DeleteDecoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData       clientData));
static int
Decode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     unsigned int     character,
			     Tcl_Interp*      interp,
			     ClientData       clientData));
static int
DecodeBuffer   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     unsigned char*   buffer,
			     int              bufLen,
			     Tcl_Interp*      interp,
			     ClientData       clientData));
static int
FlushDecoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     Tcl_Interp*      interp,
			     ClientData       clientData));
static void
ClearDecoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData       clientData));

static void
ZlibError _ANSI_ARGS_ ((Tcl_Interp* interp,
			z_streamp   state,
			int         errcode,
			CONST char* prefix));

static const char*
ZlibErrorMsg _ANSI_ARGS_ ((z_streamp   state,
			   int         errcode));

static int
MaxReadDecoder _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData       clientData));

/*
 * Converter definition.
 */

static Trf_TypeDefinition convDefinition =
{
  "zip",
  NULL, /* client data not used       */
  NULL, /* filled later (TrfInit_ZIP), THREADING: serialize initialization */
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
    MaxReadDecoder
  },
  TRF_UNSEEKABLE
};

/*
 * Definition of the control blocks for en- and decoder.
 */

typedef struct _EncoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  /* add conversion specific items here (ZIP) */

  z_stream state;	/* compressor state */

  char* output_buffer;

} EncoderControl;


typedef struct _DecoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;
  int            nowrap;

  /* add conversion specific items here (ZIP) */

  z_stream state;	/* decompressor state */
  char*    output_buffer;
  int      stop;        /* Boolean flag. Set after
			 * reaching Z_STREAM_END */
} DecoderControl;

#define KILO     (1024)
#define OUT_SIZE (32 * KILO)


/*
 *------------------------------------------------------*
 *
 *	TrfInit_ZIP --
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
TrfInit_ZIP (interp)
Tcl_Interp* interp;
{
  TrfLock; /* THREADING: serialize initialization */
  convDefinition.options = TrfZIPOptions ();
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
  TrfZipOptionBlock* o = (TrfZipOptionBlock*) optInfo;
  int res;

  START (ZipCreateEncoder); 

  c = (EncoderControl*) ckalloc (sizeof (EncoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;

  /* initialize conversion specific items here (ZIP) */

  c->state.zalloc = Z_NULL;
  c->state.zfree  = Z_NULL;
  c->state.opaque = Z_NULL;

  c->output_buffer = (char*) ckalloc (OUT_SIZE);

  if (c->output_buffer == (char*) NULL) {
    ckfree ((VOID*) c);
    DONE (ZipCreateEncoder); 
    return (ClientData) NULL;
  }

  PRINT ("deflateInit (%d, %s)\n", o->level, ZLIB_VERSION); FL;

#if 0
  res = zf.zdeflateInit_ (&c->state, o->level,
			 ZLIB_VERSION, sizeof(z_stream));
#endif

  res = zf.zdeflateInit2_ (&c->state, o->level, Z_DEFLATED,
			 o->nowrap  ?
			 -MAX_WBITS :
			 MAX_WBITS,
			 MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY,
			 ZLIB_VERSION, sizeof(z_stream));

  IN; PRINTLN (ZlibErrorMsg (&c->state, res)); FL; OT;

  if (res != Z_OK) {
    if (interp) {
      ZlibError (interp, &c->state, res, "compressor/init");
    }

    ckfree ((VOID*) c->output_buffer);
    ckfree ((VOID*) c);
    DONE (ZipCreateEncoder); 
    return (ClientData) NULL;
  }

  DONE (ZipCreateEncoder); 
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

  /* release conversion specific items here (ZIP) */

  START (ZipDeleteEncoder); 
  PRINT ("deflateEnd ()\n"); FL;

  zf.zdeflateEnd (&c->state);

  ckfree ((char*) c->output_buffer);
  ckfree ((char*) c);

  DONE (ZipDeleteEncoder); 
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

  /* execute conversion specific code here (ZIP) */

  char in;
  int res;

  START (ZipEncode); 

  in = character;

  c->state.next_in   = (Bytef*) &in;
  c->state.avail_in  = 1;

  for (;;) {
    if (c->state.avail_in <= 0) {
      PRINTLN ("Nothing to process");
      break;
    }

    c->state.next_out  = (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    PRINT ("deflate (Z_NO_FLUSH)\n"); FL;
    res = zf.zdeflate (&c->state, Z_NO_FLUSH);

    IN; PRINTLN (ZlibErrorMsg (&c->state, res)); FL; OT;

    if (res < Z_OK) {
      if (interp) {
	ZlibError (interp, &c->state, res, "compressor");
      }
      DONE (ZipEncode); 
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	DONE (ZipEncode); 
	return res;
      }
    }

    if (c->state.avail_in > 0)
      continue;

    if ((c->state.avail_out == 0) && (res == Z_OK))
      continue;

    break;
  }

  DONE (ZipEncode); 
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

  /* execute conversion specific code here (ZIP) */

  int res;

  START (ZipEncodeBuffer); 
  PRINT ("Data = %d {\n", bufLen);
  DUMP  (bufLen, buffer);
  PRINT ("}\n");

  c->state.next_in   = (Bytef*) buffer;
  c->state.avail_in  = bufLen;

  for (;;) {
    if (c->state.avail_in <= 0) {
      PRINTLN ("Nothing to process");
      break;
    }

    c->state.next_out  = (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    PRINT ("deflate (Z_NO_FLUSH)\n"); FL;
    res = zf.zdeflate (&c->state, Z_NO_FLUSH);

    IN; PRINTLN (ZlibErrorMsg (&c->state, res)); FL; OT;

    if (res < Z_OK) {
      if (interp) {
	ZlibError (interp, &c->state, res, "compressor");
      }
      DONE (ZipEncodeBuffer); 
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	DONE (ZipEncodeBuffer); 
	return res;
      }
    }

    if (c->state.avail_in > 0)
      continue;

    if ((c->state.avail_out == 0) && (res == Z_OK))
      continue;

    break;
  }

  DONE (ZipEncodeBuffer); 
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

  /* execute conversion specific code here (ZIP) */

  int res;

  START (ZipFlushEncoder); 

  c->state.next_in   = (Bytef*) NULL;
  c->state.avail_in  = 0;

  for (;;) {
    c->state.next_out  = (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    PRINT ("deflate (Z_FINISH)\n"); FL;
    res = zf.zdeflate (&c->state, Z_FINISH);

    IN; PRINTLN (ZlibErrorMsg (&c->state, res)); FL; OT;

    if (res < Z_OK) {
      if (interp) {
	ZlibError (interp, &c->state, res, "compressor/flush");
      }
      DONE (ZipFlushEncoder); 
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	DONE (ZipFlushEncoder); 
	return res;
      }
    }

    if ((c->state.avail_out == 0) && (res == Z_OK))
      continue;

    break;
  }

  DONE (ZipFlushEncoder); 
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
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  START (ZipClearEncoder); 
  PRINT ("deflateReset ()\n"); FL;

  /* execute conversion specific code here (ZIP) */

  zf.zdeflateReset (&c->state);

  DONE (ZipClearEncoder); 
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
  TrfZipOptionBlock* o = (TrfZipOptionBlock*) optInfo;
  int res;

  START (ZipCreateDecoder); 

  c = (DecoderControl*) ckalloc (sizeof (DecoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;
  c->nowrap          = o->nowrap;
  c->stop            = 0;

  /* initialize conversion specific items here (ZIP) */

  c->state.zalloc = Z_NULL;
  c->state.zfree  = Z_NULL;
  c->state.opaque = Z_NULL;

  c->output_buffer = (char*) ckalloc (OUT_SIZE);

  if (c->output_buffer == (char*) NULL) {
    ckfree ((VOID*) c);
    DONE (ZipCreateDecoder); 
    return (ClientData) NULL;
  }

  PRINT ("inflateInit (%s, nowrap=%d)\n", ZLIB_VERSION, o->nowrap); FL;

#if 0
  res = zf.zinflateInit_ (&c->state,
			 ZLIB_VERSION, sizeof (z_stream));
#endif

  res = zf.zinflateInit2_ (&c->state,
			 o->nowrap  ?
			 -MAX_WBITS :
			 MAX_WBITS,
			 ZLIB_VERSION, sizeof (z_stream));

  IN; PRINTLN (ZlibErrorMsg (&c->state, res)); FL; OT;

  if (res != Z_OK) {
    if (interp) {
      ZlibError (interp, &c->state, res, "decompressor/init");
    }

    ckfree ((VOID*) c->output_buffer);
    ckfree ((VOID*) c);
    DONE (ZipCreateDecoder); 
    return (ClientData) NULL;
  }

  DONE (ZipCreateDecoder); 
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

  /* release conversion specific items here (ZIP) */

  START (ZipDeleteDecoder); 
  PRINT ("inflateEnd ()\n"); FL;

  zf.zinflateEnd (&c->state);

  ckfree ((char*) c->output_buffer);
  ckfree ((char*) c);

  DONE (ZipDeleteDecoder); 

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

  /* execute conversion specific code here (ZIP) */
  char in;
  int res;

  START (ZipDecode); 
  /*
  if (c->stop) {
    PRINTLN ("stopped");
    DONE (ZipDecode);
    return TCL_OK;
  }
  */
  in = character;

  c->state.next_in   = (Bytef*) &in;
  c->state.avail_in  = 1;

  for (;;) {
    if (c->state.avail_in <= 0) {
      PRINTLN ("Nothing to process");
      break;
    }

    c->state.next_out  = (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    PRINT ("inflate (Z_NO_FLUSH)\n"); FL;
    res = zf.zinflate (&c->state, Z_NO_FLUSH);

    IN; PRINTLN (ZlibErrorMsg (&c->state, res)); FL; OT;

    if (res < Z_OK) {
      if (interp) {
	ZlibError (interp, &c->state, res, "decompressor");
      }
      DONE (ZipDecode);
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	DONE (ZipDecode);
	return res;
      }
    }

    /* 29.11.1999, AK */
    if (res == Z_STREAM_END) {
      /* Don't process the remainining characters, they are not part of the
       * compressed stream. Push them back into the channel downward and then
       * fake our upstream user into EOF.
       */

      PRINTLN ("STOP");
      c->stop = 1;
      break;
    } /**/

    if (c->state.avail_in > 0) {
      PRINTLN ("More to process");
      continue;
    }

    if ((c->state.avail_out == 0) && (res == Z_OK)) {
      PRINTLN ("Output space exhausted, still ok");
      continue;
    }

    break;
  }

  DONE (ZipDecode);
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

  /* execute conversion specific code here (ZIP) */
  int res;

  START (ZipDecodeBuffer);
  PRINT ("Data = %d {\n", bufLen);
  DUMP  (bufLen, buffer);
  PRINT ("}\n");
  /*
  if (c->stop) {
    PRINTLN ("stopped");
    DONE (ZipDecodeBuffer);
    return TCL_OK;
  }
  */
  c->state.next_in   = (Bytef*) buffer;
  c->state.avail_in  = bufLen;

  for (;;) {
    if (c->state.avail_in <= 0) {
      PRINTLN ("Nothing to process");
      break;
    }

    c->state.next_out  = (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    PRINT ("inflate (Z_NO_FLUSH)\n"); FL;
    res = zf.zinflate (&c->state, Z_NO_FLUSH);

    IN; PRINTLN (ZlibErrorMsg (&c->state, res)); FL; OT;

    if (res < Z_OK) {
      if (interp) {
	ZlibError (interp, &c->state, res, "decompressor");
      }
      DONE (ZipDecodeBuffer); 
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	DONE (ZipDecodeBuffer); 
	return res;
      }
    }

    /* 29.11.1999, AK */
    if (res == Z_STREAM_END) {
      /* Don't process the remainining characters, they are not part of the
       * compressed stream. Push them back into the channel downward and then
       * fake our upstream user into EOF.
       */

      PRINTLN ("STOP");
      c->stop = 1;
      break;
    }/**/


    if (c->state.avail_in > 0) {
      PRINTLN ("More to process");
      continue;
    }

    if ((c->state.avail_out == 0) && (res == Z_OK)) {
      PRINTLN ("Output space exhausted, still ok");
      continue;
    }

    break;
  }

  DONE (ZipDecodeBuffer); 
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

  /* execute conversion specific code here (ZIP) */

  int res;

  START (ZipFlushDecoder); 

  c->state.next_in  = (Bytef*) c->output_buffer; /* fake out 'inflate' */
  c->state.avail_in = 0;

  for (;;) {
    c->state.next_out  = (Bytef*) c->output_buffer;
    c->state.avail_out = OUT_SIZE;

    if (c->nowrap) {
      /*
       *   Hack required by zlib: Supply an additional dummy
       *   byte in order to force the generation of Z_STREAM_END.
       */

      c->state.avail_in = 1;
    }

    PRINT ("inflate (Z_FINISH)\n"); FL;
    res = zf.zinflate (&c->state, Z_FINISH);

    IN; PRINTLN (ZlibErrorMsg (&c->state, res));
    FL; OT;

    if ((res < Z_OK) || (res == Z_NEED_DICT)) {
      if (interp) {
	ZlibError (interp, &c->state, res, "decompressor/flush");
      }
      DONE (ZipFlushDecoder); 
      return TCL_ERROR;
    }

    if (c->state.avail_out < OUT_SIZE) {
      res = c->write (c->writeClientData, (unsigned char*) c->output_buffer,
		      OUT_SIZE - c->state.avail_out, interp);
      if (res != TCL_OK) {
	DONE (ZipFlushDecoder); 
	return res;
      }
    }

    if ((c->state.avail_out == 0) && (res == Z_OK))
      continue;

    break;
  }

  DONE (ZipFlushDecoder); 
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
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  START (ZipClearDecoder); 
  PRINT ("inflateReset ()\n"); FL;

  /* execute conversion specific code here (ZIP) */

  zf.zinflateReset (&c->state);

  DONE (ZipClearDecoder); 
}

/*
 *------------------------------------------------------*
 *
 *	MaxReadDecoder --
 *
 *	------------------------------------------------*
 *	Depending on the state of the decompressor the layer above is
 *	told to read exactly one byte or nothing at all. The latter
 *	happens iff the decompressor found that he is at the end of
 *	the compressed stream (Z_STREAM_END). The former (reading
 *	exactly one byte) is necessary to avoid reading data behind
 *	the end of the compressed stream. We are unable to give this
 *	data back to the channel below, i.e. it is lost after this
 *	transformation is unstacked. A most undesirable property. The
 *	disadvantage of the current solution is of course a loss of
 *	performance.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		The number of characters the generic Trf layer is
 *		allowed to read from the channel below.
 *
 *------------------------------------------------------*
 */

static int
MaxReadDecoder (ctrlBlock, clientData)
     Trf_ControlBlock ctrlBlock;
     ClientData       clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;
  int           max = c->stop ? 0 : 1;

  START (MaxReadDecoder);
  PRINT ("Stop = %d --> %d\n", c->stop, max); FL;
  DONE  (MaxReadDecoder); 

  return max;
}

/*
 *------------------------------------------------------*
 *
 *	ZlibError --
 *
 *	------------------------------------------------*
 *	Append error message from zlib-state to interpreter
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
ZlibError (interp, state, errcode, prefix)
Tcl_Interp* interp;
z_streamp   state;
int         errcode;
CONST char* prefix;
{
  Tcl_AppendResult (interp, "zlib error (", (char*) NULL);
  Tcl_AppendResult (interp, prefix, (char*) NULL);
  Tcl_AppendResult (interp, "): ", (char*) NULL);
  Tcl_AppendResult (interp, ZlibErrorMsg (state, errcode), (char*) NULL);
}

/*
 *------------------------------------------------------*
 *
 *	ZlibErrorMsg --
 *
 *	------------------------------------------------*
 *	Return the error message from the zlib-state.
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

static const char*
ZlibErrorMsg (state, errcode)
z_streamp   state;
int         errcode;
{
  CONST char* msg;

  if (state->msg != NULL) {
    msg = state->msg;
  } else {
    /*
     * A table-lookup might have been nicer, but this
     * is more secure against changes of the codevalues
     * used by zlib.
     */
    switch (errcode) {
    case Z_MEM_ERROR:
      msg =  "not enough memory available";
      break;

    case Z_BUF_ERROR:
      msg = "no progress was possible";
      break;

    case Z_STREAM_ERROR:
      msg = "inconsistent stream state";
      break;
      
    case Z_DATA_ERROR:
      msg = "incoming data corrupted";
      break;
      
    case Z_VERSION_ERROR:
      msg = "inconsistent version";
      break;
      
    case Z_NEED_DICT:
      msg = "dictionary required";
      break;

    case Z_STREAM_END:
      msg = "stream ends here, flushed out";
      break;

    case Z_OK:
      msg = "ok";
      break;
      
    default:
      msg = "?";
      break;
    }
  }

  return msg;
}
