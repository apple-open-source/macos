/*
 * qpcode.c --
 *
 *      Implements and registers conversion from and to quoted-printable representation.
 *
 *
 * Copyright (c) 1999 Marshall Rose (mrose@dbc.mtview.ca.us)
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
 * CVS: $Id: qpcode.c,v 1.6 2000/04/28 22:17:44 aku Exp $
 */

#include <ctype.h>
#include "transformInt.h"

/*
 * Converter description
 * ---------------------
 *
 * Reference:
 *      RFC 2045
 *
 * Encoding:
 *      Printable characters (other than "=") are passed through; otherwise a
 *      character is represented by "=" followed by the two-digit hexadecimal
 *      representation of the character's value. Ditto for trailing whitespace
 *      at the end of a line.
 *
 * Decoding:
 *      Invert the above.
 */


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
  "quoted-printable",
  NULL, /* clientData not used by conversions. */
  NULL, /* set later by TrfInit_QP */ /* THREADING: serialize initialization */
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

#define CPERLIN    76           /* according to RFC 2045 */

typedef struct _EncoderControl_ {
    Trf_WriteProc* write;
    ClientData     writeClientData;

    /* add conversion specific items here (qp encode) */

    int    charCount;
    unsigned char buf[CPERLIN + 8];

    /* DNew@Invisible.Net added the +8 or FlushEncoder runs off the
       end when called from the first call point in Encode with a
       too-long line. */

} EncoderControl;


typedef struct _DecoderControl_ {
    Trf_WriteProc* write;
    ClientData     writeClientData;

    /* add conversion specific items here (qp decode) */

    int    quoted;
    unsigned char mask;

} DecoderControl;

static char hex2nib[0x80] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};




/*
 *------------------------------------------------------*
 *
 *      TrfInit_QP --
 *
 *      ------------------------------------------------*
 *      Register the conversion implemented in this file.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              As of 'Trf_Register'.
 *
 *      Result:
 *              A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

int
TrfInit_QP (interp)
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
 *      CreateEncoder --
 *
 *      ------------------------------------------------*
 *      Allocate and initialize the control block of a
 *      data encoder.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              Allocates memory.
 *
 *      Result:
 *              An opaque reference to the control block.
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

  /* initialize conversion specific items here (qp encode) */

  ClearEncoder ((Trf_ControlBlock) c, clientData);

  return ((ClientData) c);
}

/*
 *------------------------------------------------------*
 *
 *      DeleteEncoder --
 *
 *      ------------------------------------------------*
 *      Destroy the control block of an encoder.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              Releases the memory allocated by 'CreateEncoder'
 *
 *      Result:
 *              None.
 *
 *------------------------------------------------------*
 */

static void
DeleteEncoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* release conversion specific items here (qp encode) */

  Tcl_Free ((char*) c);
}

/*
 *------------------------------------------------------*
 *
 *      Encode --
 *
 *      ------------------------------------------------*
 *      Encode the given character and write the result.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              As of the called WriteFun.
 *
 *      Result:
 *              Generated bytes implicitly via WriteFun.
 *              A standard Tcl error code.
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

  /* execute conversion specific code here (qp encode) */

  int i;
  char x = character & 0xff;

  /*in case we need to indicate trailing whitespace... */
  if ((c -> charCount >= CPERLIN - 1)
        && ((x != '\n') || (c -> buf[c -> charCount -1] != '\r'))
        && ((i = FlushEncoder (ctrlBlock, interp, clientData)) != TCL_OK))
    return i;

  /* avoid common problems in old software... */
  switch (c -> charCount) {
    case 1:
      if (c -> buf[0] == '.') {
        (void) sprintf ((char*) c -> buf, "=%02X", '.');
        c -> charCount = 3;
      }
      break;

    case 5:
      if (!strcmp ((char*) c -> buf, "From ")) {
        (void) sprintf ((char*) c -> buf, "=%02Xrom ", 'F');
        c -> charCount = 7;
      }
      break;
  }

  switch (x) {
    case '\n':
      if ((c -> charCount > 0) && (c -> buf[c -> charCount - 1] == '\r'))
	c -> charCount--;
	/* and fall... */

    case ' ':
    case '\t':
    case '\r':
      c -> buf[c -> charCount++] = character;
      break;

    default:
      if (('!' <= x) && (x <= '~')) {
        c -> buf[c -> charCount++] = character;
        break;
      }
      /* else fall... */
    case '=':
      (void) sprintf ((char*) c -> buf + c -> charCount,
		      "=%02X", (unsigned char) x);
      c -> charCount += 3;
      break;
  }

  if ((x == '\n')
        && ((i = FlushEncoder (ctrlBlock, interp, clientData)) != TCL_OK))
    return i;

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *      EncodeBuffer --
 *
 *      ------------------------------------------------*
 *      Encode the given buffer and write the result.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              As of the called WriteFun.
 *
 *      Result:
 *              Generated bytes implicitly via WriteFun.
 *              A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
