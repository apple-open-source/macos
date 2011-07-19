/*
 * bufExt.c --
 *
 *	Implementations of an extendable buffer.
 *
 * Copyright (c) 2000-2009 by Andreas Kupries <a.kupries@westend.com>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: bufExt.c,v 1.3 2009/03/16 18:50:59 andreas_kupries Exp $
 */

#include "memchanInt.h"
#include "buf.h"

/*
 * Forward declarations of all internal procedures.
 */

static int        ReadProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData,
					   VOID* outbuf, int size));
static int        WriteProc  _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData,
					   CONST VOID* inbuf, int size));
static Buf_Buffer DupProc    _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));
static void       FreeProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));
static int        SizeProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));
static int        TellProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));
static char*      DataProc   _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData));

/* Internal structure used to hold the buffer information.
 */

typedef struct ExtBuffer_ {
  Buf_Buffer buf;      /* The buffer token containing this structure. */
  int        size;     /* Size of the area for data, maximal amount of
			* bytes storable in the buffer. */
  char*      readLoc;  /* The location till which data was read from the
			* buffer. */
  char*      writeLoc; /* The location at which new data can be appended to
			* the buffer. */
  char*      limit;    /* A pointer behind the last character in the buffer. */
  char*      data;     /* Pointer to start of the actual container */
} ExtBuffer;

/* Declaration of the buffer type.
 */

static Buf_BufferType ext = {
  "extendable-buffer", /* Buffer of varying size */
  ReadProc,            /* Reading from a buffer */
  WriteProc,           /* Writing to a buffer */
  DupProc,             /* Duplicating a buffer */
  FreeProc,            /* Freeing all allocated resources of a buffer */
  SizeProc,            /* Number of bytes currently in the buffer. */
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
  ExtBuffer* iBuf = (ExtBuffer*) clientData;
  Tcl_Free (iBuf->data);
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
  ExtBuffer* iBuf = (ExtBuffer*) clientData;
  return (iBuf->writeLoc - iBuf->readLoc);
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
  ExtBuffer* iBuf = (ExtBuffer*) clientData;
  return iBuf->readLoc - iBuf->data;
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
  ExtBuffer* iBuf = (ExtBuffer*) clientData;
  return iBuf->data;
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
  ExtBuffer* iBuf   = (ExtBuffer*) clientData;
  ExtBuffer* newBuf = (ExtBuffer*) Tcl_Alloc (sizeof(ExtBuffer) +
					      (iBuf->limit - iBuf->data));
  Buf_Buffer   new    = Buf_Create (&ext, (ClientData) newBuf);

  newBuf->buf      = new;
  newBuf->data     = Tcl_Alloc (iBuf->size);
  newBuf->size     = iBuf->size;
  newBuf->readLoc  = newBuf->data + (iBuf->readLoc  - iBuf->data);
  newBuf->writeLoc = newBuf->data + (iBuf->writeLoc - iBuf->data);
  newBuf->limit    = newBuf->data + newBuf->size;

  if ((iBuf->writeLoc - iBuf->readLoc) > 0) {
    /* Copy just that part of container which was not read already
     */
    memcpy (newBuf->readLoc, iBuf->readLoc, iBuf->writeLoc - iBuf->readLoc);
  }

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
  ExtBuffer* iBuf  = (ExtBuffer*) clientData;
  int        bSize = iBuf->writeLoc - iBuf->readLoc;

  if ((bSize <= 0) || (size <= 0)) {
    return 0;
  }

  if (bSize < size) {
    size = bSize;
  }

  memcpy (outbuf, iBuf->readLoc, size);
  iBuf->readLoc += size;

  return size;
}

/*
 *------------------------------------------------------*
 *
 *	WriteProc --
 *
 *	Writes at most size bytes from inbuf and appends
 *	it the buffer
 *
 *	Sideeffects:
 *		Moves the write pointer behind the bytes
 *		just written into the buffer.
 *
 *	Result:
 *		The number of bytes actually written
 *		into the buffer.
 *
 *------------------------------------------------------*
 */

int
WriteProc (buf, clientData, inbuf, size)
     Buf_Buffer  buf;
     ClientData  clientData;
     CONST void* inbuf;
     int         size;
{
  ExtBuffer* iBuf  = (ExtBuffer*) clientData;
  int        bSize = iBuf->limit - iBuf->writeLoc;

  if (size <= 0) {
    return 0;
  }

  if (bSize < size) {
    /* Not enough memory to copy the whole input to the buffer.
     * So extend it now.
     */

    char* ndata = (char*) Tcl_Alloc (iBuf->size + size);

    memcpy (ndata, iBuf->data, iBuf->size);

    iBuf->size    += size;
    iBuf->readLoc  = ndata + (iBuf->readLoc  - iBuf->data);
    iBuf->writeLoc = ndata + (iBuf->writeLoc - iBuf->data);
    iBuf->limit    = ndata + iBuf->size;
    iBuf->data     = ndata;
  }

  memcpy (iBuf->writeLoc, inbuf, size);
  iBuf->writeLoc += size;

  return size;
}


/*
 * ------------------------------------------------------------
 */

Buf_Buffer
Buf_CreateExtendableBuffer (size)
     int size;
{
  ExtBuffer* newBuf = (ExtBuffer*) Tcl_Alloc (sizeof(ExtBuffer));
  Buf_Buffer new    = Buf_Create (&ext, (ClientData) newBuf);

  newBuf->buf      = new;
  newBuf->size     = size;
  newBuf->data     = Tcl_Alloc (size);
  newBuf->readLoc  = newBuf->data;
  newBuf->writeLoc = newBuf->data;
  newBuf->limit    = newBuf->data + size;

  return new;
}

