/*
 * $Xorg: Tekproc.c,v 1.5 2001/02/09 02:06:02 xorgcvs Exp $
 *
 * Warning, there be crufty dragons here.
 */
/* $XFree86: xc/programs/xterm/Tekproc.c,v 3.40 2003/02/25 23:36:55 dickey Exp $ */

/*

Copyright 2001-2002,2003 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
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
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

Copyright 1988  The Open Group

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

 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* Tekproc.c */

#include <xterm.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/CharSet.h>

#if OPT_TOOLBAR

#if defined(HAVE_LIB_XAW)
#include <X11/Xaw/Form.h>
#elif defined(HAVE_LIB_XAW3D)
#include <X11/Xaw3d/Form.h>
#elif defined(HAVE_LIB_NEXTAW)
#include <X11/neXtaw/Form.h>
#endif

#endif /* OPT_TOOLBAR */

#include <stdio.h>
#include <ctype.h>
#include <signal.h>

#include <Tekparse.h>
#include <data.h>
#include <error.h>
#include <menu.h>

#define DefaultGCID XGContextFromGC(DefaultGC(screen->display, DefaultScreen(screen->display)))

/* Tek defines */

#define	DOTDASHEDLINE	2
#define	DOTTEDLINE	1
#define	EAST		01
#define	LINEMASK	07
#define	LONGDASHEDLINE	4
#define	MARGIN1		0
#define	MARGIN2		1
#define MAX_PTS		150
#define MAX_VTX		300
#define	NORTH		04
#define	PENDOWN		1
#define	PENUP		0
#define	SHORTDASHEDLINE	3
#define	SOLIDLINE	0
#define	SOUTH		010
#define	TEKBOTTOMPAD	23
#define	TEKDEFHEIGHT	565
#define	TEKDEFWIDTH	750
#define	TEKHEIGHT	3072
#define	TEKHOME		((TekChar[screen->page.fontsize].nlines - 1)\
			 * TekChar[screen->page.fontsize].vsize)
#define	TEKMINHEIGHT	452
#define	TEKMINWIDTH	600
#define	TEKTOPPAD	34
#define	TEKWIDTH	4096
#define	WEST		02

#define	TekMove(x,y)	screen->cur_X = x; screen->cur_Y = y
#define	input()		Tinput()
#define	unput(c)	*Tpushback++ = c

/* *INDENT-OFF* */
static struct Tek_Char {
    int hsize;			/* in Tek units */
    int vsize;			/* in Tek units */
    int charsperline;
    int nlines;
} TekChar[TEKNUMFONTS] = {
    {56, 88, 74, 35},		/* large */
    {51, 82, 81, 38},		/* #2 */
    {34, 53, 121, 58},		/* #3 */
    {31, 48, 133, 64},		/* small */
};
/* *INDENT-ON* */

static Cursor GINcursor;
static XSegment *line_pt;
static int nplot;
static TekLink Tek0;
static jmp_buf Tekjump;
static TekLink *TekRecord;
static XSegment *Tline;

static Const int *curstate = Talptable;
static Const int *Tparsestate = Talptable;

static char defaultTranslations[] = "\
                ~Meta<KeyPress>: insert-seven-bit() \n\
                 Meta<KeyPress>: insert-eight-bit() \n\
               !Ctrl <Btn1Down>: popup-menu(mainMenu) \n\
          !Lock Ctrl <Btn1Down>: popup-menu(mainMenu) \n\
!Lock Ctrl @Num_Lock <Btn1Down>: popup-menu(mainMenu) \n\
     !Ctrl @Num_Lock <Btn1Down>: popup-menu(mainMenu) \n\
               !Ctrl <Btn2Down>: popup-menu(tekMenu) \n\
          !Lock Ctrl <Btn2Down>: popup-menu(tekMenu) \n\
!Lock Ctrl @Num_Lock <Btn2Down>: popup-menu(tekMenu) \n\
     !Ctrl @Num_Lock <Btn2Down>: popup-menu(tekMenu) \n\
          Shift ~Meta<Btn1Down>: gin-press(L) \n\
                ~Meta<Btn1Down>: gin-press(l) \n\
          Shift ~Meta<Btn2Down>: gin-press(M) \n\
                ~Meta<Btn2Down>: gin-press(m) \n\
          Shift ~Meta<Btn3Down>: gin-press(R) \n\
                ~Meta<Btn3Down>: gin-press(r)";
/* *INDENT-OFF* */
static XtActionsRec actionsList[] = {
    { "string",	HandleStringEvent },
    { "insert",	HandleKeyPressed },	/* alias for insert-seven-bit */
    { "insert-seven-bit",	HandleKeyPressed },
    { "insert-eight-bit",	HandleEightBitKeyPressed },
    { "gin-press",		HandleGINInput },
    { "secure",			HandleSecure },
    { "create-menu",		HandleCreateMenu },
    { "popup-menu",		HandlePopupMenu },
    /* menu actions */
    { "allow-send-events",	HandleAllowSends },
    { "set-visual-bell",	HandleSetVisualBell },
#ifdef ALLOWLOGGING
    { "set-logging",		HandleLogging },
#endif
    { "redraw",			HandleRedraw },
    { "send-signal",		HandleSendSignal },
    { "quit",			HandleQuit },
    { "set-scrollbar",		HandleScrollbar },
    { "set-jumpscroll",		HandleJumpscroll },
    { "set-reverse-video",	HandleReverseVideo },
    { "set-autowrap",		HandleAutoWrap },
    { "set-reversewrap",	HandleReverseWrap },
    { "set-autolinefeed",	HandleAutoLineFeed },
    { "set-appcursor",		HandleAppCursor },
    { "set-appkeypad",		HandleAppKeypad },
    { "set-scroll-on-key",	HandleScrollKey },
    { "set-scroll-on-tty-output", HandleScrollTtyOutput },
    { "set-allow132",		HandleAllow132 },
    { "set-cursesemul",		HandleCursesEmul },
    { "set-marginbell",		HandleMarginBell },
    { "set-altscreen",		HandleAltScreen },
    { "soft-reset",		HandleSoftReset },
    { "hard-reset",		HandleHardReset },
    { "set-terminal-type",	HandleSetTerminalType },
    { "set-visibility",		HandleVisibility },
    { "set-tek-text",		HandleSetTekText },
    { "tek-page",		HandleTekPage },
    { "tek-reset",		HandleTekReset },
    { "tek-copy",		HandleTekCopy },
};
/* *INDENT-ON* */

static Dimension defOne = 1;

#define GIN_TERM_NONE_STR	"none"
#define GIN_TERM_CR_STR		"CRonly"
#define GIN_TERM_EOT_STR	"CR&EOT"

#define GIN_TERM_NONE	0
#define GIN_TERM_CR	1
#define GIN_TERM_EOT	2

#ifdef VMS
#define DFT_FONT_SMALL "FIXED"
#else
#define DFT_FONT_SMALL "6x10"
#endif

static XtResource resources[] =
{
#ifdef VMS
    {XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
     XtOffset(TekWidget, core.background_pixel),
     XtRString, "White"},
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
     XtOffset(TekWidget, Tforeground),
     XtRString, "Black"},
