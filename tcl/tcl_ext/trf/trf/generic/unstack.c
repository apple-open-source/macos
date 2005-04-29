/*
 * unstack.c --
 *
 *	Implements the 'unstack' command to remove a conversion.
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
 * CVS: $Id: unstack.c,v 1.9 2000/08/09 19:13:18 aku Exp $
 */

#include	"transformInt.h"

static int
TrfUnstackObjCmd _ANSI_ARGS_ ((ClientData notUsed, Tcl_Interp* interp,
			       int objc, struct Tcl_Obj* CONST * objv));

/*
 *----------------------------------------------------------------------
 *
 * TrfUnstackCmd --
 *
 *	This procedure is invoked to process the "unstack" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Unstacks the channel, thereby restoring its parent.
 *
 *----------------------------------------------------------------------
 */

static int
TrfUnstackObjCmd (notUsed, interp, objc, objv)
     ClientData  notUsed;		/* Not used. */
     Tcl_Interp* interp;		/* Current interpreter. */
     int                     objc;	/* Number of arguments. */
     struct Tcl_Obj* CONST * objv;	/* Argument strings. */
{
  /*
   * unstack <channel>
   */

  Tcl_Channel chan;
  int         mode;

#ifdef USE_TCL_STUBS
  if (Tcl_UnstackChannel == NULL) {
    const char* cmd = Tcl_GetStringFromObj (objv [0], NULL);

    Tcl_AppendResult (interp, cmd, " is not available as the required ",
		      "patch to the core was not applied", (char*) NULL);
    return TCL_ERROR;
  }
#endif

  if ((objc < 2) || (objc > 2)) {
    Tcl_AppendResult (interp,
		      "wrong # args: should be \"unstack channel\"",
		      (char*) NULL);
    return TCL_ERROR;
  }

  chan = Tcl_GetChannel (interp, Tcl_GetStringFromObj (objv [1], NULL), &mode);

  if (chan == (Tcl_Channel) NULL) {
    return TCL_ERROR;
  }

  Tcl_UnstackChannel (interp, chan);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	TrfInit_Unstack --
 *
 *	------------------------------------------------*
 *	Register the 'unstack' command.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of 'Tcl_CreateObjCommand'.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

int
TrfInit_Unstack (interp)
Tcl_Interp* interp;
{
  Tcl_CreateObjCommand (interp, "unstack", TrfUnstackObjCmd,
			(ClientData) NULL,
			(Tcl_CmdDeleteProc *) NULL);

  return TCL_OK;
}

