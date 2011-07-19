/*
 * dig_opt.c --
 *
 *	Implements the C level procedures handling option processing
 *	for message digest generators.
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
 * CVS: $Id: dig_opt.c,v 1.11 2009/05/07 04:57:27 andreas_kupries Exp $
 */

#include "transformInt.h"

/*
 * forward declarations of all internally used procedures.
 */

static Trf_Options CreateOptions _ANSI_ARGS_ ((ClientData clientData));
static void        DeleteOptions _ANSI_ARGS_ ((Trf_Options options,
					       ClientData clientData));
static int         CheckOptions  _ANSI_ARGS_ ((Trf_Options options,
					       Tcl_Interp* interp,
					       CONST Trf_BaseOptions* baseOptions,
					       ClientData clientData));
static int         SetOption     _ANSI_ARGS_ ((Trf_Options options,
					       Tcl_Interp* interp,
					       CONST char* optname,
					       CONST Tcl_Obj* optvalue,
					       ClientData clientData));

static int         QueryOptions  _ANSI_ARGS_ ((Trf_Options options,
					       ClientData clientData));

static int         TargetType _ANSI_ARGS_ ((Tcl_Interp* interp,
					    CONST char* typeString,
					    int* isChannel));

static int         DigestMode _ANSI_ARGS_ ((Tcl_Interp* interp,
					    CONST char* modeString,
					    int* mode));

/*
 *------------------------------------------------------*
 *
 *	TrfMDOptions --
 *
 *	------------------------------------------------*
 *	Accessor to the set of vectors realizing option
 *	processing for message digest generators.
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
TrfMDOptions ()
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
 *	Create option structure for message digest generators.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Allocates memory and initializes it as
 *		option structure for message digest generators.
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
  TrfMDOptionBlock* o;

  o			= (TrfMDOptionBlock*) ckalloc (sizeof (TrfMDOptionBlock));
  o->behaviour		= TRF_IMMEDIATE; /* irrelevant until set by 'CheckOptions' */
  o->mode		= TRF_UNKNOWN_MODE;
  o->readDestination	= (char*) NULL;
  o->writeDestination	= (char*) NULL;
  o->rdIsChannel        = 0;
  o->wdIsChannel        = 1;
  o->matchFlag		= (char*) NULL;
  o->vInterp		= (Tcl_Interp*) NULL;
  o->rdChannel		= (Tcl_Channel) NULL;
  o->wdChannel		= (Tcl_Channel) NULL;

  return (Trf_Options) o;
}

