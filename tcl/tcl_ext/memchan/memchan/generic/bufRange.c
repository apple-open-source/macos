/*
 * bufRange.c --
 *
 *	Implementations of a range into a buffer.
 *
 * Copyright (c) 2000 by Andreas Kupries <a.kupries@westend.com>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: bufRange.c,v 1.2 2002/04/25 06:29:48 andreas_kupries Exp $
 */

#include "buf.h"

/*
 * Forward declarations of all internal procedures.
 */

static int        ReadProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData,
					   VOID* outbuf, int size));
static Buf_Buffer DupProc    _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));
static void       FreeProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));
static int        SizeProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));
static int        TellProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));
static char*      DataProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));


/* Internal structure used to hold the buffer information.
 */

typedef struct RangeBuffer_ {
  Buf_Buffer         buf;  /* The buffer token containing this structure. */
  int                size; /* The size of the range. */
  Buf_BufferPosition loc;  /* The logical position into the underlying buffer */
} RangeBuffer;

/* Declaration of the buffer type.
 */

static Buf_BufferType range = {
  "extendable-buffer", /* Buffer of varying size */
  ReadProc,            /* Reading from a buffer */
  NULL,                /* Writing to a range not allowed */
  DupProc,             /* Duplicating a buffer */
  FreeProc,            /* Freeing all allocated resources of a buffer */
  SizeProc,             /* Number of bytes currently in the buffer. */
  TellProc,            /* Return current location */
  DataProc             /* Return start of data */
};


/*
 *------------------------------------------------------*
 *
 *	FreeProc --
 *
 *	Deallocates the resources of the buffer.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

void
FreeProc (buf, clientData)
     Buf_Buffer buf;
     ClientData clientData;
{
  RangeBuffer* iBuf = (RangeBuffer*) clientData;

  Buf_IncrRefcount (iBuf->buf);
  Buf_FreePosition (iBuf->loc);
  Tcl_Free ((char*) iBuf);
}

/*
 *------------------------------------------------------*
 *
 *	SizeProc --
 *
 *	Returns the number of bytes currently stored in
 *	the buffer.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		See above.
 *
 *------------------------------------------------------*
 */

int
SizeProc (buf, clientData)
     Buf_Buffer buf;
     ClientData clientData;
{
  RangeBuffer* iBuf = (RangeBuffer*) clientData;
  return iBuf->size;
}

/*
 *------------------------------------------------------*
 *
 *	TellProc --
 *
 *	Returns the offset of the current read location
 *	relative to the start of the data.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		See above.
 *
 *------------------------------------------------------*
 */

int
TellProc (buf, clientData)
     Buf_Buffer buf;
     ClientData clientData;
{
  RangeBuffer* iBuf    = (RangeBuffer*) clientData;
  return Buf_PositionOffset (iBuf->loc);
}

/*
 *------------------------------------------------------*
 *
 *	DataProc --
 *
 *	Returns the start of the data area.
 *	(Here: Start of the data area in the underlying buffer)
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		See above.
 *
 *------------------------------------------------------*
 */

char*
DataProc (buf, clientData)
     Buf_Buffer buf;
     ClientData clientData;
{
  RangeBuffer* iBuf = (RangeBuffer*) clientData;
  return Buf_GetType (iBuf->buf)->dataProc (iBuf->buf, Buf_GetClientData (iBuf->buf));
}

/*
 *------------------------------------------------------*
 *
 *	DupProc --
 *
 *	Duplicates a buffer and its contents.
 *
 *	Sideeffects:
 *		Allocates memory.
 *
 *	Result:
 *		A new buffer token.
 *
 *------------------------------------------------------*
 */

Buf_Buffer
DupProc (buf, clientData)
     Buf_Buffer buf;
     ClientData clientData;
{
  RangeBuffer* iBuf   = (RangeBuffer*) clientData;
  RangeBuffer* newBuf = (RangeBuffer*) Tcl_Alloc (sizeof(RangeBuffer));
  Buf_Buffer   new    = Buf_Create (&range, (ClientData) newBuf);

  newBuf->buf  = iBuf->buf;
  newBuf->size = iBuf->size;
  newBuf->loc  = Buf_DupPosition (iBuf->loc);

  Buf_IncrRefcount (newBuf->buf);

  return new;
}

/*
 *------------------------------------------------------*
 *
 *	ReadProc --
 *
 *	Reads at most size bytes from the current location
 *	in the buffer and stores it into outbuf.
 *
 *	Sideeffects:
 *		Moves the read pointer behind the bytes
 *		just read from the buffer.
 *
 *	Result:
 *		The number of bytes actually read from
 *		the buffer.
 *
 *------------------------------------------------------*
 */

int
ReadProc (buf, clientData, outbuf, size)
     Buf_Buffer  buf;
     ClientData  clientData;
     VOID*       outbuf;
     int         size;
{
  RangeBuffer* iBuf  = (RangeBuffer*) clientData;

  if ((iBuf->size <= 0) || (size <= 0)) {
    return 0;
  }

  if (iBuf->size < size) {
    size = iBuf->size;
  }

  memcpy (outbuf, Buf_PositionPtr (iBuf->loc), size);
  Buf_MovePosition (iBuf->loc, size);
  iBuf->size -= size;

  return size;
}


/*
 * ------------------------------------------------------------
 */

Buf_Buffer
Buf_CreateRange (buf, size)
     Buf_Buffer buf;
     int        size;
{
  /* Check for a range as underlying buffer and use its original
   * as our base. The location is computed relative to the range
   * in that case.
   */

  RangeBuffer*       newBuf;
  Buf_Buffer         new;
  Buf_BufferPosition loc;

  if (Buf_Size (buf) < size) {
    /* Not enough data in the buffer for the range */
    return (Buf_Buffer) NULL;
  }

  newBuf = (RangeBuffer*) Tcl_Alloc (sizeof(RangeBuffer));
  new    = Buf_Create (&range, (ClientData) newBuf);
  loc    = Buf_Tell (buf);

  if (Buf_GetType (buf) == &range) {

    /* The offset we can retrieve from 'loc' is calculated relative to
     * the start of the data area of 'buf', but as 'buf' is a range
     * this is actually already relative to the start of the data area
     * of the underlying buffer itself (See DataProc, TellProc in this
     * file).
     *
     * So construct a location using this offset and the underlying
     * buffer to get the location we need here.
     */

    Buf_Buffer         data = ((RangeBuffer*) Buf_GetClientData (buf))->buf;
    Buf_BufferPosition dloc = Buf_PositionFromOffset (data, Buf_PositionOffset (loc));

    Buf_FreePosition (loc);

    loc = dloc;
    buf = data;
  }

  newBuf->buf  = buf;
  newBuf->size = size;
  newBuf->loc  = loc;

  Buf_IncrRefcount (buf);
  return (Buf_Buffer) new;
}
