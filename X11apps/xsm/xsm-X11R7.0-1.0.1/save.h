/* $Xorg: save.h,v 1.4 2001/02/09 02:06:01 xorgcvs Exp $ */
/******************************************************************************

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
******************************************************************************/
/* $XFree86: xc/programs/xsm/save.h,v 1.5 2001/08/01 00:45:07 tsi Exp $ */

#ifndef _SAVE_H_
#define _SAVE_H_

#include <X11/Intrinsic.h>
#include "list.h"

extern void DoSave(int saveType, int interactStyle, Bool fast);
extern void LetClientInteract(List *cl);
extern void StartPhase2(void);
extern void FinishUpSave(void);
extern void SetSaveSensitivity(Bool on);
extern void SavePopupStructureNotifyXtHandler(Widget w, XtPointer closure, 
					      XEvent *event, 
					      Boolean *continue_to_dispatch);
extern void create_save_popup(void);
extern void PopupSaveDialog(void);
extern void CheckPointXtProc(Widget w, XtPointer client_data, 
			     XtPointer callData);
extern void ShutdownSaveXtProc(Widget w, XtPointer client_data, 
			       XtPointer callData);
extern void PopupBadSave(void);
extern void ShutdownDontSaveXtProc(Widget w, XtPointer client_data, 
				   XtPointer callData);

#endif
