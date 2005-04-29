/*
 * xxx_opt.c --
 *
 *	Implements the C level procedures handling option processing
 *	for XXX transformations.
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
 * CVS: $Id: opt_template.c,v 1.2 1997/02/11 22:11:00 aku Exp $
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

#if (TCL_MAJOR_VERSION >= 8)
static int         SetOption     _ANSI_ARGS_ ((Trf_Options    options,
					       Tcl_Interp*    interp,
					       CONST char*    optname,
					       CONST Tcl_Obj* optvalue,
					       ClientData     clientData));
#else
static int         SetOption     _ANSI_ARGS_ ((Trf_Options options,
					       Tcl_Interp* interp,
					       CONST char* optname,
					       CONST char* optvalue,
					       ClientData  clientData));
#endif

static int         QueryOptions  _ANSI_ARGS_ ((Trf_Options options,
					       ClientData  clientData));


/*
 *------------------------------------------------------*
 *
 *	Trf_XXXOptions --
 *
 *	------------------------------------------------*
 *	Accessor to the set of vectors realizing option
 *	processing for XXX procedures.
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
Trf_XXXOptions ()
{
  static Trf_OptionVectors optVec =
    {
      CreateOptions,
      DeleteOptions,
      CheckOptions,
#if (TCL_MAJOR_VERSION >= 8)
      NULL,      /* no string procedure */
      SetOption,
#else
      SetOption,
      NULL,      /* no object procedure */
#endif
      QueryOptions
    };

  return &optVec;
}

/*
 *------------------------------------------------------*
 *
 *	CreateOptions --
 *
 *	------------------------------------------------*
 *	Create option structure for XXX transformations.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Allocates memory and initializes it as
 *		option structure for XXX
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
  return (Trf_Options) NULL;
}

/*
 *------------------------------------------------------*
 *
 *	DeleteOptions --
 *
 *	------------------------------------------------*
 *	Delete option structure of a XXX transformations
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
#if (TCL_MAJOR_VERSION >= 8)
CONST Tcl_Obj* optvalue;
#else
CONST char*    optvalue;
#endif
ClientData  clientData;
{
  /* Possible options:
   *
   */

  CONST char*             value;
  int len = strlen (optname + 1);

  /* move into case-code if possible */
#if (TCL_MAJOR_VERSION >= 8)
    value = Tcl_GetStringFromObj ((Tcl_Obj*) optvalue, NULL);
#else
    value = optvalue;
#endif

  switch (optname [1]) {
  default:
    goto unknown_option;
    break;
  }

  return TCL_OK;

 unknown_option:
  ADD_RES (interp, "unknown option '");
  ADD_RES (interp, optname);
  ADD_RES (interp, "'");
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
  return 0;
}