#endif
    {XtNwidth, XtCWidth, XtRDimension, sizeof(Dimension),
     XtOffsetOf(CoreRec, core.width), XtRDimension, (caddr_t) & defOne},
    {XtNheight, XtCHeight, XtRDimension, sizeof(Dimension),
     XtOffsetOf(CoreRec, core.height), XtRDimension, (caddr_t) & defOne},
    {"fontLarge", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
     XtOffsetOf(TekWidgetRec, tek.Tfont[TEK_FONT_LARGE]),
     XtRString, "9x15"},
    {"font2", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
     XtOffsetOf(TekWidgetRec, tek.Tfont[TEK_FONT_2]),
     XtRString, "6x13"},
    {"font3", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
     XtOffsetOf(TekWidgetRec, tek.Tfont[TEK_FONT_3]),
     XtRString, "8x13"},
    {"fontSmall", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
     XtOffsetOf(TekWidgetRec, tek.Tfont[TEK_FONT_SMALL]),
     XtRString, DFT_FONT_SMALL},
    {"initialFont", "InitialFont", XtRString, sizeof(char *),
     XtOffsetOf(TekWidgetRec, tek.initial_font),
     XtRString, "large"},
    {"ginTerminator", "GinTerminator", XtRString, sizeof(char *),
     XtOffsetOf(TekWidgetRec, tek.gin_terminator_str),
     XtRString, GIN_TERM_NONE_STR},
#if OPT_TOOLBAR
    {XtNmenuBar, XtCMenuBar, XtRWidget, sizeof(Widget),
     XtOffsetOf(TekWidgetRec, tek.menu_bar),
     XtRWidget, (XtPointer) 0},
    {XtNmenuHeight, XtCMenuHeight, XtRInt, sizeof(int),
     XtOffsetOf(TekWidgetRec, tek.menu_height),
     XtRString, "25"},
#endif
};

static int Tinput(void);
static int getpoint(void);
static void TCursorBack(void);
static void TCursorDown(void);
static void TCursorForward(void);
static void TCursorUp(void);
static void TekBackground(TScreen * screen);
static void TekConfigure(Widget w);
static void TekDraw(int x, int y);
static void TekEnq(int status, int x, int y);
static void TekFlush(void);
static void TekInitialize(Widget request,
			  Widget wnew,
			  ArgList args,
			  Cardinal * num_args);
static void TekPage(void);
static void TekRealize(Widget gw,
		       XtValueMask * valuemaskp,
		       XSetWindowAttributes * values);

static WidgetClassRec tekClassRec =
{
    {
/* core_class fields */
	(WidgetClass) & widgetClassRec,		/* superclass     */
	"Tek4014",		/* class_name                   */
	sizeof(TekWidgetRec),	/* widget_size                  */
	NULL,			/* class_initialize             */
	NULL,			/* class_part_initialize        */
	FALSE,			/* class_inited                 */
	TekInitialize,		/* initialize                   */
	NULL,			/* initialize_hook              */
	TekRealize,		/* realize                      */
	actionsList,		/* actions                      */
	XtNumber(actionsList),	/* num_actions                  */
	resources,		/* resources                    */
	XtNumber(resources),	/* num_resources                */
	NULLQUARK,		/* xrm_class                    */
	TRUE,			/* compress_motion              */
	TRUE,			/* compress_exposure            */
	TRUE,			/* compress_enterleave          */
	FALSE,			/* visible_interest             */
	NULL,			/* destroy                      */
	TekConfigure,		/* resize                       */
	TekExpose,		/* expose                       */
	NULL,			/* set_values                   */
	NULL,			/* set_values_hook              */
	NULL,			/* set_values_almost            */
	NULL,			/* get_values_hook              */
	NULL,			/* accept_focus                 */
	XtVersion,		/* version                      */
	NULL,			/* callback_offsets             */
	defaultTranslations,	/* tm_table                     */
	XtInheritQueryGeometry,	/* query_geometry               */
	XtInheritDisplayAccelerator,	/* display_accelerator  */
	NULL			/* extension                    */
    }
};
#define tekWidgetClass ((WidgetClass)&tekClassRec)

static Boolean Tfailed = FALSE;

int
TekInit(void)
{
    Widget form_top, menu_top;

    if (!Tfailed
	&& tekWidget == 0) {
	/* this causes the Initialize method to be called */
	tekshellwidget =
	    XtCreatePopupShell("tektronix", topLevelShellWidgetClass,
			       toplevel, ourTopLevelShellArgs,
			       number_ourTopLevelShellArgs);

	SetupMenus(tekshellwidget, &form_top, &menu_top);

	/* this causes the Realize method to be called */
	tekWidget = (TekWidget)
	    XtVaCreateManagedWidget("tek4014",
				    tekWidgetClass, form_top,
#if OPT_TOOLBAR
				    XtNmenuBar, menu_top,
				    XtNresizable, True,
				    XtNfromVert, menu_top,
				    XtNleft, XawChainLeft,
				    XtNright, XawChainRight,
				    XtNbottom, XawChainBottom,
#endif
				    (XtPointer) 0);
    }
    return (!Tfailed);
}

/*
 * If we haven't allocated the PtyData struct, do so.
 */
int
TekPtyData(void)
{
    if (Tbuffer == 0) {
	if ((Tbuffer = (PtyData *) malloc(sizeof(PtyData))) == NULL
	    || (Tpushb = (Char *) malloc(10)) == NULL
	    || (Tline = (XSegment *) malloc(MAX_VTX * sizeof(XSegment))) == NULL) {
	    fprintf(stderr, "%s: Not enough core for Tek mode\n", xterm_name);
	    if (Tpushb)
		free((char *) Tpushb);
	    if (Tbuffer)
		free((char *) Tbuffer);
	    Tfailed = TRUE;
	    return 0;
	}
    }
    return 1;
}

