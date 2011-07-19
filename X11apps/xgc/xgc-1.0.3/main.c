/*

Copyright (c) 1991  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/

/* xgc
**
** main.c
**
** Contains the bare minimum necessary to oversee the whole operation.
*/

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Shell.h>
#include <stdio.h>
#include <stdlib.h>

#include "xgc.h"
#define DEFINE_TILE
#include "tile"

static void fill_up_commandform(Widget);
static void quit(void);
static void quitAction(Widget, XEvent *, String *, Cardinal *);
static void clear_test_window(void);
static void clear_result_window(void);
static void set_foreground_and_background(void);

/* The three columns in the XgcData arrays are:
**   name: the name of the toggle button
**   text: the corresponding text in the xgc syntax
**   code: the integer that the text corresponds to, for sending stuff
**         to X calls, etc.
*/

static XgcData FunctionData[NUM_FUNCTIONS] = {
  {"clear",        "clear",        GXclear},
  {"and",          "and",          GXand},
  {"andReverse",   "andReverse",   GXandReverse},
  {"copy",         "copy",         GXcopy},
  {"andInverted",  "andInverted",  GXandInverted},
  {"noop",         "noop",         GXnoop},
  {"xor",          "xor",          GXxor},
  {"or",           "or",           GXor},
  {"nor",          "nor",          GXnor},
  {"equiv",        "equiv",        GXequiv},
  {"invert",       "invert",       GXinvert},
  {"orReverse",    "orReverse",    GXorReverse},
  {"copyInverted", "copyInverted", GXcopyInverted},
  {"orInverted",   "orInverted",   GXorInverted},
  {"nand",         "nand",         GXnand},
  {"set",          "set",          GXset}
}; 

/* The two rows in the XgcStuff structure are:
**   name of label, xgc syntax text, # of toggles, # of columns of toggles
**     (0 columns means 1 row, as many columns as necessary)
**   appropriate XgcData
*/

XgcStuff FunctionStuff = {
  {"Function","function",NUM_FUNCTIONS,4},
  FunctionData
};

static XgcData TestData[NUM_TESTS] = {
  {"Copy Area",          "CopyArea",      CopyArea},
  {"Copy Plane",         "CopyPlane",     CopyPlane},
  {"Points",             "PolyPoint",     PolyPoint},
  {"Lines",              "PolyLine",      PolyLine},
  {"Segments",           "PolySegment",   PolySegment},
  {"Rectangles",         "PolyRectangle", PolyRectangle},
  {"Arcs",               "PolyArc",       PolyArc},
  {"(Filled Polygons)",  "FillPolygon",   FillPolygon},
  {"Filled Rectangles",  "PolyFillRect",  PolyFillRect},
  {"Filled Arcs",        "PolyFillArc",   PolyFillArc},
  {"Put Image",          "PutImage",      PutImage},
  {"(Get Image)",        "GetImage",      GetImage},
  {"Text 8",             "PolyText8",     PolyText8},
  {"Image Text 8",       "ImageText8",    ImageText8},
  {"Text 16",            "PolyText16",    PolyText16},
  {"Image Text 16",      "ImageText16",   ImageText16}
};

XgcStuff TestStuff = {
  {"Test","test",NUM_TESTS,2},
  TestData
};

static XgcData LinestyleData[NUM_LINESTYLES] = {
  {"Solid",      "Solid",       LineSolid},
  {"OnOffDash",  "OnOffDash",   LineOnOffDash},
  {"DoubleDash", "DoubleDash",  LineDoubleDash}
};

XgcStuff LinestyleStuff = {
  {"LineStyle","linestyle",NUM_LINESTYLES,0},
  LinestyleData
};

static XgcData CapstyleData[NUM_CAPSTYLES] = {
  {"NotLast",    "NotLast",     CapNotLast},
  {"Butt",       "Butt",        CapButt},
  {"Round",      "Round",       CapRound},
  {"Projecting", "Projecting",  CapProjecting}
};

XgcStuff CapstyleStuff = {
  {"CapStyle","capstyle",NUM_CAPSTYLES,2},
  CapstyleData
};

static XgcData JoinstyleData[NUM_JOINSTYLES] = {
  {"Miter",   "Miter",   JoinMiter},
  {"Round",   "Round",   JoinRound},
  {"Bevel",   "Bevel",   JoinBevel}
};

XgcStuff JoinstyleStuff = {
  {"JoinStyle","joinstyle",NUM_JOINSTYLES,0},
  JoinstyleData
};

