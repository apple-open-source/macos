
/*
 * binio.c --
 *
 *	Implementation of a binary input and output.
 *
 * Copyright (c) Jan 1997, Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
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
 * CVS: $Id: binio.c,v 1.11 2009/05/07 04:57:27 andreas_kupries Exp $
 */


#include "transformInt.h"

#ifdef ENABLE_BINIO
#include <limits.h>

/*
 * Forward declarations of internal procedures.
 */

static int CopyCmd   _ANSI_ARGS_((Tcl_Interp *interp, int argc, char** argv));
static int PackCmd   _ANSI_ARGS_((Tcl_Interp *interp, int argc, char** argv));
static int UnpackCmd _ANSI_ARGS_((Tcl_Interp *interp, int argc, char** argv));
static int BinioCmd  _ANSI_ARGS_((ClientData notUsed, Tcl_Interp* interp, int argc, char** argv));

static void	ReorderBytes _ANSI_ARGS_ ((char* buf, int len /*2,4,8*/));

static int	GetHex   _ANSI_ARGS_ ((Tcl_Interp* interp, char* text, long int* result));
static int	GetOctal _ANSI_ARGS_ ((Tcl_Interp* interp, char* text, long int* result));

/*
 * Return at most this number of bytes in one call to Tcl_Read:
 */

#define KILO 1024
#ifndef READ_CHUNK_SIZE
#define	READ_CHUNK_SIZE	(16*KILO)
#endif

/*
 * Union to overlay the different possible types used in 'pack', 'unpack'.
 */

typedef union {
  double         d;
  float          f;

  long int       li;
  unsigned long  ul;

  int            i;
  unsigned int   ui;

  short int      si;
  unsigned short us;

  char           c;
  unsigned char  uc;
} conversion;

