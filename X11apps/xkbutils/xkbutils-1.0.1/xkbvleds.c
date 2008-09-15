/* $Xorg: xkbvleds.c,v 1.4 2000/08/17 19:54:51 cpqbld Exp $ */
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
/* $XFree86: xc/programs/xkbutils/xkbvleds.c,v 3.4 2001/01/17 23:46:14 dawes Exp $ */

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

#define	YES		1
#define	NO		0
#define	DONT_CARE	-1

	Display *	inDpy,*outDpy;
static	unsigned long	wanted,real,named,explicit,automatic,virtual;
static	char *		inDpyName;
	int		wantNamed= DONT_CARE;
	int		wantExplicit= DONT_CARE;
	int		wantAutomatic= DONT_CARE;
	int		wantReal= DONT_CARE;
	int		wantVirtual= DONT_CARE;
	int		evBase,errBase;
	Bool		synch;
	Bool		useUnion= True;

/***====================================================================***/

static void
usage(char *program)
{
    uInformation("Usage: %s <options>\n",program);
    uInformation("Legal options include the usual X toolkit options plus:\n");
    uInformation("  -help           Print this message\n");
    uInformation("  -indpy <name>   Name of display to watch\n");
    uInformation("  -watch <leds>   Mask of LEDs to watch\n");
    uInformation("  [-+]automatic   (Don't) watch automatic LEDs\n");
    uInformation("  [-+]explicit    (Don't) watch explicit LEDs\n");
    uInformation("  [-+]name        (Don't) watch named LEDs\n");
    uInformation("  [-+]real        (Don't) watch real LEDs\n");
    uInformation("  [-+]virtual     (Don't) watch virtual LEDs\n");
    uInformation("  -intersection   Watch only LEDs in all desired sets\n");
    uInformation("  -union          Watch LEDs in any desired sets\n");
    uInformation("The default set of LEDs is -union +name +automatic +real\n");
    return;
}

static Bool
parseArgs(int argc, char *argv[])
{
register int 	i;

    for (i=1;i<argc;i++) {
	if (uStrCaseEqual(argv[i],"-indpy")) {
	    if (i<argc-1)	inDpyName= argv[++i];
	    else {
		uWarning("No name specified for input display\n");
		uAction("Ignoring trailing -indpy argument\n");
	    }
	}
	else if (uStrCaseEqual(argv[i],"-watch")) {
	    if (i<argc-1) {
		int tmp;
		if (sscanf(argv[++i],"%i",&tmp)!=1) {
		    uWarning("Set of LEDs must be specified as an integer\n");
		    uAction("Ignoring bogus value \"%s\" for -watch flag\n",
								argv[i]);
		}
		else wanted= tmp;
	    }
	    else {
		uWarning("Didn't specify any LEDs to watch\n");
		uAction("Ignoring trailing -watch argument\n");
	    }
	}
	else if (uStrCaseEqual(argv[i],"-union")) {
	    useUnion= True;
	}
	else if (uStrCaseEqual(argv[i],"-intersection")) {
	    useUnion= False;
	}
	else if (uStrCaseEqual(argv[i],"-help")) {
	    usage(argv[0]);
	    exit(0);
	}
	else if ((argv[i][0]=='+')||(argv[i][0]=='-')) {
	    Bool 	onoff;
	    int	 *	which;
	    onoff= (argv[i][0]=='+');
	    which= NULL;
	    if (uStrCaseEqual(&argv[i][1],"name"))
		 which= &wantNamed;
	    else if (uStrCaseEqual(&argv[i][1],"explicit"))
		 which= &wantExplicit;
	    else if (uStrCaseEqual(&argv[i][1],"automatic"))
		 which= &wantAutomatic;
	    else if (uStrCaseEqual(&argv[i][1],"real"))
		 which= &wantReal;
	    else if (uStrCaseEqual(&argv[i][1],"virtual"))
		 which= &wantVirtual;
	    if (which!=NULL) {
		if (*which!=DONT_CARE) {
		    uWarning("Multiple settings for [+-]%s\n",&argv[i][1]);
		    uAction("Using %c%s, ignoring %c%s\n",
					(onoff?'+':'-'),&argv[i][1],
					(onoff?'-':'+'),&argv[i][1]);
		}
		*which= (onoff?YES:NO);
	    }
	}
    }
    return True;
}