static XgcData FillstyleData[NUM_FILLSTYLES] = {
  {"Solid",          "Solid",          FillSolid},
  {"Tiled",          "Tiled",          FillTiled},
  {"Stippled",       "Stippled",       FillStippled},
  {"OpaqueStippled", "OpaqueStippled", FillOpaqueStippled}
};

XgcStuff FillstyleStuff = {
  {"FillStyle","fillstyle",NUM_FILLSTYLES,2},
  FillstyleData
};

static XgcData FillruleData[NUM_FILLRULES] = {
  {"EvenOdd",  "EvenOdd",  EvenOddRule},
  {"Winding",  "Winding",  WindingRule}
};

XgcStuff FillruleStuff = {
  {"FillRule","fillrule",NUM_FILLRULES,0},
  FillruleData
};

static XgcData ArcmodeData[NUM_ARCMODES] = {
  {"Chord",    "Chord",    ArcChord},
 {"PieSlice", "PieSlice", ArcPieSlice}
};

XgcStuff ArcmodeStuff = {
  {"ArcMode","arcmode",NUM_ARCMODES,0},
  ArcmodeData
};

/* Pointers to all the Xgcstuffs so we can run them through a loop */

static XgcStuff *Everything[8] = {
  &FunctionStuff,
  &LinestyleStuff,
  &CapstyleStuff,
  &JoinstyleStuff,
  &FillstyleStuff,
  &FillruleStuff,
  &ArcmodeStuff,
  &TestStuff
};

#ifdef notdef
int fildes[2];			/* for pipe */
FILE *outend;
#endif

XStuff X;			/* GC stuff plus some global variables */
Boolean recording = FALSE;	/* Whether we're recording into a file */
XtAppContext appcontext;	/* To make Xt happy */
static Atom wm_delete_window;
static XtActionsRec actions[] = {
    {"quit",	quitAction}
};

static Widget bigdaddy;		/* the top level widget */
       Widget topform;		/* form surrounding the whole thing */
       Widget GCform;		/* form in which you choose the GC */
static Widget Testform;		/* form in which you choose the test */
       Widget testchoiceform;   /* form inside that */
  ChoiceDesc *testchoicedesc;	/* record of what widgets are in the
				   test choice form */
static Widget commandform;	/* form with run, quit, clear, etc. */
       Widget test;		/* where the test is run */
       Widget result;           /* where the results are displayed */
static Widget runbutton;	/* command for running */
static Widget clearbutton;	/* command for clearing the test window */
       Widget recordbutton;	/* start/stop recording */
static Widget playbackbutton;	/* playback from file */
static Widget keyinputbutton;	/* start reading from keyboard */
static Widget GCchoices[NUMCHOICES]; /* all the forms that contain stuff
				        for changing GC's*/
  ChoiceDesc *GCdescs[NUMCHOICES]; /* record of the widgets inside
				      the choice widgets */
       Widget planemaskchoice;	/* form for choosing the plane mask */
       Widget dashlistchoice;	/* form for choosing the dash list */
static Widget linewidthchoice;	/* form for choosing line width */
       Widget linewidthtext;	/* text widget within that */
static Widget fontchoice;	/* form for choosing the font */
       Widget fonttext;		/* text widget within that */
static Widget foregroundchoice;	/* form for choosing foreground */
       Widget foregroundtext;	/* text widget within that */
static Widget backgroundchoice;	/* form for choosing background */
       Widget backgroundtext;	/* text widget within that */
static Widget percentchoice;	/* form for choosing percentage of test */

/* main(argc.argv)
** ---------------
** Initializes the toolkit, initializes data, puts up the widgets,
** starts the event loop.
*/

