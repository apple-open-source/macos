/*
 * b64code.c --
 *
 *	Implements and registers conversion from and to base64-encoded
 *	representation.
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
 * CVS: $Id: b64code.c,v 1.13 2009/05/07 04:57:27 andreas_kupries Exp $
 */

#include "transformInt.h"

/*
 * Converter description
 * ---------------------
 *
 * Encoding:
 *	Each sequence of 3 bytes is expanded into 4 printable characters
 *	using the 4 6bit-sequences contained in the 3 bytes. The mapping
 *	from 6bit value to printable characters is done with the BASE64 map.
 *	Special processing is done for incomplete byte sequences at the
 *	end of the input (1,2 bytes).
 *
 * Decoding:
 *	Each sequence of 4 characters is mapped into 4 6bit values using
 *	the reverse BASE64 map and then concatenated to form 3 8bit bytes.
 *	Special processing is done for incomplete character sequences at
 *	the end of the input (1,2,3 bytes).
 */


/*
 * Declarations of internal procedures.
 */

static Trf_ControlBlock CreateEncoder  _ANSI_ARGS_ ((ClientData writeClientData, Trf_WriteProc *fun,
						     Trf_Options optInfo, Tcl_Interp*   interp,
						     ClientData clientData));
static void             DeleteEncoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));
static int              Encode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     unsigned int character,
						     Tcl_Interp* interp,
						     ClientData clientData));
static int              FlushEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     Tcl_Interp* interp,
						     ClientData clientData));
static void             ClearEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));


static Trf_ControlBlock CreateDecoder  _ANSI_ARGS_ ((ClientData writeClientData, Trf_WriteProc *fun,
						     Trf_Options optInfo, Tcl_Interp*   interp,
						     ClientData clientData));
static void             DeleteDecoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));
static int              Decode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     unsigned int character,
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
  "base64",
  NULL, /* clientData not used by conversions. */
  NULL, /* set later by Trf_InitB64, THREADING: serialize initialization */
  {
    CreateEncoder,
    DeleteEncoder,
    Encode,
    NULL,
    FlushEncoder,
    ClearEncoder,
    NULL /* no MaxRead */
  }, {
    CreateDecoder,
    DeleteDecoder,
    Decode,
    NULL,
    FlushDecoder,
    ClearDecoder,
    NULL /* no MaxRead */
  },
  TRF_RATIO (3, 4)
};

/*
 * Definition of the control blocks for en- and decoder.
 */

typedef struct _EncoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  /* add conversion specific items here (base64 encode) */

  unsigned char charCount;
  unsigned char buf [3];

#define	QPERLIN (76 >> 2)	/* according to RFC 2045 */
  int  quads;

} EncoderControl;


typedef struct _DecoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  /* add conversion specific items here (base64 decode) */

  unsigned char charCount;
  unsigned char buf [4];
  unsigned char expectFlush;

} DecoderControl;


/*
 * Character mapping for base64 encode (bin -> ascii)
 *
 * Index this array by a 6-bit value to obtain the corresponding
 * 8-bit character. The last character (index 64) is the pad char (=)
 *                                                                                            |
 *                                      1         2         3         4         5         6   6
 *                            01234567890123456789012345678901234567890123456789012345678901234 */
static CONST char* baseMap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
/* basemap: THREADING: constant, read-only => safe */

#define PAD '='

/*
 * Character mappings for base64 decode (ascii -> bin)
 *
 * Index this array by a 8 bit value to get the 6-bit binary field
 * corresponding to that value.  Any illegal characters have high bit set.
 */

#define Ccc (CONST char) /* Ccc = CONST char cast */
static CONST char baseMapReverse [] = { /* THREADING: constant, read-only => safe */
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0076, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0077,
  Ccc 0064, Ccc 0065, Ccc 0066, Ccc 0067, Ccc 0070, Ccc 0071, Ccc 0072, Ccc 0073,
  Ccc 0074, Ccc 0075, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0000, Ccc 0001, Ccc 0002, Ccc 0003, Ccc 0004, Ccc 0005, Ccc 0006,
  Ccc 0007, Ccc 0010, Ccc 0011, Ccc 0012, Ccc 0013, Ccc 0014, Ccc 0015, Ccc 0016,
  Ccc 0017, Ccc 0020, Ccc 0021, Ccc 0022, Ccc 0023, Ccc 0024, Ccc 0025, Ccc 0026,
  Ccc 0027, Ccc 0030, Ccc 0031, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0032, Ccc 0033, Ccc 0034, Ccc 0035, Ccc 0036, Ccc 0037, Ccc 0040,
  Ccc 0041, Ccc 0042, Ccc 0043, Ccc 0044, Ccc 0045, Ccc 0046, Ccc 0047, Ccc 0050,
  Ccc 0051, Ccc 0052, Ccc 0053, Ccc 0054, Ccc 0055, Ccc 0056, Ccc 0057, Ccc 0060,
  Ccc 0061, Ccc 0062, Ccc 0063, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  /* */
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200,
  Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200, Ccc 0200
};
#undef Ccc


