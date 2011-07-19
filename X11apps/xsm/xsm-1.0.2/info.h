/* $Xorg: info.h,v 1.4 2001/02/09 02:05:59 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xsm/info.h,v 3.4 2001/08/01 00:45:07 tsi Exp $ */

#ifndef _INFO_H_
#define _INFO_H_

#include "xsm.h"

extern void ShowHint(ClientRec *client);
extern void DisplayProps(ClientRec *client);
extern char * GetProgramName(char *fullname);
extern void UpdateClientList(void);
extern void ClientInfoStructureNotifyXtHandler(Widget w, XtPointer closure, 
					       XEvent *event, 
					       Boolean *continue_to_dispatch);
extern void ClientInfoXtProc(Widget w, XtPointer client_data, 
			     XtPointer callData);
extern void create_client_info_popup(void);

extern Widget clientInfoPopup;
extern Widget clientPropPopup;
extern Widget clientListWidget;
extern Widget manualRestartLabel;
extern Widget manualRestartCommands;

#endif