int
main(int argc, char *argv[])
{
  static Arg shellargs[] = {
    {XtNinput, 	      (XtArgVal) True}
  };

  static Arg testformargs[] = {	
    {XtNfromVert,     (XtArgVal) NULL} /* put it under GCform */
  };

  static Arg commandformargs[] = {
    {XtNfromVert,    (XtArgVal) NULL}, /* put it under GCform */
    {XtNfromHoriz,   (XtArgVal) NULL}  /* and to the right of Testform */
  };

  static Arg testargs[] = {
    {XtNheight,     (XtArgVal) 400},
    {XtNwidth,      (XtArgVal) 400},
    {XtNfromHoriz,  (XtArgVal) NULL} /* put it to the right of GCform */
  };

  static Arg resultargs[] = {
    {XtNheight,     (XtArgVal) 50},
    {XtNwidth,      (XtArgVal) 400},
    {XtNfromHoriz,  (XtArgVal) NULL}, /* put it to the right of GCform */
    {XtNfromVert,   (XtArgVal) NULL} /* and under test */
  };

  static Arg gcchoiceargs[] = {
    {XtNfromVert,    (XtArgVal) NULL}, /* put it under the one above it */
    {XtNfromHoriz,   (XtArgVal) NULL}, /* and next to that one */
    {XtNborderWidth, (XtArgVal) 0}     /* no ugly borders */
  };

  static Arg testchoiceargs[] = {
    {XtNborderWidth, (XtArgVal) 0}
  };

  int i;			/* counter */

  /* Open the pipe */

#ifdef notdef
  pipe(fildes);
  outend = fdopen(fildes[0],"r");
#endif

  /* Initialize toolkit stuff */

  XtSetLanguageProc(NULL, (XtLanguageProc) NULL, NULL);

  bigdaddy = XtAppInitialize(&appcontext, "Xgc", (XrmOptionDescList) NULL,
			     (Cardinal) 0, &argc, argv, (String *) NULL,
			     shellargs, XtNumber(shellargs));
  X.dpy = XtDisplay(bigdaddy);
  XtAppAddActions(appcontext, actions, XtNumber(actions));
  XtOverrideTranslations
      (bigdaddy, XtParseTranslationTable("<Message>WM_PROTOCOLS: quit()"));

  /* Initialize GC stuff */

  X.scr = DefaultScreenOfDisplay(X.dpy);
  X.gc = XCreateGC(X.dpy,RootWindowOfScreen(X.scr),0,(XGCValues *) NULL);
  X.miscgc = XCreateGC(X.dpy,RootWindowOfScreen(X.scr),0,(XGCValues *) NULL);

  /* Find out what the foreground & background are, and update the GC
  ** accordingly */

  set_foreground_and_background();

  topform = XtCreateManagedWidget("topform",formWidgetClass,bigdaddy,
				  NULL,0);

  GCform = XtCreateManagedWidget("GCform",formWidgetClass,topform,
				NULL,0);

  /* create all the GCchoices forms */

  for (i=0;i<NUMCHOICES;++i) {
    if (i==0)			/* on top */
      gcchoiceargs[0].value = (XtArgVal) NULL;
    else			/* under the last one */
      gcchoiceargs[0].value = (XtArgVal) GCchoices[i-1];

    GCchoices[i] = XtCreateManagedWidget(Everything[i]->choice.text,
					 formWidgetClass,GCform,
					 gcchoiceargs,XtNumber(gcchoiceargs));

    /* now fill up that form */
    GCdescs[i] = create_choice(GCchoices[i],Everything[i]);
  }

  /* put the planemask choice under the bottom GC choice */
  gcchoiceargs[0].value = (XtArgVal) GCchoices[NUMCHOICES-1];
  planemaskchoice = XtCreateManagedWidget("planemask",formWidgetClass,GCform,
				  gcchoiceargs,XtNumber(gcchoiceargs));
  /* fill it up */
  create_planemask_choice(planemaskchoice);

  /* put the dashlist choice under the planemask choice */
  gcchoiceargs[0].value = (XtArgVal) planemaskchoice;
  dashlistchoice = XtCreateManagedWidget("dashlist",formWidgetClass,GCform,
				  gcchoiceargs,XtNumber(gcchoiceargs));
  /* fill it up */
  create_dashlist_choice(dashlistchoice);

  /* put the linewidth choice under the dashlist choice */
  gcchoiceargs[0].value = (XtArgVal) dashlistchoice;
  linewidthchoice = XtCreateManagedWidget("linewidth",formWidgetClass,GCform,
				  gcchoiceargs,XtNumber(gcchoiceargs));
  /* fill it up */
  linewidthtext = create_text_choice(linewidthchoice,TLineWidth,2,30);

  /* put the font choice under the linewidth choice */
  gcchoiceargs[0].value = (XtArgVal) linewidthchoice;
  fontchoice = XtCreateManagedWidget("font",formWidgetClass,GCform,
				     gcchoiceargs,XtNumber(gcchoiceargs));
  /* fill it up */
  fonttext = create_text_choice(fontchoice,TFont,80,300);

  gcchoiceargs[0].value = (XtArgVal) fontchoice;
  foregroundchoice = XtCreateManagedWidget("foreground",formWidgetClass,GCform,
				   gcchoiceargs,XtNumber(gcchoiceargs));
  foregroundtext = create_text_choice(foregroundchoice,TForeground,9,50);
  /* FIXME 9 characters may not be the proper choice; really it
   * should understand a more proper pixel specification... */

  gcchoiceargs[1].value = (XtArgVal) foregroundchoice;
  backgroundchoice = XtCreateManagedWidget("background",formWidgetClass,GCform,
				   gcchoiceargs,XtNumber(gcchoiceargs));
  backgroundtext = create_text_choice(backgroundchoice,TBackground,9,50);
  
  gcchoiceargs[1].value = (XtArgVal) NULL;
  gcchoiceargs[0].value = (XtArgVal) foregroundchoice;
  percentchoice = XtCreateManagedWidget("testpercent",formWidgetClass,GCform,
				 gcchoiceargs,XtNumber(gcchoiceargs));
  X.percent = 1.0;
  create_testfrac_choice(percentchoice);

  /* make all the labels inside the choices line up nicely */
  line_up_labels(GCdescs,(int) XtNumber(GCdescs));

  /* put the test form under the GC form */
  testformargs[0].value = (XtArgVal) GCform;
  Testform = XtCreateManagedWidget("Testform",formWidgetClass,topform,
				   testformargs,XtNumber(testformargs));
  
  testchoiceform = XtCreateManagedWidget("testchoiceform",formWidgetClass,
			     Testform,testchoiceargs,XtNumber(testchoiceargs));
  testchoicedesc = create_choice(testchoiceform,Everything[CTest]);

  commandformargs[0].value = (XtArgVal) GCform;
  commandformargs[1].value = (XtArgVal) Testform;
  commandform = XtCreateManagedWidget("commandform",formWidgetClass,topform,
			      commandformargs,XtNumber(commandformargs));

  /* Put the appropriate command buttons in the command form */

  fill_up_commandform(commandform);

  testargs[2].value = (XtArgVal) GCform;    /* to the right of */
  test = XtCreateManagedWidget("test",widgetClass,topform,
			       testargs,XtNumber(testargs));

  resultargs[2].value = (XtArgVal) GCform; /* to the right of */
  resultargs[3].value = (XtArgVal) test; /* under */
  result = XtCreateManagedWidget("result",asciiTextWidgetClass,topform,
				 resultargs,XtNumber(resultargs));

  /* Now realize all the widgets */

  XtRealizeWidget(bigdaddy);

  /* Now do things we couldn't do until we had a window available */

  X.win = XtWindow(test);
  X.tile = XCreatePixmap(X.dpy,X.win,tile_width,tile_height,
			 DefaultDepthOfScreen(X.scr));

  X.tile = XCreatePixmapFromBitmapData(X.dpy,X.win,
				       (char *)tile_bits,tile_width,
				       tile_height,Black,White,
				       DefaultDepthOfScreen(X.scr));
  X.stipple = XCreateBitmapFromData(X.dpy,X.win,(char *)tile_bits,tile_width,
				    tile_height);

  XSetStipple(X.dpy,X.gc,X.stipple);
  XSetStipple(X.dpy,X.miscgc,X.stipple);

  GC_change_foreground(X.foreground,TRUE);
  GC_change_background(X.background,TRUE);

  wm_delete_window = XInternAtom(X.dpy, "WM_DELETE_WINDOW", False);
  (void) XSetWMProtocols(X.dpy, XtWindow(bigdaddy), &wm_delete_window, 1);

  /* Act like the user picked the first choice in each group */

  choose_defaults(GCdescs,(int)XtNumber(GCdescs));
  choose_defaults(&testchoicedesc,1);
  
  /* Loop forever, dealing with events */

  XtAppMainLoop(appcontext);

  return 0;
}

