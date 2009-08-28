/*
 * crypt.c --
 *
 *	Implements the 'crypt' and 'md5crypt' commands.
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
 * CVS: $Id: crypt.c,v 1.7 2000/11/18 22:42:31 aku Exp $
 */

#include "loadman.h"

static int
TrfCryptObjCmd _ANSI_ARGS_ ((ClientData notUsed, Tcl_Interp* interp,
			     int objc, struct Tcl_Obj* CONST * objv));
static int
TrfMd5CryptObjCmd _ANSI_ARGS_ ((ClientData notUsed, Tcl_Interp* interp,
				int objc, struct Tcl_Obj* CONST * objv));

/*
 *----------------------------------------------------------------------
 *
 * TrfCryptObjCmd --
 *
 *	This procedure is invoked to process the "crypt" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TrfCryptObjCmd (notUsed, interp, objc, objv)
     ClientData  notUsed;		/* Not used. */
     Tcl_Interp* interp;		/* Current interpreter. */
     int                     objc;	/* Number of arguments. */
     struct Tcl_Obj* CONST * objv;	/* Argument strings. */
{
  /*
   * crypt <passwd> <salt>
   */

#ifdef __WIN32__
  Tcl_SetObjResult (interp, Tcl_NewStringObj ("crypt is not available under Windows", -1));
  return TCL_ERROR;
#else
  const char* passwd;
  const char* salt;
  Tcl_Obj*    res;

  if (objc != 3) {
    Tcl_AppendResult (interp,
		      "wrong # args: should be \"crypt passwd salt\"",
		      (char*) NULL);
    return TCL_ERROR;
  }

  passwd = Tcl_GetStringFromObj (objv [1], NULL);
  salt   = Tcl_GetStringFromObj (objv [2], NULL);

  /* THREADING: Serialize access to result string of 'crypt'.
   */

  TrfLock;
  res = Tcl_NewStringObj ((char*) crypt (passwd, salt), -1);
  TrfUnlock;

  Tcl_SetObjResult (interp, res);
  return TCL_OK;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TrfMd5CryptObjCmd --
 *
 *	This procedure is invoked to process the "md5crypt" Tcl command.
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
TrfMd5CryptObjCmd (notUsed, interp, objc, objv)
     ClientData  notUsed;		/* Not used. */
     Tcl_Interp* interp;		/* Current interpreter. */
     int                     objc;	/* Number of arguments. */
     struct Tcl_Obj* CONST * objv;	/* Argument strings. */
{
  /*
   * md5crypt <passwd> <salt>
   */

  const char* passwd;
  const char* salt;
  char        salt_b [6];
  Tcl_Obj*    res;

  if (TrfLoadMD5 (interp) != TCL_OK) {
    return TCL_ERROR;
  }

  if (objc != 3) {
    Tcl_AppendResult (interp,
		      "wrong # args: should be \"md5crypt passwd salt\"",
		      (char*) NULL);
    return TCL_ERROR;
  }

  passwd = Tcl_GetStringFromObj (objv [1], NULL);
  salt   = Tcl_GetStringFromObj (objv [2], NULL);

  /*
   * Manipulate salt, add magic md5 prefix '$1$'.
   * The 'crypt +3' later on skips the first three characters of the result,
   * which again contain the magic marker.
   */

  salt_b [0] = '$';
  salt_b [1] = '1';
  salt_b [2] = '$';
  salt_b [3] = salt [0];
  salt_b [4] = salt [1];
  salt_b [5] = '\0';

  /* THREADING: Serialize access to result string of 'md5f.crypt'.
   */

  TrfLock;
  res = Tcl_NewStringObj ((char*) md5f.crypt (passwd, salt_b) + 3, -1);
  TrfUnlock;

  Tcl_SetObjResult (interp, res);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	TrfInit_Crypt --
 *
 *	------------------------------------------------*
 *	Register the 'crypt' and 'md5crypt' commands.
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
TrfInit_Crypt (interp)
Tcl_Interp* interp;
{
  Tcl_CreateObjCommand (interp, "crypt", TrfCryptObjCmd,
			(ClientData) NULL,
			(Tcl_CmdDeleteProc *) NULL);

  Tcl_CreateObjCommand (interp, "md5crypt", TrfMd5CryptObjCmd,
			(ClientData) NULL,
			(Tcl_CmdDeleteProc *) NULL);

  return TCL_OK;
}

