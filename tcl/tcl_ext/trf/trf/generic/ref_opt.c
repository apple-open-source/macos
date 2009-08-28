/*
 * convert.c --
 *
 *	Implements the C level procedures handling option processing
 *	of transformation reflecting the work to the tcl level.
 *
 *
 * Copyright (c) 1997 Andreas Kupries (a.kupries@westend.com)
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
 * CVS: $Id: ref_opt.c,v 1.8 2000/11/18 22:42:31 aku Exp $
 */

#include "reflect.h"

/*
 * forward declarations of all internally used procedures.
 */

static Trf_Options
CreateOptions _ANSI_ARGS_ ((ClientData clientData));

static void
DeleteOptions _ANSI_ARGS_ ((Trf_Options options,
			    ClientData  clientData));
static int
CheckOptions  _ANSI_ARGS_ ((Trf_Options            options,
			    Tcl_Interp*            interp,
			    CONST Trf_BaseOptions* baseOptions,
			    ClientData             clientData));

static int
SetOption     _ANSI_ARGS_ ((Trf_Options    options,
			    Tcl_Interp*    interp,
			    CONST char*    optname,
			    CONST Tcl_Obj* optvalue,
			    ClientData     clientData));

static int
QueryOptions  _ANSI_ARGS_ ((Trf_Options options,
			    ClientData  clientData));

static void
SeekQueryOptions  _ANSI_ARGS_ ((Tcl_Interp*          interp,
				Trf_Options          options,
				Trf_SeekInformation* seekInfo,
				ClientData           clientData));

/*
 *------------------------------------------------------*
 *
 *	TrfTransformOptions --
 *
 *	------------------------------------------------*
 *	Accessor to the set of vectors realizing option
 *	processing for reflecting transformation.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		See above.
 *
 *------------------------------------------------------*
 */

Trf_OptionVectors*
TrfTransformOptions ()
{
  static Trf_OptionVectors optVec = /* THREADING: const, read-only => safe */
    {
      CreateOptions,
      DeleteOptions,
      CheckOptions,
      NULL,      /* no string procedure for 'SetOption' */
      SetOption,
      QueryOptions,
      SeekQueryOptions /* Tcl level can change/define the ratio */
    };

  return &optVec;
}

/*
 *------------------------------------------------------*
 *
 *	CreateOptions --
 *
 *	------------------------------------------------*
 *	Create option structure for reflecting transformation.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Allocates memory and initializes it as
 *		option structure for reflecting
 *		transformations.
 *
 *	Result:
 *		A reference to the allocated block of
 *		memory.
 *
 *------------------------------------------------------*
 */

static Trf_Options
CreateOptions (clientData)
ClientData clientData;
{
  TrfTransformOptionBlock* o;

  o = (TrfTransformOptionBlock*) Tcl_Alloc (sizeof (TrfTransformOptionBlock));
  o->mode    = TRF_UNKNOWN_MODE;
  o->command = (Tcl_Obj*) NULL;

  return (Trf_Options) o;
}