/*
 *------------------------------------------------------*
 *
 *	DeleteOptions --
 *
 *	------------------------------------------------*
 *	Delete option structure of a message digest generators.
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
  TrfMDOptionBlock* o = (TrfMDOptionBlock*) options;

  if (o->readDestination) {
    ckfree ((char*) o->readDestination);
  }

  if (o->writeDestination) {
    ckfree ((char*) o->writeDestination);
  }

  if (o->matchFlag) {
    ckfree ((char*) o->matchFlag);
  }

  ckfree ((char*) o);
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
  TrfMDOptionBlock*                   o = (TrfMDOptionBlock*) options;
  Trf_MessageDigestDescription* md_desc = (Trf_MessageDigestDescription*) clientData;

  /*
   * Call digest dependent check of environment first.
   */

  START (dig_opt:CheckOptions);

  if (md_desc->checkProc != NULL) {
    if (TCL_OK != (*md_desc->checkProc) (interp)) {
      DONE (dig_opt:CheckOptions);
      return TCL_ERROR;
    }
  }

  /* TRF_IMMEDIATE: no options allowed
   * TRF_ATTACH:    -mode required
   *                TRF_ABSORB_HASH: -matchflag required (only if channel is read)
   *                TRF_WRITE_HASH:  -write/read-destination required according to
   *				      access mode of attached channel. If a channel
   *                                  is used as target, then it has to be writable.
   *                TRF_TRANSPARENT: see TRF_WRITE_HASH.
   */

  if (baseOptions->attach == (Tcl_Channel) NULL) {
    if ((o->mode             != TRF_UNKNOWN_MODE) ||
	(o->matchFlag        != (char*) NULL)     ||
	(o->readDestination  != (char*) NULL)     ||
	(o->writeDestination != (char*) NULL)) {
      /* IMMEDIATE MODE */
      Tcl_AppendResult (interp, "immediate: no options allowed", (char*) NULL);
      DONE (dig_opt:CheckOptions);
      return TCL_ERROR;
    }
  } else {
    /* ATTACH MODE / FILTER */
    if (o->mode == TRF_UNKNOWN_MODE) {
      Tcl_AppendResult (interp, "attach: -mode not defined", (char*) NULL);
      DONE (dig_opt:CheckOptions);
      return TCL_ERROR;

    } else if (o->mode == TRF_ABSORB_HASH) {
      if ((baseOptions->attach_mode & TCL_READABLE) &&
	  (o->matchFlag == (char*) NULL)) {
	Tcl_AppendResult (interp, "attach: -matchflag not defined", (char*) NULL);
	DONE (dig_opt:CheckOptions);
	return TCL_ERROR;
      }

    } else if ((o->mode == TRF_WRITE_HASH) || (o->mode == TRF_TRANSPARENT)) {
      if (o->matchFlag != (char*) NULL) {
	Tcl_AppendResult (interp, "attach: -matchflag not allowed", (char*) NULL);
	DONE (dig_opt:CheckOptions);
	return TCL_ERROR;
      }

      if (baseOptions->attach_mode & TCL_READABLE) {
	if (o->readDestination == (char*) NULL) {
	  Tcl_AppendResult (interp, "attach, external: -read-destination missing",
			    (char*) NULL);
	  DONE (dig_opt:CheckOptions);
	  return TCL_ERROR;
	} else if (o->rdIsChannel) {
	  int mode;
	  o->rdChannel = Tcl_GetChannel (interp, (char*) o->readDestination, &mode);

	  if (o->rdChannel == (Tcl_Channel) NULL) {
	    DONE (dig_opt:CheckOptions);
	    return TCL_ERROR;
	  } else  if (! (mode & TCL_WRITABLE)) {
	    Tcl_AppendResult (interp,
			      "read destination channel '", o->readDestination,
			      "' not opened for writing", (char*) NULL);
	    DONE (dig_opt:CheckOptions);
	    return TCL_ERROR;
	  }
	}
      }

      if (baseOptions->attach_mode & TCL_WRITABLE) {
	if (o->writeDestination == (char*) NULL) {
	  Tcl_AppendResult (interp, "attach, external: -write-destination missing",
			    (char*) NULL);
	  DONE (dig_opt:CheckOptions);
	  return TCL_ERROR;
	} else if (o->wdIsChannel) {
	  int mode;

	  o->wdChannel = Tcl_GetChannel (interp, (char*) o->writeDestination, &mode);

	  if (o->wdChannel == (Tcl_Channel) NULL) {
	    DONE (dig_opt:CheckOptions);
	    return TCL_ERROR;
	  } else  if (! (mode & TCL_WRITABLE)) {
	    Tcl_AppendResult (interp,
			      "write destination channel '", o->writeDestination,
			      "' not opened for writing", (char*) NULL);
	    DONE (dig_opt:CheckOptions);
	    return TCL_ERROR;
	  }
	}
      }
    } else {
      Tcl_Panic ("unknown mode given to dig_opt.c::CheckOptions");
    }
  }

  o->behaviour = (baseOptions->attach == (Tcl_Channel) NULL ?
		  TRF_IMMEDIATE :
		  TRF_ATTACH);

  PRINT ("Ok\n"); FL;
  DONE (dig_opt:CheckOptions);
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
   *	-mode			absorb|write|transparent
   *	-matchflag		<varname>
   *	-write-destination	<channel> | <variable>
   *	-read-destination	<channel> | <variable>
   */

  TrfMDOptionBlock* o = (TrfMDOptionBlock*) options;
  CONST char*       value;

  int len = strlen (optname);

  value = Tcl_GetStringFromObj ((Tcl_Obj*) optvalue, NULL);

  switch (optname [1]) {
  case 'm':
    if (len < 3)
      goto unknown_option;

    if (0 == strncmp (optname, "-mode", len)) {
      return DigestMode (interp, value, &o->mode);

    } else if (0 == strncmp (optname, "-matchflag", len)) {
      if (o->matchFlag) {
	ckfree (o->matchFlag);
      }

      o->vInterp   = interp;
      o->matchFlag = strcpy (ckalloc (1 + strlen (value)), value);

    } else
      goto unknown_option;
    break;

  case 'w':
    if (len < 8)
      goto unknown_option;

    if (0 == strncmp (optname, "-write-destination", len)) {
      if (o->writeDestination) {
	ckfree (o->writeDestination);
      }

      o->vInterp          = interp;
      o->writeDestination = strcpy (ckalloc (1+strlen (value)), value);

    } else if (0 == strncmp (optname, "-write-type", len)) {
      return TargetType (interp, value, &o->wdIsChannel);
    } else
      goto unknown_option;
    break;

  case 'r':
    if (len < 7)
      goto unknown_option;

    if (0 == strncmp (optname, "-read-destination", len)) {
      if (o->readDestination) {
	ckfree (o->readDestination);
      }

      o->vInterp         = interp;
      o->readDestination = strcpy (ckalloc (1+strlen (value)), value);

    } else if (0 == strncmp (optname, "-read-type", len)) {
      return TargetType (interp, value, &o->rdIsChannel);
    } else
      goto unknown_option;
    break;

  default:
    goto unknown_option;
    break;
  }

  return TCL_OK;

 unknown_option:
  Tcl_AppendResult (interp, "unknown option '", optname, "', should be '-mode', '-matchflag', '-write-destination', '-write-type', '-read-destination' or '-read-type'", (char*) NULL);
   
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
  /* Always use encoder for immediate execution */
  return 1;
}

