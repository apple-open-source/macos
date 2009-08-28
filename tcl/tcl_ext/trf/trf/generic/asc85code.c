/*
 * asc85code.c --
 *
 *	Implements and registers conversion from and to ASCII 85
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
 * CVS: $Id: asc85code.c,v 1.10 2000/08/09 19:13:17 aku Exp $
 */

#include "transformInt.h"

/*
 * Converter description
 * ---------------------
 *
 * Encoding:
 *	Each sequence of 4 bytes is reinterpreted as 32bit integer number
 *	in bigendian notation and split into into 5 printable characters
 *	via repeated division modulo 85 and addition of character '!'. The
 *	number 0 is a special case and converted to the single character
 *	'z'. If the last sequence in the input is shorter than 4 bytes,
 *	special processing is invoked: The sequence is padded with 0-bytes,
 *	then converted as usual, but without application of special case 0.
 *	Only the first 'len' bytes written (len being the original number
 *	of bytes in the processed sequence).
 *
 * Decoding:
 *	Each sequence of 5 printable characters is converted into a 32bit
 *	integer and the split into 4 bytes (output order is bigendian). A
 *	single character 'z' is converted into 4 0-bytes. If the last
 *	sequence is shorter than 5 bytes, special processing is invoked to
 *	recover the encoded bytes.
 */


/*
 * Declarations of internal procedures.
 */

static Trf_ControlBlock Asc85CreateEncoder  _ANSI_ARGS_ ((ClientData writeClientData, Trf_WriteProc *fun,
						     Trf_Options optInfo, Tcl_Interp*   interp,
						     ClientData clientData));
static void             Asc85DeleteEncoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));
static int              Asc85Encode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     unsigned int character,
						     Tcl_Interp* interp,
						     ClientData clientData));
static int              Asc85FlushEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     Tcl_Interp* interp,
						     ClientData clientData));
static void             Asc85ClearEncoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));


static Trf_ControlBlock Asc85CreateDecoder  _ANSI_ARGS_ ((ClientData writeClientData, Trf_WriteProc *fun,
						     Trf_Options optInfo, Tcl_Interp*   interp,
						     ClientData clientData));
static void             Asc85DeleteDecoder  _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));
static int              Asc85Decode         _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     unsigned int character,
						     Tcl_Interp* interp,
						     ClientData clientData));
static int              Asc85FlushDecoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     Tcl_Interp* interp,
						     ClientData clientData));
static void             Asc85ClearDecoder   _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
						     ClientData clientData));


/*
 * Converter definition.
 */

static Trf_TypeDefinition convDefinition =
{
  "ascii85",
  NULL, /* clientData not used by conversions. */
  NULL, /* set later by Trf_InitAscii85 */ /* THREADING: serialize initialization */
  {
    Asc85CreateEncoder,
    Asc85DeleteEncoder,
    Asc85Encode,
    NULL,
    Asc85FlushEncoder,
    Asc85ClearEncoder,
    NULL /* no MaxRead */
  }, {
    Asc85CreateDecoder,
    Asc85DeleteDecoder,
    Asc85Decode,
    NULL,
    Asc85FlushDecoder,
    Asc85ClearDecoder,
    NULL
  },
  TRF_UNSEEKABLE
};

/*
 * Definition of the control blocks for en- and decoder.
 */

typedef struct _EncoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  /* add conversion specific items here (ascii 85) */

  unsigned char charCount;
  unsigned char buf [4];

} EncoderControl;


typedef struct _DecoderControl_ {
  Trf_WriteProc* write;
  ClientData     writeClientData;

  /* add conversion specific items here (ascii 85) */

  unsigned char charCount;
  unsigned char buf [5];

} DecoderControl;


static int
CheckQuintuple _ANSI_ARGS_ ((Tcl_Interp* interp,
			     const char* quintuple,
			     int         partial));
#define ALL     (0)


/*
 *------------------------------------------------------*
 *
 *	TrfInit_Ascii85 --
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
TrfInit_Ascii85 (interp)
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
 *	Asc85CreateEncoder --
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
Asc85CreateEncoder (writeClientData, fun, optInfo, interp, clientData)
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

  /* initialize conversion specific items here (ascii 85) */

  c->charCount = 0;
  memset (c->buf, '\0', 4);

  return (ClientData) c;
}

/*
 *------------------------------------------------------*
 *
 *	Asc85DeleteEncoder --
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
Asc85DeleteEncoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* release conversion specific items here (ascii 85) */

  Tcl_Free ((char*) c);
}

