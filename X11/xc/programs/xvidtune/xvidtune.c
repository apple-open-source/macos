/* $XFree86: xc/programs/xvidtune/xvidtune.c,v 3.33 2003/02/22 04:08:44 tsi Exp $ */

/*

Copyright (c) 1995  Kaleb S. KEITHLEY

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL Kaleb S. KEITHLEY BE LIABLE FOR ANY CLAIM, DAMAGES 
OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Kaleb S. KEITHLEY 
shall not be used in advertising or otherwise to promote the sale, use 
or other dealings in this Software without prior written authorization
from Kaleb S. KEITHLEY.

*/

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xmu/StdSel.h>
#include <X11/Xmd.h>
#include <X11/extensions/xf86vmode.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int MajorVersion, MinorVersion;
int EventBase, ErrorBase;
int dot_clock, mode_flags;
unsigned long    TestTimeout=5000;  /* Default test timeout */
XtSignalId sigId;

/* Minimum extension version required */
#define MINMAJOR 0
#define MINMINOR 5

/* Mode flags -- ignore flags not in V_FLAG_MASK */
#define V_FLAG_MASK	0x1FF;
#define V_PHSYNC	0x001 
#define V_NHSYNC	0x002
#define V_PVSYNC	0x004
#define V_NVSYNC	0x008
#define V_INTERLACE	0x010 
#define V_DBLSCAN	0x020
#define V_CSYNC		0x040
#define V_PCSYNC	0x080
#define V_NCSYNC	0x100

typedef enum { HDisplay, HSyncStart, HSyncEnd, HTotal,
	VDisplay, VSyncStart, VSyncEnd, VTotal, Flags, 
	InvertVclk, BlankDelay1, BlankDelay2, EarlySc,
	PixelClock, HSyncRate, VSyncRate, fields_num } fields;

typedef struct {
    fields	me;
    fields	use;
    int		val;
    int		lastpercent;
    int		range;
    Widget	textwidget;
    Widget	scrollwidget;
} ScrollData;

static struct _AppResources {
    ScrollData	field[fields_num];
    Bool	ad_installed;
    int		orig[fields_num];
    int		old[fields_num];
} AppRes = {
    {
	{ HDisplay, },
	{ HSyncStart, HDisplay, },
	{ HSyncEnd, HDisplay, },
	{ HTotal, HDisplay, },
	{ VDisplay, },
	{ VSyncStart, VDisplay, },
	{ VSyncEnd, VDisplay, },
	{ VTotal, VDisplay, },
	{ Flags, },
	{ InvertVclk, },
	{ BlankDelay1, },
	{ BlankDelay2, },
	{ EarlySc, },
	{ PixelClock, },
	{ HSyncRate, },
	{ VSyncRate, },
    },
};

static XtResource Resources[] = {
    { "adInstalled", "AdInstalled", XtRBool, sizeof(Bool),
	XtOffsetOf(struct _AppResources, ad_installed),
	XtRImmediate, (XtPointer)FALSE },
    { "hSyncStartRange", "SyncStartRange", XtRInt, sizeof(int),
	XtOffsetOf(struct _AppResources, field[HSyncStart].range), 
	XtRImmediate, (XtPointer)200 },
    { "hSyncEndRange", "SyncEndRange", XtRInt, sizeof(int),
	XtOffsetOf(struct _AppResources, field[HSyncEnd].range), 
	XtRImmediate, (XtPointer)400 },
    { "hTotalRange", "TotalRange", XtRInt, sizeof(int),
	XtOffsetOf(struct _AppResources, field[HTotal].range), 
	XtRImmediate, (XtPointer)400 },
    { "vSyncStartRange", "SyncStartRange", XtRInt, sizeof(int),
	XtOffsetOf(struct _AppResources, field[VSyncStart].range), 
	XtRImmediate, (XtPointer)20 },
    { "vSyncEndRange", "SyncEndRange", XtRInt, sizeof(int),
	XtOffsetOf(struct _AppResources, field[VSyncEnd].range), 
	XtRImmediate, (XtPointer)40 },
    { "vTotalRange", "TotalRange", XtRInt, sizeof(int),
	XtOffsetOf(struct _AppResources, field[VTotal].range), 
	XtRImmediate, (XtPointer)80 },
};

static XtTranslations trans;

static Atom wm_delete_window;
static Widget invalid_mode_popup;
static Widget testing_popup;
static Widget Top;
static Widget auto_apply_toggle;

static Bool S3Specials = False;
static char modebuf[160];

static void UpdateSyncRates(Bool dolabels);

static void
CleanUp(Display *dpy)
{
    /* Make sure mode switching is not locked out at exit */
    XF86VidModeLockModeSwitch(dpy, DefaultScreen(dpy), FALSE);
    XFlush(dpy);
}

static void
CatchSig(int signal)
{
    XtNoticeSignal(sigId);
}

static void
CatchXtSig(XtPointer closure, XtSignalId *id)
{
    CleanUp(XtDisplay(Top));
    exit(3);
}

static Bool
GetModeLine (Display* dpy, int scrn)
{
    XF86VidModeModeLine mode_line;
    fields i;

    if (!XF86VidModeGetModeLine (dpy, scrn, &dot_clock, &mode_line))
	return FALSE;

    AppRes.field[HDisplay].val = mode_line.hdisplay;
    AppRes.field[HSyncStart].val = mode_line.hsyncstart;
    AppRes.field[HSyncEnd].val = mode_line.hsyncend;
    AppRes.field[HTotal].val = mode_line.htotal;
    AppRes.field[VDisplay].val = mode_line.vdisplay;
    AppRes.field[VSyncStart].val = mode_line.vsyncstart;
    AppRes.field[VSyncEnd].val = mode_line.vsyncend;
    AppRes.field[VTotal].val = mode_line.vtotal;
    mode_flags = mode_line.flags;
    AppRes.field[Flags].val = mode_flags & V_FLAG_MASK;
    AppRes.field[PixelClock].val = dot_clock;
    UpdateSyncRates(FALSE);
    if (mode_line.privsize && mode_line.private) {
	S3Specials = True;
	AppRes.field[InvertVclk].val = mode_line.private[1];
	AppRes.field[BlankDelay1].val = mode_line.private[2] & 7;
	AppRes.field[BlankDelay2].val = (mode_line.private[2] >> 4) & 7;
	AppRes.field[EarlySc].val = mode_line.private[3];
    }

    for (i = HDisplay; i < fields_num; i++) 
	AppRes.orig[i] = AppRes.field[i].val;
    return TRUE;
}

static Bool
GetMonitor (Display* dpy, int scrn)
{
    XF86VidModeMonitor monitor;
    int i;

    if (!XF86VidModeGetMonitor (dpy, scrn, &monitor))
	return FALSE;

    printf("Vendor: %s, Model: %s\n", monitor.vendor, monitor.model);
    printf("Num hsync: %d, Num vsync: %d\n", monitor.nhsync, monitor.nvsync);
    for (i = 0; i < monitor.nhsync; i++) {
	printf("hsync range %d: %6.2f - %6.2f\n", i, monitor.hsync[i].lo,
	       monitor.hsync[i].hi);
    }
    for (i = 0; i < monitor.nvsync; i++) {
	printf("vsync range %d: %6.2f - %6.2f\n", i, monitor.vsync[i].lo,
	       monitor.vsync[i].hi);
    }
    return TRUE;
}

