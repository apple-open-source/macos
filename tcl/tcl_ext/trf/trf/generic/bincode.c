/*
 * bincode.c --
 *
 *	Implements and registers conversion from and to binary representation.
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
 * CVS: $Id: bincode.c,v 1.12 2009/05/07 04:57:27 andreas_kupries Exp $
 */

#include <limits.h>
#include "transformInt.h"

/* On some systems this definition seems to be missing.
 */

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif


/*
 * Converter description
 * ---------------------
 *
 * Encoding:
 *	Every byte is converted into CHAR_BIT characters, using elements
 *	in the set {"0", "1"} only. Thus a binary representation is
 *	generated. The MSBit is output first.
 *
 * Decoding:
 *	Only characters in the set {"0", "1"} are allowed as input.
 *	Each (CHAR_BIT)-tuple is converted into a single byte. A partial
 *	tuple at the end of input is converted as if padded with "0"s.
 */


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
			     ClientData       clientData));


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


/*
 * Converter definition.
 */

static Trf_TypeDefinition convDefinition =
{
  "bin",
  NULL, /* clientData not used by converters */
  NULL, /* set later by TrfInit_Bin, THREADING: serialize initialization */
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
  TRF_RATIO (1, 8)
};

/*
 * Use table lookup
 */

static const char* code [] = { /* THREADING: constant, read-only => safe */
  "00000000", "00000001", "00000010", "00000011", "00000100", "00000101", "00000110", "00000111",
  "00001000", "00001001", "00001010", "00001011", "00001100", "00001101", "00001110", "00001111",
  "00010000", "00010001", "00010010", "00010011", "00010100", "00010101", "00010110", "00010111",
  "00011000", "00011001", "00011010", "00011011", "00011100", "00011101", "00011110", "00011111",
  "00100000", "00100001", "00100010", "00100011", "00100100", "00100101", "00100110", "00100111",
  "00101000", "00101001", "00101010", "00101011", "00101100", "00101101", "00101110", "00101111",
  "00110000", "00110001", "00110010", "00110011", "00110100", "00110101", "00110110", "00110111",
  "00111000", "00111001", "00111010", "00111011", "00111100", "00111101", "00111110", "00111111",
  "01000000", "01000001", "01000010", "01000011", "01000100", "01000101", "01000110", "01000111",
  "01001000", "01001001", "01001010", "01001011", "01001100", "01001101", "01001110", "01001111",
  "01010000", "01010001", "01010010", "01010011", "01010100", "01010101", "01010110", "01010111",
  "01011000", "01011001", "01011010", "01011011", "01011100", "01011101", "01011110", "01011111",
  "01100000", "01100001", "01100010", "01100011", "01100100", "01100101", "01100110", "01100111",
  "01101000", "01101001", "01101010", "01101011", "01101100", "01101101", "01101110", "01101111",
  "01110000", "01110001", "01110010", "01110011", "01110100", "01110101", "01110110", "01110111",
  "01111000", "01111001", "01111010", "01111011", "01111100", "01111101", "01111110", "01111111",
  "10000000", "10000001", "10000010", "10000011", "10000100", "10000101", "10000110", "10000111",
  "10001000", "10001001", "10001010", "10001011", "10001100", "10001101", "10001110", "10001111",
  "10010000", "10010001", "10010010", "10010011", "10010100", "10010101", "10010110", "10010111",
  "10011000", "10011001", "10011010", "10011011", "10011100", "10011101", "10011110", "10011111",
  "10100000", "10100001", "10100010", "10100011", "10100100", "10100101", "10100110", "10100111",
  "10101000", "10101001", "10101010", "10101011", "10101100", "10101101", "10101110", "10101111",
  "10110000", "10110001", "10110010", "10110011", "10110100", "10110101", "10110110", "10110111",
  "10111000", "10111001", "10111010", "10111011", "10111100", "10111101", "10111110", "10111111",
  "11000000", "11000001", "11000010", "11000011", "11000100", "11000101", "11000110", "11000111",
  "11001000", "11001001", "11001010", "11001011", "11001100", "11001101", "11001110", "11001111",
  "11010000", "11010001", "11010010", "11010011", "11010100", "11010101", "11010110", "11010111",
  "11011000", "11011001", "11011010", "11011011", "11011100", "11011101", "11011110", "11011111",
  "11100000", "11100001", "11100010", "11100011", "11100100", "11100101", "11100110", "11100111",
  "11101000", "11101001", "11101010", "11101011", "11101100", "11101101", "11101110", "11101111",
  "11110000", "11110001", "11110010", "11110011", "11110100", "11110101", "11110110", "11110111",
  "11111000", "11111001", "11111010", "11111011", "11111100", "11111101", "11111110", "11111111",
};


