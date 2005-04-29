#ifndef BUF_H
/*
 * buf.h --
 *
 *	Definitions for buffer objects.
 *
 * Copyright (C) 2000 Andreas Kupries (a.kupries@westend.com)
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
 * CVS: $Id: buf.h,v 1.3 2002/08/23 18:04:40 andreas_kupries Exp $
 */


#include <errno.h>
#include <tcl.h>

/*
 * Windows needs to know which symbols to export.  Unix does not.
 * BUILD_Memchan should be undefined for Unix.
 */

#ifdef BUILD_Memchan
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif /* BUILD_Memchan */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The definitions in this header and the accompanying code define a
 * generic buffer object.
 *
 * The code here is partially based upon the buffer structures used by
 * the tcl core and uponconcepts laid out in the STREAMS paper at
 * http://cm.bell-labs.com/cm/cs/who/dmr/st.html
 *
 * I hope that it can and will be used in a future reorganization of
 * the core I/O system.
 */

/* Basis:
 *	Refcounted buffers. The structures actually holding
 *	information. Buffers are typed to allow differentation
 *	between simple data and special control blocks.
 *
 *	To the outside they are opaque tokens.
 *	All access to them have to go through the public API.
 */

typedef struct Buf_Buffer_* Buf_Buffer;

/*
 * Definition of the type for variables referencing a position
 * in a buffer. Opaque token.
 */

typedef struct Buf_BufferPosition_* Buf_BufferPosition;

/*
 * Another opaque structure: Queues of buffers.
 * The queues defined here are thread-safe !
 */

typedef struct Buf_BufferQueue_* Buf_BufferQueue;



/* The structure for a buffer type.
 * Before that the interfaces of the procedures used by buffer types.
 */

typedef int (Buf_ReadProc) _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData,
					VOID* outbuf, int size));

typedef int (Buf_WriteProc) _ANSI_ARGS_ ((Buf_Buffer buf, ClientData clientData,
					 CONST VOID* inbuf, int size));

typedef Buf_Buffer (Buf_DuplicateProc) _ANSI_ARGS_ ((Buf_Buffer buf,
						    ClientData clientData));

typedef void (Buf_FreeProc) _ANSI_ARGS_ ((Buf_Buffer buf,
					 ClientData clientData));

typedef int (Buf_SizeProc) _ANSI_ARGS_ ((Buf_Buffer buf,
					ClientData clientData));

typedef int (Buf_TellProc) _ANSI_ARGS_ ((Buf_Buffer buf,
					ClientData clientData));

typedef char* (Buf_DataProc) _ANSI_ARGS_ ((Buf_Buffer buf,
					  ClientData clientData));

typedef struct Buf_BufferType_ {
  char* typeName;               /* The name of the buffer type.
				 * Statically allocated.
				 * Not touched by the system */
  Buf_ReadProc*      readProc;  /* Procedure called to read data
				 * from a buffer of this type. */
  Buf_WriteProc*     writeProc; /* Procedure called to write data
				 * into a buffer of this type. */
  Buf_DuplicateProc* dupProc;   /* Procedure called to duplicate
				 * a buffer of this type. */
  Buf_FreeProc*      freeProc;  /* Procedure called to free
				 * a buffer of this type. */
  Buf_SizeProc*      sizeProc;  /* Procedure called to ask for the
				 * size of a buffer of this type. */
  Buf_TellProc*      tellProc;  /* Procedure called to ask for the
				 * offset of the read location. */
  Buf_DataProc*      dataProc;  /* Procedure called to ask for a
				 * pointer to the data area of a
				 * buffer. */
} Buf_BufferType;


#include "bufDecls.h"


#ifdef __cplusplus
}
#endif /* C++ */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* BUF_H */
