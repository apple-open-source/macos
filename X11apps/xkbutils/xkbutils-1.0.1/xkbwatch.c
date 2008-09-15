/* $Xorg: xkbwatch.c,v 1.4 2000/08/17 19:54:51 cpqbld Exp $ */
/************************************************************
 Copyright (c) 1995 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be 
 used in advertising or publicity pertaining to distribution 
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability 
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.
 
 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/
/* $XFree86: xc/programs/xkbutils/xkbwatch.c,v 3.4 2001/01/17 23:46:14 dawes Exp $ */

#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/Box.h>

#define	OPAQUE_DEFINED
#define	BOOLEAN_DEFINED
#define	DEBUG_VAR_NOT_LOCAL
#define	DEBUG_VAR debugFlags
#include "utils.h"
#include "LED.h"

/***====================================================================***/

	Display *	inDpy,*outDpy;
	int		evBase,errBase;
	Bool		synch;

/***====================================================================***/


static XrmOptionDescRec options[] = {
{"-off",	"*on.on",		XrmoptionNoArg,		"FALSE"},
{"-on",		"*on.on",		XrmoptionNoArg,		"TRUE"}
};

/***====================================================================***/

int
main(int argc, char *argv[])
{
Widget		toplevel;
XtAppContext	app_con;
Widget		panel;
Widget		base[XkbNumModifiers];
Widget		latched[XkbNumModifiers];
Widget		locked[XkbNumModifiers];
Widget		effective[XkbNumModifiers];
Widget		compat[XkbNumModifiers];
Widget		baseBox,latchBox,lockBox,effBox,compatBox;
register int	i;
unsigned	bit;
XkbEvent	ev;
XkbStateRec	state;
static Arg	hArgs[]= {{ XtNorientation, (XtArgVal)XtorientHorizontal }};
static Arg	vArgs[]= {{ XtNorientation, (XtArgVal)XtorientVertical }};
static Arg	onArgs[]=  {{ XtNon, (XtArgVal)True }};
static Arg	offArgs[]=  {{ XtNon, (XtArgVal)False }};
static char *	fallback_resources[] = {
    "*Box*background: grey50",
    "*Box*borderWidth: 0",
    "*Box*vSpace: 1",
    NULL
};

    uSetEntryFile(NullString);
    uSetDebugFile(NullString);
    uSetErrorFile(NullString);
    toplevel = XtOpenApplication(&app_con, "XkbWatch",
				 options, XtNumber(options), &argc, argv, 
				 fallback_resources,
				 sessionShellWidgetClass, NULL, ZERO);
    if (toplevel==NULL) {
	uFatalError("Couldn't create application top level\n");
	exit(1);
    }
    inDpy= outDpy= XtDisplay(toplevel);
    if (inDpy) {
	int i1,mn,mj;
	mj= XkbMajorVersion;
	mn= XkbMinorVersion;
	if (!XkbQueryExtension(inDpy,&i1,&evBase,&errBase,&mj,&mn)) {
	    uFatalError("Server doesn't support a compatible XKB\n");
	    exit(1);
	}
    }
    panel= XtCreateManagedWidget("xkbwatch",boxWidgetClass,toplevel,vArgs,1);
    if (panel==NULL) {
	uFatalError("Couldn't create top level box\n");
	exit(1);
    }
    baseBox= XtCreateManagedWidget("base",boxWidgetClass,panel,hArgs,1);
    if (baseBox==NULL)
	uFatalError("Couldn't create base modifiers box\n");
    latchBox= XtCreateManagedWidget("latched",boxWidgetClass,panel,hArgs,1);
    if (latchBox==NULL)
	uFatalError("Couldn't create latched modifiers box\n");
    lockBox= XtCreateManagedWidget("locked",boxWidgetClass,panel,hArgs,1);
    if (lockBox==NULL)
	uFatalError("Couldn't create locked modifiers box\n");
    effBox= XtCreateManagedWidget("effective",boxWidgetClass,panel,hArgs,1);
    if (effBox==NULL)
	uFatalError("Couldn't create effective modifiers box\n");
    compatBox= XtCreateManagedWidget("compat",boxWidgetClass,panel,hArgs,1);
    if (compatBox==NULL)
	uFatalError("Couldn't create compatibility state box\n");
    XkbSelectEvents(inDpy,XkbUseCoreKbd,XkbStateNotifyMask,XkbStateNotifyMask);
    XkbGetState(inDpy,XkbUseCoreKbd,&state);
    for (i=XkbNumModifiers-1,bit=0x80;i>=0;i--,bit>>=1) {
	ArgList	list;
	char	buf[30];
	sprintf(buf,"base%d",i);
	if (state.base_mods&bit)	list= onArgs;
	else				list= offArgs;
	base[i]= XtCreateManagedWidget(buf,ledWidgetClass,baseBox,list,1);
	sprintf(buf,"latched%d",i);
	if (state.latched_mods&bit)	list= onArgs;
	else				list= offArgs;
	latched[i]= XtCreateManagedWidget(buf,ledWidgetClass,latchBox,list,1);
	sprintf(buf,"locked%d",i);
	if (state.locked_mods&bit)	list= onArgs;
	else				list= offArgs;
	locked[i]= XtCreateManagedWidget(buf,ledWidgetClass,lockBox,list,1);
	sprintf(buf,"effective%d",i);
	if (state.mods&bit)		list= onArgs;
	else				list= offArgs;
	effective[i]= XtCreateManagedWidget(buf,ledWidgetClass,effBox,list,1);
	sprintf(buf,"compat%d",i);
	if (state.compat_state&bit)	list= onArgs;
	else				list= offArgs;
	compat[i]= XtCreateManagedWidget(buf,ledWidgetClass,compatBox,list,1);
    }
    XtRealizeWidget(toplevel);
    while (1) {
        XtAppNextEvent(app_con,&ev.core);
	if (ev.core.type==evBase+XkbEventCode) {
	    if (ev.any.xkb_type==XkbStateNotify) {
		unsigned changed;
		if (ev.state.changed&XkbModifierBaseMask) {
		    changed= ev.state.base_mods^state.base_mods;
		    state.base_mods= ev.state.base_mods;
		    for (i=0,bit=1;i<XkbNumModifiers;i++,bit<<=1) {
			if (changed&bit) {
			    ArgList	list;
			    if (state.base_mods&bit)	list= onArgs;
			    else			list= offArgs;
			    XtSetValues(base[i],list,1);
			}
		    }
		}
		if (ev.state.changed&XkbModifierLatchMask) {
		    changed= ev.state.latched_mods^state.latched_mods;
		    state.latched_mods= ev.state.latched_mods;
		    for (i=0,bit=1;i<XkbNumModifiers;i++,bit<<=1) {
			if (changed&bit) {
			    ArgList	list;
			    if (state.latched_mods&bit)	list= onArgs;
			    else			list= offArgs;
			    XtSetValues(latched[i],list,1);
			}
		    }
		}
		if (ev.state.changed&XkbModifierLockMask) {
		    changed= ev.state.locked_mods^state.locked_mods;
		    state.locked_mods= ev.state.locked_mods;
		    for (i=0,bit=1;i<XkbNumModifiers;i++,bit<<=1) {
			if (changed&bit) {
			    ArgList	list;
			    if (state.locked_mods&bit)	list= onArgs;
			    else			list= offArgs;
			    XtSetValues(locked[i],list,1);
			}
		    }
		}
		if (ev.state.changed&XkbModifierStateMask) {
		    changed= ev.state.mods^state.mods;
		    state.mods= ev.state.mods;
		    for (i=0,bit=1;i<XkbNumModifiers;i++,bit<<=1) {
			if (changed&bit) {
			    ArgList	list;
			    if (state.mods&bit)	list= onArgs;
			    else		list= offArgs;
			    XtSetValues(effective[i],list,1);
			}
		    }
		}
		if (ev.state.changed&XkbCompatStateMask) {
		    changed= ev.state.compat_state^state.compat_state;
		    state.compat_state= ev.state.compat_state;
		    for (i=0,bit=1;i<XkbNumModifiers;i++,bit<<=1) {
			if (changed&bit) {
			    ArgList	list;
			    if (state.compat_state&bit)	list= onArgs;
			    else			list= offArgs;
			    XtSetValues(compat[i],list,1);
			}
		    }
		}
	    }
	}
	else XtDispatchEvent(&ev.core);
    }
/* BAIL: */
    if (inDpy) 
	XCloseDisplay(inDpy);
    if (outDpy!=inDpy)
	XCloseDisplay(outDpy);
    inDpy= outDpy= NULL;
    return 0;
}
