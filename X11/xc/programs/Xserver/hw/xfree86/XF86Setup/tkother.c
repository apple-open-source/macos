/* $XConsortium: tkother.c /main/2 1996/10/19 19:06:55 kaleb $ */





/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tkother.c,v 3.1 1996/12/27 06:54:26 dawes Exp $ */
/*
 * Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Joseph Moss not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Joseph Moss makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * JOSEPH MOSS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL JOSEPH MOSS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


/*

  This file contains routines to add a few misc commands to Tcl
  which require the Tk toolkit

 */

#include <stdlib.h>
#include <stdio.h>
#include <X11/Xos.h>
#include <X11/Xproto.h>
#include <X11/Xfuncs.h>
#include <tcl.h>
#include <tk.h>
#include <sys/types.h>
#include <sys/stat.h>

static int	TK_XF86FindWindow(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

/*
   Adds all the new commands to the Tcl interpreter
*/

int
XF86TkOther_Init(interp)
    Tcl_Interp	*interp;
{
	Tcl_CreateCommand(interp, "findfocuswindow",
		TK_XF86FindWindow, (ClientData) NULL,
		(void (*)()) NULL);

	return TCL_OK;
}

/*
  Search (starting from the given win) along the specified axis for a
  window which accepts focus, move the specified number pixels at a time
  when searching.  For each increment it looks first from the center
  of the starting window, then midway between the center and one side,
  then midway between the center and the other side, then from each
  side.  For example:

         A +---------------+
           |               |
           +---------------+ B
           4   2   1   3   5 

  if searching down from the window (which stretches from point A to
  point B), the search will start 1*increment pixels down from one,
  it then looks 1*increment pixels down from two, etc,, then 2*increment
  pixels down from one, 2*increment down from two, and so on until
  a widow is found which will accept the keyboard focus.

  If no suitable window is found the original window is returned.
*/

static char fwusage[] = "Usage: findfocuswindow <window> x|y <increment>";

int
TK_XF86FindWindow(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window	topwin, oldwin, tkwin;
	int		ulx, uly, wid, ht, limit, increment;
	int		x, y, idx, pos[5];

	if (argc != 4) {
		sprintf(interp->result, fwusage);
		return TCL_ERROR;
	}

	if ((topwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	if ((oldwin = Tk_NameToWindow(interp, argv[1], topwin)) == NULL)
		return TCL_ERROR;

	if (Tcl_GetInt(interp, argv[3], &increment) != TCL_OK)
		return TCL_ERROR;

	Tk_GetRootCoords(oldwin, &ulx, &uly);
	wid = Tk_Width(oldwin);
	ht  = Tk_Height(oldwin);

	if (!strcmp(argv[2], "x")) {
		limit = WidthOfScreen(Tk_Screen(oldwin));
		if (increment < 0)
		    x = ulx;
		else
		    x = ulx+wid;
		pos[0] = (2*uly+ht)/2;
		pos[1] = (uly+pos[0])/2;
		pos[2] = (uly+ht+pos[0])/2;
		pos[3] = uly;
		pos[4] = uly+ht;
		while (0 < x && x < limit) {
		    x += increment;
		    for (idx=0; idx < 5; idx++) {
			tkwin = Tk_CoordsToWindow(x, pos[idx], topwin);
			if ( tkwin ) {
			    if (Tcl_VarEval(interp, "tkFocusOK ",
				    Tk_PathName(tkwin), NULL) != TCL_OK)
				return TCL_ERROR;
			    if (!strcmp(interp->result, "1")) {
				Tcl_SetResult(interp, Tk_PathName(tkwin),
				    TCL_STATIC);
				return TCL_OK;
			    }
			}
		    }
		}
	} else if (!strcmp(argv[2], "y")) {
		limit = HeightOfScreen(Tk_Screen(oldwin));
		if (increment < 0)
		    y = uly;
		else
		    y = uly+ht;
		pos[0] = (2*ulx+wid)/2;
		pos[1] = (ulx+pos[0])/2;
		pos[2] = (ulx+wid+pos[0])/2;
		pos[3] = ulx;
		pos[4] = ulx+wid;
		while (0 < y && y < limit) {
		    y += increment;
		    for (idx=0; idx < 5; idx++) {
			tkwin = Tk_CoordsToWindow(pos[idx], y, topwin);
			if ( tkwin ) {
			    if (Tcl_VarEval(interp, "tkFocusOK ",
				    Tk_PathName(tkwin), NULL) != TCL_OK)
				return TCL_ERROR;
			    if (!strcmp(interp->result, "1")) {
				Tcl_SetResult(interp, Tk_PathName(tkwin),
				    TCL_STATIC);
				return TCL_OK;
			    }
			}
		    }
		}
	} else {
		sprintf(interp->result, fwusage);
		return TCL_ERROR;
	}
	Tcl_SetResult(interp, Tk_PathName(oldwin), TCL_STATIC);
	return TCL_OK;
}