EncodeBuffer (ctrlBlock, buffer, bufLen, interp, clientData)
Trf_ControlBlock ctrlBlock;
unsigned char* buffer;
int bufLen;
Tcl_Interp* interp;
ClientData clientData;
{
  /* EncoderControl* c = (EncoderControl*) ctrlBlock; unused */
  /* execute conversion specific code here (qp encode) */

  int    i = TCL_OK;

  while (bufLen-- > 0) {
    if ((i = Encode (ctrlBlock, *buffer++ & 0xff, interp, clientData))
        != TCL_OK)
      break;
  }

  return i;
}

/*
 *------------------------------------------------------*
 *
 *      FlushEncoder --
 *
 *      ------------------------------------------------*
 *      Encode an incomplete character sequence (if possible).
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              As of the called WriteFun.
 *
 *      Result:
 *              Generated bytes implicitly via WriteFun.
 *              A standard Tcl error code.
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

  /* execute conversion specific code here (qp encode) */

  int i;

  if (c -> charCount == 0)
    return TCL_OK;
  
  if (c -> buf[c -> charCount - 1] == '\n') {
    if (c -> charCount > 1)
      switch (c -> buf[c -> charCount - 2]) {
        case ' ':
        case '\t':
          (void) strcpy ((char*) c -> buf + c -> charCount - 1, "=\n\n");
          c -> charCount += 2;
          break;

        default:
          break;
      }
  } else {
    (void) strcpy ((char*) c -> buf + c -> charCount, "=\n");
    c -> charCount += 2;
  }
  
  if ((i = c -> write (c -> writeClientData, c -> buf, c -> charCount,
                       interp)) != TCL_OK)
    return i;

  ClearEncoder (ctrlBlock, clientData);

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *      ClearEncoder --
 *
 *      ------------------------------------------------*
 *      Discard an incomplete character sequence.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              See above.
 *
 *      Result:
 *              None.
 *
 *------------------------------------------------------*
 */

static void
ClearEncoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData clientData;
{
  EncoderControl* c = (EncoderControl*) ctrlBlock;

  /* execute conversion specific code here (qp encode) */

  c -> charCount = 0;
  memset (c -> buf, '\0', sizeof c -> buf);
}

/*
 *------------------------------------------------------*
 *
 *      CreateDecoder --
 *
 *      ------------------------------------------------*
 *      Allocate and initialize the control block of a
 *      data decoder.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              Allocates memory.
 *
 *      Result:
 *              An opaque reference to the control block.
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

  /* initialize conversion specific items here (qp decode) */

  ClearDecoder ((Trf_ControlBlock) c, clientData);

  return ((ClientData) c);
}

/*
 *------------------------------------------------------*
 *
 *      DeleteDecoder --
 *
 *      ------------------------------------------------*
 *      Destroy the control block of an decoder.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              Releases the memory allocated by 'CreateDecoder'
 *
 *      Result:
 *              None.
 *
 *------------------------------------------------------*
 */

static void
DeleteDecoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* release conversion specific items here (qp decode) */

  Tcl_Free ((char*) c);
}