static Bool
ModeSettable(void)
{
    if (AppRes.field[HTotal].val == 0 || AppRes.field[VTotal].val == 0)
	return FALSE;
    return TRUE;
}

static int hitError = 0;
static int (*xtErrorfunc)(Display *, XErrorEvent *);

static int
vidmodeError(Display *dis, XErrorEvent *err)
{
  if ((err->error_code >= ErrorBase &&
      err->error_code < ErrorBase + XF86VidModeNumberErrors) ||
      err->error_code == BadValue) {
     hitError=1;
  } else {
     CleanUp(dis);
     if (xtErrorfunc) 
	(*xtErrorfunc)(dis, err);
  }
  return 0; /* ignored */
}

static void
SetScrollbars (void)
{
    fields i;

    for (i = HDisplay; i <= Flags; i++) {

	ScrollData* sdp = &AppRes.field[i];

	if (sdp->scrollwidget != (Widget) NULL) {
	    int base;
	    float percent;

	    base = AppRes.field[sdp->use].val;
	    percent = ((float)(sdp->val - base)) / ((float)sdp->range);
	    XawScrollbarSetThumb (sdp->scrollwidget, percent, 0.0);
	}
    }
}

static void
QuitCB (Widget w, XtPointer client, XtPointer call)
{
    CleanUp(XtDisplay(w));
#if XtSpecificationRelease < 6
    exit (0);
#else
    XtAppSetExitFlag (XtWidgetToApplicationContext (w));
#endif
}

static void
popdownInvalid(Widget w, XtPointer client, XtPointer call)
{
   XtPopdown((Widget)client);
}

static void
ApplyCB (Widget w, XtPointer client, XtPointer call)
{
    XF86VidModeModeLine mode_line;
    INT32 S3private[4];
    int i;
    char* string;
    Boolean state;

    mode_line.hdisplay = AppRes.field[HDisplay].val;
    mode_line.hsyncstart = AppRes.field[HSyncStart].val;
    mode_line.hsyncend = AppRes.field[HSyncEnd].val;
    mode_line.htotal = AppRes.field[HTotal].val;
    mode_line.vdisplay = AppRes.field[VDisplay].val;
    mode_line.vsyncstart = AppRes.field[VSyncStart].val;
    mode_line.vsyncend = AppRes.field[VSyncEnd].val;
    mode_line.vtotal = AppRes.field[VTotal].val;
    /* Don't read flags from widget */
#if 0
    XtVaGetValues (AppRes.field[Flags].textwidget,
		XtNstring, &string, NULL);
    (void) sscanf (string, "%x", &i);
#endif
    mode_line.flags = mode_flags;
    if (S3Specials) {
	mode_line.privsize = 4;
	mode_line.private = S3private;
	mode_line.private[0] = (1 << 1) | (1 << 2) | (1 << 3);
	XtVaGetValues(AppRes.field[InvertVclk].textwidget,
			XtNstate, &state, NULL);
	AppRes.field[InvertVclk].val = state ? 1 : 0;
	mode_line.private[1] = AppRes.field[InvertVclk].val;
	XtVaGetValues (AppRes.field[BlankDelay1].textwidget,
			XtNstring, &string, NULL);
	(void) sscanf (string, "%x", &i);
	AppRes.field[BlankDelay1].val = i;
	mode_line.private[2] = AppRes.field[BlankDelay1].val;
	XtVaGetValues (AppRes.field[BlankDelay2].textwidget,
			XtNstring, &string, NULL);
	(void) sscanf (string, "%x", &i);
	AppRes.field[BlankDelay2].val = i;
	mode_line.private[2] |= AppRes.field[BlankDelay2].val << 4;
	XtVaGetValues(AppRes.field[EarlySc].textwidget,
			XtNstate, &state, NULL);
	AppRes.field[EarlySc].val = state ? 1 : 0;
	mode_line.private[3] = AppRes.field[EarlySc].val;
    } else
	mode_line.privsize = 0;
    
   hitError = 0;

   XF86VidModeModModeLine (XtDisplay (w), DefaultScreen (XtDisplay (w)), 
		&mode_line);
   XSync(XtDisplay (w), False); /* process errors  */
   if (hitError) {
       XBell(XtDisplay (w), 80);
       XtPopup(invalid_mode_popup, XtGrabExclusive /*XtGrabNone*/);
   }
}


static void
SetLabel(fields i)
{
   ScrollData* sdp = &AppRes.field[i];

   if (sdp->textwidget != (Widget) NULL) {
      char buf[10];
      Boolean state;

      /*
       * Disable AutoApply so that the apply doesn't happen more than
       * once as a consequence of callbacks being called because of the
       * XtSetValues calls
       */

      XtVaGetValues(auto_apply_toggle, XtNstate, &state, NULL);
      if (state)
	 XtVaSetValues(auto_apply_toggle, XtNstate, 0, NULL);

      if (i == Flags)
	 (void) sprintf (buf, "%04x", sdp->val);
      else if (i >= PixelClock && i <= VSyncRate)
	 (void) sprintf (buf, "%6.2f", (float)sdp->val / 1000.0);
      else if (i == BlankDelay1 || i == BlankDelay2) {
	 (void) sprintf (buf, "%d", sdp->val);
      } else
	 (void) sprintf (buf, "%5d", sdp->val);
	 
      sdp->lastpercent = -1;
      if (i == Flags) {
	 XawTextBlock text;

	 text.firstPos = 0;
	 text.length = 4;
	 text.ptr = buf;
	 text.format = XawFmt8Bit;
	 XawTextReplace (sdp->textwidget, 0, 4, &text);
      } else if (i == BlankDelay1 || i == BlankDelay2) {
	 XawTextBlock text;

	 text.firstPos = 0;
	 text.length = 1;
	 text.ptr = buf;
	 XawTextReplace (sdp->textwidget, 0, 1, &text);
      } else if (i == InvertVclk || i == EarlySc) {
	XtVaSetValues (sdp->textwidget, XtNstate, sdp->val, NULL);
      } else
	XtVaSetValues (sdp->textwidget, XtNlabel, buf, NULL);

      if (state)
	 XtVaSetValues(auto_apply_toggle, XtNstate, 1, NULL);
   }

}

static void
UpdateSyncRates(Bool dolabels)
{
    if (!ModeSettable())
	return;
    
    AppRes.field[HSyncRate].val = AppRes.field[PixelClock].val * 1000 /
				  AppRes.field[HTotal].val;
    AppRes.field[VSyncRate].val = AppRes.field[HSyncRate].val * 1000 /
				  AppRes.field[VTotal].val;
    if (mode_flags & V_INTERLACE)
	AppRes.field[VSyncRate].val *= 2;
    else if (mode_flags & V_DBLSCAN)
	AppRes.field[VSyncRate].val /= 2;
    if (dolabels) {
	SetLabel(HSyncRate);
	SetLabel(VSyncRate);
    }
}

static void
RestoreCB (Widget w, XtPointer client, XtPointer call)
{
    fields i;
    Boolean state;

    for (i = HDisplay; i < fields_num; i++) {
	AppRes.field[i].val = AppRes.orig[i];
	SetLabel(i);
    }
    SetScrollbars ();
    XtVaGetValues(auto_apply_toggle, XtNstate, &state, NULL);
    if (state)
	 ApplyCB (w, client, call);
}