/*
 *------------------------------------------------------*
 *
 *	TargetType --
 *
 *	------------------------------------------------*
 *	Determines from a string what destination was
 *	given to the message digest.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		May leave an error message in the
 *		interpreter result area.
 *
 *	Result:
 *		A standard Tcl error code, in case of
 *		success 'isChannel' is set too.
 *
 *------------------------------------------------------*
 */

static int
TargetType (interp, typeString, isChannel)
Tcl_Interp* interp;
CONST char* typeString;
int*        isChannel;
{
  int len = strlen (typeString);

  switch (typeString [0]) {
  case 'v':
    if (0 == strncmp ("variable", typeString, len)) {
      *isChannel = 0;
    } else
      goto unknown_type;
    break;

  case 'c':
    if (0 == strncmp ("channel", typeString, len)) {
      *isChannel = 1;
    } else
      goto unknown_type;
    break;

  default:
  unknown_type:
    Tcl_AppendResult (interp, "unknown target-type '",
		      typeString, "'", (char*) NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	DigestMode --
 *
 *	------------------------------------------------*
 *	Determines the operation mode of the digest.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		May leave an error message in the
 *		interpreter result area.
 *
 *	Result:
 *		A standard Tcl error code, in case of
 *		success 'mode' is set too.
 *
 *------------------------------------------------------*
 */

static int
DigestMode (interp, modeString, mode)
Tcl_Interp* interp;
CONST char* modeString;
int*        mode;
{
  int len = strlen (modeString);

  switch (modeString [0]) {
  case 'a':
    if (0 == strncmp (modeString, "absorb", len)) {
      *mode = TRF_ABSORB_HASH;
    } else
      goto unknown_mode;
    break;

  case 'w':
    if (0 == strncmp (modeString, "write", len)) {
      *mode = TRF_WRITE_HASH;
    } else
      goto unknown_mode;
    break;

  case 't':
    if (0 == strncmp (modeString, "transparent", len)) {
      *mode = TRF_TRANSPARENT;
    } else
      goto unknown_mode;
    break;

  default:
  unknown_mode:
    Tcl_AppendResult (interp, "unknown mode '", modeString, "', should be 'absorb', 'write' or 'transparent'", (char*) NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}