/*
 *------------------------------------------------------*
 *
 *	TrfInit_B64 --
 *
 *	------------------------------------------------*
 *	Register the conversion implemented in this file.
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
TrfInit_B64 (interp)
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
ClientData clientData;
{
  EncoderControl* c;

  c = (EncoderControl*) ckalloc (sizeof (EncoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;

  /* initialize conversion specific items here (base64 encode) */

  c->charCount = 0;
  memset (c->buf, '\0', 3);
  c->quads = 0;

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

  /* release conversion specific items here (base64 encode) */

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

  /* execute conversion specific code here (base64 encode) */

  c->buf [c->charCount] = character;
  c->charCount ++;

  if (c->charCount == 3) {
    int i;
    unsigned char buf [4];

    TrfSplit3to4     (c->buf, buf, 3);

#define CRLF_AT(i,v) ((v[i] == '\r') && (v[i+1] == '\n'))
#define CRLF_IN(v) (CRLF_AT(0,v) || CRLF_AT(1,v))

    /* dbg + * /
#if 0
    if (CRLF_IN (c->buf)){ 
      printf ("split (%d,%d,%d) = %d,%d,%d,%d = ",
	      c->buf [0], c->buf [1], c->buf [2],
	      buf [0], buf [1], buf [2], buf [3]);
    }
#endif
    / **/

    TrfApplyEncoding (buf, 4, baseMap);

    /** /
    if (CRLF_IN (c->buf)) {
      printf ("%c%c%c%c\n", buf [0], buf [1], buf [2], buf [3]);
      fflush (stdout);
    }
    / * dbg - */

    c->charCount = 0;
    memset (c->buf, '\0', 3);

    if ((i = c->write (c->writeClientData, buf, 4, interp)) != TCL_OK)
      return i;
    if (++c->quads >= QPERLIN) {
      c->quads = 0;
      return c->write (c->writeClientData, (unsigned char*) "\n", 1, interp);
    }
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

  /* execute conversion specific code here (base64 encode) */

  if (c->charCount > 0) {
    int i;
    unsigned char buf [4];

    TrfSplit3to4     (c->buf, buf, c->charCount);
    TrfApplyEncoding (buf, 4, baseMap);

    c->charCount = 0;
    memset (c->buf, '\0', 3);

    if ((i = c->write (c->writeClientData, buf, 4, interp)) != TCL_OK)
      return i;
  }

  c->quads = 0;
  return c->write (c->writeClientData, (unsigned char*) "\n", 1, interp);
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

  /* execute conversion specific code here (base64 encode) */

  c->charCount = 0;
  memset (c->buf, '\0', 3);
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
ClientData clientData;
{
  DecoderControl* c;

  c = (DecoderControl*) ckalloc (sizeof (DecoderControl));
  c->write           = fun;
  c->writeClientData = writeClientData;

  /* initialize conversion specific items here (base64 decode) */

  c->charCount = 0;
  memset (c->buf, '\0', 4);
  c->expectFlush = 0;

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

  /* release conversion specific items here (base64 decode) */

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

  /* execute conversion specific code here (base64 decode) */

  if ((character == '\r') || (character == '\n'))
      return TCL_OK;

  /* ignore any ! illegal character - RFC 2045 */

  if (((char) baseMapReverse [character]) & 0x80)
    return TCL_OK;

  if (c->expectFlush) {
    /*
     * We had a quadruple with pad characters at the last call,
     * this had to be the last characters in input!  coming here
     * now indicates, that the padding characters were in the
     * middle of the string, therefore illegal.
     */

    if (interp) {
      Tcl_ResetResult  (interp);
      Tcl_AppendResult (interp, "illegal padding inside the string", (char*) NULL);
    }
    return TCL_ERROR;
  }


  c->buf [c->charCount] = character;
  c->charCount ++;

  if (c->charCount == 4) {
    int res, hasPadding;
    unsigned char buf [3];

    hasPadding = 0;
    res = TrfReverseEncoding (c->buf, 4, baseMapReverse,
			      PAD, &hasPadding);

    if (res != TCL_OK) {
      if (interp) {
	Tcl_ResetResult  (interp);
	Tcl_AppendResult (interp, "illegal character found in input", (char*) NULL);
      }
      return res;
    }

    if (hasPadding)
      c->expectFlush = 1;

    TrfMerge4to3 (c->buf, buf);

    c->charCount = 0;
    memset (c->buf, '\0', 4);

    return c->write (c->writeClientData, buf, 3-hasPadding, interp);
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

  /* execute conversion specific code here (base64 decode) */

  /*
   * expectFlush && c->charcount > 0 impossible, catched
   * in 'Decode' already.
   */

  if (c->charCount > 0) {
    /*
     * Convert, as if padded with the pad-character.
     */

    int res, hasPadding;
    unsigned char buf [3];

    hasPadding = 0;
    res = TrfReverseEncoding (c->buf, c->charCount, baseMapReverse,
			      PAD, &hasPadding);

    if (res != TCL_OK) {
      if (interp) {
	Tcl_ResetResult  (interp);
	Tcl_AppendResult (interp, "illegal character found in input", (char*) NULL);
      }
      return res;
    }

    TrfMerge4to3 (c->buf, buf);

    c->charCount = 0;
    memset (c->buf, '\0', 4);

    return c->write (c->writeClientData, buf, 3-hasPadding, interp);
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
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (base64 decode) */

  c->charCount = 0;
  memset (c->buf, '\0', 4);
  c->expectFlush = 0;
}