static void
ApplyIfAutoCB (Widget w, XtPointer client, XtPointer call)
{
   Boolean state;

   XtVaGetValues(auto_apply_toggle, XtNstate, &state, NULL);
   if (state)
       ApplyCB (w, client, call);
}


static void
FetchCB (Widget w, XtPointer client, XtPointer call)
{
    fields i;
    (void) GetModeLine(XtDisplay (w), DefaultScreen (XtDisplay (w)));
    SetScrollbars ();
    for (i = HDisplay; i < fields_num; i++) {
        SetLabel(i);
    }   
}

static XtIntervalId TOid;

static void
TestTO (XtPointer client, XtIntervalId* id)
{
    fields i;
    for (i = HDisplay; i < fields_num; i++)
	AppRes.field[i].val = AppRes.orig[i];

    ApplyCB ((Widget) client, NULL, NULL);

    for (i = HDisplay; i < fields_num; i++)
	AppRes.field[i].val = AppRes.old[i];
    SetScrollbars ();

    XtPopdown(testing_popup);
}

static void
TestTOCB (Widget w, XtPointer client, XtPointer call)
{
  XtRemoveTimeOut(TOid);
  TestTO(w, (XtIntervalId *) NULL);
}

static void
TestCB (Widget w, XtPointer client, XtPointer call)
{
    fields i;
    for (i = HDisplay; i < fields_num; i++)
	AppRes.old[i] = AppRes.field[i].val;

    XtPopup(testing_popup, XtGrabExclusive /*XtGrabNone*/);
    XSync(XtDisplay(w), False);   
    TOid = XtAppAddTimeOut (XtWidgetToApplicationContext (w),
		TestTimeout, TestTO, (XtPointer) w);

    ApplyCB (w, client, call);
}

static Boolean
ConvertSelection(
    Widget w,
    Atom *selection, Atom *target, Atom *type,
    XtPointer *value,
    unsigned long *length,
    int *format)
{
    if (XmuConvertStandardSelection(w, CurrentTime, selection, target, type,
                                    (XPointer *) value, length, format))
        return True;

    if (*target == XA_STRING) {
        *type = XA_STRING;
        *value = modebuf;
        *length = strlen(*value);
        *format = 8;
        return True;
    }
    return False;
}

static void
ShowCB(Widget w, XtPointer client, XtPointer call)
{
    Time time;
    char tmpbuf[16];

    sprintf(tmpbuf, "\"%dx%d\"",
	   AppRes.field[HDisplay].val, AppRes.field[VDisplay].val);
    sprintf(modebuf, "%-11s   %6.2f   %4d %4d %4d %4d   %4d %4d %4d %4d",
	   tmpbuf, (float)dot_clock/1000.0,
	   AppRes.field[HDisplay].val,
	   AppRes.field[HSyncStart].val,
	   AppRes.field[HSyncEnd].val,
	   AppRes.field[HTotal].val,
	   AppRes.field[VDisplay].val,
	   AppRes.field[VSyncStart].val,
	   AppRes.field[VSyncEnd].val,
	   AppRes.field[VTotal].val);
    /* Print out the flags (if any) */
    if (mode_flags & V_PHSYNC)    strcat(modebuf, " +hsync");
    if (mode_flags & V_NHSYNC)    strcat(modebuf, " -hsync");
    if (mode_flags & V_PVSYNC)    strcat(modebuf, " +vsync");
    if (mode_flags & V_NVSYNC)    strcat(modebuf, " -vsync");
    if (mode_flags & V_INTERLACE) strcat(modebuf, " interlace");
    if (mode_flags & V_CSYNC)     strcat(modebuf, " composite");
    if (mode_flags & V_PCSYNC)    strcat(modebuf, " +csync");
    if (mode_flags & V_NCSYNC)    strcat(modebuf, " -csync");
    if (mode_flags & V_DBLSCAN)   strcat(modebuf, " doublescan");
    printf("%s\n", modebuf);
    time = XtLastTimestampProcessed(XtDisplay(w));
    XtOwnSelection(w, XA_PRIMARY, time, ConvertSelection, NULL, NULL);
    if (S3Specials) {
	int i;
	Boolean state;
	char *string;

	XtVaGetValues(AppRes.field[InvertVclk].textwidget,
			XtNstate, &state, NULL);
	AppRes.field[InvertVclk].val = state ? 1 : 0;
	XtVaGetValues (AppRes.field[BlankDelay1].textwidget,
			XtNstring, &string, NULL);
	(void) sscanf (string, "%x", &i);
	AppRes.field[BlankDelay1].val = i;
	XtVaGetValues (AppRes.field[BlankDelay2].textwidget,
			XtNstring, &string, NULL);
	(void) sscanf (string, "%x", &i);
	AppRes.field[BlankDelay2].val = i;
	XtVaGetValues(AppRes.field[EarlySc].textwidget,
			XtNstate, &state, NULL);
	AppRes.field[EarlySc].val = state ? 1 : 0;
	if (AppRes.field[InvertVclk].val != AppRes.orig[InvertVclk])
	    printf("InvertVCLK\t\"%dx%d\" %d\n", AppRes.field[HDisplay].val,
		AppRes.field[VDisplay].val, AppRes.field[InvertVclk].val);
	if (AppRes.field[EarlySc].val != AppRes.orig[EarlySc])
	    printf("EarlySC\t\t\"%dx%d\" %d\n", AppRes.field[HDisplay].val,
		AppRes.field[VDisplay].val, AppRes.field[EarlySc].val);
	if (AppRes.field[BlankDelay1].val != AppRes.orig[BlankDelay1]
	    || AppRes.field[BlankDelay2].val != AppRes.orig[BlankDelay2])
	    printf("BlankDelay\t\"%dx%d\" %d %d\n", AppRes.field[HDisplay].val,
		AppRes.field[VDisplay].val, AppRes.field[BlankDelay1].val,
		AppRes.field[BlankDelay2].val);
    }
    printf("\n");
}