/*
 *------------------------------------------------------*
 *
 *	Asc85Encode --
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
Asc85Encode (ctrlBlock, character, interp, clientData)
Trf_ControlBlock ctrlBlock;
unsigned int character;
Tcl_Interp* interp;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (ascii 85) */

  c->buf [c->charCount] = character;
  c->charCount ++;

  if (c->charCount == 4) {
    char          result [5];
    int           len;
    unsigned long num;

    num = ((c->buf [0] << 24) |
	   (c->buf [1] << 16) |
	   (c->buf [2] <<  8) |
	   (c->buf [3] <<  0));
  
    if (num == 0) {
      /*
       * special case 'all zeros' is mapped to 'z' instead of '!!!!!'
       */

      len        = 1;
      result [0] = 'z';
    } else {
      len        = 5;
      result [4] = '!' + (char) (num % 85);  num /= 85;
      result [3] = '!' + (char) (num % 85);  num /= 85;
      result [2] = '!' + (char) (num % 85);  num /= 85;
      result [1] = '!' + (char) (num % 85);  num /= 85;
      result [0] = '!' + (char) (num % 85);  num /= 85;
    }

    c->charCount = 0;
    memset (c->buf, '\0', 4);

    return c->write (c->writeClientData, (unsigned char*) result, len, interp);
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	Asc85FlushEncoder --
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
Asc85FlushEncoder (ctrlBlock, interp, clientData)
Trf_ControlBlock ctrlBlock;
Tcl_Interp* interp;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (ascii 85) */

  if (c->charCount > 0) {
    char          result [5];
    int           len;
    unsigned long num;

    /*
     * split in the usual way, then write out the head part.
     * c->buf is already '\0'-padded (due to reset code after every conversion).
     */

    num = ((c->buf [0] << 24) |
	   (c->buf [1] << 16) |
	   (c->buf [2] <<  8) |
	   (c->buf [3] <<  0));
  	  
    len        = c->charCount + 1;
    result [4] = '!' + (char) (num % 85);  num /= 85;
    result [3] = '!' + (char) (num % 85);  num /= 85;
    result [2] = '!' + (char) (num % 85);  num /= 85;
    result [1] = '!' + (char) (num % 85);  num /= 85;
    result [0] = '!' + (char) (num % 85);  num /= 85;

    c->charCount = 0;
    memset (c->buf, '\0', 4);

    return c->write (c->writeClientData, (unsigned char*) result, len, interp);
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	Asc85ClearEncoder --
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
Asc85ClearEncoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (ascii 85) */
  c->charCount = 0;
  memset (c->buf, '\0', 4);
}

/*
 *------------------------------------------------------*
 *
 *	Asc85CreateDecoder --
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
Asc85CreateDecoder (writeClientData, fun, optInfo, interp, clientData)
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

  /* initialize conversion specific items here (ascii 85) */

  c->charCount = 0;
  memset (c->buf, '\0', 5);

  return (ClientData) c;
}

/*
 *------------------------------------------------------*
 *
 *	Asc85DeleteDecoder --
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
Asc85DeleteDecoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* release conversion specific items here (ascii 85) */

  Tcl_Free ((char*) c);
}

/*
 *------------------------------------------------------*
 *
 *	Asc85Decode --
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
Asc85Decode (ctrlBlock, character, interp, clientData)
Trf_ControlBlock ctrlBlock;
unsigned int character;
Tcl_Interp* interp;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;
  unsigned char   result [4];
  int             len = 0;

  /* execute conversion specific code here (ascii 85) */

  if ((c->charCount == 0) && (character == 'z')) {
    /*
     * convert special case of 'all zero's.
     */

    memset (result, '\0', 4);
    len = 4;
  } else {
    /*
     * Standard case.
     */

    c->buf [c->charCount] = character;
    c->charCount ++;

    if (c->charCount == 5) {
      unsigned long num = 0;
      int           k;

      if (TCL_OK != CheckQuintuple (interp, (char*) c->buf, ALL))
	return TCL_ERROR;

      for (k=0; k < 5; k++) {
	num = num * 85 + (c->buf [k] - '!');
      }

      len = 4;
      for (k=3; 0 <= k; k --) {
	/* result [k] = num % 256;  num  /= 256; */
	   result [k] = (char) (num & 0xff); num >>= 8;
      }

      c->charCount = 0;
      memset (c->buf, '\0', 5);
    }
  }

  if (len > 0)
    return c->write (c->writeClientData, result, len, interp);

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	Asc85FlushDecoder --
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
Asc85FlushDecoder (ctrlBlock, interp, clientData)
Trf_ControlBlock ctrlBlock;
Tcl_Interp* interp;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (ascii 85) */

  if (c->charCount > 0) {
    if (c->charCount < 2) {
      if (interp) {
	Tcl_ResetResult  (interp);
	Tcl_AppendResult (interp, "partial character sequence at end to ", (char*) NULL);
	Tcl_AppendResult (interp, "short (2 characters required at least)", (char*) NULL);
      }

      return TCL_ERROR;
    } else {
      /* Partial tuple to convert.
       *
       * This is a bit tricky. Binary to ascii conversion
       * of a partial quadruple yields 5! characters, but
       * only the first ones were written.  During the
       * number generation these missing values are substituted
       * by 0, resulting in a value smaller than the one
       * we started with.  Luckily we do know the expected
       * binary representation (trailing zeros).  So we just
       * have to increment our value to get the correct one.
       * To make this faster, bit shifting is used.  In
       * addition we do know, that only the upper part is
       * relevant, so we don't shift up again and avoid
       * overhead during division.
       *
       * Example:
       *
       *	'rld'
       * == 0x72 0x6c 0x64
       * == 0x726c6400
       * == 1919706112
       * == 36 65 78 22 67
       *    36 65 78 22	(written) 'Ebo7'
       *
       * And now back:
       *
       *    36 65 68 22
       * == 22584777
       * == 1919706045	(after -x-)
       * == 0x726c63bd
       *            ~~ mask these, then increment next digit by one
       * -> 0x726c6400 !
       *
       *              same effect via shift and increment (/o/)
       * ->   0x726c64
       *              retrieve bytes as usual now
       */

      unsigned long num = 0;
      int           partial = c->charCount;
      int           k, zlen;
      unsigned char result [4];


      if (TCL_OK != CheckQuintuple (interp, (char*) c->buf, partial))
	return TCL_ERROR;

      for (k=0; k < partial; k++) {
	num = num*85 + (c->buf [k] - '!');
      }

      /* -x- */
      for (; k < 5; k++)
	num *= 85;

      /* /o/ */
      partial --;
      zlen = (4-partial)*8;
      num  = ((num >> zlen) + 1);/* << zlen;*/

      for (k = (partial-1); 0 <= k; k--) {
	/* result [k] = num % 256;  num  /= 256; */
	   result [k] = (char) (num & 0xff); num >>= 8;
      }

      c->charCount = 0;
      memset (c->buf, '\0', 5);

      return c->write (c->writeClientData, result, partial, interp);
    }
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	Asc85ClearDecoder --
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
Asc85ClearDecoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (ascii 85) */

  c->charCount = 0;
  memset (c->buf, '\0', 5);
}

