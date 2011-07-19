/*
 * reflect.c --
 *
 *	Implements and registers conversion channel relying on
 *	tcl-scripts to do the conversion. In other words: The
 *	transformation functionality is reflected up into the
 *	tcl-level. In case of binary data this will be usable
 *	only with tcl 8.0 and up.
 *
 *
 * Copyright (c) 1995 Andreas Kupries (a.kupries@westend.com)
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
 * CVS: $Id: reflect.c,v 1.25 2009/05/07 04:57:27 andreas_kupries Exp $
 */

#include "reflect.h"

/*
 * Converter description
 * ---------------------
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

static int
MaxRead        _ANSI_ARGS_ ((Trf_ControlBlock ctrlBlock,
			     ClientData       clientData));

/*
 * Converter definition.
 */

static Trf_TypeDefinition reflectDefinition =
{
  "transform",
  NULL, /* filled by TrfInit_Transform, THREAD: serialize initialization */
  NULL, /* filled by TrfInit_Transform, THREAD: serialize initialization */
  {
    CreateEncoder,
    DeleteEncoder,
    NULL,
    EncodeBuffer,
    FlushEncoder,
    ClearEncoder,
    MaxRead
  }, {
    CreateDecoder,
    DeleteDecoder,
    NULL,
    DecodeBuffer,
    FlushDecoder,
    ClearDecoder,
    MaxRead
  },
  TRF_UNSEEKABLE
};



