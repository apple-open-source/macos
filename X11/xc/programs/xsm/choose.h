/* $Xorg: choose.h,v 1.4 2001/02/09 02:05:59 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xsm/choose.h,v 1.6 2001/12/14 20:02:24 dawes Exp $ */

#ifndef _CHOOSE_H_
#define _CHOOSE_H_

#include <X11/Intrinsic.h>

extern int GetSessionNames(int *count_ret, String **short_names_ret, 
			   String **long_names_ret, Bool **locked_ret);
extern void FreeSessionNames(int count, String *namesShort, String *namesLong,
			     Bool *lockFlags);
extern void ChooseWindowStructureNotifyXtHandler(Widget w, XtPointer closure, 
						 XEvent *event, 
						 Boolean *continue_to_dispatch);
extern void ChooseSession(void);
extern void create_choose_session_popup(void);

#endif