static void
AdjustCB(Widget w, XtPointer client, XtPointer call)
{
   int what = (long) client;
   Boolean state;
   
   switch (what) {
    case HSyncStart:
      if (AppRes.field[HSyncEnd].val + 4 < AppRes.field[HTotal].val) {
	 AppRes.field[HSyncEnd].val += 4;
	 AppRes.field[HSyncStart].val += 4;
	 SetLabel(HSyncStart);	 
	 SetLabel(HSyncEnd);		 
      } else
	XBell(XtDisplay(w), 80);
      break;
    case -HSyncStart:
      if (AppRes.field[HSyncStart].val - 4 > AppRes.field[HDisplay].val) {
	 AppRes.field[HSyncEnd].val -= 4;
	 AppRes.field[HSyncStart].val -= 4;
	 SetLabel(HSyncStart);	 
	 SetLabel(HSyncEnd);	 	 	 
      } else
	XBell(XtDisplay(w), 80);
      break;
    case HTotal:
      AppRes.field[HTotal].val += 4;
      SetLabel(HTotal);	       
      UpdateSyncRates(TRUE);
      break;      
    case -HTotal:
      if (AppRes.field[HTotal].val - 4 >  AppRes.field[HSyncEnd].val) {	 
	AppRes.field[HTotal].val -= 4;
	SetLabel(HTotal);	 
	UpdateSyncRates(TRUE);
      } else
	XBell(XtDisplay(w), 80);
      break;
    case VSyncStart:
      if (AppRes.field[VSyncEnd].val + 4 < AppRes.field[VTotal].val) {
	 AppRes.field[VSyncEnd].val += 4;
	 AppRes.field[VSyncStart].val += 4;
	 SetLabel(VSyncStart);	 
	 SetLabel(VSyncEnd); 	 
      } else
	XBell(XtDisplay(w), 80);
      break;
    case -VSyncStart:
      if (AppRes.field[VSyncStart].val - 4 > AppRes.field[VDisplay].val) {
	 AppRes.field[VSyncEnd].val -= 4;
	 AppRes.field[VSyncStart].val -= 4;
	 SetLabel(VSyncStart);	 
	 SetLabel(VSyncEnd);	 	 
      } else
	XBell(XtDisplay(w), 80);
      break;
    case VTotal:
      AppRes.field[VTotal].val += 4;
      SetLabel(VTotal);      
      UpdateSyncRates(TRUE);
      break;      
    case -VTotal:
      if (AppRes.field[VTotal].val - 4 >  AppRes.field[VSyncEnd].val) {	 
	AppRes.field[VTotal].val -= 4;
	SetLabel(VTotal);
	UpdateSyncRates(TRUE);
      } else
	XBell(XtDisplay(w), 80);
      break;
   }  
   SetScrollbars ();
   XtVaGetValues(auto_apply_toggle, XtNstate, &state, NULL);
   if (state)
       ApplyCB (w, client, call);
}


#if 0
static void
EditCB (Widget w, XtPointer client, XtPointer call)
{
    int base, current, i, len;
    int lower, upper;
    float percent;
    ScrollData* sdp = (ScrollData*) client;

    len = strlen (sdp->string);

    for (i = 0; i < len; i++) {
	if (!(isdigit (sdp->string[i]) || isspace (sdp->string[i]))) {
	    XBell (XtDisplay(XtParent(w)), 100);
	    return;
	}
    }
    switch (sdp->me) {
    case HSyncStart: 
	lower = atoi (AppRes.field[HDisplay].string);
	upper = atoi (AppRes.field[HSyncEnd].string);
	break;

    case HSyncEnd:
	lower = atoi (AppRes.field[HSyncStart].string);
	upper = atoi (AppRes.field[HTotal].string);
	break;

    case HTotal:
	lower = atoi (AppRes.field[HSyncEnd].string);
	upper = atoi (AppRes.field[HDisplay].string) + 
		AppRes.field[HTotal].range;
	break;

    case VSyncStart: 
	lower = atoi (AppRes.field[VDisplay].string);
	upper = atoi (AppRes.field[VSyncEnd].string);
	break;

    case VSyncEnd:
	lower = atoi (AppRes.field[VSyncStart].string);
	upper = atoi (AppRes.field[VTotal].string);
	break;

    case VTotal:
	lower = atoi (AppRes.field[VSyncEnd].string);
	upper = atoi (AppRes.field[VDisplay].string) + 
		AppRes.field[VTotal].range;
	break;
    }
    current = atoi (sdp->string);
    if (current < lower || current > upper) {
	XawTextBlock text;
	char tmp[6];

	if (current < lower) {
	    (void) sprintf (tmp, "%5d", lower);
	    current = lower;
	} else {
	    (void) sprintf (tmp, "%5d", upper);
	    current = upper;
	}
	text.firstPos = 0;
	text.length = strlen (tmp);
	text.ptr = tmp;
	text.format = XawFmt8Bit;
	XawTextReplace (sdp->textwidget, 0, text.length, &text);
    }
    base = atoi (AppRes.field[sdp->use].string);
    percent = ((float)(current - base)) / ((float)sdp->range);
    XawScrollbarSetThumb (sdp->scrollwidget, percent, 0.0);
}
#endif

static void
FlagsEditCB (Widget w, XtPointer client, XtPointer call)
{
    int i, len;
    char* string;
    fields findex = (fields) client;
    ScrollData* sdp = &AppRes.field[findex];

    XtVaGetValues (w, XtNstring, &string, NULL);
    len = strlen (string);
    if (len > 4) {
	char buf[5];

	XBell (XtDisplay(XtParent(w)), 100);
	(void) strncpy (buf, string, 4);
	buf[4] = '\0';
	XtVaSetValues (sdp->textwidget, XtNstring, buf, NULL);
	XawTextSetInsertionPoint (sdp->textwidget, 4);
    }

    for (i = 0; i < len; i++) {
	if (!isxdigit (string[i])) {
	    XBell (XtDisplay(XtParent(w)), 100);
	}
    }
}

static void
BlankEditCB (Widget w, XtPointer client, XtPointer call)
{
    int len;
    char* string;
    fields findex = (fields) client;
    ScrollData* sdp = &AppRes.field[findex];
    char buf[2], old;
    Boolean state;
    Boolean noAuto = False;

    XtVaGetValues (w, XtNstring, &string, NULL);
    len = strlen (string);
    if (len == 0) {
	XBell (XtDisplay(XtParent(w)), 100);
	strcpy(buf, "0");
	XtVaSetValues (sdp->textwidget, XtNstring, buf, NULL);
	XawTextSetInsertionPoint (sdp->textwidget, 1);
        return;
    }
    if (len > 1) {
	if (XawTextGetInsertionPoint(sdp->textwidget) < 1) {
	    buf[0] = string[0];
	    old = string[1];
	} else {
	    buf[0] = string[1];
	    old = string[0];
	}
	if (buf[0] == '+' && old < '7')
	    buf[0] = old + 1;
	else if (buf[0] == '-' && old > '0')
	    buf[0] = old - 1;
	if (!isdigit(buf[0]) || buf[0] > '7') {
	    XBell (XtDisplay(XtParent(w)), 100);
	    buf[0] = old;
	    if (!isdigit(buf[0]) || buf[0] > '7')
		buf[0] = '0';
	    noAuto = True;
	}
	buf[1] = '\0';
	XtVaSetValues (sdp->textwidget, XtNstring, buf, NULL);
	XawTextSetInsertionPoint (sdp->textwidget, 1);
    }
    XtVaGetValues(auto_apply_toggle, XtNstate, &state, NULL);
    if (state && !noAuto)
	ApplyCB (sdp->textwidget, client, call);
}

static void
ChangeBlankCB (Widget w, XtPointer client, XtPointer call)
{
    char* string;
    char buf[2];
    fields findex;
    ScrollData* sdp;
    Boolean state;
    int what = (long) client;


    if (what < 0)
	findex = (fields)-what;
    else
	findex = (fields)what;
    sdp = &AppRes.field[findex];

    XtVaGetValues (sdp->textwidget, XtNstring, &string, NULL);
    if (what > 0)
	string[0]++;
    else
	string[0]--;

    if (string[0] < '0' || string[0] > '7') {
	XBell (XtDisplay(XtParent(w)), 100);
	return;
    }

    buf[0] = string[0];
    buf[1] = '\0';
    XtVaSetValues (sdp->textwidget, XtNstring, buf, NULL);
    XawTextSetInsertionPoint (sdp->textwidget, 1);

    XtVaGetValues(auto_apply_toggle, XtNstate, &state, NULL);
    if (state)
	ApplyCB (sdp->textwidget, client, call);
}

