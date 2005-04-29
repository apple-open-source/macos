/*
 * octcode.c --
 *
 *	Implements and registers conversion from and to octal representation.
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
 * CVS: $Id: octcode.c,v 1.10 1999/11/12 22:43:32 aku Exp $
 */

#include "transformInt.h"

/*
 * Converter description
 * ---------------------
 *
 * Encoding:
 *	Every byte is converted into 3 characters, using elements
 *	in the set {0, ..., 7} only. Thus a octal representation is
 *	generated. The MSBit is output first.
 *
 * Decoding:
 *	Only characters in the set {0, ..., 7} are allowed as input.
 *	Each 3-tuple is converted into a single byte. One or two
 *	characters at the end of input are converted as if padded
 *	with "0"s.
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
  "oct",
  NULL, /* clientData not used by converters */
  NULL, /* set later by TrfInit_Oct, THREADING: serialize initialization */
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
  TRF_RATIO (1, 3)
};

/*
 * Use table lookup
 */

static const char* code [] = { /* THREADING: constant, read-only => safe */
  "000", "001", "002", "003", "004", "005", "006", "007",
  "010", "011", "012", "013", "014", "015", "016", "017",
  "020", "021", "022", "023", "024", "025", "026", "027",
  "030", "031", "032", "033", "034", "035", "036", "037",
  "040", "041", "042", "043", "044", "045", "046", "047",
  "050", "051", "052", "053", "054", "055", "056", "057",
  "060", "061", "062", "063", "064", "065", "066", "067",
  "070", "071", "072", "073", "074", "075", "076", "077",

  "100", "101", "102", "103", "104", "105", "106", "107",
  "110", "111", "112", "113", "114", "115", "116", "117",
  "120", "121", "122", "123", "124", "125", "126", "127",
  "130", "131", "132", "133", "134", "135", "136", "137",
  "140", "141", "142", "143", "144", "145", "146", "147",
  "150", "151", "152", "153", "154", "155", "156", "157",
  "160", "161", "162", "163", "164", "165", "166", "167",
  "170", "171", "172", "173", "174", "175", "176", "177",

  "200", "201", "202", "203", "204", "205", "206", "207",
  "210", "211", "212", "213", "214", "215", "216", "217",
  "220", "221", "222", "223", "224", "225", "226", "227",
  "230", "231", "232", "233", "234", "235", "236", "237",
  "240", "241", "242", "243", "244", "245", "246", "247",
  "250", "251", "252", "253", "254", "255", "256", "257",
  "260", "261", "262", "263", "264", "265", "266", "267",
  "270", "271", "272", "273", "274", "275", "276", "277",

  "300", "301", "302", "303", "304", "305", "306", "307",
  "310", "311", "312", "313", "314", "315", "316", "317",
  "320", "321", "322", "323", "324", "325", "326", "327",
  "330", "331", "332", "333", "334", "335", "336", "337",
  "340", "341", "342", "343", "344", "345", "346", "347",
  "350", "351", "352", "353", "354", "355", "356", "357",
  "360", "361", "362", "363", "364", "365", "366", "367",
  "370", "371", "372", "373", "374", "375", "376", "377",
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

  unsigned char charCount;  /* number of characters assembled so far (0..3) */
  unsigned char bench;      /* buffer for assembled byte */

} DecoderControl;


/*
 *------------------------------------------------------*
 *
 *	TrfInit_Oct --
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
TrfInit_Oct (interp)
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
  unsigned char   buffer [3];

  char high = (character >> 6) & 0x03;
  char mid  = (character >> 3) & 0x07;
  char low  =  character       & 0x07;

  buffer [0] =  high + '0';
  buffer [1] =  mid  + '0';
  buffer [2] =  low  + '0';

  return c->write (c->writeClientData, buffer, 3, interp);
#endif

  return c->write (c->writeClientData, (unsigned char*) code [character & 0x00ff], 3, interp);
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
  char*  out = (char*) Tcl_Alloc (3*bufLen+1);
  int    res, i, j;
  CONST char*  ch;

  for (i=0, j=0; i < bufLen; i++) {
    ch = code [buffer [i] & 0x00ff];
    out [j] = ch [0]; j++;
    out [j] = ch [1]; j++;
    out [j] = ch [2]; j++;
  }
  out [j] = '\0';

  res = c->write (c->writeClientData, (unsigned char*) out, 3*bufLen, interp);

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

#define IN_RANGE(low,x,high) (((low) <= (x)) && ((x) <= (high)))

  if ((! IN_RANGE ('0', character, '7')) ||
      ((c->charCount == 0) &&
       (character > '3'))) {

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

  character -= '0';

  c->bench |= (character << (3 * (2 - c->charCount)));
  c->charCount ++;

  if (c->charCount >= 3) {
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

  DecoderControl* c      = (DecoderControl*) ctrlBlock;
  char*  out             = (char*) Tcl_Alloc (2+bufLen/3);
  int    res, i, j;
  unsigned char character;

  for (i=0, j=0; i < bufLen; i++) {
    character = buffer [i];

    if ((! IN_RANGE ('0', character, '7')) ||
	((c->charCount == 0) &&
	 (character > '3'))) {

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

    character -= '0';

    c->bench |= (character << (3 * (2 - c->charCount)));
    c->charCount ++;

    if (c->charCount >= 3) {
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
