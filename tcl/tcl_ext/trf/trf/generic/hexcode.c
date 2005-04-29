/*
 * hexcode.c --
 *
 *	Implements and registers conversion from and to hexadecimal representation.
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
 * CVS: $Id: hexcode.c,v 1.11 1999/11/12 22:43:31 aku Exp $
 */

#include "transformInt.h"

/*
 * Converter description
 * ---------------------
 *
 * Encoding:
 *	Every byte is converted into 2 characters, using elements
 *	in the set {0-9,a-z} only. Thus a hexadecimal representation is
 *	generated. The MSBit is output first.
 *
 * Decoding:
 *	Only characters in the set {0-9a-zA-Z} are allowed as input.
 *	Each 2-tuple is converted into a single byte. A single character
 *	at the end of input is converted as if padded with "0".
 */


/*
 * Declarations of internal procedures.
 */

static Trf_ControlBlock
CreateEncoder  _ANSI_ARGS_ ((ClientData writeClientData, Trf_WriteProc *fun,
			     Trf_Options optInfo, Tcl_Interp*   interp,
			     ClientData clientData));
static void
DeleteEncoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData clientData));
static int
Encode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     unsigned int character,
			     Tcl_Interp* interp,
			     ClientData clientData));
static int
EncodeBuffer   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     unsigned char* buffer,
			     int bufLen,
			     Tcl_Interp* interp,
			     ClientData clientData));
static int
FlushEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     Tcl_Interp* interp,
			     ClientData clientData));
static void
ClearEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData clientData));


static Trf_ControlBlock
CreateDecoder  _ANSI_ARGS_ ((ClientData writeClientData, Trf_WriteProc *fun,
			     Trf_Options optInfo, Tcl_Interp*   interp,
			     ClientData clientData));
static void
DeleteDecoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData clientData));
static int
Decode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     unsigned int character,
			     Tcl_Interp* interp,
			     ClientData clientData));
static int
DecodeBuffer   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     unsigned char* buffer,
			     int bufLen,
			     Tcl_Interp* interp,
			     ClientData clientData));
static int
FlushDecoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     Tcl_Interp* interp,
			     ClientData clientData));
static void
ClearDecoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData clientData));


/*
 * Converter definition.
 */

static Trf_TypeDefinition convDefinition =
{
  "hex",
  NULL, /* clientData not sed by converters */
  NULL, /* set by 'TrfInit_Hex'. THREAD: serialize initialization */
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
  TRF_RATIO (1, 2)
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

  unsigned char charCount;  /* number of characters assembled so far (0..1) */
  unsigned char bench;      /* buffer for assembled byte */

} DecoderControl;


/*
 * Use table lookup
 */

static const char* code [] = { /* THREADING: constant, read-only => safe */
  "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F",
  "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "1A", "1B", "1C", "1D", "1E", "1F",
  "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F",
  "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3A", "3B", "3C", "3D", "3E", "3F",
  "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4A", "4B", "4C", "4D", "4E", "4F",
  "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E", "5F",
  "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B", "6C", "6D", "6E", "6F",
  "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "7A", "7B", "7C", "7D", "7E", "7F",
  "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
  "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B", "9C", "9D", "9E", "9F",
  "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF",
  "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
  "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB", "CC", "CD", "CE", "CF",
  "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF",
  "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
  "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB", "FC", "FD", "FE", "FF",
};


/*
 *------------------------------------------------------*
 *
 *	TrfInit_Hex --
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
TrfInit_Hex (interp)
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

  c = (EncoderControl*) Tcl_Alloc (sizeof (EncoderControl));
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
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

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
unsigned int character;
Tcl_Interp* interp;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

#if 0
  unsigned char   buffer [2];

  char high = (character >> 4) & 0x0F;
  char low  =  character       & 0x0F;

  high += ((high < 10) ? '0' : 'A'-10);
  low  += ((low  < 10) ? '0' : 'A'-10);

  buffer [0] = high;
  buffer [1] = low;

  return c->write (c->writeClientData, buffer, 2, interp);
#endif

  return c->write (c->writeClientData, (unsigned char*) code [character & 0x00ff], 2, interp);
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
  char*  out = (char*) Tcl_Alloc (2*bufLen+1);
  int    res, i, j;
  CONST char*  ch;

  for (i=0, j=0; i < bufLen; i++) {
    ch = code [buffer [i] & 0x00ff];
    out [j] = ch [0]; j++;
    out [j] = ch [1]; j++;
  }
  out [j] = '\0';

  res = c->write (c->writeClientData, (unsigned char*) out, 2*bufLen, interp);

  Tcl_Free ((char*) out);
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
Tcl_Interp* interp;
ClientData clientData;
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
ClientData clientData;
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
ClientData clientData;
{
  DecoderControl* c;

  c = (DecoderControl*) Tcl_Alloc (sizeof (DecoderControl));
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
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

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
unsigned int character;
Tcl_Interp* interp;
ClientData clientData;
{
  DecoderControl* c      = (DecoderControl*) ctrlBlock;
  unsigned char   nibble = character;

#define IN_RANGE(low,x,high) (((low) <= (x)) && ((x) <= (high)))

  if (IN_RANGE ('0', nibble, '9'))
    nibble -= '0';
  else if (IN_RANGE ('a', nibble, 'f'))
    nibble -= 'a'-10;
  else if (IN_RANGE ('A', nibble, 'F'))
    nibble -= 'A'-10;
  else {
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
  }

  c->bench |= (nibble << (4 * (1- c->charCount)));
  c->charCount ++;

  if (c->charCount >= 2) {
    int res = c->write (c->writeClientData, &c->bench, 1, interp);

    c->bench     = '\0';
    c->charCount = 0;

    return res;
  }

  return TCL_OK;

#undef IN_RANGE
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

  DecoderControl* c   = (DecoderControl*) ctrlBlock;
  char*  out = (char*) Tcl_Alloc (1+bufLen/2);
  int    res, i, j;
  unsigned char nibble;

  for (i=0, j=0; i < bufLen; i++) {
    nibble = buffer [i];

    if (IN_RANGE ('0', nibble, '9'))
      nibble -= '0';
    else if (IN_RANGE ('a', nibble, 'f'))
      nibble -= 'a' - 10;
    else if (IN_RANGE ('A', nibble, 'F'))
      nibble -= 'A' - 10;
    else {
      if (interp) {
	char buf [10];

	if (nibble < ' ' || nibble > 127) {
	  sprintf (buf, "0x%02x", nibble);
	} else {
	  buf [0] = '\'';
	  buf [1] = nibble;
	  buf [2] = '\'';
	  buf [3] = '\0';
	}

	Tcl_ResetResult  (interp);
	Tcl_AppendResult (interp, "illegal character ", buf,
			  " found in input", (char*) NULL);
      }

      Tcl_Free (out);
      return TCL_ERROR;
    }

    c->bench |= (nibble << (4 * (1 - c->charCount)));
    c->charCount ++;

    if (c->charCount >= 2) {
      out [j] = c->bench;

      c->bench     = '\0';
      c->charCount = 0;

      j ++;
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
Tcl_Interp* interp;
ClientData clientData;
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
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  c->bench     = '\0';
  c->charCount = 0;
}