static int
isValid(int val, int field)
{
   switch(field) {
     case HSyncStart:
	if (val+8 > AppRes.field[HSyncEnd].val)
	   val = AppRes.field[HSyncEnd].val - 8;
        break;
     case HSyncEnd:
        if (val-8 < AppRes.field[HSyncStart].val)
	    val = AppRes.field[HSyncStart].val + 8;
        if (val > AppRes.field[HTotal].val)
	    val = AppRes.field[HTotal].val;
        break;
     case HTotal:
	if (val < AppRes.field[HSyncEnd].val)
	   val = AppRes.field[HSyncEnd].val;
         break;
     case VSyncStart:
	if (val+8 > AppRes.field[VSyncEnd].val)
	   val = AppRes.field[VSyncEnd].val - 8;
        break;
     case VSyncEnd:
        if (val-8 < AppRes.field[VSyncStart].val)
	    val = AppRes.field[VSyncStart].val + 8;
        if (val > AppRes.field[VTotal].val)
	    val = AppRes.field[VTotal].val;
        break;
     case VTotal:
	if (val < AppRes.field[VSyncEnd].val)
	   val = AppRes.field[VSyncEnd].val;
        break;
   }
   return val;
}

static void
ScrollCB (Widget w, XtPointer client, XtPointer call)
{
    float percent = *(float*) call;
    int ipercent = percent * 100;
    int fieldindex = (long) client;
    ScrollData* sdp = &AppRes.field[fieldindex];


    
    if (ipercent != sdp->lastpercent) {
        int tmp_val;
	char buf[6];

	tmp_val = AppRes.field[sdp->use].val;
	tmp_val += (int) (((float)sdp->range) * percent);

        sdp->val = isValid(tmp_val, fieldindex);
        
	sdp->lastpercent = ipercent;
	(void) sprintf (buf, "%5d", sdp->val);
	XtVaSetValues (sdp->textwidget, XtNlabel, buf, NULL);
        if (sdp->val != tmp_val) {
            int base;
            float percent;

            base = AppRes.field[sdp->use].val;
            percent = ((float)(sdp->val - base)) / ((float)sdp->range);
            /* This doesn't always work, why? */
            XawScrollbarSetThumb (sdp->scrollwidget, percent, 0.0);
	}
	if (fieldindex == HTotal || fieldindex == VTotal)
	    UpdateSyncRates(TRUE);
    }
}

static void
SwitchCB (Widget w, XtPointer client, XtPointer call)
{
    XF86VidModeLockModeSwitch(XtDisplay(w), DefaultScreen (XtDisplay (w)),
			      FALSE);
    XF86VidModeSwitchMode(XtDisplay(w), DefaultScreen (XtDisplay (w)),
			  (int)(long) client);
    XF86VidModeLockModeSwitch(XtDisplay(w), DefaultScreen (XtDisplay (w)),
			      TRUE);
    FetchCB(w, NULL, NULL);
}

static void
AddCallback (
    Widget w,
    String  callback_name,
    XtCallbackProc callback,
    XtPointer client_data)
{
    Widget src;

    XtVaGetValues (w, XtNtextSource, &src, NULL);
    XtAddCallback (src, callback_name, callback, client_data);
}

static void
CreateTyp (
    Widget form,
    fields findex,
    String w1name,
    String w2name,
    String w3name)
{
    Widget wids[3];
    char buf[10];

    wids[0] = XtCreateWidget (w1name, labelWidgetClass, form, NULL, 0);
    if (findex >= PixelClock && findex <= VSyncRate)
	(void) sprintf(buf, "%6.2f", (float)AppRes.field[findex].val / 1000.0);
    else
	(void) sprintf (buf, "%5d", AppRes.field[findex].val);
    wids[1] = XtVaCreateWidget (w2name, labelWidgetClass,
		form, XtNlabel, buf, NULL);
    if (w3name != NULL) {
	wids[2] = XtCreateWidget (w3name, scrollbarWidgetClass, form, NULL, 0);
	XtAddCallback (wids[2], XtNjumpProc, ScrollCB, (XtPointer) findex);
	XtManageChildren (wids, 3);
    } else {
	wids[2] = (Widget) NULL;
	XtManageChildren (wids, 2);
    }
    AppRes.field[findex].textwidget = wids[1];
    AppRes.field[findex].scrollwidget = wids[2];
}


static void
AckWarn (Widget w, XtPointer client, XtPointer call)
{
    XtPopdown((Widget) client);
    XtDestroyWidget((Widget) client);
}

static void
AckNoTune (Widget w, XtPointer client, XtPointer call)
{
    CleanUp(XtDisplay(w));
#if XtSpecificationRelease < 6
    exit (0);
#else
    XtAppSetExitFlag (XtWidgetToApplicationContext (w));
#endif
}

static void
displayWarning(Widget top)
{
    Widget w, popup, popupBox;
    int x, y;

    x =  DisplayWidth(XtDisplay (top),DefaultScreen (XtDisplay (top))) / 3;
    y =  DisplayHeight(XtDisplay (top),DefaultScreen (XtDisplay (top))) / 3;

    popup = XtVaCreatePopupShell("Warning", 
			    transientShellWidgetClass, top,
			    XtNtitle, "WARNING",
			    XtNx, x,
			    XtNy, y,
			    NULL);

    popupBox = XtVaCreateManagedWidget(
               "WarningBox",
               boxWidgetClass,
               popup,
               NULL);

    w = XtVaCreateManagedWidget( "WarnLabel",
                                     labelWidgetClass,
				     popupBox,
                                     NULL);

    w = XtVaCreateManagedWidget( "WarnOK",
                                     commandWidgetClass,
				     popupBox,
                                     NULL);

    XtAddCallback (w, XtNcallback, AckWarn, (XtPointer)popup);

    w = XtVaCreateManagedWidget( "WarnCancel",
                                     commandWidgetClass,
				     popupBox,
                                     NULL);
    XtAddCallback (w, XtNcallback, QuitCB, (XtPointer)NULL);

    XtPopup(popup, XtGrabExclusive);
    
}

static void
displayNoTune(Widget top)
{
    Widget w, popup, popupBox;

    popup = XtCreateWidget ("Notice", formWidgetClass, top, NULL, 0);
    popupBox = XtVaCreateManagedWidget(
               "WarningBox",
               boxWidgetClass,
               popup,
               NULL);

    w = XtVaCreateManagedWidget( "NoTuneLabel",
                                     labelWidgetClass,
				     popupBox,
                                     NULL);

    w = XtVaCreateManagedWidget( "NoTuneOK",
                                     commandWidgetClass,
				     popupBox,
                                     NULL);

    XtAddCallback (w, XtNcallback, AckNoTune, (XtPointer)popup);

    XtManageChild (popup);
}

