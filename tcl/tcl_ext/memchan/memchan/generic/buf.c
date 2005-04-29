/*
 * buf.c --
 *
 *	Implementations of functions in the platform independent public Buf API.
 *
 * Copyright (c) 2000 by Andreas Kupries <a.kupries@westend.com>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: buf.c,v 1.1 2000/09/26 21:17:49 aku Exp $
 */

#include "buf.h"

/*
 * Codestring used to associate the
 * initialization flag with an interpreter.
 */

#define ASSOC "memchan:buf"

/* Internal declaration of buffers.
 */

typedef struct Buffer_ {
  Buf_BufferType* type;       /* Reference to the type of the buffer */
  ClientData      clientData; /* The information pertinent to the used type. */
  int             refCount;   /* Number of references hold by other parts
			       * of the application to this buffer. Initialized
			       * to 0 by the creator procedures implemented here.
			       */
} Buffer;

/* Internal declaration of a buffer position.
 */

typedef struct BufferPosition_ {
  Buf_Buffer buf;    /* The buffer this position belongs to */
  int        offset; /* The offset into the data area */
} BufferPosition;

/*
 *------------------------------------------------------*
 *
 *	Buf_IsInitialized --
 *
 *	Check wether the buffer system is initialized for
 *	the specified interpreter
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		A boolean value. 0 indicates no
 *		initialization, 1 the opposite.
 *
 *------------------------------------------------------*
 */

int
Buf_IsInitialized (interp)
     Tcl_Interp* interp;
{
  Tcl_InterpDeleteProc* proc = (Tcl_InterpDeleteProc*) NULL;
  return (int) Tcl_GetAssocData (interp, ASSOC, &proc);
}

/*
 *------------------------------------------------------*
 *
 *	Buf_Init --
 *
 *	Initializes the buffer system in the specified
 *	interpreter.
 *
 *	Sideeffects:
 *		Associates data with the specified
 *		interpreter to remember the initialization
 *
 *	Result:
 *		A standard TCL error code.
 *
 *------------------------------------------------------*
 */


int
Buf_Init (interp)
     Tcl_Interp* interp;
{
  /* There is nothing to initialize here. But we have an operation
   * querying the state of initialization, so we have to remember
   * at least this.
   */

  if (Buf_IsInitialized (interp)) {
    /* catch multiple initialization of an interpreter
     */
    return TCL_OK;
  }

  Tcl_SetAssocData (interp, ASSOC,
		    (Tcl_InterpDeleteProc*) NULL,
		    (ClientData) 1);

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_RegisterType --
 *
 *	Registers a new buffer type.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

void
Buf_RegisterType (bufType)
     Buf_BufferType* bufType;
{
  /* There are currently no operations in the interface which require
   * an internal list of registered buffer types, etc. So this function
   * can and will be a no-op for now.
   */
}

/*
 *------------------------------------------------------*
 *
 *	Buf_IncrRefcount --
 *
 *	Tells the specified buffer that a new reference
 *	to it is hold by someone else.
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
Buf_IncrRefcount (buf)
     Buf_Buffer buf;
{
  ((Buffer*) buf)->refCount ++;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_DecrRefcount --
 *
 *	Tells the specified buffer that a reference to it
 *	is no longer in existence.
 *
 *	Sideeffects:
 *		Deletes the buffer if the last reference
 *		to it is released.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

void
Buf_DecrRefcount (buf)
     Buf_Buffer buf;
{
  Buffer* iBuf = (Buffer*) buf;

  iBuf->refCount --;

  if (iBuf->refCount <= 0) {
    /* No references are hold by anyone else anymore.
     * So remove the buffer, we won't need it again.
     */

    iBuf->type->freeProc (buf, iBuf->clientData);
    Tcl_Free ((char*) iBuf);
  }
}

/*
 *------------------------------------------------------*
 *
 *	Buf_IsShared --
 *
 *	Checks wether the buffer is shared among
 *	different parts of the application.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		A boolean value. 1 indicates a shared
 *		buffer, 0 the opposite.
 *
 *------------------------------------------------------*
 */

int
Buf_IsShared (buf)
     Buf_Buffer buf;
{
  return (((Buffer*) buf)->refCount > 1);
}

/*
 *------------------------------------------------------*
 *
 *	Buf_GetType --
 *
 *	Retrieves the type structure of a buffer.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		A reference to the type of the buffer.
 *
 *------------------------------------------------------*
 */

