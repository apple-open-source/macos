/*
 *Copyright (C) 1994-2000 The XFree86 Project, Inc. All Rights Reserved.
 *
 *Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 *"Software"), to deal in the Software without restriction, including
 *without limitation the rights to use, copy, modify, merge, publish,
 *distribute, sublicense, and/or sell copies of the Software, and to
 *permit persons to whom the Software is furnished to do so, subject to
 *the following conditions:
 *
 *The above copyright notice and this permission notice shall be
 *included in all copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *NONINFRINGEMENT. IN NO EVENT SHALL THE XFREE86 PROJECT BE LIABLE FOR
 *ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *Except as contained in this notice, the name of the XFree86 Project
 *shall not be used in advertising or otherwise to promote the sale, use
 *or other dealings in this Software without prior written authorization
 *from the XFree86 Project.
 *
 * Authors:	Harold L Hunt II
 */
/* $XFree86: xc/programs/Xserver/hw/xwin/wincutpaste.c,v 1.2 2001/09/07 08:41:54 alanh Exp $ */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#include <X11/XWDFile.h>

#ifdef HAS_MMAP
#include <sys/mman.h>
#ifndef MAP_FILE
#define MAP_FILE 0
#endif /* MAP_FILE */
#endif /* HAS_MMAP */

#include "X.h"
#include "Xos.h"
#include "miscstruct.h"
#include "keysym.h"
#include <X11/Xlib.h>

#undef MINSHORT
#undef MAXSHORT

#define NONAMELESSUNION
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif


#include <windows.h>
#include <windowsx.h>

#undef CreateWindow
#undef FreeResource

Display		*g_display = NULL;
Window		g_window = 0;

Bool
winInitializeClipboard ()
{
  g_display = XOpenDisplay (NULL);
  if (g_display == NULL)
    FatalError ("winInitializeClipboard () - XOpenDisplay () returned NULL\n");

  g_window = XCreateSimpleWindow (g_display,
				  RootWindow (g_display, 0),
				  1, 1,
				  500, 500,
				  0,
				  BlackPixel (g_display, 0),
				  BlackPixel (g_display, 0));
  if (g_window == 0)
    FatalError ("winInitializeClipboard () - XCreateSimpleWindow () returned "
		"0\n");


  
  
  /* Don't display our message window */
#if 0
  XMapWindow (g_display, g_window);
#endif
  
  return TRUE;
}




#if 0
#define NEED_EVENTS
#include <X.h>
#include <Xproto.h>
#include "selection.h"
#include "input.h"
#include <Xatom.h>

extern WindowPtr *WindowTable; /* Why isn't this in a header file? */
extern Selection *CurrentSelections;
extern int NumCurrentSelections;


static Bool inSetXCutText = FALSE;

void
winSetXCutText (char *str, int len)
{
    int i = 0;

    inSetXCutText = TRUE;
    ChangeWindowProperty (WindowTable[0], XA_CUT_BUFFER0, XA_STRING,
			  8, PropModeReplace, len,
			  (pointer)str, TRUE);
    
    while ((i < NumCurrentSelections) && 
	   CurrentSelections[i].selection != XA_PRIMARY)
	i++;

    if (i < NumCurrentSelections) {
	xEvent event;

	if (CurrentSelections[i].client) {
	    event.u.u.type = SelectionClear;
	    event.u.selectionClear.time = GetTimeInMillis();
	    event.u.selectionClear.window = CurrentSelections[i].window;
	    event.u.selectionClear.atom = CurrentSelections[i].selection;
	    (void) TryClientEvents (CurrentSelections[i].client,
				    &event,
				    1,
				    NoEventMask,
				    NoEventMask,
				    NullGrab);
	}

	CurrentSelections[i].window = None;
	CurrentSelections[i].pWin = NULL;
	CurrentSelections[i].client = NullClient;
    }

    inSetXCutText = FALSE;
}
#endif