#if 0
static void
s3Special(Widget top)
{
    Widget w, popup, form, invert_vclk_toggle, wids[6];
    char buf1[5] = {'\0',};
    int x, y;

    x =  DisplayWidth(XtDisplay (top),DefaultScreen (XtDisplay (top))) / 3;
    y =  DisplayHeight(XtDisplay (top),DefaultScreen (XtDisplay (top))) / 3;

    popup = XtVaCreatePopupShell("S3Adjust", 
			    transientShellWidgetClass, top,
			    XtNtitle, "S3Adjust",
			    XtNx, x,
			    XtNy, y,
			    NULL);

    form = XtVaCreateManagedWidget(
               "S3Box",
               formWidgetClass,
               popup,
               NULL);

    w = XtVaCreateManagedWidget( "S3Title",
                                     labelWidgetClass,
				     form,
                                     NULL);

    invert_vclk_toggle = XtVaCreateManagedWidget( "InvertVclk-toggle",
                                     toggleWidgetClass,
				     form,
                                     NULL);

    wids[0] = XtCreateWidget ("Blank1-label", labelWidgetClass,
		form, NULL, 0);
    wids[1] = XtVaCreateWidget ("Blank1-text", asciiTextWidgetClass,
		form, XtNstring, buf1, NULL);
    AddCallback (wids[1], XtNcallback, FlagsEditCB, (XtPointer) NULL);

    XtManageChildren (wids, 2);

    XtPopup(popup, XtGrabNone);
    
}
#endif



static void
CreateHierarchy(Widget top)
{
    char buf[5];
    Widget form, forms[14], s3form;
    Widget wids[10];
    Widget boxW,messageW, popdownW, w;   
    int i;
    int x, y;
    static String form_names[] = {
	"HDisplay-form",
	"HSyncStart-form",
	"HSyncEnd-form",
	"HTotal-form",
	"VDisplay-form",
	"VSyncStart-form",
	"VSyncEnd-form",
	"VTotal-form",
	"Flags-form",
	"Buttons-form",
	"PixelClock-form",
	"HSyncRate-form",
	"VSyncRate-form",
	"Buttons2-form",
	};

    form = XtCreateWidget ("form", formWidgetClass, top, NULL, 0);
    for (i = 0; i < 14; i++)
	forms[i] = XtCreateWidget (form_names[i], formWidgetClass, 
		form, NULL, 0);

    CreateTyp (forms[0], HDisplay, "HDisplay-label", "HDisplay-text", NULL);
    CreateTyp (forms[1], HSyncStart, "HSyncStart-label",
		"HSyncStart-text", "HSyncStart-scrollbar");
    CreateTyp (forms[2], HSyncEnd, "HSyncEnd-label", "HSyncEnd-text", 
		"HSyncEnd-scrollbar");
    CreateTyp (forms[3], HTotal, "HTotal-label", "HTotal-text", 
		"HTotal-scrollbar");

    w = XtVaCreateManagedWidget(
                                     "Left-button",
                                     commandWidgetClass,
                                     forms[3],
                                     NULL);
    XtAddCallback (w, XtNcallback, AdjustCB, (XtPointer)HSyncStart);
    w = XtVaCreateManagedWidget(
                                     "Right-button",
                                     commandWidgetClass,
                                     forms[3],
                                     NULL);
    XtAddCallback (w, XtNcallback, AdjustCB, (XtPointer)-HSyncStart);
    w=  XtVaCreateManagedWidget(
                                     "Wider-button",
                                     commandWidgetClass,
                                     forms[3],
                                     NULL);
    XtAddCallback (w, XtNcallback, AdjustCB, (XtPointer)-HTotal);
    w = XtVaCreateManagedWidget(
                                     "Narrower-button",
                                     commandWidgetClass,
                                     forms[3],
                                     NULL);
    XtAddCallback (w, XtNcallback, AdjustCB, (XtPointer)HTotal);
    CreateTyp (forms[4], VDisplay, "VDisplay-label", "VDisplay-text", NULL);
    CreateTyp (forms[5], VSyncStart, "VSyncStart-label",
		"VSyncStart-text", "VSyncStart-scrollbar");
    CreateTyp (forms[6], VSyncEnd, "VSyncEnd-label", "VSyncEnd-text", 
		"VSyncEnd-scrollbar");
    CreateTyp (forms[7], VTotal, "VTotal-label", "VTotal-text", 
		"VTotal-scrollbar");
    w = XtVaCreateManagedWidget(
                                     "Up-button",
                                     commandWidgetClass,
                                     forms[7],
                                     NULL);
    XtAddCallback (w, XtNcallback, AdjustCB, (XtPointer)VSyncStart);
    w = XtVaCreateManagedWidget(
                                     "Down-button",
                                     commandWidgetClass,
                                     forms[7],
                                     NULL);
    XtAddCallback (w, XtNcallback, AdjustCB, (XtPointer)-VSyncStart);   
    w=  XtVaCreateManagedWidget(
                                     "Shorter-button",
                                     commandWidgetClass,
                                     forms[7],
                                     NULL);
    XtAddCallback (w, XtNcallback, AdjustCB, (XtPointer)VTotal);   
    w = XtVaCreateManagedWidget(
                                     "Taller-button",
                                     commandWidgetClass,
                                     forms[7],
                                     NULL);
    XtAddCallback (w, XtNcallback, AdjustCB, (XtPointer)-VTotal);

    (void) sprintf (buf, "%04x", AppRes.field[Flags].val);
    wids[0] = XtCreateWidget ("Flags-label", labelWidgetClass,
		forms[8], NULL, 0);
    wids[1] = XtVaCreateWidget ("Flags-text", asciiTextWidgetClass,
		forms[8], XtNstring, buf, XtNtranslations, trans, NULL);
    AddCallback (wids[1], XtNcallback, FlagsEditCB, (XtPointer) Flags);
    XtManageChildren (wids, 2);
    AppRes.field[Flags].textwidget = wids[1];

    wids[0] = XtCreateWidget ("Quit-button", commandWidgetClass, 
		forms[9], NULL, 0);
    XtAddCallback (wids[0], XtNcallback, QuitCB, NULL);

    wids[1] = XtCreateWidget ("Apply-button", commandWidgetClass, 
		forms[9], NULL, 0);
    XtAddCallback (wids[1], XtNcallback, ApplyCB, NULL);

    wids[2] = XtCreateWidget ("AutoApply-toggle", toggleWidgetClass, 
		forms[9], NULL, 0);
    auto_apply_toggle = wids[2];

    wids[3] = XtCreateWidget ("Test-button", commandWidgetClass, 
		forms[9], NULL, 0);
    XtAddCallback (wids[3], XtNcallback, TestCB, NULL);

    wids[4] = XtCreateWidget ("Restore-button", commandWidgetClass, 
		forms[9], NULL, 0);
    XtAddCallback (wids[4], XtNcallback, RestoreCB, NULL);

    XtManageChildren (wids, 5);


    CreateTyp (forms[10], PixelClock, "PixelClock-label", "PixelClock-text",
	       NULL);
    CreateTyp (forms[11], HSyncRate, "HSyncRate-label", "HSyncRate-text",
	       NULL);
    CreateTyp (forms[12], VSyncRate, "VSyncRate-label", "VSyncRate-text",
	       NULL);

    wids[0] = XtCreateWidget ("Fetch-button", commandWidgetClass, 
		forms[13], NULL, 0);
    XtAddCallback (wids[0], XtNcallback, FetchCB, NULL);

    wids[1] = XtCreateWidget ("Show-button", commandWidgetClass, 
		forms[13], NULL, 0);
    XtAddCallback (wids[1], XtNcallback, ShowCB, NULL);

    wids[2] = XtCreateWidget ("Next-button", commandWidgetClass, 
		forms[13], NULL, 0);
    XtAddCallback (wids[2], XtNcallback, SwitchCB, (XtPointer)1);

    wids[3] = XtCreateWidget ("Prev-button", commandWidgetClass, 
		forms[13], NULL, 0);
    XtAddCallback (wids[3], XtNcallback, SwitchCB, (XtPointer)-1);

    XtManageChildren (wids, 4);

    XtManageChildren (forms, 14);

    if (S3Specials) {
	char buf[2] = "0";
	s3form = XtCreateWidget ("S3-form", formWidgetClass, 
		form, NULL, 0);
	wids[0] = XtVaCreateWidget("InvertVclk-toggle", toggleWidgetClass,
			s3form, XtNstate, AppRes.field[InvertVclk].val, NULL);
	XtAddCallback (wids[0], XtNcallback, ApplyIfAutoCB, NULL);
	AppRes.field[InvertVclk].textwidget = wids[0];
	wids[1] = XtVaCreateWidget("EarlySc-toggle", toggleWidgetClass,
			s3form, XtNstate, AppRes.field[EarlySc].val, NULL);
	XtAddCallback (wids[1], XtNcallback, ApplyIfAutoCB, NULL);
	AppRes.field[EarlySc].textwidget = wids[1];
	wids[2] = XtCreateWidget("Blank1-label", labelWidgetClass, s3form,
			NULL, 0);
	wids[3] = XtVaCreateWidget("Blank1Dec-button", commandWidgetClass,
				   s3form, NULL);
	XtAddCallback (wids[3], XtNcallback, ChangeBlankCB,
			(XtPointer)-BlankDelay1);
	(void) sprintf (buf, "%d", AppRes.field[BlankDelay1].val);
	wids[4] = XtVaCreateWidget("Blank1-text", asciiTextWidgetClass,
			s3form, XtNstring, buf, XtNtranslations, trans, NULL);
	AddCallback(wids[4], XtNcallback, BlankEditCB, (XPointer) BlankDelay1);
	AppRes.field[BlankDelay1].textwidget = wids[4];
	wids[5] = XtVaCreateWidget("Blank1Inc-button", commandWidgetClass,
				   s3form, NULL);
	XtAddCallback (wids[5], XtNcallback, ChangeBlankCB,
			(XtPointer)BlankDelay1);

	wids[6] = XtCreateWidget("Blank2-label", labelWidgetClass, s3form,
			NULL, 0);
	wids[7] = XtVaCreateWidget("Blank2Dec-button", commandWidgetClass,
				   s3form, NULL);
	XtAddCallback (wids[7], XtNcallback, ChangeBlankCB,
			(XtPointer)-BlankDelay2);
	(void) sprintf (buf, "%d", AppRes.field[BlankDelay2].val);
	wids[8] = XtVaCreateWidget("Blank2-text", asciiTextWidgetClass,
			s3form, XtNstring, buf, XtNtranslations, trans, NULL);
	AddCallback(wids[8], XtNcallback, BlankEditCB, (XPointer) BlankDelay2);
	AppRes.field[BlankDelay2].textwidget = wids[8];
	wids[9] = XtVaCreateWidget("Blank2Inc-button", commandWidgetClass,
				   s3form, NULL);
	XtAddCallback (wids[9], XtNcallback, ChangeBlankCB,
			(XtPointer)BlankDelay2);
	XtManageChildren (wids, 10);
	XtManageChild(s3form);
    }

    XtManageChild (form);

    SetScrollbars ();
    x = DisplayWidth(XtDisplay (top),DefaultScreen (XtDisplay (top))) / 2;
    y = DisplayHeight(XtDisplay (top),DefaultScreen (XtDisplay (top))) / 2;

    invalid_mode_popup = XtVaCreatePopupShell("invalidMode", 
			    transientShellWidgetClass, top,
			    XtNtitle, "Invalid Mode requested",
			    XtNx, x - 20,
			    XtNy, y - 40,
			    NULL);

    testing_popup = XtVaCreatePopupShell("testing", 
			    transientShellWidgetClass, top,
			    XtNtitle, "Testing_1_2_3",
			    XtNx, x - 20,
			    XtNy, y - 40,
			    NULL);
    boxW = XtVaCreateManagedWidget(
                                   "TestingBox",
                                   boxWidgetClass,
                                   testing_popup,
                                   NULL);

    w = XtVaCreateManagedWidget(
		   "testingMessage",
                   labelWidgetClass,
                   boxW,
                   NULL);

    w = XtVaCreateManagedWidget(
                               "Abort",
                                commandWidgetClass,
                                boxW,
                                NULL);

    XtAddCallback (w, XtNcallback, (XtCallbackProc) TestTOCB,
		  (XtPointer) NULL);

    boxW = XtVaCreateManagedWidget(
                                   "invalidBox",
                                   boxWidgetClass,
                                   invalid_mode_popup,
                                   NULL);
        
    messageW = XtVaCreateManagedWidget(
		   "ErrorMessage",
                   labelWidgetClass,
                   boxW,
                   NULL);

   popdownW = XtVaCreateManagedWidget(
                                     "AckError",
                                     commandWidgetClass,
                                     boxW,
                                     NULL);

   XtAddCallback (popdownW, XtNcallback, (XtCallbackProc)popdownInvalid, 
		  (XtPointer) invalid_mode_popup);
}