/*
 * Definition of the control blocks for en- and decoder.
 */

typedef struct _EncoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

} EncoderControl;


typedef struct _DecoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  unsigned char charCount;  /* number of characters assembled so far (0..7) */
  unsigned char bench;      /* buffer for assembled byte */

} DecoderControl;


/*
 *------------------------------------------------------*
 *
 *	TrfInit_Bin --
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
TrfInit_Bin (interp)
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

#if 0
  unsigned char  buffer [CHAR_BIT];
  unsigned char  out;
  unsigned short i;
  unsigned char  mask; 

  out = character;

  memset (buffer, '0', CHAR_BIT);

  for (out = character, i=0, mask = 1;
       i < CHAR_BIT;
       i++, mask <<= 1) {
    buffer [(CHAR_BIT-1)-i] = ((out & mask) ? '1' : '0');
  }

  return c->write (c->writeClientData, buffer, CHAR_BIT, interp);
#endif

  return c->write (c->writeClientData, (unsigned char*) code [character & 0x00ff], CHAR_BIT, interp);
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
  EncoderControl* c   = (EncoderControl*) ctrlBlock;
  char*  out = (char*) ckalloc (8*bufLen+1);
  int    res, i, j;
  CONST char*  ch;

  for (i=0, j=0; i < bufLen; i++) {
    ch = code [buffer [i] & 0x00ff];
    out [j] = ch [0]; j++;    out [j] = ch [1]; j++;
    out [j] = ch [2]; j++;    out [j] = ch [3]; j++;
    out [j] = ch [4]; j++;    out [j] = ch [5]; j++;
    out [j] = ch [6]; j++;    out [j] = ch [7]; j++;
  }
  out [j] = '\0';

  res = c->write (c->writeClientData, (unsigned char*) out, 8*bufLen, interp);

  ckfree ((char*) out);
  return res;
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
  /* nothing to to */
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
  /* nothing to do */
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

  c->charCount = 0;
  c->bench     = '\0';

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
  DecoderControl* c  = (DecoderControl*) ctrlBlock;
  unsigned char   in = character;

  switch (in) {
  case '0':
    c->charCount ++;
    break;

  case '1':
    c->bench |= (1 << (CHAR_BIT-1 - c->charCount));
    c->charCount ++;
    break;

  default:
    if (interp) {
      char buf [10];

      if (character < ' ' || character > 127) {
	sprintf (buf, "0x%02x", character);
      } else {
	buf [0] = '\'';
	buf [1] = character;
	buf [2] = '\'';
	buf [3] = '\0';
      }

      Tcl_ResetResult  (interp);
      Tcl_AppendResult (interp, "illegal character ", buf,
			" found in input", (char*) NULL);
    }
    return TCL_ERROR;
    break;
  }

  if (c->charCount >= CHAR_BIT) {
    int res = c->write (c->writeClientData, &c->bench, 1, interp);

    c->bench     = '\0';
    c->charCount = 0;

    return res;
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
#define IN_RANGE(low,x,high) (((low) <= (x)) && ((x) <= (high)))

  DecoderControl* c      = (DecoderControl*) ctrlBlock;
  char*  out             = (char*) ckalloc (7+bufLen/8);
  int    res, i, j;
  unsigned char character;

  for (i=0, j=0; i < bufLen; i++) {
    character = buffer [i];

    switch (character) {
    case '0':
      c->charCount ++;
      break;

    case '1':
      c->bench |= (1 << (CHAR_BIT-1 - c->charCount));
      c->charCount ++;
      break;

    default:
      if (interp) {
	char buf [10];

	if (character < ' ' || character > 127) {
	  sprintf (buf, "0x%02x", character);
	} else {
	  buf [0] = '\'';
	  buf [1] = character;
	  buf [2] = '\'';
	  buf [3] = '\0';
	}

	Tcl_ResetResult  (interp);
	Tcl_AppendResult (interp, "illegal character ", buf,
			  " found in input", (char*) NULL);
      }
      return TCL_ERROR;
      break;
    }

    if (c->charCount >= CHAR_BIT) {
      out [j] = c->bench;
      j ++;

      c->bench     = '\0';
      c->charCount = 0;
    }
  }

  res = c->write (c->writeClientData, (unsigned char*) out, j, interp);
  return res;

#undef IN_RANGE
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
  int           res = TCL_OK;

  if (c->charCount > 0) {
    res = c->write (c->writeClientData, &c->bench, 1, interp);

    c->bench     = '\0';
    c->charCount = 0;
  }
  
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
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  c->bench     = '\0';
  c->charCount = 0;
}