/***====================================================================***/

static Display *
GetDisplay(char *program, char *dpyName)
{
int		mjr,mnr,error;
Display	*	dpy;

    mjr= XkbMajorVersion;
    mnr= XkbMinorVersion;
    dpy= XkbOpenDisplay(dpyName,&evBase,&errBase,&mjr,&mnr,&error);
    if (dpy==NULL) {
	switch (error) {
	    case XkbOD_BadLibraryVersion:
		uInformation("%s was compiled with XKB version %d.%02d\n",
				program,XkbMajorVersion,XkbMinorVersion);
		uError("X library supports incompatible version %d.%02d\n",
				mjr,mnr);
		break;
	    case XkbOD_ConnectionRefused:
		uError("Cannot open display \"%s\"\n",dpyName);
		break;
	    case XkbOD_NonXkbServer:
		uError("XKB extension not present on %s\n",dpyName);
		break;
	    case XkbOD_BadServerVersion:
		uInformation("%s was compiled with XKB version %d.%02d\n",
				program,XkbMajorVersion,XkbMinorVersion);
		uError("Server %s uses incompatible version %d.%02d\n",
				dpyName,mjr,mnr);
		break;
	    default:
		uInternalError("Unknown error %d from XkbOpenDisplay\n",error);
	}
    }
    else if (synch)
	XSynchronize(dpy,True);
    return dpy;
}

/***====================================================================***/