/*
 *------------------------------------------------------*
 *
 *	CopyCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'binio copy' command.
 *	See the manpages for details on what it does.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See user documentation.
 *
 *	Result:
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
static int
CopyCmd (interp, argc, argv)
Tcl_Interp* interp;     /* The interpreter we are working in */
int         argc;	/* # arguments */
char**      argv;	/* trailing arguments */
{
  /*
   * Allowed syntax:
   * 	inChannel outChannel ?count?
   *
   * code taken from 'unsupported0'.
   */

  Tcl_Channel inChan, outChan;
  int requested;
  char *bufPtr;
  int actuallyRead, actuallyWritten, totalRead, toReadNow, mode;
    
  /*
   * Assume we want to copy the entire channel.
   */
  
  requested = INT_MAX;

  if ((argc < 2) || (argc > 3)) {
    Tcl_AppendResult(interp,
		     "wrong # args: should be \"binio copy inChannel outChannel ?chunkSize?\"",
		     (char *) NULL);
    return TCL_ERROR;
  }

  inChan = Tcl_GetChannel(interp, argv[0], &mode);
  if (inChan == (Tcl_Channel) NULL) {
    return TCL_ERROR;
  }

  if ((mode & TCL_READABLE) == 0) {
    Tcl_AppendResult(interp, "channel \"", argv[0],
		     "\" wasn't opened for reading", (char *) NULL);
    return TCL_ERROR;
  }

  outChan = Tcl_GetChannel(interp, argv[1], &mode);
  if (outChan == (Tcl_Channel) NULL) {
    return TCL_ERROR;
  }

  if ((mode & TCL_WRITABLE) == 0) {
    Tcl_AppendResult(interp, "channel \"", argv[1],
		     "\" wasn't opened for writing", (char *) NULL);
    return TCL_ERROR;
  }
    
  if (argc == 3) {
    if (Tcl_GetInt(interp, argv[2], &requested) != TCL_OK) {
      return TCL_ERROR;
    }
    if (requested < 0) {
      requested = INT_MAX;
    }
  }

  bufPtr = ckalloc((unsigned) READ_CHUNK_SIZE);

  for (totalRead = 0;
       requested > 0;
       totalRead += actuallyRead, requested -= actuallyRead) {

    toReadNow = requested;
    if (toReadNow > READ_CHUNK_SIZE) {
      toReadNow = READ_CHUNK_SIZE;
    }

    actuallyRead = Tcl_Read(inChan, bufPtr, toReadNow);

    if (actuallyRead < 0) {
      ckfree (bufPtr);
      Tcl_AppendResult(interp, argv[0], ": ", Tcl_GetChannelName(inChan),
		       Tcl_PosixError(interp), (char *) NULL);
      return TCL_ERROR;
    } else if (actuallyRead == 0) {
      ckfree (bufPtr);
      sprintf(interp->result, "%d", totalRead);
      return TCL_OK;
    }

    actuallyWritten = Tcl_Write(outChan, bufPtr, actuallyRead);
    if (actuallyWritten < 0) {
      ckfree (bufPtr);
      Tcl_AppendResult(interp, argv[0], ": ", Tcl_GetChannelName(outChan),
		       Tcl_PosixError(interp), (char *) NULL);
      return TCL_ERROR;
    }
  }

  ckfree(bufPtr);
    
  sprintf(interp->result, "%d", totalRead);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	PackCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'binio pack' command.
 *	See the manpages for details on what it does.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See user documentation.
 *
 *	Result:
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
static int
PackCmd (interp, argc, argv)
Tcl_Interp* interp;     /* The interpreter we are working in */
int         argc;	/* # arguments */
char**      argv;	/* trailing arguments */
{
  Tcl_Channel outChan;	/* The channel to write to */
  char*       format;
  conversion  cvt;
  char        buffer [50];
  char*       bufPtr = (char*) NULL;
  int         bufLen = 0;
  int         packed, actuallyWritten, reorder, mode;

  /*
   * Allowed syntax:
   * 	outChannel format ?data1 data2 ...?
   */

  if (argc < 2) {
    Tcl_AppendResult(interp,
		     "wrong # args: should be \"binio pack outChannel format ?data1 data2 ...?\"",
		     (char *) NULL);
    return TCL_ERROR;
  }

  outChan = Tcl_GetChannel(interp, argv[0], &mode);
  if (outChan == (Tcl_Channel) NULL) {
    return TCL_ERROR;
  }

  if ((mode & TCL_WRITABLE) == 0) {
    Tcl_AppendResult(interp, "channel \"", argv[0],
		     "\" wasn't opened for writing", (char *) NULL);
    return TCL_ERROR;
  }

  format = argv [1];
  argc  -= 2;
  argv  += 2;

  for (packed = 0 ; format [0] != '\0'; format += 2, argc --, argv ++, packed ++) {
    if (format [0] != '%') {
      char buf [3];
      buf [0] = format [0];
      buf [1] = format [1];
      buf [2] = '\0';

      Tcl_AppendResult (interp, "unknown format specification '", buf, "'", (char*) NULL);
      return TCL_ERROR;
    }

    if (argc == 0) {
      Tcl_AppendResult (interp, "more format specifiers than data items", (char*) NULL);
      return TCL_ERROR;      
    }

    reorder = 1; /* prepare for usual case */

    /*
     * Possible specifications:
     * - %d specifies that the corresponding value is a four byte signed int.
     * - %u specifies that the corresponding value is a four byte unsigned int.
     * - %o specifies that the corresponding value is a four byte octal signed int.
     * - %x specifies that the corresponding value is a four byte hexadecimal signed int.
     * - %l specifies that the corresponding value is an eight byte signed int.
     * - %L specifies that the corresponding value is an eight byte unsigned int.
     * - %D specifies that the corresponding value is a two byte signed int.
     * - %U specifies that the corresponding value is a two byte unsigned int.
     * - %O specifies that the corresponding value is a two byte octal signed int.
     * - %X specifies that the corresponding value is a two byte hexadecimal signed int.
     * - %c specifies that the corresponding value is a one byte signed int (char).
     * - %C specifies that the corresponding value is a one byte unsigned int.
     * - %f specifies that the corresponding value is a four byte floating point number.
     * - %F specifies that the corresponding value is an eight byte floating point number.
     * - %s specifies that the corresponding value is a NULL terminated string.
     */

    switch (format [1]) {
    case 'd':
    case 'u':
    case 'l':
    case 'L':
    case 'D':
    case 'U':
    case 'c':
    case 'C':
      if (TCL_OK != Tcl_GetInt (interp, argv [0], &cvt.i)) {
	return TCL_ERROR;
      }

      switch (format [1]) {
      case 'd':
	bufPtr = (char*) &cvt.i;
	bufLen = sizeof (int);
	break;

      case 'u':
	cvt.ui = (unsigned int) cvt.i;
	bufPtr = (char*) &cvt.ui;
	bufLen = sizeof (unsigned int);
	break;

      case 'l':
	cvt.li = (long int) cvt.i;
	bufPtr = (char*) &cvt.li;
	bufLen = sizeof (long int);
	break;

      case 'L':
	cvt.ul = (unsigned long) cvt.i;
	bufPtr = (char*) &cvt.ul;
	bufLen = sizeof (unsigned long);
	break;

      case 'D':
	cvt.si = (short int) cvt.i;
	bufPtr = (char*) &cvt.si;
	bufLen = sizeof (short int);
	break;

      case 'U':
	cvt.us = (short int) cvt.i;
	bufPtr = (char*) &cvt.us;
	bufLen = sizeof (unsigned short);
	break;

      case 'c':
	cvt.c = (char) cvt.i;
	bufPtr = (char*) &cvt.c;
	bufLen = sizeof (char);
	break;

      case 'C':
	cvt.uc = (unsigned char) cvt.i;
	bufPtr = (char*) &cvt.uc;
	bufLen = sizeof (unsigned char);
	break;
      } /* switch */
      break;

    case 'o':
    case 'O':
      if (TCL_OK != GetOctal (interp, argv [0], &cvt.li)) {
	return TCL_ERROR;
      }

      if (format [1] == 'O') {
	cvt.si = (short int) cvt.i;
	bufPtr = (char*) &cvt.si;
	bufLen = sizeof (short int);
      } else {
	cvt.i  = (int) cvt.li;
	bufPtr = (char*) &cvt.i;
	bufLen = sizeof (int);
      }
      break;

    case 'x':
    case 'X':
      if (TCL_OK != GetHex (interp, argv [0], &cvt.li)) {
	return TCL_ERROR;
      }

      if (format [1] == 'X') {
	cvt.si = (short int) cvt.i;
	bufPtr = (char*) &cvt.si;
	bufLen = sizeof (short int);
      } else {
	cvt.i  = (int) cvt.li;
	bufPtr = (char*) &cvt.i;
	bufLen = sizeof (int);
      }
      break;

    case 'f':
    case 'F':
      if (TCL_OK != Tcl_GetDouble (interp, argv [0], &cvt.d)) {
	return TCL_ERROR;
      }

      if (format [1] == 'f') {
	cvt.f = (float) cvt.d;
	bufPtr = (char*) &cvt.f;
	bufLen = sizeof (float);
      } else {
	bufPtr = (char*) &cvt.d;
	bufLen = sizeof (double);
      }
      break;

    case 's':
      bufPtr  = argv [0];
      bufLen  = strlen (argv [0]);
      reorder = 0;
      break;
    } /* switch */


    /* check, wether reordering is required or not.
     * upon answer `yes` do the reordering here too.
     */
    if ((bufLen > 1) && reorder &&
	(Tcl_GetHostByteorder () != Tcl_GetChannelByteorder (outChan))) {
      ReorderBytes (bufPtr, bufLen);
    }

    actuallyWritten = Tcl_Write (outChan, bufPtr, bufLen);
    if (actuallyWritten < 0) {
      Tcl_AppendResult(interp, "binio pack: ", Tcl_GetChannelName(outChan),
		       Tcl_PosixError(interp), (char *) NULL);
      return TCL_ERROR;
    }
  } /* for (format) */

  /* return number of packed items */
  sprintf (buffer, "%d", packed);
  Tcl_AppendResult (interp, buffer, (char*) NULL);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	UnpackCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'binio unpack' command.
 *	See the manpages for details on what it does.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See user documentation.
 *
 *	Result:
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
static int
UnpackCmd (interp, argc, argv)
Tcl_Interp* interp;     /* The interpreter we are working in */
int         argc;	/* # arguments */
char**      argv;	/* trailing arguments */
{
  Tcl_Channel inChan;		/* The channel to read from */
  conversion  cvt;
  int         mode, unpacked, actuallyRead;
  int         length;   /* length of single item,
			 * < 0 => variable length (string)
			 * 0 is illegal.
			 */
  char        buffer [50];   /* to hold most of the read information (and its conversion) */
  char*       format;
  char*       val;


  /*
   * Allowed syntax:
   * 	format ?var1 var2 ...?
   */

  if (argc < 2) {
    Tcl_AppendResult(interp,
		     "wrong # args: should be \"binio unpack outChannel format ?var1 var2 ...?\"",
		     (char *) NULL);
    return TCL_ERROR;
  }

  inChan = Tcl_GetChannel(interp, argv[0], &mode);
  if (inChan == (Tcl_Channel) NULL) {
    return TCL_ERROR;
  }

  if ((mode & TCL_READABLE) == 0) {
    Tcl_AppendResult(interp, "channel \"", argv[0],
		     "\" wasn't opened for reading", (char *) NULL);
    return TCL_ERROR;
  }

  if (Tcl_Eof (inChan)) {
    /*
     * cannot convert behind end of channel.
     * no error, just unpack nothing !
     */

    Tcl_AppendResult (interp, "0", (char*) NULL);
    return TCL_OK;
  }

  format = argv [1];
  argc  -= 2;
  argv  += 2;

  for (unpacked = 0 ; format [0] != '\0'; format += 2, argc --, argv ++, unpacked ++) {
    if (format [0] != '%') {
      char buf [3];
      buf [0] = format [0];
      buf [1] = format [1];
      buf [2] = '\0';

      Tcl_AppendResult (interp, "unknown format specification '", buf, "'", (char*) NULL);
      return TCL_ERROR;
    }

    if (argc == 0) {
      Tcl_AppendResult (interp, "more format specifiers than variables", (char*) NULL);
      return TCL_ERROR;      
    }

    length  = 0; /* illegal marker, to catch missing cases later */

    /*
     * Possible specifications:
     * - %d specifies that the corresponding value is a four byte signed int.
     * - %u specifies that the corresponding value is a four byte unsigned int.
     * - %o specifies that the corresponding value is a four byte octal signed int.
     * - %x specifies that the corresponding value is a four byte hexadecimal signed int.
     * - %f specifies that the corresponding value is a four byte floating point number.
     *
     * - %l specifies that the corresponding value is an eight byte signed int.
     * - %L specifies that the corresponding value is an eight byte unsigned int.
     * - %F specifies that the corresponding value is an eight byte floating point number.
     *
     * - %D specifies that the corresponding value is a two byte signed int.
     * - %U specifies that the corresponding value is a two byte unsigned int.
     * - %O specifies that the corresponding value is a two byte octal signed int.
     * - %X specifies that the corresponding value is a two byte hexadecimal signed int.
     *
     * - %c specifies that the corresponding value is a one byte signed int (char).
     * - %C specifies that the corresponding value is a one byte unsigned int.
     *
     * - %s specifies that the corresponding value is a NULL terminated string.
     */

    /* first: determine number of bytes required, then read these.
     * at last do the conversion and write into the associated variable.
     */

    switch (format [1]) {
    case 'l':
    case 'L':
#if SIZEOF_LONG_INT != 8
      Tcl_AppendResult (interp, "binio unpack: %l / %L not supported, no 8byte integers here", NULL);
      return TCL_ERROR;
#endif
    case 'F':
      length = 8;
      break;

    case 'd':
    case 'u':
    case 'o':
    case 'x':
    case 'f':
      length = 4;
      break;

    case 'D':
    case 'U':
    case 'O':
    case 'X':
      length = 2;
      break;

    case 'c':
    case 'C':
      length = 1;
      break;

    case 's':
      length = -1; /* variable length, string terminated by '\0'. */
      break;
    }

    if (length == 0) {
      format [2] = '\0';
      Tcl_AppendResult (interp, "binio unpack: internal error, missing case for format ", format, NULL);
      return TCL_ERROR;
    } else if (length < 0) {
      /* variable length, string terminated by '\0'. (%s) */

      Tcl_DString data;
      Tcl_DStringInit (&data);

      while (! Tcl_Eof (inChan)) {
	actuallyRead = Tcl_Read (inChan, buffer, 1);

	if (actuallyRead < 0) {
	  Tcl_AppendResult(interp, "binio unpack: ", Tcl_GetChannelName(inChan),
			   Tcl_PosixError(interp), (char *) NULL);
	  return TCL_ERROR;
	} else if (actuallyRead > 0) {
	  Tcl_DStringAppend (&data, buffer, 1);
	  if (buffer [0] == '\0') {
	    break;
	  }
	}
      } /* while */

      val = Tcl_SetVar (interp, argv [0], data.string, TCL_LEAVE_ERR_MSG);
      Tcl_DStringFree (&data);

      if (val == NULL) {
	return TCL_ERROR;
      }
    } else {
      /* handle item with fixed lengths */


      actuallyRead = Tcl_Read (inChan, buffer, length);
      if (actuallyRead < 0) {
	Tcl_AppendResult(interp, "binio unpack: ", Tcl_GetChannelName(inChan),
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
      }

      /* check, wether reordering is required or not.
       * upon answer `yes` do the reordering here too.
       */

      if ((length > 1) &&
	  (Tcl_GetHostByteorder () != Tcl_GetChannelByteorder (inChan))) {
	ReorderBytes (buffer, length);
      }

      switch (format [1]) {
      case 'd':
#if SIZEOF_INT == 4
	/* 'int' is our 4 byte integer on this machine */
	memcpy ((VOID*) &cvt.i, (VOID*) buffer, length);
	sprintf (buffer, "%d", cvt.i);
#else
	/* 'int' seems to be equal to 'short' (2 byte), so use 'long int' instead */
	memcpy ((VOID*) &cvt.li, (VOID*) buffer, length);
	sprintf (buffer, "%ld", cvt.li);
#endif
	break;

      case 'o':
#if SIZEOF_INT == 4
	/* 'int' is our 4 byte integer on this machine */
	memcpy ((VOID*) &cvt.i, (VOID*) buffer, length);
	sprintf (buffer, "%o", cvt.i);
#else
	/* 'int' seems to be equal to 'short' (2 byte), so use 'long int' instead */
	memcpy ((VOID*) &cvt.li, (VOID*) buffer, length);
	sprintf (buffer, "%lo", cvt.li);
#endif
	break;

      case 'x':
#if SIZEOF_INT == 4
	/* 'int' is our 4 byte integer on this machine */
	memcpy ((VOID*) &cvt.i, (VOID*) buffer, length);
	sprintf (buffer, "%08x", cvt.i);
#else
	/* 'int' seems to be equal to 'short' (2 byte), so use 'long int' instead */
	memcpy ((VOID*) &cvt.li, (VOID*) buffer, length);
	sprintf (buffer, "%08lx", cvt.li);
#endif
	break;

      case 'u':
#if SIZEOF_INT == 4
	/* 'unsigned int' is our 4 byte integer on this machine */
	memcpy ((VOID*) &cvt.ui, (VOID*) buffer, length);
	sprintf (buffer, "%u", cvt.ui);
#else
	/* 'int' seems to be equal to 'short' (2 byte), so use 'unsigned long' instead */
	memcpy ((VOID*) &cvt.ul, (VOID*) buffer, length);
	sprintf (buffer, "%lu", cvt.ul);
#endif
	break;

      case 'D':
	/* 'short int' is our 2 byte integer on this machine */
	memcpy ((VOID*) &cvt.si, (VOID*) buffer, length);
	sprintf (buffer, "%d", cvt.si);
	break;

      case 'O':
	/* 'short int' is our 2 byte integer on this machine */
	memcpy ((VOID*) &cvt.si, (VOID*) buffer, length);
	sprintf (buffer, "%o", cvt.si);
	break;

      case 'X':
	/* 'short int' is our 2 byte integer on this machine */
	memcpy ((VOID*) &cvt.si, (VOID*) buffer, length);
	sprintf (buffer, "%04x", cvt.si);
	break;

      case 'U':
	/* 'unsigned short' is our 2 byte integer on this machine */
	memcpy ((VOID*) &cvt.us, (VOID*) buffer, length);
	sprintf (buffer, "%u", cvt.us);
	break;

      case 'l':
	/* assume SIZEOF_LONG_INT == 8 */
	memcpy ((VOID*) &cvt.li, (VOID*) buffer, length);
	sprintf (buffer, "%ld", cvt.li);
	break;

      case 'L':
	/* assume SIZEOF_LONG_INT == 8 */
	memcpy ((VOID*) &cvt.ul, (VOID*) buffer, length);
	sprintf (buffer, "%lu", cvt.ul);
	break;

      case 'c':
	memcpy ((VOID*) &cvt.c, (VOID*) buffer, length);
	cvt.i = cvt.c;
	sprintf (buffer, "%d", cvt.i);
	break;

      case 'C':
	memcpy ((VOID*) &cvt.uc, (VOID*) buffer, length);
	cvt.ui = cvt.uc;
	sprintf (buffer, "%u", cvt.ui);
	break;

      case 'f':
	memcpy ((VOID*) &cvt.f, (VOID*) buffer, length);
	sprintf (buffer, "%f", cvt.f);
	break;

      case 'F':
	memcpy ((VOID*) &cvt.d, (VOID*) buffer, length);
	sprintf (buffer, "%f", cvt.d);
	break;

      case 's':
	Tcl_AppendResult (interp, "binio unpack: internal error, wrong branch for %s", NULL);
	return TCL_ERROR;
	break;
      } /* switch */

      val = Tcl_SetVar (interp, argv [0], buffer, TCL_LEAVE_ERR_MSG);
      if (val == NULL) {
	return TCL_ERROR;
      }      
    } /* if (length < 0) */
  } /* for (format) */

  /* return number of unpacked items */
  sprintf (buffer, "%d", unpacked);
  Tcl_AppendResult (interp, buffer, (char*) NULL);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	ReorderBytes --
 *
 *	------------------------------------------------*
 *	This procedure reorders the bytes in a buffer to
 *	match real and intended byteorder.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		The incoming buffer 'buf' contains the
 *		reorderd bytes.
 *
 *------------------------------------------------------*
 */

static void
ReorderBytes (buf, len)
char* buf;
int   len;
{
#define FLIP(a,b) c = buf [a]; buf [a] = buf [b]; buf [b] = c;

  char c;

  if (len == 2) {
    FLIP (0,1);
  } else if (len == 4) {
    FLIP (0,3);
    FLIP (1,2);
  } else if (len == 8) {
    FLIP (0,7);
    FLIP (1,6);
    FLIP (2,5);
    FLIP (3,4);
  } else {
    Tcl_Panic ("unknown buffer size %d", len);
  }
}

/*
 *------------------------------------------------------*
 *
 *	GetHex --
 *
 *	------------------------------------------------*
 *	Read a string containing a number in hexadecimal
 *	representation and convert it into a long integer.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		'result' contains the conversion result.
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */

static int
GetHex (interp, text, result)
Tcl_Interp* interp;
char*       text;
long int*   result;
{
  int match;
  match = sscanf (text, "%lx", result);

  if (match != 1) {
    Tcl_AppendResult (interp, "expected hexadecimal integer, but got \"",
		      text, "\"", (char*) NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	GetOctal --
 *
 *	------------------------------------------------*
 *	Read a string containing a number in octal
 *	representation and convert it into a long integer.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		'result' contains the conversion result.
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */

static int
GetOctal (interp, text, result)
Tcl_Interp* interp;
char*       text;
long int*   result;
{
  int match;
  match = sscanf (text, "%lo", result);

  if (match != 1) {
    Tcl_AppendResult (interp, "expected octal integer, but got \"",
		      text, "\"", (char*) NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	BinioCmd --
 *
 *	------------------------------------------------*
 *	This procedure realizes the 'binio' command.
 *	See the manpages for details on what it does.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See the user documentation.
 *
 *	Result:
 *		A standard Tcl result.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
static int
BinioCmd (notUsed, interp, argc, argv)
ClientData  notUsed;		/* Not used. */
Tcl_Interp* interp;		/* Current interpreter. */
int         argc;		/* Number of arguments. */
char**      argv;		/* Argument strings. */
{
  /*
   * Allowed syntax:
   *
   * binio copy   inChannel outChannel ?count?
   * binio pack   outChannel format ?data1 data2 ...?
   * binio unpack inChannel  format ?var1 var2 ...?
   */

  int len;
  char c;
  Tcl_Channel a;
  int mode;

  if (argc < 3) {
    Tcl_AppendResult (interp,
		      "wrong # args: should be \"binio option channel ?arg arg...?\"",
		      (char*) NULL);
    return TCL_ERROR;
  }

  c = argv [1][0];
  len = strlen (argv [1]);

  a = Tcl_GetChannel (interp, argv [2], &mode);

  if (a == (Tcl_Channel) NULL) {
    Tcl_ResetResult (interp);
    Tcl_AppendResult (interp,
		      "binio ", argv [1],
		      ": channel expected as 2nd argument, got \"",
		      argv [2], "\"", (char*) NULL);

    return TCL_ERROR;
  }

  switch (c) {
  case 'c':
    if (0 == strncmp (argv [1], "copy", len)) {
      return CopyCmd (interp, argc - 2, argv + 2);
    } else
      goto unknown_option;
    break;

  case 'p':
    if (0 == strncmp (argv [1], "pack", len)) {
      return PackCmd (interp, argc - 2, argv + 2);
    } else
      goto unknown_option;
    break;

  case 'u':
    if (0 == strncmp (argv [1], "unpack", len)) {
      return UnpackCmd (interp, argc - 2, argv + 2);
    } else
      goto unknown_option;
    break;

  default:
  unknown_option:
    Tcl_AppendResult (interp,
		      "binio: bad option \"", argv [1],
		      "\": should be one of copy, pack or unpack",
		      (char*) NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}
#endif /* ENABLE_BINIO */

/*
 *------------------------------------------------------*
 *
 *	TrfInit_Binio --
 *
 *	------------------------------------------------*
 *	Initializes this command.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of 'Tcl_CreateCommand'.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

int
TrfInit_Binio (interp)
Tcl_Interp* interp;
{
#ifdef ENABLE_BINIO
  Tcl_CreateCommand (interp, "binio", BinioCmd,
		     (ClientData) NULL,
		     (Tcl_CmdDeleteProc *) NULL);
#endif /* ENABLE_BINIO */
  return TCL_OK;
}