/*
 *------------------------------------------------------*
 *
 *	TrfInit_Transform --
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
TrfInit_Transform (interp)
Tcl_Interp* interp;
{
  TrfLock; /* THREADING: serialize initialization */
  reflectDefinition.options = TrfTransformOptions ();
  TrfUnlock;

  return Trf_Register (interp, &reflectDefinition);
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
ClientData     writeClientData;
Trf_WriteProc* fun;
Trf_Options    optInfo;
Tcl_Interp*    interp;
ClientData     clientData;
{
  ReflectControl*          c;
  TrfTransformOptionBlock* o = (TrfTransformOptionBlock*) optInfo;
  int                    res;

  c = (ReflectControl*) ckalloc (sizeof (ReflectControl));
  c->write           = fun;
  c->writeClientData = writeClientData;
  c->interp          = interp;

  /* Store reference, tell the interpreter about it. */
  c->command      = o->command;
  Tcl_IncrRefCount (c->command);

  c->maxRead = -1;
  c->naturalRatio.numBytesTransform = 0;
  c->naturalRatio.numBytesDown = 0;

  res = RefExecuteCallback (c, interp,
			    (unsigned char*) "create/write",
			    NULL, 0, TRANSMIT_DONT, 0);

  if (res != TCL_OK) {
    Tcl_DecrRefCount (c->command);
    ckfree ((VOID*) c);
    return (ClientData) NULL;
  }

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
  ReflectControl* c = (ReflectControl*) ctrlBlock;

  RefExecuteCallback (c, NULL, (unsigned char*) "delete/write",
		      NULL, 0, TRANSMIT_DONT, 0);

  Tcl_DecrRefCount (c->command);
  ckfree ((VOID*) c);
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
unsigned char* buffer;
int bufLen;
Tcl_Interp* interp;
ClientData clientData;
{
  ReflectControl* c = (ReflectControl*) ctrlBlock;

  return RefExecuteCallback (c, interp,
			     (unsigned char*) "write",
			     buffer, bufLen, TRANSMIT_DOWN, 1);
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
  ReflectControl* c = (ReflectControl*) ctrlBlock;

  return RefExecuteCallback (c, interp,
			     (unsigned char*) "flush/write",
			     NULL, 0, TRANSMIT_DOWN, 1);
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
  ReflectControl* c = (ReflectControl*) ctrlBlock;

  RefExecuteCallback (c, (Tcl_Interp*) NULL,
		      (unsigned char*) "clear/write",
		      NULL, 0, TRANSMIT_DONT, 0);
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
ClientData     writeClientData;
Trf_WriteProc* fun;
Trf_Options    optInfo;
Tcl_Interp*    interp;
ClientData     clientData;
{
  ReflectControl*          c;
  TrfTransformOptionBlock* o = (TrfTransformOptionBlock*) optInfo;
  int                      res;

  c = (ReflectControl*) ckalloc (sizeof (ReflectControl));
  c->write           = fun;
  c->writeClientData = writeClientData;
  c->interp          = interp;

  c->maxRead = -1;
  c->naturalRatio.numBytesTransform = 0;
  c->naturalRatio.numBytesDown = 0;

  /* Store reference, tell the interpreter about it. */
  c->command      = o->command;
  Tcl_IncrRefCount (c->command);

  res = RefExecuteCallback (c, interp,
			    (unsigned char*) "create/read",
			    NULL, 0, TRANSMIT_DONT, 0);

  if (res != TCL_OK) {
    Tcl_DecrRefCount (c->command);

    ckfree ((VOID*) c);
    return (ClientData) NULL;
  }

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
  ReflectControl* c = (ReflectControl*) ctrlBlock;

  RefExecuteCallback (c, NULL, (unsigned char*) "delete/read",
		      NULL, 0, TRANSMIT_DONT, 0);

  Tcl_DecrRefCount (c->command);
  ckfree ((VOID*) c);
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
unsigned char* buffer;
int bufLen;
Tcl_Interp* interp;
ClientData clientData;
{
  ReflectControl* c = (ReflectControl*) ctrlBlock;

  return RefExecuteCallback (c, interp,
			     (unsigned char*) "read",
			     buffer, bufLen, TRANSMIT_DOWN, 1);
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
  ReflectControl* c = (ReflectControl*) ctrlBlock;

  return RefExecuteCallback (c, interp,
			     (unsigned char*) "flush/read",
			     NULL, 0, TRANSMIT_DOWN, 1);
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
  ReflectControl* c = (ReflectControl*) ctrlBlock;

  RefExecuteCallback (c, (Tcl_Interp*) NULL,
		      (unsigned char*) "clear/read",
		      NULL, 0, TRANSMIT_DONT, 0);
}

/*
 *------------------------------------------------------*
 *
 *	MaxRead --
 *
 *	------------------------------------------------*
 *	Query the tcl level of the transformation about
 *      the max. number of bytes to read next time.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of the tcl level.
 *
 *	Result:
 *		The max. number of bytes to read.
 *
 *------------------------------------------------------*
 */

static int
MaxRead (ctrlBlock, clientData) 
Trf_ControlBlock ctrlBlock;
ClientData       clientData;
{
  ReflectControl* c = (ReflectControl*) ctrlBlock;

  c->maxRead = -1; /* unbounded consumption */

  RefExecuteCallback (c, (Tcl_Interp*) NULL,
		      (unsigned char*) "query/maxRead",
		      NULL, 0, TRANSMIT_NUM /* -> maxRead */, 1);

  return c->maxRead;
}

/*
 *------------------------------------------------------*
 *
 *	RefExecuteCallback --
 *
 *	------------------------------------------------*
 *	Execute callback for buffer and operation.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Everything possible, depending on the
 *		script executed.
 *
 *	Result:
 *		A standard TCL error code. In case of an
 *		error a message is left in the result area
 *		of the specified interpreter.
 *
 *------------------------------------------------------*
 */

int
RefExecuteCallback (c, interp, op, buf, bufLen, transmit, preserve)
ReflectControl* c;        /* Transformation instance */
Tcl_Interp*     interp;   /* Interpreter we are running in, possibly NULL */
unsigned char*  op;       /* Operation to perform by the tcl-level */
unsigned char*  buf;      /* Data for the operation */
int             bufLen;   /* Length of data above */
int             transmit; /* What to do with the result, see TRANSMIT_xxx */
int             preserve; /* Preserve result of transformation interp ? y/n */
{
  /*
   * Step 1, create the complete command to execute. Do this by appending
   * operation and buffer to operate upon to a copy of the callback
   * definition. We *cannot* create a list containing 3 objects and then use
   * 'Tcl_EvalObjv', because the command may contain additional prefixed
   * arguments. Feathers curried commands would come in handy here.
   */

  int             res = TCL_OK;
  Tcl_Obj*        resObj; /* See below, switch (transmit) */
  Tcl_Obj**       listObj;
  int             resLen;
  unsigned char*  resBuf;
#if GT81
  Tcl_SavedResult ciSave;
#endif
  Tcl_Obj* command;
  Tcl_Obj* temp;

  START (RefExecuteCallback);
  PRINT ("args = (%s | %d | %d | %d)\n", op, bufLen, transmit, preserve); FL;

  command = Tcl_DuplicateObj (c->command);

#if GT81
  if (preserve) {
    PRINTLN ("preserve");
    Tcl_SaveResult (c->interp, &ciSave);
  }
#endif

  if (command == (Tcl_Obj*) NULL) {
    /* Memory allocation problem */
    res = TCL_ERROR;
    PRINT ("command not duplicated @ %d\n", __LINE__);
    goto cleanup;
  }

  Tcl_IncrRefCount (command);

  temp = Tcl_NewStringObj ((char*) op, -1);

  if (temp == (Tcl_Obj*) NULL) {
    /* Memory allocation problem */
    PRINT ("op object not allocated @ %d\n", __LINE__);
    res = TCL_ERROR;
    goto cleanup;
  }

  res = Tcl_ListObjAppendElement (interp, command, temp);

  if (res != TCL_OK)
    goto cleanup;

  /*
   * Use a byte-array to prevent the misinterpretation of binary data
   * coming through as UTF while at the tcl level.
   */

#if GT81
  temp = Tcl_NewByteArrayObj (buf, bufLen);
#else
  temp = Tcl_NewStringObj    ((char*) buf, bufLen);
#endif

  if (temp == (Tcl_Obj*) NULL) {
    /* Memory allocation problem */
#if GT81
    PRINT ("bytearray not allocated @ %d\n", __LINE__);
#else
    PRINT ("string not allocated @ %d\n", __LINE__);
#endif
    res = TCL_ERROR;
    goto cleanup;
  }

  res = Tcl_ListObjAppendElement (interp, command, temp);

  if (res != TCL_OK)
    goto cleanup;

  /*
   * Step 2, execute the command at the global level of the interpreter
   * used to create the transformation. Destroy the command afterward.
   * If an error occured, the current interpreter is defined and not equal
   * to the interpreter for the callback, then copy the error message into
   * current interpreter. Don't copy if in preservation mode.
   */

  res = Tcl_GlobalEvalObj (c->interp, command);
  Tcl_DecrRefCount (command);
  command = (Tcl_Obj*) NULL;

  if (res != TCL_OK) {
    /* copy error message from 'c->interp' to actual 'interp'. */

    if ((interp != (Tcl_Interp*) NULL) &&
	(c->interp != interp) &&
	!preserve) {
    
        Tcl_SetObjResult (interp, Tcl_GetObjResult (c->interp));
    }

    PRINTLN ("!error"); FL;
    goto cleanup;
  }

  /*
   * Step 3, transmit a possible conversion result to the underlying
   * channel, or ourselves
   */

  switch (transmit) {
  case TRANSMIT_DONT:
    /* nothing to do */
    break;

  case TRANSMIT_DOWN:
    /* Caller said to expect data in interpreter result area.
     * Take it, then write it out to the channel system.
     */
    resObj = Tcl_GetObjResult (c->interp);
#if GT81
    resBuf = (unsigned char*) Tcl_GetByteArrayFromObj (resObj, &resLen);
#else
    resBuf = (unsigned char*) Tcl_GetStringFromObj (resObj, &resLen);
#endif
    res = c->write (c->writeClientData, resBuf, resLen, interp);
    break;

  case TRANSMIT_NUM:
    /* Interpret result as integer number */
    resObj = Tcl_GetObjResult (c->interp);

    Tcl_GetIntFromObj (c->interp, resObj, &c->maxRead);
    break;

  case TRANSMIT_RATIO:
    /* Result should be 2-element list. Ignore superfluous list elements.
     */
    resObj = Tcl_GetObjResult (c->interp);
    resLen = -1;
    res = Tcl_ListObjLength(c->interp, resObj, &resLen);

    c->naturalRatio.numBytesTransform = 0;
    c->naturalRatio.numBytesDown      = 0;

    if ((res != TCL_OK) || (resLen < 2)) {
      PRINT ("TRANSMIT_RATIO problem (%d, %d)\n",
	     res == TCL_OK, resLen);
      PRINTLN ("reset result");

      Tcl_ResetResult (c->interp);
      goto cleanup;
    }

    res = Tcl_ListObjGetElements(c->interp, resObj, &resLen, &listObj);

    Tcl_GetIntFromObj (c->interp, listObj [0],
		       &c->naturalRatio.numBytesTransform);
    Tcl_GetIntFromObj (c->interp, listObj [1],
		       &c->naturalRatio.numBytesDown);
    break;
  }

  PRINTLN ("reset result");
  Tcl_ResetResult (c->interp);

#if GT81
  if (preserve) {
    PRINTLN ("restore");
    Tcl_RestoreResult (c->interp, &ciSave);
  }
#endif

  DONE (RefExecuteCallback);
  return res;

cleanup:
  PRINTLN ("cleanup...");

#if GT81
  if (preserve) {
    PRINTLN ("restore");
    Tcl_RestoreResult (c->interp, &ciSave);
  }
#endif

  if (command != (Tcl_Obj*) NULL) {
    PRINTLN ("decr-ref command");
    Tcl_DecrRefCount (command);
  }

  DONE (RefExecuteCallback);
  return res;
}