static void
Tekparse(void)
{
    register TScreen *screen = &term->screen;
    register int c = 0, x, y;
    Char ch;
    int nextstate;

    for (;;) {
	c = input();
	/*
	 * The parsing tables all have 256 entries.  If we're supporting
	 * wide characters, we handle them by treating them the same as
	 * printing characters.
	 */
#if OPT_WIDE_CHARS
	if (c > 255) {
	    nextstate = (Tparsestate == Talptable)
		? CASE_PRINT
		: CASE_IGNORE;
	} else
#endif
	    nextstate = Tparsestate[c];
	TRACE(("Tekparse %d -> %d\n", c, nextstate));

	switch (nextstate) {
	case CASE_REPORT:
	    TRACE(("case: report address\n"));
	    if (screen->TekGIN) {
		TekGINoff();
		TekEnqMouse(0);
	    } else {
		c = 064;	/* has hard copy unit */
		if (screen->margin == MARGIN2)
		    c |= 02;
		TekEnq(c, screen->cur_X, screen->cur_Y);
	    }
	    TekRecord->ptr[-1] = NAK;	/* remove from recording */
	    Tparsestate = curstate;
	    break;

	case CASE_VT_MODE:
	    TRACE(("case: special return to vt102 mode\n"));
	    Tparsestate = curstate;
	    TekRecord->ptr[-1] = NAK;	/* remove from recording */
#ifdef ALLOWLOGGING
	    if (screen->logging) {
		FlushLog(screen);
		screen->logstart = DecodedData(&(VTbuffer));
	    }
#endif
	    return;

	case CASE_SPT_STATE:
	    TRACE(("case: Enter Special Point Plot mode\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    Tparsestate = curstate = Tspttable;
	    break;

	case CASE_GIN:
	    TRACE(("case: Do Tek GIN mode\n"));
	    screen->TekGIN = &TekRecord->ptr[-1];
	    /* Set cross-hair cursor raster array */
	    if ((GINcursor =
		 make_colored_cursor(XC_tcross, screen->mousecolor,
				     screen->mousecolorback)) != 0) {
		XDefineCursor(screen->display, TWindow(screen),
			      GINcursor);
	    }
	    Tparsestate = Tbyptable;	/* Bypass mode */
	    break;

	case CASE_BEL:
	    TRACE(("case: BEL\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    if (!TekRefresh)
		Bell(XkbBI_TerminalBell, 0);
	    Tparsestate = curstate;	/* clear bypass condition */
	    break;

	case CASE_BS:
	    TRACE(("case: BS\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    Tparsestate = curstate;	/* clear bypass condition */
	    TCursorBack();
	    break;

	case CASE_PT_STATE:
	    TRACE(("case: Enter Tek Point Plot mode\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    Tparsestate = curstate = Tpttable;
	    break;

	case CASE_PLT_STATE:
	    TRACE(("case: Enter Tek Plot mode\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    Tparsestate = curstate = Tplttable;
	    if ((c = input()) == BEL)
		screen->pen = PENDOWN;
	    else {
		unput(c);
		screen->pen = PENUP;
	    }
	    break;

	case CASE_TAB:
	    TRACE(("case: HT\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    Tparsestate = curstate;	/* clear bypass condition */
	    TCursorForward();
	    break;

	case CASE_IPL_STATE:
	    TRACE(("case: Enter Tek Incremental Plot mode\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    Tparsestate = curstate = Tipltable;
	    break;

	case CASE_ALP_STATE:
	    TRACE(("case: Enter Tek Alpha mode from any other mode\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    /* if in one of graphics states, move alpha cursor */
	    if (nplot > 0)	/* flush line Tbuffer */
		TekFlush();
	    Tparsestate = curstate = Talptable;
	    break;

	case CASE_UP:
	    TRACE(("case: cursor up\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    Tparsestate = curstate;	/* clear bypass condition */
	    TCursorUp();
	    break;

	case CASE_COPY:
	    TRACE(("case: make copy\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    TekCopy();
	    TekRecord->ptr[-1] = NAK;	/* remove from recording */
	    Tparsestate = curstate;	/* clear bypass condition */
	    break;

	case CASE_PAGE:
	    TRACE(("case: Page Function\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    TekPage();		/* clear bypass condition */
	    break;

	case CASE_BES_STATE:
	    TRACE(("case: Byp: an escape char\n"));
	    Tparsestate = Tbestable;
	    break;

	case CASE_BYP_STATE:
	    TRACE(("case: set bypass condition\n"));
	    Tparsestate = Tbyptable;
	    break;

	case CASE_IGNORE:
	    TRACE(("case: Esc: totally ignore CR, ESC, LF, ~\n"));
	    break;

	case CASE_ASCII:
	    TRACE(("case: Select ASCII char set\n"));
	    /* ignore for now */
	    Tparsestate = curstate;
	    break;

	case CASE_APL:
	    TRACE(("case: Select APL char set\n"));
	    /* ignore for now */
	    Tparsestate = curstate;
	    break;

	case CASE_CHAR_SIZE:
	    TRACE(("case: character size selector\n"));
	    TekSetFontSize(c & 03);
	    Tparsestate = curstate;
	    break;

	case CASE_BEAM_VEC:
	    TRACE(("case: beam and vector selector\n"));
	    /* only line types */
	    if ((c &= LINEMASK) != screen->cur.linetype) {
		if (nplot > 0)
		    TekFlush();
		if (c <= TEKNUMLINES)
		    screen->cur.linetype = c;
	    }
	    Tparsestate = curstate;
	    break;

	case CASE_CURSTATE:
	    Tparsestate = curstate;
	    break;

	case CASE_PENUP:
	    TRACE(("case: Ipl: penup\n"));
	    screen->pen = PENUP;
	    break;

	case CASE_PENDOWN:
	    TRACE(("case: Ipl: pendown\n"));
	    screen->pen = PENDOWN;
	    break;

	case CASE_IPL_POINT:
	    TRACE(("case: Ipl: point\n"));
	    x = screen->cur_X;
	    y = screen->cur_Y;
	    if (c & NORTH)
		y++;
	    else if (c & SOUTH)
		y--;
	    if (c & EAST)
		x++;
	    else if (c & WEST)
		x--;
	    if (screen->pen == PENDOWN)
		TekDraw(x, y);
	    else
		TekMove(x, y);
	    break;

	case CASE_PLT_VEC:
	    TRACE(("case: Plt: vector\n"));
	    unput(c);
	    if (getpoint()) {
		if (screen->pen == PENDOWN)
		    TekDraw(screen->cur.x, screen->cur.y);
		else
		    TekMove(screen->cur.x, screen->cur.y);
		screen->pen = PENDOWN;
	    }
	    break;

	case CASE_PT_POINT:
	    TRACE(("case: Pt: point\n"));
	    unput(c);
	    if (getpoint()) {
		TekMove(screen->cur.x, screen->cur.y);
		TekDraw(screen->cur.x, screen->cur.y);
	    }
	    break;

	case CASE_SPT_POINT:
	    TRACE(("case: Spt: point\n"));
	    /* ignore intensity character in c */
	    if (getpoint()) {
		TekMove(screen->cur.x, screen->cur.y);
		TekDraw(screen->cur.x, screen->cur.y);
	    }
	    break;

	case CASE_CR:
	    TRACE(("case: CR\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    if (nplot > 0)	/* flush line Tbuffer */
		TekFlush();
	    screen->cur_X = screen->margin == MARGIN1 ? 0 :
		TEKWIDTH / 2;
	    Tparsestate = curstate = Talptable;
	    break;

	case CASE_ESC_STATE:
	    TRACE(("case: ESC\n"));
	    Tparsestate = Tesctable;
	    break;

	case CASE_LF:
	    TRACE(("case: LF\n"));
	    if (screen->TekGIN)
		TekGINoff();
	    TCursorDown();
	    if (!TekRefresh)
		do_xevents();
	    break;

	case CASE_SP:
	    TRACE(("case: SP\n"));
	    TCursorForward();
	    break;

	case CASE_PRINT:
	    TRACE(("case: printable character\n"));
	    ch = c;
	    c = screen->cur.fontsize;
	    x = (int) (screen->cur_X * TekScale(screen))
		+ screen->border;
	    y = (int) ((TEKHEIGHT + TEKTOPPAD - screen->cur_Y) * TekScale(screen))
		+ screen->border;

#if OPT_WIDE_CHARS
	    if (screen->wide_chars
		&& (ch > 255)) {
		XChar2b sbuf;
		sbuf.byte2 = CharOf(ch);
		sbuf.byte1 = CharOf(ch >> 8);
		XDrawImageString16(screen->display,
				   TWindow(screen),
				   screen->TnormalGC,
				   x,
				   y,
				   &sbuf,
				   1);
	    } else
#endif
		XDrawString(screen->display,
			    TWindow(screen),
			    screen->TnormalGC,
			    x,
			    y,
			    (char *) &ch,
			    1);
	    TCursorForward();
	    break;
	case CASE_OSC:
	    /* FIXME:  someone should disentangle the input queues
	     * of this code so that it can be state-driven.
	     */
	    TRACE(("case: do osc escape\n"));
	    {
		Char buf2[512];
		int c2, len = 0;
		while ((c2 = Tinput()) != BEL) {
		    if (!isprint(c2 & 0x7f)
			|| len + 2 >= (int) sizeof(buf2))
			break;
		    buf2[len++] = c2;
		}
		if (c2 == BEL)
		    do_osc(buf2, len, BEL);
	    }
	    Tparsestate = curstate;
	    break;
	}
    }
}

static int rcnt;
static char *rptr;
#ifdef VMS
static int Tselect_mask;
#else /* VMS */
static fd_set Tselect_mask;
#endif /* VMS */

static int
Tinput(void)
{
    register TScreen *screen = &term->screen;
    register int i;
    register TekLink *tek;

    if (Tpushback > Tpushb)
	return (*--Tpushback);
    if (TekRefresh) {
	if (rcnt-- > 0)
	    return (*rptr++);
	if ((tek = TekRefresh->next) != 0) {
	    TekRefresh = tek;
	    rptr = tek->data;
	    rcnt = tek->count - 1;
	    TekSetFontSize(tek->fontsize);
	    return (*rptr++);
	}
	TekRefresh = (TekLink *) 0;
	longjmp(Tekjump, 1);
    }
  again:
    if (Tbuffer->cnt-- <= 0) {
	if (nplot > 0)		/* flush line Tbuffer */
	    TekFlush();
#ifdef VMS
	Tselect_mask = pty_mask;	/* force a read */
#else /* VMS */
	XFD_COPYSET(&pty_mask, &Tselect_mask);
#endif /* VMS */
	for (;;) {
#ifdef CRAY
	    struct timeval crocktimeout;
	    crocktimeout.tv_sec = 0;
	    crocktimeout.tv_usec = 0;
	    (void) Select(max_plus1,
			  &Tselect_mask, NULL, NULL,
			  &crocktimeout);
#endif
#ifdef VMS
	    if (Tselect_mask & pty_mask) {
#ifdef ALLOWLOGGING
		if (screen->logging)
		    FlushLog(screen);
#endif
		if (read_queue.flink != 0) {
		    Tbuffer->cnt = tt_read(Tbuffer->ptr = Tbuffer->buf);
		    if (Tbuffer->cnt == 0) {
			Panic("input: read returned zero\n", 0);
		    } else {
			break;
		    }
		} else {
		    sys$hiber();
		}
	    }
#else /* VMS */
	    if (getPtyData(screen, &Tselect_mask, Tbuffer)) {
		break;
	    }
#endif /* VMS */
	    if (Ttoggled && curstate == Talptable) {
		TCursorToggle(TOGGLE);
		Ttoggled = FALSE;
	    }
	    if (XtAppPending(app_con) & XtIMXEvent) {
#ifdef VMS
		Tselect_mask = X_mask;
#else /* VMS */
		XFD_COPYSET(&X_mask, &Tselect_mask);
#endif /* VMS */
	    } else {
		XFlush(screen->display);
#ifdef VMS
		Tselect_mask = Select_mask;

#else /* VMS */
		XFD_COPYSET(&Select_mask, &Tselect_mask);
		if ((i = Select(max_plus1,
				&Tselect_mask, NULL, NULL,
				NULL)) < 0) {
		    if (errno != EINTR)
			SysError(ERROR_TSELECT);
		    continue;
		}
#endif /* VMS */
	    }
#ifdef VMS
	    if (Tselect_mask & X_mask) {
		xevents();
		if (Tbuffer->cnt > 0)
		    goto again;
	    }
#else /* VMS */
	    if (FD_ISSET(ConnectionNumber(screen->display), &Tselect_mask)) {
		xevents();
		if (Tbuffer->cnt > 0)
		    goto again;
	    }
#endif /* VMS */
	}
	Tbuffer->cnt--;
	if (!Ttoggled && curstate == Talptable) {
	    TCursorToggle(TOGGLE);
	    Ttoggled = TRUE;
	}
    }
    tek = TekRecord;
    if (tek->count >= TEK_LINK_BLOCK_SIZE
	|| tek->fontsize != screen->cur.fontsize) {
	if ((TekRecord = tek->next = (TekLink *) malloc(sizeof(TekLink)))
	    == (TekLink *) 0)
	    Panic("Tinput: malloc error (%d)\n", errno);
	tek = tek->next;
	tek->next = (TekLink *) 0;
	tek->fontsize = screen->cur.fontsize;
	tek->count = 0;
	tek->ptr = tek->data;
    }
    tek->count++;
    return (*tek->ptr++ = *(Tbuffer->ptr)++);
}

/* this should become the Tek Widget's Resize proc */
static void
TekConfigure(Widget w)
{
    register TScreen *screen = &term->screen;
    register int border = 2 * screen->border;
    register double d;

    if (TWindow(screen))
	XClearWindow(screen->display, TWindow(screen));
    TWidth(screen) = w->core.width - border;
    THeight(screen) = w->core.height - border;
    TekScale(screen) = (double) TWidth(screen) / TEKWIDTH;
    if ((d = (double) THeight(screen) / (TEKHEIGHT + TEKTOPPAD + TEKBOTTOMPAD))
	< TekScale(screen))
	TekScale(screen) = d;
    TFullWidth(screen) = w->core.width;
    TFullHeight(screen) = w->core.height;
}

/*ARGSUSED*/
void
TekExpose(Widget w GCC_UNUSED,
	  XEvent * event GCC_UNUSED,
	  Region region GCC_UNUSED)
{
    register TScreen *screen = &term->screen;

#ifdef lint
    region = region;
#endif
    if (!Ttoggled)
	TCursorToggle(CLEAR);
    Ttoggled = TRUE;
    Tpushback = Tpushb;
    screen->cur_X = 0;
    screen->cur_Y = TEKHOME;
    TekSetFontSize(screen->page.fontsize);
    screen->cur = screen->page;
    screen->margin = MARGIN1;
    if (screen->TekGIN) {
	screen->TekGIN = NULL;
	TekGINoff();
    }
    TekRefresh = &Tek0;
    rptr = TekRefresh->data;
    rcnt = TekRefresh->count;
    Tparsestate = curstate = Talptable;
    if (waiting_for_initial_map)
	first_map_occurred();
    if (!screen->waitrefresh)
	dorefresh();
}

void
dorefresh(void)
{
    register TScreen *screen = &term->screen;
    static Cursor wait_cursor = None;

    if (wait_cursor == None)
	wait_cursor = make_colored_cursor(XC_watch, screen->mousecolor,
					  screen->mousecolorback);
    XDefineCursor(screen->display, TShellWindow, wait_cursor);
    XFlush(screen->display);
    if (!setjmp(Tekjump))
	Tekparse();
    XDefineCursor(screen->display, TShellWindow,
		  (screen->TekGIN && GINcursor) ? GINcursor : screen->arrow);
}

static void
TekPage(void)
{
    register TScreen *screen = &term->screen;
    register TekLink *tek;

    XClearWindow(screen->display, TWindow(screen));
    screen->cur_X = 0;
    screen->cur_Y = TEKHOME;
    screen->margin = MARGIN1;
    screen->page = screen->cur;
    if (screen->TekGIN)
	TekGINoff();
    tek = TekRecord = &Tek0;
    tek->fontsize = screen->cur.fontsize;
    tek->count = 0;
    tek->ptr = tek->data;
    tek = tek->next;
    if (tek)
	do {
	    TekLink *tek2 = tek->next;

	    free((char *) tek);
	    tek = tek2;
	} while (tek);
    TekRecord->next = (TekLink *) 0;
    TekRefresh = (TekLink *) 0;
    Ttoggled = TRUE;
    Tparsestate = curstate = Talptable;		/* Tek Alpha mode */
}

#define	EXTRABITS	017
#define	FIVEBITS	037
#define	HIBITS		(FIVEBITS << SHIFTHI)
#define	LOBITS		(FIVEBITS << SHIFTLO)
#define	SHIFTHI		7
#define	SHIFTLO		2
#define	TWOBITS		03

static int
getpoint(void)
{
    register int c, x, y, e, lo_y = 0;
    register TScreen *screen = &term->screen;

    x = screen->cur.x;
    y = screen->cur.y;
    for (;;) {
	if ((c = input()) < ' ') {	/* control character */
	    unput(c);
	    return (0);
	}
	if (c < '@') {		/* Hi X or Hi Y */
	    if (lo_y) {		/* seen a Lo Y, so this must be Hi X */
		x &= ~HIBITS;
		x |= (c & FIVEBITS) << SHIFTHI;
		continue;
	    }
	    /* else Hi Y */
	    y &= ~HIBITS;
	    y |= (c & FIVEBITS) << SHIFTHI;
	    continue;
	}
	if (c < '`') {		/* Lo X */
	    x &= ~LOBITS;
	    x |= (c & FIVEBITS) << SHIFTLO;
	    screen->cur.x = x;
	    screen->cur.y = y;
	    return (1);		/* OK */
	}
	/* else Lo Y */
	if (lo_y) {		/* seen a Lo Y, so other must be extra bits */
	    e = (y >> SHIFTLO) & EXTRABITS;
	    x &= ~TWOBITS;
	    x |= e & TWOBITS;
	    y &= ~TWOBITS;
	    y |= (e >> SHIFTLO) & TWOBITS;
	}
	y &= ~LOBITS;
	y |= (c & FIVEBITS) << SHIFTLO;
	lo_y++;
    }
}

static void
TCursorBack(void)
{
    register TScreen *screen = &term->screen;
    register struct Tek_Char *t;
    register int x, l;

    x = (screen->cur_X -=
	 (t = &TekChar[screen->cur.fontsize])->hsize
	);

    if (((screen->margin == MARGIN1) && (x < 0))
	|| ((screen->margin == MARGIN2) && (x < TEKWIDTH / 2))) {
	if ((l = (screen->cur_Y + (t->vsize - 1)) / t->vsize + 1) >=
	    t->nlines) {
	    screen->margin = !screen->margin;
	    l = 0;
	}
	screen->cur_Y = l * t->vsize;
	screen->cur_X = (t->charsperline - 1) * t->hsize;
    }
}

static void
TCursorForward(void)
{
    register TScreen *screen = &term->screen;
    register struct Tek_Char *t;
    register int l;

    if ((screen->cur_X +=
	 (t = &TekChar[screen->cur.fontsize])->hsize
	) > TEKWIDTH
	) {
	if ((l = screen->cur_Y / t->vsize - 1) < 0) {
	    screen->margin = !screen->margin;
	    l = t->nlines - 1;
	}
	screen->cur_Y = l * t->vsize;
	screen->cur_X = screen->margin == MARGIN1 ? 0 : TEKWIDTH / 2;
    }
}

static void
TCursorUp(void)
{
    register TScreen *screen = &term->screen;
    register struct Tek_Char *t;
    register int l;

    t = &TekChar[screen->cur.fontsize];

    if ((l = (screen->cur_Y + (t->vsize - 1)) / t->vsize + 1) >= t->nlines) {
	l = 0;
	if ((screen->margin = !screen->margin) != MARGIN1) {
	    if (screen->cur_X < TEKWIDTH / 2)
		screen->cur_X += TEKWIDTH / 2;
	} else if (screen->cur_X >= TEKWIDTH / 2)
	    screen->cur_X -= TEKWIDTH / 2;
    }
    screen->cur_Y = l * t->vsize;
}

static void
TCursorDown(void)
{
    register TScreen *screen = &term->screen;
    register struct Tek_Char *t;
    register int l;

    t = &TekChar[screen->cur.fontsize];

    if ((l = screen->cur_Y / t->vsize - 1) < 0) {
	l = t->nlines - 1;
	if ((screen->margin = !screen->margin) != MARGIN1) {
	    if (screen->cur_X < TEKWIDTH / 2)
		screen->cur_X += TEKWIDTH / 2;
	} else if (screen->cur_X >= TEKWIDTH / 2)
	    screen->cur_X -= TEKWIDTH / 2;
    }
    screen->cur_Y = l * t->vsize;
}

static void
AddToDraw(int x1, int y1, int x2, int y2)
{
    register TScreen *screen = &term->screen;
    register XSegment *lp;

    if (nplot >= MAX_PTS) {
	TekFlush();
    }
    lp = line_pt++;
    lp->x1 = x1 = (int) (x1 * TekScale(screen) + screen->border);
    lp->y1 = y1 = (int) ((TEKHEIGHT + TEKTOPPAD - y1) * TekScale(screen) +
			 screen->border);
    lp->x2 = x2 = (int) (x2 * TekScale(screen) + screen->border);
    lp->y2 = y2 = (int) ((TEKHEIGHT + TEKTOPPAD - y2) * TekScale(screen) +
			 screen->border);
    nplot++;
}

static void
TekDraw(int x, int y)
{
    register TScreen *screen = &term->screen;

    if (nplot == 0 || T_lastx != screen->cur_X || T_lasty != screen->cur_Y) {
	/*
	 * We flush on each unconnected line segment if the line
	 * type is not solid.  This solves a bug in X when drawing
	 * points while the line type is not solid.
	 */
	if (nplot > 0 && screen->cur.linetype != SOLIDLINE)
	    TekFlush();
    }
    AddToDraw(screen->cur_X, screen->cur_Y, x, y);
    T_lastx = screen->cur_X = x;
    T_lasty = screen->cur_Y = y;
}

static void
TekFlush(void)
{
    register TScreen *screen = &term->screen;

    XDrawSegments(screen->display, TWindow(screen),
		  ((screen->cur.linetype == SOLIDLINE) ? screen->TnormalGC :
		   screen->linepat[screen->cur.linetype - 1]),
		  Tline, nplot);
    nplot = 0;
    line_pt = Tline;
}

void
TekGINoff(void)
{
    register TScreen *screen = &term->screen;

    XDefineCursor(screen->display, TShellWindow, screen->arrow);
    if (GINcursor)
	XFreeCursor(screen->display, GINcursor);
    if (screen->TekGIN) {
	*screen->TekGIN = CAN;	/* modify recording */
	screen->TekGIN = NULL;
    }
}

void
TekEnqMouse(int c)		/* character pressed */
{
    register TScreen *screen = &term->screen;
    int mousex, mousey, rootx, rooty;
    unsigned int mask;		/* XQueryPointer */
    Window root, subw;

    XQueryPointer(
		     screen->display, TWindow(screen),
		     &root, &subw,
		     &rootx, &rooty,
		     &mousex, &mousey,
		     &mask);
    if ((mousex = (int) ((mousex - screen->border) / TekScale(screen))) < 0)
	mousex = 0;
    else if (mousex >= TEKWIDTH)
	mousex = TEKWIDTH - 1;
    if ((mousey = (int) (TEKHEIGHT + TEKTOPPAD - (mousey - screen->border) /
			 TekScale(screen))) < 0)
	mousey = 0;
    else if (mousey >= TEKHEIGHT)
	mousey = TEKHEIGHT - 1;
    TekEnq(c, mousex, mousey);
}

static void
TekEnq(int status,
       register int x,
       register int y)
{
    register TScreen *screen = &term->screen;
    Char cplot[7];
    int len = 5;
    int adj = (status != 0) ? 0 : 1;

    cplot[0] = status;
    /* Translate x and y to Tektronix code */
    cplot[1] = 040 | ((x >> SHIFTHI) & FIVEBITS);
    cplot[2] = 040 | ((x >> SHIFTLO) & FIVEBITS);
    cplot[3] = 040 | ((y >> SHIFTHI) & FIVEBITS);
    cplot[4] = 040 | ((y >> SHIFTLO) & FIVEBITS);

    if (screen->gin_terminator != GIN_TERM_NONE)
	cplot[len++] = '\r';
    if (screen->gin_terminator == GIN_TERM_EOT)
	cplot[len++] = '\004';
#ifdef VMS
    tt_write(cplot + adj, len - adj);
#else /* VMS */
    v_write(screen->respond, cplot + adj, len - adj);
#endif /* VMS */
}

void
TekRun(void)
{
    register TScreen *screen = &term->screen;
    register int i;

    if (!TWindow(screen) && !TekInit()) {
	if (VWindow(screen)) {
	    screen->TekEmu = FALSE;
	    return;
	}
	Exit(ERROR_TINIT);
    }
    if (!screen->Tshow) {
	set_tek_visibility(TRUE);
    }
    update_vttekmode();
    update_vtshow();
    update_tekshow();
    set_tekhide_sensitivity();

    Tpushback = Tpushb;
    Tbuffer->ptr = DecodedData(Tbuffer);
    for (i = Tbuffer->cnt = VTbuffer.cnt; i > 0; i--)
	*(Tbuffer->ptr)++ = *(VTbuffer.ptr)++;
    Tbuffer->ptr = DecodedData(Tbuffer);
    Ttoggled = TRUE;
    if (!setjmp(Tekend))
	Tekparse();
    if (!Ttoggled) {
	TCursorToggle(TOGGLE);
	Ttoggled = TRUE;
    }
    screen->TekEmu = FALSE;
}

#define DOTTED_LENGTH 2
#define DOT_DASHED_LENGTH 4
#define SHORT_DASHED_LENGTH 2
#define LONG_DASHED_LENGTH 2

static int dash_length[TEKNUMLINES] =
{
    DOTTED_LENGTH,
    DOT_DASHED_LENGTH,
    SHORT_DASHED_LENGTH,
    LONG_DASHED_LENGTH,
};

static unsigned char dotted[DOTTED_LENGTH] =
{3, 1};
static unsigned char dot_dashed[DOT_DASHED_LENGTH] =
{3, 4, 3, 1};
static unsigned char short_dashed[SHORT_DASHED_LENGTH] =
{4, 4};
static unsigned char long_dashed[LONG_DASHED_LENGTH] =
{4, 7};

static unsigned char *dashes[TEKNUMLINES] =
{
    dotted,
    dot_dashed,
    short_dashed,
    long_dashed,
};

/*
 * The following is called to create the tekWidget
 */

static void
TekInitialize(Widget request GCC_UNUSED,
	      Widget wnew GCC_UNUSED,
	      ArgList args GCC_UNUSED,
	      Cardinal * num_args GCC_UNUSED)
{
    Widget tekparent = SHELL_OF(wnew);

    /* look for focus related events on the shell, because we need
     * to care about the shell's border being part of our focus.
     */
    XtAddEventHandler(tekparent, EnterWindowMask, FALSE,
		      HandleEnterWindow, (caddr_t) NULL);
    XtAddEventHandler(tekparent, LeaveWindowMask, FALSE,
		      HandleLeaveWindow, (caddr_t) NULL);
    XtAddEventHandler(tekparent, FocusChangeMask, FALSE,
		      HandleFocusChange, (caddr_t) NULL);
    XtAddEventHandler((Widget) wnew, PropertyChangeMask, FALSE,
		      HandleBellPropertyChange, (Opaque) NULL);
}

static void
TekRealize(Widget gw,
	   XtValueMask * valuemaskp,
	   XSetWindowAttributes * values)
{
    TekWidget tw = (TekWidget) gw;
    register TScreen *screen = &term->screen;
    register int i;
    register TekLink *tek;
    register double d;
    register int border = 2 * screen->border;
    int pr;
    XGCValues gcv;
    int winX, winY, width, height;
    XSizeHints sizehints;
    char Tdefault[32];

#ifndef NO_ACTIVE_ICON
    term->screen.whichTwin = &term->screen.fullTwin;
#endif /* NO_ACTIVE_ICON */

    tw->core.border_pixel = term->core.border_pixel;

    for (i = 0; i < TEKNUMFONTS; i++) {
	if (!tw->tek.Tfont[i])
	    tw->tek.Tfont[i] = XQueryFont(screen->display, DefaultGCID);
	tw->tek.tobaseline[i] = tw->tek.Tfont[i]->ascent;
    }

    if (!TekPtyData())
	return;

    screen->xorplane = 1;

    screen->Tbackground = term->core.background_pixel;
    screen->Tforeground = screen->foreground;
    screen->Tcursorcolor = screen->cursorcolor;

    if (term->misc.T_geometry == NULL) {
	int defwidth, defheight;

	if (term->misc.tekSmall) {
	    defwidth = TEKMINWIDTH;
	    defheight = TEKMINHEIGHT;
	} else {
	    defwidth = TEKDEFWIDTH;
	    defheight = TEKDEFHEIGHT;
	}
	sprintf(Tdefault, "=%dx%d", defwidth + border, defheight + border);
	term->misc.T_geometry = Tdefault;
    }

    winX = 1;
    winY = 1;
    width = TEKDEFWIDTH + border;
    height = TEKDEFHEIGHT + border;

    pr = XParseGeometry(term->misc.T_geometry,
			&winX,
			&winY,
			(unsigned int *) &width,
			(unsigned int *) &height);
    if ((pr & XValue) && (pr & XNegative))
	winX += DisplayWidth(screen->display, DefaultScreen(screen->display))
	    - width - (SHELL_OF(term)->core.border_width * 2);
    if ((pr & YValue) && (pr & YNegative))
	winY += DisplayHeight(screen->display, DefaultScreen(screen->display))
	    - height - (SHELL_OF(term)->core.border_width * 2);

    /* set up size hints */
    sizehints.min_width = TEKMINWIDTH + border;
    sizehints.min_height = TEKMINHEIGHT + border;
    sizehints.width_inc = 1;
    sizehints.height_inc = 1;
    sizehints.flags = PMinSize | PResizeInc;
    sizehints.x = winX;
    sizehints.y = winY;
    if ((XValue & pr) || (YValue & pr)) {
	sizehints.flags |= USSize | USPosition;
	sizehints.flags |= PWinGravity;
	switch (pr & (XNegative | YNegative)) {
	case 0:
	    sizehints.win_gravity = NorthWestGravity;
	    break;
	case XNegative:
	    sizehints.win_gravity = NorthEastGravity;
	    break;
	case YNegative:
	    sizehints.win_gravity = SouthWestGravity;
	    break;
	default:
	    sizehints.win_gravity = SouthEastGravity;
	    break;
	}
    } else {
	sizehints.flags |= PSize;
    }
    sizehints.width = width;
    sizehints.height = height;
    if ((WidthValue & pr) || (HeightValue & pr))
	sizehints.flags |= USSize;
    else
	sizehints.flags |= PSize;

    (void) XtMakeResizeRequest((Widget) tw, width, height,
			       &tw->core.width, &tw->core.height);

    /* XXX This is bogus.  We are parsing geometries too late.  This
     * is information that the shell widget ought to have before we get
     * realized, so that it can do the right thing.
     */
    if (sizehints.flags & USPosition)
	XMoveWindow(XtDisplay(tw), XtWindow(SHELL_OF(tw)),
		    sizehints.x, sizehints.y);

    XSetWMNormalHints(XtDisplay(tw), XtWindow(SHELL_OF(tw)),
		      &sizehints);
    XFlush(XtDisplay(tw));	/* get it out to window manager */

    values->win_gravity = NorthWestGravity;
    values->background_pixel = screen->Tbackground;

    XtWindow(tw) = TWindow(screen) =
	XCreateWindow(screen->display,
		      XtWindow(SHELL_OF(tw)),
		      tw->core.x, tw->core.y,
		      tw->core.width, tw->core.height, tw->core.border_width,
		      (int) tw->core.depth,
		      InputOutput, CopyFromParent,
		      ((*valuemaskp) | CWBackPixel | CWWinGravity),
		      values);

    TFullWidth(screen) = width;
    TFullHeight(screen) = height;
    TWidth(screen) = width - border;
    THeight(screen) = height - border;
    TekScale(screen) = (double) TWidth(screen) / TEKWIDTH;
    if ((d = (double) THeight(screen) / (TEKHEIGHT + TEKTOPPAD +
					 TEKBOTTOMPAD)) < TekScale(screen))
	TekScale(screen) = d;

    screen->cur.fontsize = TEK_FONT_LARGE;
    if (tw->tek.initial_font) {
	char *s = tw->tek.initial_font;

	if (XmuCompareISOLatin1(s, "large") == 0)
	    screen->cur.fontsize = TEK_FONT_LARGE;
	else if (XmuCompareISOLatin1(s, "2") == 0 ||
		 XmuCompareISOLatin1(s, "two") == 0)
	    screen->cur.fontsize = TEK_FONT_2;
	else if (XmuCompareISOLatin1(s, "3") == 0 ||
		 XmuCompareISOLatin1(s, "three") == 0)
	    screen->cur.fontsize = TEK_FONT_3;
	else if (XmuCompareISOLatin1(s, "small") == 0)
	    screen->cur.fontsize = TEK_FONT_SMALL;
    }
#define TestGIN(s) XmuCompareISOLatin1(tw->tek.gin_terminator_str, s)

    if (TestGIN(GIN_TERM_NONE_STR) == 0)
	screen->gin_terminator = GIN_TERM_NONE;
    else if (TestGIN(GIN_TERM_CR_STR) == 0)
	screen->gin_terminator = GIN_TERM_CR;
    else if (TestGIN(GIN_TERM_EOT_STR) == 0)
	screen->gin_terminator = GIN_TERM_EOT;
    else
	fprintf(stderr, "%s: illegal GIN terminator setting \"%s\"\n",
		xterm_name, tw->tek.gin_terminator_str);

    gcv.graphics_exposures = TRUE;	/* default */
    gcv.font = tw->tek.Tfont[screen->cur.fontsize]->fid;
    gcv.foreground = screen->Tforeground;
    gcv.background = screen->Tbackground;

    /* if font wasn't successfully opened, then gcv.font will contain
       the Default GC's ID, meaning that we must use the server default font.
     */
    TEKgcFontMask = (gcv.font == DefaultGCID) ? 0 : GCFont;
    screen->TnormalGC = XCreateGC(screen->display, TWindow(screen),
				  (TEKgcFontMask | GCGraphicsExposures |
				   GCForeground | GCBackground), &gcv);

    gcv.function = GXinvert;
    gcv.plane_mask = screen->xorplane = (screen->Tbackground ^
					 screen->Tcursorcolor);
    gcv.join_style = JoinMiter;	/* default */
    gcv.line_width = 1;
    screen->TcursorGC = XCreateGC(screen->display, TWindow(screen),
				  (GCFunction | GCPlaneMask), &gcv);

    gcv.foreground = screen->Tforeground;
    gcv.line_style = LineOnOffDash;
    gcv.line_width = 0;
    for (i = 0; i < TEKNUMLINES; i++) {
	screen->linepat[i] = XCreateGC(screen->display, TWindow(screen),
				       (GCForeground | GCLineStyle), &gcv);
	XSetDashes(screen->display, screen->linepat[i], 0,
		   (char *) dashes[i], dash_length[i]);
    }

    TekBackground(screen);

    screen->margin = MARGIN1;	/* Margin 1             */
    screen->TekGIN = FALSE;	/* GIN off              */

    XDefineCursor(screen->display, TShellWindow, screen->pointer_cursor);

    {				/* there's gotta be a better way... */
	static Arg args[] =
	{
	    {XtNtitle, (XtArgVal) NULL},
	    {XtNiconName, (XtArgVal) NULL},
	};
	char *icon_name, *title, *tek_icon_name, *tek_title;

	args[0].value = (XtArgVal) & icon_name;
	args[1].value = (XtArgVal) & title;
	XtGetValues(SHELL_OF(tw), args, 2);
	tek_icon_name = XtMalloc(strlen(icon_name) + 7);
	strcpy(tek_icon_name, icon_name);
	strcat(tek_icon_name, "(Tek)");
	tek_title = XtMalloc(strlen(title) + 7);
	strcpy(tek_title, title);
	strcat(tek_title, "(Tek)");
	args[0].value = (XtArgVal) tek_icon_name;
	args[1].value = (XtArgVal) tek_title;
	XtSetValues(SHELL_OF(tw), args, 2);
	XtFree(tek_icon_name);
	XtFree(tek_title);
    }

    tek = TekRecord = &Tek0;
    tek->next = (TekLink *) 0;
    tek->fontsize = screen->cur.fontsize;
    tek->count = 0;
    tek->ptr = tek->data;
    Tpushback = Tpushb;
    Tbuffer->ptr = DecodedData(Tbuffer);
    screen->cur_X = 0;
    screen->cur_Y = TEKHOME;
    line_pt = Tline;
    Ttoggled = TRUE;
    screen->page = screen->cur;
    return;
}

void
TekSetFontSize(int newitem)
{
    register TScreen *screen = &term->screen;
    int oldsize = screen->cur.fontsize;
    int newsize = MI2FS(newitem);
    Font fid;

    if (!tekWidget || oldsize == newsize)
	return;
    if (!Ttoggled)
	TCursorToggle(TOGGLE);
    set_tekfont_menu_item(oldsize, FALSE);

    fid = tekWidget->tek.Tfont[newsize]->fid;
    if (fid == DefaultGCID)
	/* we didn't succeed in opening a real font
	   for this size.  Instead, use server default. */
	XCopyGC(screen->display,
		DefaultGC(screen->display, DefaultScreen(screen->display)),
		GCFont, screen->TnormalGC);
    else
	XSetFont(screen->display, screen->TnormalGC, fid);

    screen->cur.fontsize = newsize;
    set_tekfont_menu_item(newsize, TRUE);
    if (!Ttoggled)
	TCursorToggle(TOGGLE);
}

void
ChangeTekColors(register TScreen * screen, ScrnColors * pNew)
{
    register int i;
    XGCValues gcv;

    if (COLOR_DEFINED(pNew, TEK_FG)) {
	screen->Tforeground = COLOR_VALUE(pNew, TEK_FG);
	XSetForeground(screen->display, screen->TnormalGC,
		       screen->Tforeground);
    }
    if (COLOR_DEFINED(pNew, TEK_BG)) {
	screen->Tbackground = COLOR_VALUE(pNew, TEK_BG);
	XSetBackground(screen->display, screen->TnormalGC,
		       screen->Tbackground);
    }

    if (tekWidget) {
	if (tekWidget->core.border_pixel == screen->Tbackground) {
	    tekWidget->core.border_pixel = screen->Tforeground;
	    XtParent(tekWidget)->core.border_pixel =
		screen->Tforeground;
	    if (XtWindow(XtParent(tekWidget)))
		XSetWindowBorder(screen->display,
				 XtWindow(XtParent(tekWidget)),
				 tekWidget->core.border_pixel);
	}
    }

    for (i = 0; i < TEKNUMLINES; i++) {
	XSetForeground(screen->display, screen->linepat[i],
		       screen->Tforeground);
    }

    screen->Tcursorcolor = screen->Tforeground;

    gcv.plane_mask = screen->xorplane = (screen->Tbackground ^
					 screen->Tcursorcolor);
    XChangeGC(screen->display, screen->TcursorGC, GCPlaneMask, &gcv);
    TekBackground(screen);
    return;
}

void
TekReverseVideo(register TScreen * screen)
{
    register int i;
    XGCValues gcv;

    i = screen->Tbackground;
    screen->Tbackground = screen->Tforeground;
    screen->Tforeground = i;

    XSetForeground(screen->display, screen->TnormalGC,
		   screen->Tforeground);
    XSetBackground(screen->display, screen->TnormalGC,
		   screen->Tbackground);

    if (tekWidget) {
	if (tekWidget->core.border_pixel == screen->Tbackground) {
	    tekWidget->core.border_pixel = screen->Tforeground;
	    XtParent(tekWidget)->core.border_pixel =
		screen->Tforeground;
	    if (XtWindow(XtParent(tekWidget)))
		XSetWindowBorder(screen->display,
				 XtWindow(XtParent(tekWidget)),
				 tekWidget->core.border_pixel);
	}
    }

    for (i = 0; i < TEKNUMLINES; i++) {
	XSetForeground(screen->display, screen->linepat[i],
		       screen->Tforeground);
    }

    screen->Tcursorcolor = screen->Tforeground;

    gcv.plane_mask = screen->xorplane = (screen->Tbackground ^
					 screen->Tcursorcolor);
    XChangeGC(screen->display, screen->TcursorGC, GCPlaneMask, &gcv);
    TekBackground(screen);
}

static void
TekBackground(register TScreen * screen)
{
    if (TWindow(screen))
	XSetWindowBackground(screen->display, TWindow(screen),
			     screen->Tbackground);
}

/*
 * Toggles cursor on or off at cursor position in screen.
 */
void
TCursorToggle(int toggle)	/* TOGGLE or CLEAR */
{
    register TScreen *screen = &term->screen;
    register int c, x, y;
    unsigned int cellwidth, cellheight;

    if (!screen->Tshow)
	return;

    c = screen->cur.fontsize;
    cellwidth = (unsigned) tekWidget->tek.Tfont[c]->max_bounds.width;
    cellheight = (unsigned) (tekWidget->tek.Tfont[c]->ascent +
			     tekWidget->tek.Tfont[c]->descent);

    x = (int) ((screen->cur_X * TekScale(screen)) + screen->border);
    y = (int) (((TEKHEIGHT + TEKTOPPAD - screen->cur_Y) * TekScale(screen))
	       + screen->border - tekWidget->tek.tobaseline[c]);

    if (toggle == TOGGLE) {
	if (screen->select || screen->always_highlight)
	    XFillRectangle(screen->display, TWindow(screen),
			   screen->TcursorGC, x, y,
			   cellwidth, cellheight);
	else {			/* fix to use different GC! */
	    XDrawRectangle(screen->display, TWindow(screen),
			   screen->TcursorGC, x, y,
			   cellwidth - 1, cellheight - 1);
	}
    } else {
	/* Clear the entire rectangle, even though we may only
	 * have drawn an outline.  This fits with our refresh
	 * scheme of redrawing the entire window on any expose
	 * event and is easier than trying to figure out exactly
	 * which part of the cursor needs to be erased.
	 */
	XClearArea(screen->display, TWindow(screen), x, y,
		   cellwidth, cellheight, FALSE);
    }
}

void
TekSimulatePageButton(Bool reset)
{
    register TScreen *screen = &term->screen;

    if (!tekWidget)
	return;
    if (reset) {
	bzero((char *) &screen->cur, sizeof screen->cur);
    }
    TekRefresh = (TekLink *) 0;
    TekPage();
    screen->cur_X = 0;
    screen->cur_Y = TEKHOME;
}

/* write copy of screen to a file */

void
TekCopy(void)
{
    TScreen *screen = &term->screen;

    TekLink *Tp;
    char buf[32];
    char initbuf[5];
    int tekcopyfd;

    timestamp_filename(buf, "COPY");
    if (access(buf, F_OK) >= 0
	&& access(buf, W_OK) < 0) {
	Bell(XkbBI_MinorError, 0);
	return;
    }
#ifndef VMS
    if (access(".", W_OK) < 0) {	/* can't write in directory */
	Bell(XkbBI_MinorError, 0);
	return;
    }
#endif

    if ((tekcopyfd = open_userfile(screen->uid, screen->gid, buf, False)) >= 0) {
	sprintf(initbuf, "%c%c%c%c",
		ESC, screen->page.fontsize + '8',
		ESC, screen->page.linetype + '`');
	write(tekcopyfd, initbuf, 4);
	Tp = &Tek0;
	do {
	    write(tekcopyfd, (char *) Tp->data, Tp->count);
	    Tp = Tp->next;
	} while (Tp);
	close(tekcopyfd);
    }
}