/* fill_up_commandform(w)
** ----------------------
** Put the appropriate command buttons in the command form (w).
*/

static void
fill_up_commandform(Widget w)
{
  static XtCallbackRec runcallbacklist[] = {
    {(XtCallbackProc) run_test,  NULL},
    {NULL,                       NULL}
  };

  static XtCallbackRec quitcallbacklist[] = {
    {(XtCallbackProc) quit,      NULL},
    {NULL,                       NULL}
  };

  static XtCallbackRec clearcallbacklist[] = {
    {(XtCallbackProc) clear_test_window,    NULL},
    {(XtCallbackProc) clear_result_window,  NULL},
    {NULL,                                  NULL}
  };

  static XtCallbackRec playbackcallbacklist[] = {
    {(XtCallbackProc) start_playback,       NULL},
    {NULL,                                  NULL}
  };

  static XtCallbackRec keyinputcallbacklist[] = {
    {(XtCallbackProc) read_from_keyboard,   NULL},
    {NULL,                                  NULL}
  };

  static XtCallbackRec recordcallbacklist[] = {
    {(XtCallbackProc) toggle_recordbutton,  NULL},
    {NULL,                                  NULL}
  };

  static Arg runargs[] = {
    {XtNcallback,    (XtArgVal) NULL}
  };

  static Arg clearargs[] = {
    {XtNcallback,    (XtArgVal) NULL},
    {XtNfromVert,    (XtArgVal) NULL}, /* put it under runbutton */
    {XtNvertDistance,(XtArgVal) 10}
  };

  static Arg recordargs[] = {
    {XtNcallback,    (XtArgVal) NULL},
    {XtNfromVert,    (XtArgVal) NULL}, /* put it under clearbutton */
    {XtNvertDistance,(XtArgVal) 10},
    {XtNresizable,   (XtArgVal) True} /* so we can change the name */
  };

  static Arg playbackargs[] = {
    {XtNcallback,    (XtArgVal) NULL},
    {XtNfromVert,    (XtArgVal) NULL} /* put it under recordbutton */
  };

  static Arg keyinputargs[] = {
    {XtNcallback,     (XtArgVal) NULL},
    {XtNfromVert,    (XtArgVal) NULL} /* put it under playbackbutton */
  };

  static Arg quitargs[] = {
    {XtNcallback,    (XtArgVal) NULL},
    {XtNfromVert,    (XtArgVal) NULL}, /* put it under keyinputbutton */
    {XtNvertDistance,(XtArgVal) 10}
  };

  runargs[0].value = (XtArgVal) runcallbacklist;
  runbutton = XtCreateManagedWidget("Run",commandWidgetClass,
			      w,runargs,XtNumber(runargs));

  clearargs[0].value = (XtArgVal) clearcallbacklist;
  clearargs[1].value = (XtArgVal) runbutton; /* under */
  clearbutton = XtCreateManagedWidget("Clear window",commandWidgetClass,
         		      w,clearargs,XtNumber(clearargs));

  recordargs[0].value = (XtArgVal) recordcallbacklist;
  recordargs[1].value = (XtArgVal) clearbutton;	/* under */
  recordbutton = XtCreateManagedWidget("Record",commandWidgetClass,
			      w,recordargs,XtNumber(recordargs));

  playbackargs[0].value = (XtArgVal) playbackcallbacklist;
  playbackargs[1].value = (XtArgVal) recordbutton; /* under */
  playbackbutton = XtCreateManagedWidget("Playback",commandWidgetClass,
			      w,playbackargs,XtNumber(playbackargs));

  keyinputargs[0].value = (XtArgVal) keyinputcallbacklist;
  keyinputargs[1].value = (XtArgVal) playbackbutton;
  keyinputbutton = XtCreateManagedWidget("Read Input",commandWidgetClass,
			      w,keyinputargs,XtNumber(keyinputargs));

  quitargs[0].value = (XtArgVal) quitcallbacklist;
  quitargs[1].value = (XtArgVal) keyinputbutton; /* under */
  (void) XtCreateManagedWidget("Quit",commandWidgetClass,
			       w,quitargs,XtNumber(quitargs));
    
}    
/* quit()
** ------
** Leave the program nicely.
*/

static void
quit(void)
{
  close_file_if_recording();
  exit(0);
}

static void quitAction(Widget w, XEvent *e, String *p, Cardinal *n)
{
    if (e->type == ClientMessage && e->xclient.data.l[0] != wm_delete_window)
	XBell(XtDisplay(w), 0);
    else 
	quit();
}

/* clear_test_window()
** -------------------
** Clear the test window.
*/

static void
clear_test_window(void)
{
  XClearWindow(X.dpy,XtWindow(test));
}

/* clear_result_window()
** ---------------------
** Clear the result window.
*/

static void
clear_result_window(void)
{
  set_text(result, "");
}

/* set_foreground_and_background()
** -------------------------------
** Finds the user-specified foreground and background by querying
** the resource manager, and sets state accordingly.  Also specifies
** the initial font for text tests.
*/

static void
set_foreground_and_background(void)
{
  X.gcv.foreground = X.foreground = 0;
  X.gcv.background = X.background = 0xffffffff;

  X.fontname = "6x10";
  GC_change_font(X.fontname,FALSE);
}
