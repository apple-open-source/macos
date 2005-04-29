/*
 * zip_opt.c --
 *
 *	Implements the C level procedures handling option processing
 *	for ZIP transformations.
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
 * CVS: $Id: zip_opt.c,v 1.11 2000/11/18 22:42:32 aku Exp $
 */

#include "transformInt.h"

/*
 * forward declarations of all internally used procedures.
 */

static Trf_Options CreateOptions _ANSI_ARGS_ ((ClientData clientData));

static void        DeleteOptions _ANSI_ARGS_ ((Trf_Options options,
					       ClientData  clientData));

static int         CheckOptions  _ANSI_ARGS_ ((Trf_Options            options,
					       Tcl_Interp*            interp,
					       CONST Trf_BaseOptions* baseOptions,
					       ClientData             clientData));

static int         SetOption     _ANSI_ARGS_ ((Trf_Options    options,
					       Tcl_Interp*    interp,
					       CONST char*    optname,
					       CONST Tcl_Obj* optvalue,
					       ClientData     clientData));

static int         QueryOptions  _ANSI_ARGS_ ((Trf_Options options,
					       ClientData  clientData));


/*
 *------------------------------------------------------*
 *
 *	TrfZIPOptions --
 *
 *	------------------------------------------------*
 *	Accessor to the set of vectors realizing option
 *	processing for ZIP procedures.
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
TrfZIPOptions ()
{
  static Trf_OptionVectors optVec = /* THREADING: constant, read-only => safe */
    {
      CreateOptions,
      DeleteOptions,
      CheckOptions,
      NULL,      /* no string procedure for 'SetOption' */
      SetOption,
      QueryOptions,
      NULL       /* unseekable, unchanged by options */
    };

  return &optVec;
}

/*
 *------------------------------------------------------*
 *
 *	CreateOptions --
 *
 *	------------------------------------------------*
 *	Create option structure for ZIP transformations.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Allocates memory and initializes it as
 *		option structure for ZIP
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
  TrfZipOptionBlock* o;

  o = (TrfZipOptionBlock*) Tcl_Alloc (sizeof (TrfZipOptionBlock));

  o->mode   = TRF_UNKNOWN_MODE;
  o->level  = TRF_DEFAULT_LEVEL;
  o->nowrap = 0;

  return (Trf_Options) o;
}

/*
 *------------------------------------------------------*
 *
 *	DeleteOptions --
 *
 *	------------------------------------------------*
 *	Delete option structure of a ZIP transformations
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
  Tcl_Free ((VOID*) options);
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
  TrfZipOptionBlock* o = (TrfZipOptionBlock*) options;

  /*
   * 'zip' is used, therefore load the required library.
   * And bail out if it is not available.
   */

  if (TCL_OK != TrfLoadZlib (interp)) {
    return TCL_ERROR;
  }

  /*
   * Now perform the real option check.
   */

  if (baseOptions->attach == (Tcl_Channel) NULL) /* IMMEDIATE? */ {
    if (o->mode == TRF_UNKNOWN_MODE) {
      Tcl_AppendResult (interp, "-mode option not set", (char*) NULL);
      return TCL_ERROR;
    }
  } else /* ATTACH */ {
    if (o->mode == TRF_UNKNOWN_MODE) {
      o->mode = TRF_COMPRESS;
    }
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
Trf_Options options;
Tcl_Interp* interp;
CONST char* optname;
CONST Tcl_Obj* optvalue;
ClientData  clientData;
{
  /* Possible options:
   *
   * -level  <number>
   * -level  default
   * -mode   compress|decompress
   * -nowrap <boolean>
   * -nowrap default
   */

  TrfZipOptionBlock* o = (TrfZipOptionBlock*) options;
  int              len = strlen (optname + 1);
  CONST char*      value;

  switch (optname [1]) {
  case 'l':
    if (0 != strncmp (optname, "-level", len))
      goto unknown_option;

    value = Tcl_GetStringFromObj ((Tcl_Obj*) optvalue, NULL);
    
    len = strlen (value);
    if (0 == strncmp (value, "default", len)) {
      o->level = TRF_DEFAULT_LEVEL;
    } else {
      int res, val;

      int v;
      res = Tcl_GetIntFromObj (interp, (Tcl_Obj*) optvalue, &v);
      val = v;

      if (res != TCL_OK) {
	return res;
      }

      if ((val < TRF_MIN_LEVEL) || (val > TRF_MAX_LEVEL)) {
	Tcl_AppendResult (interp, "level out of range ", (char*) NULL);
	Tcl_AppendResult (interp, TRF_MIN_LEVEL_STR, (char*) NULL);
	Tcl_AppendResult (interp, "..", (char*) NULL);
	Tcl_AppendResult (interp, TRF_MAX_LEVEL_STR, (char*) NULL);
	return TCL_ERROR;
      }

      o->level = val;
    }
    break;

  case 'm':
    if (0 != strncmp (optname, "-mode", len))
      goto unknown_option;

    value = Tcl_GetStringFromObj ((Tcl_Obj*) optvalue, NULL);
    len   = strlen (value);

    switch (value [0]) {
    case 'c':
      if (0 != strncmp (value, "compress", len))
	goto unknown_mode;
      
      o->mode = TRF_COMPRESS;
      break;

    case 'd':
      if (0 != strncmp (value, "decompress", len))
	goto unknown_mode;
      
      o->mode = TRF_DECOMPRESS;
      break;

    default:
    unknown_mode:
      Tcl_AppendResult (interp, "unknown mode '", (char*) NULL);
      Tcl_AppendResult (interp, value, (char*) NULL);
      Tcl_AppendResult (interp, "', should be 'compress' or 'decompress'", (char*) NULL);
      return TCL_ERROR;
      break;
    } /* switch optvalue */
    break;

  case 'n':
    if (0 != strncmp (optname, "-nowrap", len))
      goto unknown_option;

    value = Tcl_GetStringFromObj ((Tcl_Obj*) optvalue, NULL);
    
    len = strlen (value);
    if (0 == strncmp (value, "default", len)) {
      o->nowrap = 0;
    } else {
      int res, val;

      res = Tcl_GetBooleanFromObj (interp, (Tcl_Obj*) optvalue, &val);

      if (res != TCL_OK) {
	return res;
      }

      o->nowrap = val;
    }
    break;

  default:
    goto unknown_option;
    break;
  }

  return TCL_OK;

 unknown_option:
  Tcl_AppendResult (interp, "unknown option '", (char*) NULL);
  Tcl_AppendResult (interp, optname, (char*) NULL);
  Tcl_AppendResult (interp, "', should be '-level', '-mode' or '-nowrap'", (char*) NULL);
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
ClientData  clientData;
{
  TrfZipOptionBlock* o = (TrfZipOptionBlock*) options;

  return (o->mode == TRF_COMPRESS ? 1 : 0);
}