/*
 *------------------------------------------------------*
 *
 *      Decode --
 *
 *      ------------------------------------------------*
 *      Decode the given character and write the result.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              As of the called WriteFun.
 *
 *      Result:
 *              Generated bytes implicitly via WriteFun.
 *              A standard Tcl error code.
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

  /* execute conversion specific code here (qp decode) */

  int i;
  char x = character & 0xff;

  switch (c -> quoted) {
    case 0:
      switch (x) {
        default:
          if ((x < '!') || ('~' < x))
            goto invalid_encoding;
          /* else fall... */
        case ' ':
        case '\t':
        case '\n':
          i = c -> write (c -> writeClientData, (unsigned char*) &x, 1,
			  interp);
          break;

	case '\r':
	  i = TCL_OK;
	  break;

        case '=':
          c -> quoted = 1;
          i = TCL_OK;
          break;
      }
      break;

    case 1:
      switch (x) {
        case '\n':
          c -> quoted = 0;
          i = TCL_OK;
          break;
 
        case '\r':
          i = TCL_OK;
          break;

        default:
          if (!isxdigit (x))
              goto invalid_hex;
          c -> mask = hex2nib[x & 0x7f];
          c -> quoted = 2;
          i = TCL_OK;
          break;
      }
      break;

    default:
      if (!isxdigit (x))
        goto invalid_hex;
      c -> mask <<= 4;
      c -> mask |= hex2nib[x & 0x7f];
      c -> quoted = 0;
      i = c -> write (c -> writeClientData, &c -> mask, 1, interp);
      break;
  }

  return i;

invalid_hex: ;
    if (interp) {
      Tcl_ResetResult (interp);
      Tcl_AppendResult (interp, "expecting hexadecimal digit", (char *) NULL);
    }
    return TCL_ERROR;

invalid_encoding: ;
    if (interp) {
      Tcl_ResetResult (interp);
      Tcl_AppendResult (interp, "expecting character in range [!..~]",
                        (char *) NULL);
    }
    return TCL_ERROR;
}

/*
 *------------------------------------------------------*
 *
 *      DecodeBuffer --
 *
 *      ------------------------------------------------*
 *      Decode the given buffer and write the result.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              As of the called WriteFun.
 *
 *      Result:
 *              Generated bytes implicitly via WriteFun.
 *              A standard Tcl error code.
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
  /* DecoderControl* c = (DecoderControl*) ctrlBlock; unused */
  /* execute conversion specific code here (qp decode) */

  int    i = TCL_OK;

  while (bufLen-- > 0) {
    if ((i = Decode (ctrlBlock, *buffer++ & 0xff, interp, clientData))
        != TCL_OK)
      break;
  }
  
  return i;
}

/*
 *------------------------------------------------------*
 *
 *      FlushDecoder --
 *
 *      ------------------------------------------------*
 *      Decode an incomplete character sequence (if possible).
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              As of the called WriteFun.
 *
 *      Result:
 *              Generated bytes implicitly via WriteFun.
 *              A standard Tcl error code.
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

  /* execute conversion specific code here (qp decode) */

  if (c -> quoted) {
    if (interp) {
      Tcl_ResetResult (interp);
      Tcl_AppendResult (interp, c -> quoted > 1
                                  ? "expecting another hexadecimal digit"
                                  : "expecting addition characters",
                        (char *) NULL);
    }

    return TCL_ERROR;
  }

  ClearDecoder (ctrlBlock, clientData);

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *      ClearDecoder --
 *
 *      ------------------------------------------------*
 *      Discard an incomplete character sequence.
 *      ------------------------------------------------*
 *
 *      Sideeffects:
 *              See above.
 *
 *      Result:
 *              None.
 *
 *------------------------------------------------------*
 */

static void
ClearDecoder (ctrlBlock, clientData)
Trf_ControlBlock ctrlBlock;
ClientData clientData;
{
  DecoderControl* c = (DecoderControl*) ctrlBlock;

  /* execute conversion specific code here (qp decode) */

  c -> quoted = 0;
  c -> mask = 0;
}