Buf_BufferType*
Buf_GetType (buf)
     Buf_Buffer buf;
{
  return ((Buffer*) buf)->type;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_GetTypeName --
 *
 *	Retrieves the name of the type of a buffer.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		A reference to the string containing the
 *		name of the type of the buffer.
 *
 *------------------------------------------------------*
 */

CONST char*
Buf_GetTypeName (buf)
     Buf_Buffer buf;
{
  return ((Buffer*) buf)->type->typeName;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_Size --
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
Buf_Size (buf)
     Buf_Buffer buf;
{
  Buffer* iBuf = (Buffer*) buf;
  return iBuf->type->sizeProc (buf, iBuf->clientData);
}

/*
 *------------------------------------------------------*
 *
 *	Buf_GetClientData --
 *
 *	Retrieves the type specific client information
 *	of a buffer.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		The client information of the buffer.
 *
 *------------------------------------------------------*
 */

ClientData
Buf_GetClientData (buf)
     Buf_Buffer buf;
{
  return ((Buffer*) buf)->clientData;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_Create --
 *
 *	Create a new buffer (refcount 0) from its type
 *	and the asssociated type specific information
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
Buf_Create (bufType, clientData)
     Buf_BufferType* bufType;
     ClientData      clientData;
{
  Buffer* iBuf     = (Buffer*) Tcl_Alloc (sizeof (Buffer));

  iBuf->type       = bufType;
  iBuf->clientData = clientData;
  iBuf->refCount   = 0;

  return (Buf_Buffer) iBuf;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_Dup --
 *
 *	Duplicates a buffer and its contents.
 *
 *	Sideeffects:
 *		As of the used buffer driver.
 *
 *	Result:
 *		A new buffer token.
 *
 *------------------------------------------------------*
 */

Buf_Buffer
Buf_Dup (buf)
     Buf_Buffer buf;
{
  Buffer* iBuf = (Buffer*) buf;
  return iBuf->type->dupProc (buf, iBuf->clientData);
}

/*
 *------------------------------------------------------*
 *
 *	Buf_Read --
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
Buf_Read (buf, outbuf, size)
     Buf_Buffer  buf;
     VOID*       outbuf;
     int         size;
{
  Buffer* iBuf = (Buffer*) buf;
  return iBuf->type->readProc (buf, iBuf->clientData, outbuf, size);
}

/*
 *------------------------------------------------------*
 *
 *	Buf_Write --
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
Buf_Write (buf, inbuf, size)
     Buf_Buffer  buf;
     CONST VOID* inbuf;
     int         size;
{
  Buffer* iBuf = (Buffer*) buf;

  if (iBuf->type->writeProc == NULL) {
    return 0;
  } else {
    return iBuf->type->writeProc (buf, iBuf->clientData, inbuf, size);
  }
}

/*
 *------------------------------------------------------*
 *
 *	Buf_PositionPtr --
 *
 *	Converts the logical location in the buffer into
 *	a reference to the data area.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		A character pointer
 *
 *------------------------------------------------------*
 */

char*
Buf_PositionPtr (loc)
     Buf_BufferPosition loc;
{
  BufferPosition* bPos = (BufferPosition*) loc;
  Buffer*         iBuf = (Buffer*) bPos->buf;
  char*           data = iBuf->type->dataProc (bPos->buf, iBuf->clientData);

  return data + bPos->offset;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_PositionOffset --
 *
 *	Converts the logical location in the buffer into
 *	an offset relative to the start of the data area
 *	of the underlying buffer.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		An integer value.
 *
 *------------------------------------------------------*
 */

int
Buf_PositionOffset (loc)
     Buf_BufferPosition loc;
{
  BufferPosition* bPos = (BufferPosition*) loc;
  return bPos->offset;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_Tell --
 *
 *	Creates a logical position in the buffer pointing
 *	to the current location of the read pointer.
 *
 *	Sideeffects:
 *		Allocates memory, adds a reference to
 *		the buffer.
 *
 *	Result:
 *		A buffer position.
 *
 *------------------------------------------------------*
 */

Buf_BufferPosition
Buf_Tell (buf)
     Buf_Buffer buf;
{
  Buffer*         iBuf = (Buffer*) buf;
  BufferPosition* bPos = (BufferPosition*) Tcl_Alloc (sizeof (BufferPosition));

  bPos->buf    = buf;
  bPos->offset = iBuf->type->tellProc (buf, iBuf->clientData);

  /*
   * The position holds a reference to the buffer, remember that.
   */

  Buf_IncrRefcount (buf);

  return (Buf_BufferPosition) bPos;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_FreePosition --
 *
 *	Deletes a logical position in a buffer.
 *
 *	Sideeffects:
 *		Deallocates memory, removes a reference to
 *		the buffer, may free the referenced buffer.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

void
Buf_FreePosition (loc)
     Buf_BufferPosition loc;
{
  BufferPosition* bPos = (BufferPosition*) loc;

  Buf_DecrRefcount (bPos->buf);
  Tcl_Free ((char*) bPos);
}

/*
 *------------------------------------------------------*
 *
 *	Buf_MovePosition --
 *
 *	Move a logical position in a buffer.
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
Buf_MovePosition (loc, offset)
     Buf_BufferPosition loc;
     int                offset;
{
  BufferPosition* bPos = (BufferPosition*) loc;

  if ((bPos->offset + offset) < 0) {
    Tcl_Panic ("Moved buffer location out of range");
  }

  bPos->offset += offset;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_DupPosition --
 *
 *	Duplicates a logical position in a buffer.
 *
 *	Sideeffects:
 *		Allocates memory, adds another reference
 *		to the underlying buffer.
 *
 *	Result:
 *		A token for the new buffer position.
 *
 *------------------------------------------------------*
 */

Buf_BufferPosition
Buf_DupPosition (loc)
     Buf_BufferPosition loc;
{
  BufferPosition* bPos   = (BufferPosition*) loc;
  BufferPosition* newPos = (BufferPosition*) Tcl_Alloc (sizeof (BufferPosition));

  newPos->buf    = bPos->buf;
  newPos->offset = bPos->offset;

  Buf_IncrRefcount (bPos->buf);

  return (Buf_BufferPosition) newPos;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_PositionFromOffset --
 *
 *	Creates a logical position from an offset into
 *	the data area and a buffer.
 *
 *	Sideeffects:
 *		Allocates memory, adds another reference
 *		to the buffer.
 *
 *	Result:
 *		A token for the new buffer position.
 *
 *------------------------------------------------------*
 */

Buf_BufferPosition
Buf_PositionFromOffset (buf, offset)
     Buf_Buffer buf;
     int        offset;
{
  BufferPosition* newPos = (BufferPosition*) Tcl_Alloc (sizeof (BufferPosition));

  newPos->buf    = buf;
  newPos->offset = offset;

  Buf_IncrRefcount (buf);

  return (Buf_BufferPosition) newPos;
}