static void
QuitAction (Widget w, XEvent* e, String* vector, Cardinal* count)
{
    if ((e->type == ClientMessage
      && e->xclient.data.l[0] == (long) wm_delete_window)
	|| e->type == KeyPress)
	QuitCB(w, NULL, NULL);
}

static void
RestoreAction (Widget w, XEvent* e, String* vector, Cardinal* count)
{
    Boolean state;

    RestoreCB(w, NULL, NULL);
    XtVaGetValues(auto_apply_toggle, XtNstate, &state, NULL);
    if (!state)
	ApplyCB (w, NULL, NULL);
}


static void
ShowAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
    ShowCB(w, NULL, NULL);
}

static void
MoveLeftAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
    AdjustCB(w, (XtPointer)HSyncStart, NULL);
}

static void
MoveRightAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
    AdjustCB(w, (XtPointer)-HSyncStart, NULL);
}

static void
NarrowerAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
    AdjustCB(w, (XtPointer)HTotal, NULL);
}

static void
WiderAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
    AdjustCB(w, (XtPointer)-HTotal, NULL);
}

static void
MoveUpAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
    AdjustCB(w, (XtPointer)VSyncStart, NULL);
}

static void
MoveDownAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
    AdjustCB(w, (XtPointer)-VSyncStart, NULL);
}

static void
TallerAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
    AdjustCB(w, (XtPointer)-VTotal, NULL);
}

static void
ShorterAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
    AdjustCB(w, (XtPointer)VTotal, NULL);
}

static void
NextModeAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
	SwitchCB(w, (XPointer) 1, NULL);
}

static void
PrevModeAction(Widget w, XEvent* e, String* vector, Cardinal* count)
{
	SwitchCB(w, (XPointer) -1, NULL);
}