/*
 *------------------------------------------------------*
 *
 *	CheckQuintuple --
 *
 *	------------------------------------------------*
 *	the specified tuple is checked for correctness.
 *	'partial > 0' indicates the request to check the
 *	last tuple in the input, its value specifies the
 *	number of bytes the tuple is made of.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		An error message is placed in the result-
 *		area of the specified interpreter, if the
 *		tuple is illegal for some reason.
 *
 *	Result:
 *		a standard TCL error code.
 *
 *------------------------------------------------------*
 */

static int
CheckQuintuple (interp, quintuple, partial)
Tcl_Interp* interp;
const char* quintuple;
int         partial;
{
#define IN_RANGE(c) (('!' <= (c)) && ((c) <= 'u'))

  int i, n, ok;

  n = ((partial > 0) ? partial : 5);

  /*
   * Scan tuple for characters out of range !..u
   * The special case 'z' must have been handled before!
   */

  ok = 1;
  for (i=0; i < n; i ++) {
    if (! IN_RANGE (quintuple [i])) {
      ok = 0;
      break;
    }
  }

  if (! ok) {
    if (interp) {
      char quint [6];

      /*
       * produce string to insert in error message
       */

      for (i=0; i < n; i ++) {
	quint [i] = quintuple [i];
      }

      quint [i] = '\0';

      Tcl_ResetResult  (interp);
      if (partial > 0) {
	Tcl_AppendResult (interp, "illegal quintuple '", (char*) NULL);
	Tcl_AppendResult (interp, quint, (char*) NULL);
	Tcl_AppendResult (interp, "' at end of input (illegal characters)", (char*) NULL);
      } else {
	Tcl_AppendResult (interp, "illegal quintuple '", (char*) NULL);
	Tcl_AppendResult (interp, quint, (char*) NULL);
	Tcl_AppendResult (interp, "' in input (illegal characters)", (char*) NULL);
      }
    }

    return TCL_ERROR;
  }

#undef IN_RANGE

  /* Second check (required for complete quintuples only):
   * Upper limit for encoded value is 2^32-1 == 0xffffffff == s8W-!
   */

#define EQ(i,x) (quintuple [i] == (x))
#define GR(i,x) (quintuple [i] >  (x))

  if ((partial == 0) &&
      ((GR (0,'s')) ||
       (EQ (0,'s') && GR (1,'8')) ||
       (EQ (0,'s') && EQ (1,'8') && GR (2, 'W')) ||
       (EQ (0,'s') && EQ (1,'8') && EQ (2, 'W') && GR (3, '-')) ||
       (EQ (0,'s') && EQ (1,'8') && EQ (2, 'W') && EQ (3, '-') && GR (4, '!')))) {
    if (interp) {
      char quint [6];

      /* produce string to insert in error message
       */

      for (i=0; i < n; i ++) {
	quint [i] = quintuple [i];
      }

      quint [i] = '\0';

      Tcl_ResetResult  (interp);
      Tcl_AppendResult (interp, "illegal quintuple '", (char*) NULL);
      Tcl_AppendResult (interp, quint, (char*) NULL);
      Tcl_AppendResult (interp, "' in input (> 2^32-1)", (char*) NULL);
    }

    return TCL_ERROR;
  }

  return TCL_OK;
}