int
main(int argc, char *argv[])
{
Widget		toplevel;
XtAppContext	app_con;
Widget		panel;
Widget		leds[XkbNumIndicators];
register int	i;
unsigned	bit;
unsigned	n;
XkbDescPtr	xkb;
XkbEvent	ev;
static Arg	boxArgs[]= {{ XtNorientation, (XtArgVal)XtorientHorizontal }};
static Arg	onArgs[]=  {{ XtNon, (XtArgVal)True }};
static Arg	offArgs[]=  {{ XtNon, (XtArgVal)False }};
static char *	fallback_resources[] = {
    "*Box*background: grey40",
    NULL
};

    uSetEntryFile(NullString);
    uSetDebugFile(NullString);
    uSetErrorFile(NullString);
    bzero(leds,XkbNumIndicators*sizeof(Widget));
    toplevel = XtOpenApplication(&app_con, "XkbLEDPanel", NULL, 0, &argc, argv, 
				 fallback_resources,
				 sessionShellWidgetClass, NULL, ZERO);
    if (toplevel==NULL) {
	uFatalError("Couldn't create application top level\n");
	return 1;
    }
    if ((argc>1)&&(!parseArgs(argc,argv))) {
	usage(argv[0]);
	return 1;
    }
    if ((wanted==0)&&(wantNamed==DONT_CARE)&&(wantExplicit==DONT_CARE)&&
			(wantAutomatic==DONT_CARE)&&(wantReal==DONT_CARE)) {
	wantNamed= YES;
	wantReal= YES;
	wantAutomatic= YES;
    }
    outDpy= XtDisplay(toplevel);
    if (inDpyName!=NULL) {
	inDpy= GetDisplay(argv[0],inDpyName);
	if (!inDpy)
	    return 1;
    }
    else {
	inDpy= outDpy;
    }
    if (inDpy) {
	int i1,mn,mj;
	mj= XkbMajorVersion;
	mn= XkbMinorVersion;
	if (!XkbLibraryVersion(&mj,&mn)) {
	    uInformation("%s was compiled with XKB version %d.%02d\n",
				argv[0],XkbMajorVersion,XkbMinorVersion);
	    uError("X library supports incompatible version %d.%02d\n",
				mj,mn);
	}
	if (!XkbQueryExtension(inDpy,&i1,&evBase,&errBase,&mj,&mn)) {
	    uFatalError("Server doesn't support a compatible XKB\n");
	    return 1;
	}
    }
    else {
	uFatalError("No input display\n");
	return 1;
    }
    panel= XtCreateManagedWidget("xkbleds",boxWidgetClass,toplevel,boxArgs,1);
    if (panel==NULL) {
	uFatalError("Couldn't create list of leds\n");
	return 1;
    }
    real= virtual= named= explicit= automatic= 0;
    if (wantReal || wantNamed || wantAutomatic || wantExplicit || wantVirtual) {
	register int i,bit;
	xkb= XkbGetMap(inDpy,0,XkbUseCoreKbd);
	if (!xkb) {
	    uFatalError("Couldn't read keymap\n");
	    return 1;
	}
	if (XkbGetIndicatorMap(inDpy,XkbAllIndicatorsMask,xkb)!=Success) {
	    uFatalError("Couldn't read indicator map\n");
	    return 1;
	}
	if (XkbGetNames(inDpy,XkbAllNamesMask,xkb)!=Success) {
	    uFatalError("Couldn't read indicator names\n");
	    return 1;
	}
	for (i=0,bit=1;i<XkbNumIndicators;i++,bit<<=1) {
	    XkbIndicatorMapPtr map= &xkb->indicators->maps[i];
	    if (xkb->names->indicators[i]!=None)
		named|= bit;
	    if (xkb->indicators->phys_indicators&bit)
		real|= bit;
	    if ((((map->which_groups!=0)&&(map->groups!=0))||
		((map->which_mods!=0)&&
		((map->mods.real_mods!=0)||(map->mods.vmods!=0)))||
		(map->ctrls!=0))&&
		((map->flags&XkbIM_NoAutomatic)==0)) {
		automatic|= bit;
	    }
	    else explicit|= bit;
	}
	virtual= ~real;
	if (wantReal==NO)			real= ~real;
	else if (wantReal==DONT_CARE)		real= (useUnion?0:~0);
	if (wantVirtual==NO)			virtual= ~virtual;
	else if (wantVirtual==DONT_CARE)	virtual= (useUnion?0:~0);
	if (wantNamed==NO)			named= ~named;
	else if (wantNamed==DONT_CARE)		named= (useUnion?0:~0);
	if (wantAutomatic==NO)			automatic= ~automatic;
	else if (wantAutomatic==DONT_CARE)	automatic= (useUnion?0:~0);
	if (wantExplicit==NO)			explicit= ~explicit;
	else if (wantExplicit==DONT_CARE)	explicit= (useUnion?0:~0);
	if (useUnion)
	     wanted|= real|virtual|named|automatic|explicit;
	else wanted&= real&virtual&named&automatic&explicit;
    }
    else xkb= NULL;
    if (wanted==0) {
	uError("No indicator maps match the selected criteria\n");
	uAction("Exiting\n");
	return 1;
    }

    XkbSelectEvents(inDpy,XkbUseCoreKbd,XkbIndicatorStateNotifyMask,
						XkbIndicatorStateNotifyMask);
    XkbGetIndicatorState(inDpy,XkbUseCoreKbd,&n);
    bit= (1<<(XkbNumIndicators-1));
    for (i=XkbNumIndicators-1;i>=0;i--,bit>>=1) {
	if (wanted&bit) {
	    char 	buf[12];
	    ArgList	list;

	    sprintf(buf,"led%d",i+1);
	    if (n&bit)	list= onArgs;
	    else	list= offArgs;
	    leds[i]= XtCreateManagedWidget(buf,ledWidgetClass,panel,list,1);
	}
    }
    XtRealizeWidget(toplevel);
    while (1) {
        XtAppNextEvent(app_con,&ev.core);
	if (ev.core.type==evBase+XkbEventCode) {
	    if (ev.any.xkb_type==XkbIndicatorStateNotify) {
		for (i=0,bit=1;i<XkbNumIndicators;i++,bit<<=1) {
		    if ((ev.indicators.changed&bit)&&(leds[i])) {
			ArgList	list;
			if (ev.indicators.state&bit)	list= onArgs;
			else				list= offArgs;
			XtSetValues(leds[i],list,1);
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