static void
usage(void)
{
    fprintf(stderr, "Usage: xvidtune [option]\n");
    fprintf(stderr, "    where option is one of:\n");
    fprintf(stderr, "        -show                             Print current modeline to stdout\n");
    fprintf(stderr, "        -next                             Switch to next video mode\n");
    fprintf(stderr, "        -prev                             Switch to previous video mode\n");
    fprintf(stderr, "        -unlock                           Enable mode switch hot-keys\n");
    fprintf(stderr, "        -timeout [seconds]                Set testmode timeout in seconds,\n");
    exit(1);
}


int
main (int argc, char** argv)
{
    Widget top;
    XtAppContext app;
    Display* dpy;
    Bool modeSettable = TRUE;
    
    static XtActionsRec actions[] = { { "xvidtune-quit", QuitAction },
				      { "xvidtune-restore", RestoreAction },
				      { "xvidtune-show", ShowAction },
				      {	"xvidtune-moveleft", MoveLeftAction },
				      {	"xvidtune-moveright", MoveRightAction },
				      {	"xvidtune-wider", WiderAction },
				      {	"xvidtune-narrower", NarrowerAction },
				      {	"xvidtune-moveup", MoveUpAction },
				      {	"xvidtune-movedown", MoveDownAction },
				      {	"xvidtune-taller", TallerAction },
				      {	"xvidtune-shorter", ShorterAction },
				      {	"xvidtune-nextmode", NextModeAction },
				      {	"xvidtune-prevmode", PrevModeAction } };

    Top = top = XtVaOpenApplication (&app, "Xvidtune", NULL, 0, &argc, argv,
		NULL, applicationShellWidgetClass, 
		XtNmappedWhenManaged, False, NULL);

    XtGetApplicationResources (top, (XtPointer)&AppRes,
		Resources, XtNumber(Resources),
		NULL, 0);

    if (!AppRes.ad_installed) {
	fprintf(stderr, "Please install the program before using\n");
	return 3;
    }

    if (!XF86VidModeQueryVersion(XtDisplay (top), &MajorVersion, &MinorVersion)) {
	fprintf(stderr, "Unable to query video extension version\n");
	return 2;
    }

    if (!XF86VidModeQueryExtension(XtDisplay (top), &EventBase, &ErrorBase)) {
	fprintf(stderr, "Unable to query video extension information\n");
	return 2;
    }

    /* Fail if the extension version in the server is too old */
    if (MajorVersion < MINMAJOR || 
	(MajorVersion == MINMAJOR && MinorVersion < MINMINOR)) {
	fprintf(stderr,
		"Xserver is running an old XFree86-VidModeExtension version"
		" (%d.%d)\n", MajorVersion, MinorVersion);
	fprintf(stderr, "Minimum required version is %d.%d\n",
		MINMAJOR, MINMINOR);
	exit(2);
    }
 
    /* This should probably be done differently */

    if(  argc == 3  ) { /* this can only be the timeout case */
        if(  (!strcmp(argv[1], "-timeout"))  ) {
            TestTimeout = ((unsigned long) atol( argv[2] )) * 1000L;
	} 
	else
	    usage();
    }

    if (argc > 1) {
	int i = 0;
        
	if (argc != 2)
		usage();
	if (!strcmp(argv[1], "-show")) {
	  if (!GetModeLine(XtDisplay (top), DefaultScreen (XtDisplay (top)))) {
	    fprintf(stderr, "Unable to get mode info\n");
	    CleanUp(XtDisplay (top));
	    return 2;
	  }
	  ShowCB(top, NULL, NULL);
	  return 0;
	} else if (!strcmp(argv[1], "-next"))
	    i = 1;
	else if (!strcmp(argv[1], "-prev"))
	    i = -1;
	else if (!strcmp(argv[1], "-unlock")) {
	    CleanUp(XtDisplay (top));
	    XSync(XtDisplay (top), True);
	    return 0;
	} else
		usage();
	if (i != 0) {
	    XF86VidModeSwitchMode(XtDisplay (top),
				  DefaultScreen (XtDisplay (top)), i);
	    XSync(XtDisplay (top), True);
	    return 0;
	}
    }
    if (!GetMonitor(XtDisplay (top), DefaultScreen (XtDisplay (top)))) {
	fprintf(stderr, "Unable to query monitor info\n");
	return 2;
    }

    if (!XF86VidModeLockModeSwitch(XtDisplay (top),
				   DefaultScreen (XtDisplay (top)), TRUE)) {
	fprintf(stderr, "Failed to disable mode-switch hot-keys\n");
	return 2;
    }

    signal(SIGINT, CatchSig);
    signal(SIGQUIT, CatchSig);
    signal(SIGTERM, CatchSig);
    signal(SIGHUP, CatchSig);
    sigId = XtAppAddSignal(app, CatchXtSig, NULL);

    if (!GetModeLine(XtDisplay (top), DefaultScreen (XtDisplay (top)))) {
	fprintf(stderr, "Unable to get mode info\n");
	CleanUp(XtDisplay (top));
	return 2;
    }

    xtErrorfunc = XSetErrorHandler(vidmodeError); 

    trans = XtParseTranslationTable ("\
	<Key>0: insert-char()\n<Key>1: insert-char()\n\
	<Key>2: insert-char()\n<Key>3: insert-char()\n\
	<Key>4: insert-char()\n<Key>5: insert-char()\n\
	<Key>6: insert-char()\n<Key>7: insert-char()\n\
	<Key>8: insert-char()\n<Key>9: insert-char()\n\
	<Key>a: insert-char()\n<Key>b: insert-char()\n\
	<Key>c: insert-char()\n<Key>d: insert-char()\n\
	<Key>e: insert-char()\n<Key>f: insert-char()\n\
	<Key>+: insert-char()\n<Key>-: insert-char()\n\
	<Key>r: xvidtune-restore()\n<Key>q: xvidtune-quit()\n\
	<Key>BackSpace: delete-previous-character()\n\
	<Key>Right: forward-character()\n<Key>KP_Right: forward-character()\n\
	<Key>Left: backward-character()\n<Key>KP_Left: backward-character()\n\
	<Key>Delete: delete-previous-character()\n\
	<Key>KP_Delete: delete-previous-character()\n\
	<EnterWindow>: enter-window()\n<LeaveWindow>: leave-window()\n\
	<FocusIn>: focus-in()\n<FocusOut>: focus-out()\n\
	<Btn1Down>: select-start()\n");

    if (!ModeSettable()) {
	printf("Video are not settable on this chip\n");
	displayNoTune(top);
	modeSettable = FALSE;
    } else
	CreateHierarchy (top);
    

    XtAppAddActions (app, actions, XtNumber(actions));

    XtOverrideTranslations (top,
		XtParseTranslationTable ("<Message>WM_PROTOCOLS: xvidtune-quit()"));

    XtRealizeWidget (top);

    dpy = XtDisplay(top);

    wm_delete_window = XInternAtom (dpy, "WM_DELETE_WINDOW", False);

    (void) XSetWMProtocols (dpy, XtWindow (top), &wm_delete_window, 1);

    XtMapWidget (top);

    if (modeSettable)
	displayWarning(top);

    /* really we should run our own event dispatching here until the
     * warning has been read...
     */
    XtAppMainLoop (app);

    return 0;
}