/*
 *------------------------------------------------------*
 *
 *	DeleteOptions --
 *
 *	------------------------------------------------*
 *	Delete option structure of a reflecting transformation
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		A memory block allocated by 'CreateOptions'
 *		is released.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
DeleteOptions (options, clientData)
Trf_Options options;
ClientData  clientData;
{
  TrfTransformOptionBlock* o = (TrfTransformOptionBlock*) options;

  if (o->command != NULL) {
    Tcl_DecrRefCount (o->command);
  }

  Tcl_Free ((VOID*) o);
}

/*
 *------------------------------------------------------*
 *
 *	CheckOptions --
 *
 *	------------------------------------------------*
 *	Check the given option structure for errors.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		May modify the given structure to set
 *		default values into uninitialized parts.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
CheckOptions (options, interp, baseOptions, clientData)
Trf_Options            options;
Tcl_Interp*            interp;
CONST Trf_BaseOptions* baseOptions;
ClientData             clientData;
{
  TrfTransformOptionBlock* o = (TrfTransformOptionBlock*) options;

  if (o->command == NULL) {
    Tcl_AppendResult (interp, "command not specified", (char*) NULL);
    return TCL_ERROR;
  }

  if ((o->command->bytes == 0) && (o->command->typePtr == NULL)) {
    /* object defined, but empty, reject this too */
    Tcl_AppendResult (interp, "command specified, but empty", (char*) NULL);
    return TCL_ERROR;
  }

  if (baseOptions->attach == (Tcl_Channel) NULL) /* IMMEDIATE? */ {
    if (o->mode == TRF_UNKNOWN_MODE) {
      Tcl_AppendResult (interp, "-mode option not set", (char*) NULL);
      return TCL_ERROR;
    }
  } else /* ATTACH */ {
    if (o->mode != TRF_UNKNOWN_MODE) {
      /* operation mode irrelevant for attached transformation,
       * and specification therefore ruled as illegal.
       */
      Tcl_AppendResult (interp, "mode illegal for attached transformation",
			(char*) NULL);
      return TCL_ERROR;
    }
    o->mode = TRF_WRITE_MODE;
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	SetOption --
 *
 *	------------------------------------------------*
 *	Define value of given option.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Sets the given value into the option
 *		structure
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
SetOption (options, interp, optname, optvalue, clientData)
Trf_Options    options;
Tcl_Interp*    interp;
CONST char*    optname;
CONST Tcl_Obj* optvalue;
ClientData     clientData;
{
  TrfTransformOptionBlock* o = (TrfTransformOptionBlock*) options;
  int                     len;
  CONST char*             value;

  len = strlen (optname+1);

  switch (optname [1]) {
  case 'm':
    if (0 != strncmp (optname, "-mode", len))
      goto unknown_option;

    value = Tcl_GetStringFromObj ((Tcl_Obj*) optvalue, NULL);
    len = strlen (value);

    switch (value [0]) {
    case 'r':
      if (0 != strncmp (value, "read", len))
	goto unknown_mode;
      
      o->mode = TRF_READ_MODE;
      break;

    case 'w':
      if (0 != strncmp (value, "write", len))
	goto unknown_mode;
      
      o->mode = TRF_WRITE_MODE;
      break;

    default:
    unknown_mode:
      Tcl_AppendResult (interp, "unknown mode '", (char*) NULL);
      Tcl_AppendResult (interp, value, (char*) NULL);
      Tcl_AppendResult (interp, "', should be 'read' or 'write'", (char*) NULL);
      return TCL_ERROR;
      break;
    } /* switch optvalue */
    break;

  case 'c':
    if (0 != strncmp (optname, "-command", len))
      goto unknown_option;

    /* 'optvalue' contains the command to execute for a buffer */

    /*
     * Store reference, tell the interpreter about it.
     * We have to unCONST it explicitly to allow modification
     * of its reference counter
     */
    o->command = (Tcl_Obj*) optvalue;
    Tcl_IncrRefCount (o->command);
    break;

  default:
    goto unknown_option;
    break;
  }

  return TCL_OK;

 unknown_option:
  Tcl_AppendResult (interp, "unknown option '", (char*) NULL);
  Tcl_AppendResult (interp, optname, (char*) NULL);
  Tcl_AppendResult (interp, "', should be '-mode' or '-command'", (char*) NULL);
  return TCL_ERROR;
}

/*
 *------------------------------------------------------*
 *
 *	QueryOptions --
 *
 *	------------------------------------------------*
 *	Returns a value indicating wether the encoder or
 *	decoder set of vectors is to be used by immediate
 *	execution.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None
 *
 *	Result:
 *		1 - use encoder vectors.
 *		0 - use decoder vectors.
 *
 *------------------------------------------------------*
 */

static int
QueryOptions (options, clientData)
Trf_Options options;
ClientData clientData;
{
  TrfTransformOptionBlock* o = (TrfTransformOptionBlock*) options;

  return (o->mode == TRF_WRITE_MODE ? 1 : 0);
}

/*
 *------------------------------------------------------*
 *
 *	SeekQueryOptions --
 *
 *	------------------------------------------------*
 *	Modifies the natural seek policy according to the
 *	configuration of the transformation (queries the
 *	tcl level).
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		May modify 'seekInfo'.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
SeekQueryOptions (interp, options, seekInfo, clientData)
     Tcl_Interp*          interp;
     Trf_Options          options;
     Trf_SeekInformation* seekInfo;
     ClientData           clientData;
{
  TrfTransformOptionBlock* o = (TrfTransformOptionBlock*) options;
  ReflectControl           rc;

  START (SeekQueryOptions);

  rc.interp                         = interp;
  rc.naturalRatio.numBytesTransform = seekInfo->numBytesTransform;
  rc.naturalRatio.numBytesDown      = seekInfo->numBytesDown;
  rc.command                        = o->command;
  Tcl_IncrRefCount (rc.command);


  PRINT ("in  = (%d, %d)\n",
	 seekInfo->numBytesTransform, seekInfo->numBytesDown); FL;

  RefExecuteCallback (&rc, (Tcl_Interp*) interp,
		      (unsigned char*) "query/ratio",
		      NULL, 0, TRANSMIT_RATIO /* -> naturalRatio */, 1);

  seekInfo->numBytesTransform = rc.naturalRatio.numBytesTransform;
  seekInfo->numBytesDown      = rc.naturalRatio.numBytesDown;

  Tcl_DecrRefCount (rc.command);


  PRINT ("out = (%d, %d)\n",
	 seekInfo->numBytesTransform, seekInfo->numBytesDown); FL;

  DONE (SeekQueryOptions);
}
