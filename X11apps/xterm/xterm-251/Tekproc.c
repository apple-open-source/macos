/* $XTermId: Tekproc.c,v 1.173 2009/10/12 00:31:59 tom Exp $ */

/*
 * Warning, there be crufty dragons here.
 */

/*

Copyright 2001-2008,2009 by Thomas E. Dickey

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

#define RES_OFFSET(field)	XtOffsetOf(TekWidgetRec, field)

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
#elif defined(HAVE_LIB_XAWPLUS)
#include <X11/XawPlus/Form.h>
#endif

#endif /* OPT_TOOLBAR */

#include <stdio.h>
#include <ctype.h>
#include <signal.h>

#include <Tekparse.h>
#include <data.h>
#include <error.h>
#include <menu.h>

#define DefaultGCID XGContextFromGC(DefaultGC(XtDisplay(tw), DefaultScreen(XtDisplay(tw))))

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
#define	TEKHOME		( (TekChar[tekscr->page.fontsize].nlines - 1) \
			 * TekChar[tekscr->page.fontsize].vsize)
#define	TEKMINHEIGHT	452
#define	TEKMINWIDTH	600
#define	TEKTOPPAD	34
#define	TEKWIDTH	4096
#define	WEST		02

#define	TekMove(tw,x,y)	tekscr->cur_X = x; tekscr->cur_Y = y
#define	input()		Tinput(tw)
#define	unput(c)	*Tpushback++ = (Char) c
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
#if OPT_TOOLBAR
    { "set-toolbar",		HandleToolbar },
#endif
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
    {XtNwidth, XtCWidth, XtRDimension, sizeof(Dimension),
     XtOffsetOf(CoreRec, core.width), XtRDimension, (caddr_t) & defOne},
    {XtNheight, XtCHeight, XtRDimension, sizeof(Dimension),
     XtOffsetOf(CoreRec, core.height), XtRDimension, (caddr_t) & defOne},
    Fres("fontLarge", XtCFont, tek.Tfont[TEK_FONT_LARGE], "9x15"),
    Fres("font2", XtCFont, tek.Tfont[TEK_FONT_2], "6x13"),
    Fres("font3", XtCFont, tek.Tfont[TEK_FONT_3], "8x13"),
    Fres("fontSmall", XtCFont, tek.Tfont[TEK_FONT_SMALL], DFT_FONT_SMALL),
    Sres(XtNinitialFont, XtCInitialFont, tek.initial_font, "large"),
    Sres("ginTerminator", "GinTerminator", tek.gin_terminator_str, GIN_TERM_NONE_STR),
#if OPT_TOOLBAR
    Wres(XtNmenuBar, XtCMenuBar, tek.tb_info.menu_bar, 0),
    Ires(XtNmenuHeight, XtCMenuHeight, tek.tb_info.menu_height, 25),
#endif
};

static IChar Tinput(TekWidget /* tw */ );
static int getpoint(TekWidget /* tw */ );
static void TCursorBack(TekWidget /* tw */ );
static void TCursorDown(TekWidget /* tw */ );
static void TCursorForward(TekWidget /* tw */ );
static void TCursorUp(TekWidget /* tw */ );
static void TekBackground(TekWidget /* tw */ ,
			  TScreen * /* screen */ );
static void TekConfigure(Widget /* w */ );
static void TekDraw(TekWidget /* tw */ ,
		    int /* x */ ,
		    int /* y */ );
static void TekEnq(TekWidget /* tw */ ,
		   unsigned /* status */ ,
		   int /* x */ ,
		   int /* y */ );
static void TekFlush(TekWidget /* tw */ );
static void TekInitialize(Widget /* request */ ,
			  Widget /* wnew */ ,
			  ArgList /* args */ ,
			  Cardinal * /* num_args */ );
static void TekPage(TekWidget /* tw */ );
static void TekRealize(Widget /* gw */ ,
		       XtValueMask * /* valuemaskp */ ,
		       XSetWindowAttributes * /* values */ );

static WidgetClassRec tekClassRec =
{
    {
/* core_class fields */
	(WidgetClass) & widgetClassRec,		/* superclass     */
	"Tek4014",		/* class_name                   */
	sizeof(TekWidgetRec),	/* widget_size                  */
	NULL,			/* class_initialize             */
	NULL,			/* class_part_initialize        */
	False,			/* class_inited                 */
	TekInitialize,		/* initialize                   */
	NULL,			/* initialize_hook              */
	TekRealize,		/* realize                      */
	actionsList,		/* actions                      */
	XtNumber(actionsList),	/* num_actions                  */
	resources,		/* resources                    */
	XtNumber(resources),	/* num_resources                */
	NULLQUARK,		/* xrm_class                    */
	True,			/* compress_motion              */
	True,			/* compress_exposure            */
	True,			/* compress_enterleave          */
	False,			/* visible_interest             */
	NULL,			/* destroy                      */
	TekConfigure,		/* resize                       */
	TekExpose,		/* expose                       */
	NULL,			/* set_values                   */
	NULL,			/* set_values_hook              */
	XtInheritSetValuesAlmost,	/* set_values_almost    */
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
WidgetClass tekWidgetClass = (WidgetClass) & tekClassRec;

static Bool Tfailed = False;

int
TekInit(void)
{
    Widget form_top, menu_top;
    Dimension menu_high;

    if (!Tfailed
	&& tekWidget == 0) {
	Cardinal nargs = 0;
	Arg myArgs[3];
	Boolean iconic = 0;

	TRACE(("TekInit\n"));
	XtSetArg(myArgs[nargs], XtNiconic, &iconic);
	++nargs;
	XtGetValues(toplevel, myArgs, nargs);

	nargs = 0;
	XtSetArg(myArgs[nargs], XtNiconic, iconic);
	++nargs;
	XtSetArg(myArgs[nargs], XtNallowShellResize, True);
	++nargs;
	XtSetArg(myArgs[nargs], XtNinput, True);
	++nargs;

	/* this causes the Initialize method to be called */
	tekshellwidget =
	    XtCreatePopupShell("tektronix", topLevelShellWidgetClass,
			       toplevel, myArgs, nargs);

	SetupMenus(tekshellwidget, &form_top, &menu_top, &menu_high);

	/* this causes the Realize method to be called */
	tekWidget = (TekWidget)
	    XtVaCreateManagedWidget("tek4014",
				    tekWidgetClass, form_top,
#if OPT_TOOLBAR
				    XtNmenuBar, menu_top,
				    XtNresizable, True,
				    XtNfromVert, menu_top,
				    XtNtop, XawChainTop,
				    XtNleft, XawChainLeft,
				    XtNright, XawChainRight,
				    XtNbottom, XawChainBottom,
				    XtNmenuHeight, menu_high,
#endif
				    (XtPointer) 0);
#if OPT_TOOLBAR
	ShowToolbar(resource.toolBar);
#endif
    }
    return (!Tfailed);
}

/*
 * If we haven't allocated the PtyData struct, do so.
 */
int
TekPtyData(void)
{
    if (Tpushb == 0) {
	if ((Tpushb = TypeMallocN(Char, 10)) == NULL
	    || (Tline = TypeMallocN(XSegment, MAX_VTX)) == NULL) {
	    fprintf(stderr, "%s: Not enough core for Tek mode\n", xterm_name);
	    if (Tpushb)
		free(Tpushb);
	    Tfailed = True;
	    return 0;
	}
    }
    return 1;
}

static void
Tekparse(TekWidget tw)
{
    TScreen *screen = TScreenOf(term);
    TekScreen *tekscr = TekScreenOf(tw);
    int x, y;
    IChar c = 0;
    IChar ch;
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
	TRACE(("Tekparse %04X -> %d\n", c, nextstate));

	switch (nextstate) {
	case CASE_REPORT:
	    TRACE(("case: report address\n"));
	    if (tekscr->TekGIN) {
		TekGINoff(tw);
		TekEnqMouse(tw, 0);
	    } else {
		c = 064;	/* has hard copy unit */
		if (tekscr->margin == MARGIN2)
		    c |= 02;
		TekEnq(tw, c, tekscr->cur_X, tekscr->cur_Y);
	    }
	    TekRecord->ptr[-1] = ANSI_NAK;	/* remove from recording */
	    Tparsestate = curstate;
	    break;

	case CASE_VT_MODE:
	    TRACE(("case: special return to vt102 mode\n"));
	    Tparsestate = curstate;
	    TekRecord->ptr[-1] = ANSI_NAK;	/* remove from recording */
	    FlushLog(&(term->screen));
	    return;

	case CASE_SPT_STATE:
	    TRACE(("case: Enter Special Point Plot mode\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    Tparsestate = curstate = Tspttable;
	    break;

	case CASE_GIN:
	    TRACE(("case: Do Tek GIN mode\n"));
	    tekscr->TekGIN = &TekRecord->ptr[-1];
	    /* Set cross-hair cursor raster array */
	    if ((GINcursor =
		 make_colored_cursor(XC_tcross,
				     T_COLOR(screen, MOUSE_FG),
				     T_COLOR(screen, MOUSE_BG))) != 0) {
		XDefineCursor(XtDisplay(tw), TWindow(tekscr),
			      GINcursor);
	    }
	    Tparsestate = Tbyptable;	/* Bypass mode */
	    break;

	case CASE_BEL:
	    TRACE(("case: BEL\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    if (!tekRefreshList)
		Bell(XkbBI_TerminalBell, 0);
	    Tparsestate = curstate;	/* clear bypass condition */
	    break;

	case CASE_BS:
	    TRACE(("case: BS\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    Tparsestate = curstate;	/* clear bypass condition */
	    TCursorBack(tw);
	    break;

	case CASE_PT_STATE:
	    TRACE(("case: Enter Tek Point Plot mode\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    Tparsestate = curstate = Tpttable;
	    break;

	case CASE_PLT_STATE:
	    TRACE(("case: Enter Tek Plot mode\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    Tparsestate = curstate = Tplttable;
	    if ((c = input()) == ANSI_BEL)
		tekscr->pen = PENDOWN;
	    else {
		unput(c);
		tekscr->pen = PENUP;
	    }
	    break;

	case CASE_TAB:
	    TRACE(("case: HT\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    Tparsestate = curstate;	/* clear bypass condition */
	    TCursorForward(tw);
	    break;

	case CASE_IPL_STATE:
	    TRACE(("case: Enter Tek Incremental Plot mode\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    Tparsestate = curstate = Tipltable;
	    break;

	case CASE_ALP_STATE:
	    TRACE(("case: Enter Tek Alpha mode from any other mode\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    /* if in one of graphics states, move alpha cursor */
	    if (nplot > 0)	/* flush line VTbuffer */
		TekFlush(tw);
	    Tparsestate = curstate = Talptable;
	    break;

	case CASE_UP:
	    TRACE(("case: cursor up\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    Tparsestate = curstate;	/* clear bypass condition */
	    TCursorUp(tw);
	    break;

	case CASE_COPY:
	    TRACE(("case: make copy\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    TekCopy(tw);
	    TekRecord->ptr[-1] = ANSI_NAK;	/* remove from recording */
	    Tparsestate = curstate;	/* clear bypass condition */
	    break;

	case CASE_PAGE:
	    TRACE(("case: Page Function\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    TekPage(tw);	/* clear bypass condition */
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
	    TekSetFontSize(tw, False, (int) (c & 03));
	    Tparsestate = curstate;
	    break;

	case CASE_BEAM_VEC:
	    TRACE(("case: beam and vector selector\n"));
	    /* only line types */
	    if ((c &= LINEMASK) != tekscr->cur.linetype) {
		if (nplot > 0)
		    TekFlush(tw);
		if (c <= TEKNUMLINES)
		    tekscr->cur.linetype = c;
	    }
	    Tparsestate = curstate;
	    break;

	case CASE_CURSTATE:
	    Tparsestate = curstate;
	    break;

	case CASE_PENUP:
	    TRACE(("case: Ipl: penup\n"));
	    tekscr->pen = PENUP;
	    break;

	case CASE_PENDOWN:
	    TRACE(("case: Ipl: pendown\n"));
	    tekscr->pen = PENDOWN;
	    break;

	case CASE_IPL_POINT:
	    TRACE(("case: Ipl: point\n"));
	    x = tekscr->cur_X;
	    y = tekscr->cur_Y;
	    if (c & NORTH)
		y++;
	    else if (c & SOUTH)
		y--;
	    if (c & EAST)
		x++;
	    else if (c & WEST)
		x--;
	    if (tekscr->pen == PENDOWN)
		TekDraw(tw, x, y);
	    else
		TekMove(tw, x, y);
	    break;

	case CASE_PLT_VEC:
	    TRACE(("case: Plt: vector\n"));
	    unput(c);
	    if (getpoint(tw)) {
		if (tekscr->pen == PENDOWN) {
		    TekDraw(tw, tekscr->cur.x, tekscr->cur.y);
		} else {
		    TekMove(tw, tekscr->cur.x, tekscr->cur.y);
		}
		tekscr->pen = PENDOWN;
	    }
	    break;

	case CASE_PT_POINT:
	    TRACE(("case: Pt: point\n"));
	    unput(c);
	    if (getpoint(tw)) {
		TekMove(tw, tekscr->cur.x, tekscr->cur.y);
		TekDraw(tw, tekscr->cur.x, tekscr->cur.y);
	    }
	    break;

	case CASE_SPT_POINT:
	    TRACE(("case: Spt: point\n"));
	    /* ignore intensity character in c */
	    if (getpoint(tw)) {
		TekMove(tw, tekscr->cur.x, tekscr->cur.y);
		TekDraw(tw, tekscr->cur.x, tekscr->cur.y);
	    }
	    break;

	case CASE_CR:
	    TRACE(("case: CR\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    if (nplot > 0)	/* flush line VTbuffer */
		TekFlush(tw);
	    tekscr->cur_X = tekscr->margin == MARGIN1 ? 0 :
		TEKWIDTH / 2;
	    Tparsestate = curstate = Talptable;
	    break;

	case CASE_ESC_STATE:
	    TRACE(("case: ESC\n"));
	    Tparsestate = Tesctable;
	    break;

	case CASE_LF:
	    TRACE(("case: LF\n"));
	    if (tekscr->TekGIN)
		TekGINoff(tw);
	    TCursorDown(tw);
	    if (!tekRefreshList)
		do_xevents();
	    break;

	case CASE_SP:
	    TRACE(("case: SP\n"));
	    TCursorForward(tw);
	    break;

	case CASE_PRINT:
	    TRACE(("case: printable character\n"));
	    ch = c;
	    c = (IChar) tekscr->cur.fontsize;
	    x = (int) (tekscr->cur_X * TekScale(tekscr))
		+ screen->border;
	    y = (int) ((TEKHEIGHT + TEKTOPPAD - tekscr->cur_Y) * TekScale(tekscr))
		+ screen->border;

#if OPT_WIDE_CHARS
	    if (screen->wide_chars
		&& (ch > 255)) {
		XChar2b sbuf;
		sbuf.byte2 = LO_BYTE(ch);
		sbuf.byte1 = HI_BYTE(ch);
		XDrawImageString16(XtDisplay(tw),
				   TWindow(tekscr),
				   tekscr->TnormalGC,
				   x,
				   y,
				   &sbuf,
				   1);
	    } else
#endif
	    {
		char ch2 = (char) ch;
		XDrawString(XtDisplay(tw),
			    TWindow(tekscr),
			    tekscr->TnormalGC,
			    x,
			    y,
			    &ch2,
			    1);
	    }
	    TCursorForward(tw);
	    break;
	case CASE_OSC:
	    /* FIXME:  someone should disentangle the input queues
	     * of this code so that it can be state-driven.
	     */
	    TRACE(("case: do osc escape\n"));
	    {
		/*
		 * do_osc() can call TekExpose(), which calls TekRefresh(),
		 * and sends us recurring here - don't do that...
		 */
		static int nested;

		Char buf2[512];
		IChar c2;
		unsigned len = 0;
		while ((c2 = input()) != ANSI_BEL) {
		    if (!isprint((int) (c2 & 0x7f))
			|| len + 2 >= (int) sizeof(buf2))
			break;
		    buf2[len++] = (Char) c2;
		}
		buf2[len] = 0;
		if (!nested++) {
		    if (c2 == ANSI_BEL)
			do_osc(term, buf2, len, ANSI_BEL);
		}
		--nested;
	    }
	    Tparsestate = curstate;
	    break;
	}
    }
}

static int rcnt;
static char *rptr;
static PtySelect Tselect_mask;

static IChar
Tinput(TekWidget tw)
{
    TekScreen *tekscr = TekScreenOf(tw);
    TScreen *screen = TScreenOf(term);
    TekLink *tek;

    if (Tpushback > Tpushb)
	return (*--Tpushback);
    if (tekRefreshList) {
	if (rcnt-- > 0)
	    return (IChar) (*rptr++);
	if ((tek = tekRefreshList->next) != 0) {
	    tekRefreshList = tek;
	    rptr = tek->data;
	    rcnt = tek->count - 1;
	    TekSetFontSize(tw, False, tek->fontsize);
	    return (IChar) (*rptr++);
	}
	tekRefreshList = (TekLink *) 0;
	longjmp(Tekjump, 1);
    }
  again:
    if (VTbuffer->next >= VTbuffer->last) {
	int update = VTbuffer->update;

	if (nplot > 0)		/* flush line */
	    TekFlush(tw);
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
	    if (readPtyData(screen, &Tselect_mask, VTbuffer)) {
		break;
	    }
	    if (Ttoggled && curstate == Talptable) {
		TCursorToggle(tw, TOGGLE);
		Ttoggled = False;
	    }
	    if (XtAppPending(app_con) & XtIMXEvent) {
#ifdef VMS
		Tselect_mask = X_mask;
#else /* VMS */
		XFD_COPYSET(&X_mask, &Tselect_mask);
#endif /* VMS */
	    } else {
		XFlush(XtDisplay(tw));
#ifdef VMS
		Tselect_mask = Select_mask;

#else /* VMS */
		XFD_COPYSET(&Select_mask, &Tselect_mask);
		if (Select(max_plus1, &Tselect_mask, NULL, NULL, NULL) < 0) {
		    if (errno != EINTR)
			SysError(ERROR_TSELECT);
		    continue;
		}
#endif /* VMS */
	    }
#ifdef VMS
	    if (Tselect_mask & X_mask) {
		xevents();
		if (VTbuffer->update != update)
		    goto again;
	    }
#else /* VMS */
	    if (FD_ISSET(ConnectionNumber(XtDisplay(tw)), &Tselect_mask)) {
		xevents();
		if (VTbuffer->update != update)
		    goto again;
	    }
#endif /* VMS */
	}
	if (!Ttoggled && curstate == Talptable) {
	    TCursorToggle(tw, TOGGLE);
	    Ttoggled = True;
	}
    }
    tek = TekRecord;
    if (tek->count >= TEK_LINK_BLOCK_SIZE
	|| tek->fontsize != tekscr->cur.fontsize) {
	if ((TekRecord = tek->next = CastMalloc(TekLink)) == 0)
	    Panic("Tinput: malloc error (%d)\n", errno);
	tek = tek->next;
	tek->next = (TekLink *) 0;
	tek->fontsize = (unsigned short) tekscr->cur.fontsize;
	tek->count = 0;
	tek->ptr = tek->data;
    }
    tek->count++;

    (void) morePtyData(screen, VTbuffer);
    return (IChar) (*tek->ptr++ = (char) nextPtyData(screen, VTbuffer));
}

static void
TekClear(TekWidget tw)
{
    TekScreen *tekscr = TekScreenOf(tw);

    if (TWindow(tekscr))
	XClearWindow(XtDisplay(tw), TWindow(tekscr));
}

/* this should become the Tek Widget's Resize proc */
static void
TekConfigure(Widget w)
{
    TekWidget tw = getTekWidget(w);
    if (tw != 0) {
	TekScreen *tekscr = TekScreenOf(tw);
	TScreen *screen = TScreenOf(term);
	int border = 2 * screen->border;
	double d;

	TekClear(tw);
	TWidth(tekscr) = w->core.width - border;
	THeight(tekscr) = w->core.height - border;
	TekScale(tekscr) = (double) TWidth(tekscr) / TEKWIDTH;
	if ((d = (double) THeight(tekscr) / (TEKHEIGHT + TEKTOPPAD + TEKBOTTOMPAD))
	    < TekScale(tekscr))
	    TekScale(tekscr) = d;
	TFullWidth(tekscr) = w->core.width;
	TFullHeight(tekscr) = w->core.height;
    }
}

/*ARGSUSED*/
void
TekExpose(Widget w,
	  XEvent * event GCC_UNUSED,
	  Region region GCC_UNUSED)
{
    TekWidget tw = getTekWidget(w);
    if (tw != 0) {
	TekScreen *tekscr = TekScreenOf(tw);

	TRACE(("TekExpose {{\n"));

#ifdef lint
	region = region;
#endif
	if (!Ttoggled)
	    TCursorToggle(tw, CLEAR);
	Ttoggled = True;
	Tpushback = Tpushb;
	tekscr->cur_X = 0;
	tekscr->cur_Y = TEKHOME;
	tekscr->cur = tekscr->page;
	TekSetFontSize(tw, False, tekscr->cur.fontsize);
	tekscr->margin = MARGIN1;
	if (tekscr->TekGIN) {
	    tekscr->TekGIN = NULL;
	    TekGINoff(tw);
	}
	tekRefreshList = &Tek0;
	rptr = tekRefreshList->data;
	rcnt = tekRefreshList->count;
	Tparsestate = curstate = Talptable;
	TRACE(("TekExpose resets data to replay %d bytes\n", rcnt));
	first_map_occurred();
	if (!tekscr->waitrefresh)
	    TekRefresh(tw);
	TRACE(("}} TekExpose\n"));
    }
}

void
TekRefresh(TekWidget tw)
{
    if (tw != 0) {
	TekScreen *tekscr = TekScreenOf(tw);
	TScreen *screen = TScreenOf(term);
	static Cursor wait_cursor = None;

	if (wait_cursor == None)
	    wait_cursor = make_colored_cursor(XC_watch,
					      T_COLOR(screen, MOUSE_FG),
					      T_COLOR(screen, MOUSE_BG));
	XDefineCursor(XtDisplay(tw), TWindow(tekscr), wait_cursor);
	XFlush(XtDisplay(tw));
	if (!setjmp(Tekjump))
	    Tekparse(tw);
	XDefineCursor(XtDisplay(tw), TWindow(tekscr),
		      (tekscr->TekGIN && GINcursor) ? GINcursor : tekscr->arrow);
    }
}

void
TekRepaint(TekWidget tw)
{
    TRACE(("TekRepaint\n"));
    TekClear(tw);
    TekExpose((Widget) tw, (XEvent *) NULL, (Region) NULL);
}

static void
TekPage(TekWidget tw)
{
    TekScreen *tekscr = TekScreenOf(tw);
    TekLink *tek;

    TekClear(tw);
    tekscr->cur_X = 0;
    tekscr->cur_Y = TEKHOME;
    tekscr->margin = MARGIN1;
    tekscr->page = tekscr->cur;
    if (tekscr->TekGIN)
	TekGINoff(tw);
    tek = TekRecord = &Tek0;
    tek->fontsize = (unsigned short) tekscr->cur.fontsize;
    tek->count = 0;
    tek->ptr = tek->data;
    tek = tek->next;
    if (tek)
	do {
	    TekLink *tek2 = tek->next;

	    free(tek);
	    tek = tek2;
	} while (tek);
    TekRecord->next = (TekLink *) 0;
    tekRefreshList = (TekLink *) 0;
    Ttoggled = True;
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
getpoint(TekWidget tw)
{
    int c, x, y, e, lo_y = 0;
    TekScreen *tekscr = TekScreenOf(tw);

    x = tekscr->cur.x;
    y = tekscr->cur.y;
    for (;;) {
	if ((c = (int) input()) < ' ') {	/* control character */
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
	    tekscr->cur.x = x;
	    tekscr->cur.y = y;
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
TCursorBack(TekWidget tw)
{
    TekScreen *tekscr = TekScreenOf(tw);
    struct Tek_Char *t;
    int x, l;

    x = (tekscr->cur_X -=
	 (t = &TekChar[tekscr->cur.fontsize])->hsize
	);

    if (((tekscr->margin == MARGIN1) && (x < 0))
	|| ((tekscr->margin == MARGIN2) && (x < TEKWIDTH / 2))) {
	if ((l = (tekscr->cur_Y + (t->vsize - 1)) / t->vsize + 1) >=
	    t->nlines) {
	    tekscr->margin = !tekscr->margin;
	    l = 0;
	}
	tekscr->cur_Y = l * t->vsize;
	tekscr->cur_X = (t->charsperline - 1) * t->hsize;
    }
}

static void
TCursorForward(TekWidget tw)
{
    TekScreen *tekscr = TekScreenOf(tw);
    struct Tek_Char *t;
    int l;

    if ((tekscr->cur_X +=
	 (t = &TekChar[tekscr->cur.fontsize])->hsize
	) > TEKWIDTH
	) {
	if ((l = tekscr->cur_Y / t->vsize - 1) < 0) {
	    tekscr->margin = !tekscr->margin;
	    l = t->nlines - 1;
	}
	tekscr->cur_Y = l * t->vsize;
	tekscr->cur_X = tekscr->margin == MARGIN1 ? 0 : TEKWIDTH / 2;
    }
}

static void
TCursorUp(TekWidget tw)
{
    TekScreen *tekscr = TekScreenOf(tw);
    struct Tek_Char *t;
    int l;

    t = &TekChar[tekscr->cur.fontsize];

    if ((l = (tekscr->cur_Y + (t->vsize - 1)) / t->vsize + 1) >= t->nlines) {
	l = 0;
	if ((tekscr->margin = !tekscr->margin) != MARGIN1) {
	    if (tekscr->cur_X < TEKWIDTH / 2)
		tekscr->cur_X += TEKWIDTH / 2;
	} else if (tekscr->cur_X >= TEKWIDTH / 2)
	    tekscr->cur_X -= TEKWIDTH / 2;
    }
    tekscr->cur_Y = l * t->vsize;
}

static void
TCursorDown(TekWidget tw)
{
    TekScreen *tekscr = TekScreenOf(tw);
    struct Tek_Char *t;
    int l;

    t = &TekChar[tekscr->cur.fontsize];

    if ((l = tekscr->cur_Y / t->vsize - 1) < 0) {
	l = t->nlines - 1;
	if ((tekscr->margin = !tekscr->margin) != MARGIN1) {
	    if (tekscr->cur_X < TEKWIDTH / 2)
		tekscr->cur_X += TEKWIDTH / 2;
	} else if (tekscr->cur_X >= TEKWIDTH / 2)
	    tekscr->cur_X -= TEKWIDTH / 2;
    }
    tekscr->cur_Y = l * t->vsize;
}

static void
AddToDraw(TekWidget tw, int x1, int y1, int x2, int y2)
{
    TekScreen *tekscr = TekScreenOf(tw);
    TScreen *screen = TScreenOf(term);
    XSegment *lp;

    TRACE(("AddToDraw (%d,%d) (%d,%d)\n", x1, y1, x2, y2));
    if (nplot >= MAX_PTS) {
	TekFlush(tw);
    }
    lp = line_pt++;
    lp->x1 = (short) (x1 * TekScale(tekscr) + screen->border);
    lp->y1 = (short) ((TEKHEIGHT + TEKTOPPAD - y1) * TekScale(tekscr) +
		      screen->border);
    lp->x2 = (short) (x2 * TekScale(tekscr) + screen->border);
    lp->y2 = (short) ((TEKHEIGHT + TEKTOPPAD - y2) * TekScale(tekscr) +
		      screen->border);
    nplot++;
    TRACE(("...AddToDraw %d points\n", nplot));
}

static void
TekDraw(TekWidget tw, int x, int y)
{
    TekScreen *tekscr = TekScreenOf(tw);

    if (nplot == 0 || T_lastx != tekscr->cur_X || T_lasty != tekscr->cur_Y) {
	/*
	 * We flush on each unconnected line segment if the line
	 * type is not solid.  This solves a bug in X when drawing
	 * points while the line type is not solid.
	 */
	if (nplot > 0 && tekscr->cur.linetype != SOLIDLINE)
	    TekFlush(tw);
    }
    AddToDraw(tw, tekscr->cur_X, tekscr->cur_Y, x, y);
    T_lastx = tekscr->cur_X = x;
    T_lasty = tekscr->cur_Y = y;
}

static void
TekFlush(TekWidget tw)
{
    TekScreen *tekscr = TekScreenOf(tw);

    TRACE(("TekFlush\n"));
    XDrawSegments(XtDisplay(tw), TWindow(tekscr),
		  ((tekscr->cur.linetype == SOLIDLINE)
		   ? tekscr->TnormalGC
		   : tekscr->linepat[tekscr->cur.linetype - 1]),
		  Tline, nplot);
    nplot = 0;
    line_pt = Tline;
}

void
TekGINoff(TekWidget tw)
{
    TekScreen *tekscr = TekScreenOf(tw);

    TRACE(("TekGINoff\n"));
    XDefineCursor(XtDisplay(tw), TWindow(tekscr), tekscr->arrow);
    if (GINcursor)
	XFreeCursor(XtDisplay(tw), GINcursor);
    if (tekscr->TekGIN) {
	*tekscr->TekGIN = ANSI_CAN;	/* modify recording */
	tekscr->TekGIN = NULL;
    }
}

void
TekEnqMouse(TekWidget tw, int c)	/* character pressed */
{
    TekScreen *tekscr = TekScreenOf(tw);
    TScreen *screen = TScreenOf(term);
    int mousex, mousey, rootx, rooty;
    unsigned int mask;		/* XQueryPointer */
    Window root, subw;

    TRACE(("TekEnqMouse\n"));
    XQueryPointer(
		     XtDisplay(tw), TWindow(tekscr),
		     &root, &subw,
		     &rootx, &rooty,
		     &mousex, &mousey,
		     &mask);
    if ((mousex = (int) ((mousex - screen->border) / TekScale(tekscr))) < 0)
	mousex = 0;
    else if (mousex >= TEKWIDTH)
	mousex = TEKWIDTH - 1;
    if ((mousey = (int) (TEKHEIGHT + TEKTOPPAD - (mousey - screen->border) /
			 TekScale(tekscr))) < 0)
	mousey = 0;
    else if (mousey >= TEKHEIGHT)
	mousey = TEKHEIGHT - 1;
    TekEnq(tw, (unsigned) c, mousex, mousey);
}

static void
TekEnq(TekWidget tw,
       unsigned status,
       int x,
       int y)
{
    TekScreen *tekscr = TekScreenOf(tw);
    TScreen *screen = TScreenOf(term);
    Char cplot[7];
    int len = 5;
    int adj = (status != 0) ? 0 : 1;

    TRACE(("TekEnq\n"));
    cplot[0] = (Char) status;
    /* Translate x and y to Tektronix code */
    cplot[1] = (Char) (040 | ((x >> SHIFTHI) & FIVEBITS));
    cplot[2] = (Char) (040 | ((x >> SHIFTLO) & FIVEBITS));
    cplot[3] = (Char) (040 | ((y >> SHIFTHI) & FIVEBITS));
    cplot[4] = (Char) (040 | ((y >> SHIFTLO) & FIVEBITS));

    if (tekscr->gin_terminator != GIN_TERM_NONE)
	cplot[len++] = '\r';
    if (tekscr->gin_terminator == GIN_TERM_EOT)
	cplot[len++] = '\004';
#ifdef VMS
    tt_write(cplot + adj, len - adj);
#else /* VMS */
    v_write(screen->respond, cplot + adj, (unsigned) (len - adj));
#endif /* VMS */
}

void
TekRun(void)
{
    if (tekWidget == 0) {
	TekInit();
    }
    if (tekWidget != 0) {
	TRACE(("TekRun ...\n"));

	if (!TEK4014_SHOWN(term)) {
	    set_tek_visibility(True);
	}
	update_vttekmode();
	update_vtshow();
	update_tekshow();
	set_tekhide_sensitivity();

	Tpushback = Tpushb;
	Ttoggled = True;
	if (!setjmp(Tekend))
	    Tekparse(tekWidget);
	if (!Ttoggled) {
	    TCursorToggle(tekWidget, TOGGLE);
	    Ttoggled = True;
	}
	TEK4014_ACTIVE(term) = False;
    } else {
	TEK4014_ACTIVE(term) = False;
	if (VWindow(&(term->screen)) == 0) {
	    Exit(ERROR_TINIT);
	}
    }
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
	      Cardinal *num_args GCC_UNUSED)
{
    Widget tekparent = SHELL_OF(wnew);

    TRACE(("TekInitialize\n"));

    /* look for focus related events on the shell, because we need
     * to care about the shell's border being part of our focus.
     */
    XtAddEventHandler(tekparent, EnterWindowMask, False,
		      HandleEnterWindow, (Opaque) 0);
    XtAddEventHandler(tekparent, LeaveWindowMask, False,
		      HandleLeaveWindow, (Opaque) 0);
    XtAddEventHandler(tekparent, FocusChangeMask, False,
		      HandleFocusChange, (Opaque) 0);
    XtAddEventHandler(wnew, PropertyChangeMask, False,
		      HandleBellPropertyChange, (Opaque) 0);

#ifndef NO_ACTIVE_ICON
    ((TekWidget) wnew)->screen.whichTwin = &((TekWidget) wnew)->screen.fullTwin;
#endif /* NO_ACTIVE_ICON */

}

static void
TekRealize(Widget gw,
	   XtValueMask * valuemaskp,
	   XSetWindowAttributes * values)
{
    TekWidget tw = (TekWidget) gw;
    TekScreen *tekscr = TekScreenOf(tw);
    TScreen *screen = TScreenOf(term);
    int i;
    TekLink *tek;
    double d;
    int border = 2 * screen->border;
    int pr;
    XGCValues gcv;
    int winX, winY;
    unsigned width, height;
    char Tdefault[32];
    unsigned long TEKgcFontMask;

    TRACE(("TekRealize\n"));
    memset(tekscr, 0, sizeof(tekscr));

#ifndef NO_ACTIVE_ICON
    tekscr->whichTwin = &tekscr->fullTwin;
#endif /* NO_ACTIVE_ICON */

    BorderPixel(tw) = BorderPixel(term);

    tekscr->arrow = make_colored_cursor(XC_left_ptr,
					T_COLOR(screen, MOUSE_FG),
					T_COLOR(screen, MOUSE_BG));

    for (i = 0; i < TEKNUMFONTS; i++) {
	if (!tw->tek.Tfont[i]) {
	    tw->tek.Tfont[i] = XQueryFont(XtDisplay(tw), DefaultGCID);
	}
	TRACE(("Tfont[%d] %dx%d\n",
	       i,
	       tw->tek.Tfont[i]->ascent +
	       tw->tek.Tfont[i]->descent,
	       tw->tek.Tfont[i]->max_bounds.width));
	tw->tek.tobaseline[i] = tw->tek.Tfont[i]->ascent;
    }

    if (!TekPtyData())
	return;

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
    width = (unsigned) (TEKDEFWIDTH + border);
    height = (unsigned) (TEKDEFHEIGHT + border);

    TRACE(("parsing T_geometry %s\n", NonNull(term->misc.T_geometry)));
    pr = XParseGeometry(term->misc.T_geometry,
			&winX,
			&winY,
			&width,
			&height);
    TRACE(("... position %d,%d size %dx%d\n", winY, winX, height, width));
    if ((pr & XValue) && (pr & XNegative))
	winX += DisplayWidth(XtDisplay(tw), DefaultScreen(XtDisplay(tw)))
	    - (int) width - (BorderWidth(SHELL_OF(term)) * 2);
    if ((pr & YValue) && (pr & YNegative))
	winY += DisplayHeight(XtDisplay(tw), DefaultScreen(XtDisplay(tw)))
	    - (int) height - (BorderWidth(SHELL_OF(term)) * 2);

    /* set up size hints */
    tw->hints.min_width = TEKMINWIDTH + border;
    tw->hints.min_height = TEKMINHEIGHT + border;
    tw->hints.width_inc = 1;
    tw->hints.height_inc = 1;
    tw->hints.flags = PMinSize | PResizeInc;
    tw->hints.x = winX;
    tw->hints.y = winY;
    if ((XValue & pr) || (YValue & pr)) {
	tw->hints.flags |= USSize | USPosition;
	tw->hints.flags |= PWinGravity;
	switch (pr & (XNegative | YNegative)) {
	case 0:
	    tw->hints.win_gravity = NorthWestGravity;
	    break;
	case XNegative:
	    tw->hints.win_gravity = NorthEastGravity;
	    break;
	case YNegative:
	    tw->hints.win_gravity = SouthWestGravity;
	    break;
	default:
	    tw->hints.win_gravity = SouthEastGravity;
	    break;
	}
    } else {
	/* set a default size, but do *not* set position */
	tw->hints.flags |= PSize;
    }
    tw->hints.width = (int) width;
    tw->hints.height = (int) height;
    if ((WidthValue & pr) || (HeightValue & pr))
	tw->hints.flags |= USSize;
    else
	tw->hints.flags |= PSize;

    (void) REQ_RESIZE((Widget) tw,
		      (Dimension) width, (Dimension) height,
		      &tw->core.width, &tw->core.height);

    /* XXX This is bogus.  We are parsing geometries too late.  This
     * is information that the shell widget ought to have before we get
     * realized, so that it can do the right thing.
     */
    if (tw->hints.flags & USPosition)
	XMoveWindow(XtDisplay(tw), TShellWindow, tw->hints.x, tw->hints.y);

    XSetWMNormalHints(XtDisplay(tw), TShellWindow, &tw->hints);
    XFlush(XtDisplay(tw));	/* get it out to window manager */

    values->win_gravity = NorthWestGravity;
    values->background_pixel = T_COLOR(screen, TEK_BG);

    XtWindow(tw) = TWindow(tekscr) =
	XCreateWindow(XtDisplay(tw),
		      XtWindow(SHELL_OF(tw)),
		      tw->core.x, tw->core.y,
		      tw->core.width, tw->core.height,
		      BorderWidth(tw),
		      (int) tw->core.depth,
		      InputOutput, CopyFromParent,
		      ((*valuemaskp) | CWBackPixel | CWWinGravity),
		      values);

    TFullWidth(tekscr) = (Dimension) width;
    TFullHeight(tekscr) = (Dimension) height;
    TWidth(tekscr) = (int) width - border;
    THeight(tekscr) = (int) height - border;
    TekScale(tekscr) = (double) TWidth(tekscr) / TEKWIDTH;
    if ((d = (double) THeight(tekscr) / (TEKHEIGHT + TEKTOPPAD +
					 TEKBOTTOMPAD)) < TekScale(tekscr))
	TekScale(tekscr) = d;

    tekscr->cur.fontsize = TEK_FONT_LARGE;
    if (tw->tek.initial_font) {
	int result = TekGetFontSize(tw->tek.initial_font);
	if (result >= 0)
	    tekscr->cur.fontsize = result;
    }
#define TestGIN(s) XmuCompareISOLatin1(tw->tek.gin_terminator_str, s)

    if (TestGIN(GIN_TERM_NONE_STR) == 0)
	tekscr->gin_terminator = GIN_TERM_NONE;
    else if (TestGIN(GIN_TERM_CR_STR) == 0)
	tekscr->gin_terminator = GIN_TERM_CR;
    else if (TestGIN(GIN_TERM_EOT_STR) == 0)
	tekscr->gin_terminator = GIN_TERM_EOT;
    else
	fprintf(stderr, "%s: illegal GIN terminator setting \"%s\"\n",
		xterm_name, tw->tek.gin_terminator_str);

    gcv.graphics_exposures = True;	/* default */
    gcv.font = tw->tek.Tfont[tekscr->cur.fontsize]->fid;
    gcv.foreground = T_COLOR(screen, TEK_FG);
    gcv.background = T_COLOR(screen, TEK_BG);

    /* if font wasn't successfully opened, then gcv.font will contain
       the Default GC's ID, meaning that we must use the server default font.
     */
    TEKgcFontMask = (unsigned long) ((gcv.font == DefaultGCID) ? 0 : GCFont);
    tekscr->TnormalGC = XCreateGC(XtDisplay(tw), TWindow(tekscr),
				  (TEKgcFontMask | GCGraphicsExposures |
				   GCForeground | GCBackground),
				  &gcv);

    gcv.function = GXinvert;
    gcv.plane_mask = (T_COLOR(screen, TEK_BG) ^
		      T_COLOR(screen, TEK_CURSOR));
    gcv.join_style = JoinMiter;	/* default */
    gcv.line_width = 1;
    tekscr->TcursorGC = XCreateGC(XtDisplay(tw), TWindow(tekscr),
				  (GCFunction | GCPlaneMask), &gcv);

    gcv.foreground = T_COLOR(screen, TEK_FG);
    gcv.line_style = LineOnOffDash;
    gcv.line_width = 0;
    for (i = 0; i < TEKNUMLINES; i++) {
	tekscr->linepat[i] = XCreateGC(XtDisplay(tw), TWindow(tekscr),
				       (GCForeground | GCLineStyle), &gcv);
	XSetDashes(XtDisplay(tw), tekscr->linepat[i], 0,
		   (char *) dashes[i], dash_length[i]);
    }

    TekBackground(tw, screen);

    tekscr->margin = MARGIN1;	/* Margin 1             */
    tekscr->TekGIN = False;	/* GIN off              */

    XDefineCursor(XtDisplay(tw), TWindow(tekscr), tekscr->arrow);

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
	tek_icon_name = XtMalloc((Cardinal) strlen(icon_name) + 7);
	strcpy(tek_icon_name, icon_name);
	strcat(tek_icon_name, "(Tek)");
	tek_title = XtMalloc((Cardinal) strlen(title) + 7);
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
    tek->fontsize = (unsigned short) tekscr->cur.fontsize;
    tek->count = 0;
    tek->ptr = tek->data;
    Tpushback = Tpushb;
    tekscr->cur_X = 0;
    tekscr->cur_Y = TEKHOME;
    line_pt = Tline;
    Ttoggled = True;
    tekscr->page = tekscr->cur;
    return;
}

int
TekGetFontSize(const char *param)
{
    int result;

    if (XmuCompareISOLatin1(param, "l") == 0 ||
	XmuCompareISOLatin1(param, "large") == 0)
	result = TEK_FONT_LARGE;
    else if (XmuCompareISOLatin1(param, "2") == 0 ||
	     XmuCompareISOLatin1(param, "two") == 0)
	result = TEK_FONT_2;
    else if (XmuCompareISOLatin1(param, "3") == 0 ||
	     XmuCompareISOLatin1(param, "three") == 0)
	result = TEK_FONT_3;
    else if (XmuCompareISOLatin1(param, "s") == 0 ||
	     XmuCompareISOLatin1(param, "small") == 0)
	result = TEK_FONT_SMALL;
    else
	result = -1;

    return result;
}

void
TekSetFontSize(TekWidget tw, Bool fromMenu, int newitem)
{
    if (tw != 0) {
	TekScreen *tekscr = TekScreenOf(tw);
	int oldsize = tekscr->cur.fontsize;
	int newsize = MI2FS(newitem);
	Font fid;

	TRACE(("TekSetFontSize(%d) size %d ->%d\n", newitem, oldsize, newsize));
	if (newsize < 0 || newsize >= TEKNUMFONTS) {
	    Bell(XkbBI_MinorError, 0);
	} else if (oldsize != newsize) {
	    if (!Ttoggled)
		TCursorToggle(tw, TOGGLE);
	    set_tekfont_menu_item(oldsize, False);

	    tekscr->cur.fontsize = newsize;
	    if (fromMenu)
		tekscr->page.fontsize = newsize;

	    fid = tw->tek.Tfont[newsize]->fid;
	    if (fid == DefaultGCID) {
		/* we didn't succeed in opening a real font
		   for this size.  Instead, use server default. */
		XCopyGC(XtDisplay(tw),
			DefaultGC(XtDisplay(tw), DefaultScreen(XtDisplay(tw))),
			GCFont, tekscr->TnormalGC);
	    } else {
		XSetFont(XtDisplay(tw), tekscr->TnormalGC, fid);
	    }

	    set_tekfont_menu_item(newsize, True);
	    if (!Ttoggled)
		TCursorToggle(tw, TOGGLE);

	    if (fromMenu) {
		/* we'll get an exposure event after changing fontsize, so we
		 * have to clear the screen to avoid painting over the previous
		 * text.
		 */
		TekClear(tw);
	    }
	}
    }
}

void
ChangeTekColors(TekWidget tw, TScreen * screen, ScrnColors * pNew)
{
    TekScreen *tekscr = TekScreenOf(tw);
    int i;
    XGCValues gcv;

    if (COLOR_DEFINED(pNew, TEK_FG)) {
	T_COLOR(screen, TEK_FG) = COLOR_VALUE(pNew, TEK_FG);
	TRACE(("... TEK_FG: %#lx\n", T_COLOR(screen, TEK_FG)));
    }
    if (COLOR_DEFINED(pNew, TEK_BG)) {
	T_COLOR(screen, TEK_BG) = COLOR_VALUE(pNew, TEK_BG);
	TRACE(("... TEK_BG: %#lx\n", T_COLOR(screen, TEK_BG)));
    }
    if (COLOR_DEFINED(pNew, TEK_CURSOR)) {
	T_COLOR(screen, TEK_CURSOR) = COLOR_VALUE(pNew, TEK_CURSOR);
	TRACE(("... TEK_CURSOR: %#lx\n", T_COLOR(screen, TEK_CURSOR)));
    } else {
	T_COLOR(screen, TEK_CURSOR) = T_COLOR(screen, TEK_FG);
	TRACE(("... TEK_CURSOR: %#lx\n", T_COLOR(screen, TEK_CURSOR)));
    }

    if (tw) {
	XSetForeground(XtDisplay(tw), tekscr->TnormalGC,
		       T_COLOR(screen, TEK_FG));
	XSetBackground(XtDisplay(tw), tekscr->TnormalGC,
		       T_COLOR(screen, TEK_BG));
	if (BorderPixel(tw) == T_COLOR(screen, TEK_BG)) {
	    BorderPixel(tw) = T_COLOR(screen, TEK_FG);
	    BorderPixel(XtParent(tw)) = T_COLOR(screen, TEK_FG);
	    if (XtWindow(XtParent(tw)))
		XSetWindowBorder(XtDisplay(tw),
				 XtWindow(XtParent(tw)),
				 BorderPixel(tw));
	}

	for (i = 0; i < TEKNUMLINES; i++) {
	    XSetForeground(XtDisplay(tw), tekscr->linepat[i],
			   T_COLOR(screen, TEK_FG));
	}

	gcv.plane_mask = (T_COLOR(screen, TEK_BG) ^
			  T_COLOR(screen, TEK_CURSOR));
	XChangeGC(XtDisplay(tw), tekscr->TcursorGC, GCPlaneMask, &gcv);
	TekBackground(tw, screen);
    }
    return;
}

void
TekReverseVideo(TekWidget tw)
{
    TScreen *screen = TScreenOf(term);
    TekScreen *tekscr = TekScreenOf(tw);
    int i;
    Pixel tmp;
    XGCValues gcv;

    EXCHANGE(T_COLOR(screen, TEK_FG), T_COLOR(screen, TEK_BG), tmp);

    T_COLOR(screen, TEK_CURSOR) = T_COLOR(screen, TEK_FG);

    if (tw) {
	XSetForeground(XtDisplay(tw), tekscr->TnormalGC, T_COLOR(screen, TEK_FG));
	XSetBackground(XtDisplay(tw), tekscr->TnormalGC, T_COLOR(screen, TEK_BG));

	if (BorderPixel(tw) == T_COLOR(screen, TEK_BG)) {
	    BorderPixel(tw) = T_COLOR(screen, TEK_FG);
	    BorderPixel(XtParent(tw)) = T_COLOR(screen, TEK_FG);
	    if (XtWindow(XtParent(tw)))
		XSetWindowBorder(XtDisplay(tw),
				 XtWindow(XtParent(tw)),
				 BorderPixel(tw));
	}

	for (i = 0; i < TEKNUMLINES; i++) {
	    XSetForeground(XtDisplay(tw), tekscr->linepat[i],
			   T_COLOR(screen, TEK_FG));
	}

	gcv.plane_mask = (T_COLOR(screen, TEK_BG) ^
			  T_COLOR(screen, TEK_CURSOR));
	XChangeGC(XtDisplay(tw), tekscr->TcursorGC, GCPlaneMask, &gcv);
	TekBackground(tw, screen);
    }
}

static void
TekBackground(TekWidget tw, TScreen * screen)
{
    TekScreen *tekscr = TekScreenOf(tw);

    if (TWindow(tekscr))
	XSetWindowBackground(XtDisplay(tw), TWindow(tekscr),
			     T_COLOR(screen, TEK_BG));
}

/*
 * Toggles cursor on or off at cursor position in screen.
 */
void
TCursorToggle(TekWidget tw, int toggle)		/* TOGGLE or CLEAR */
{
    TekScreen *tekscr = TekScreenOf(tw);
    TScreen *screen = TScreenOf(term);
    int c, x, y;
    unsigned int cellwidth, cellheight;

    if (!TEK4014_SHOWN(term))
	return;

    TRACE(("TCursorToggle %s\n", (toggle == TOGGLE) ? "toggle" : "clear"));
    c = tekscr->cur.fontsize;
    cellwidth = (unsigned) tw->tek.Tfont[c]->max_bounds.width;
    cellheight = (unsigned) (tw->tek.Tfont[c]->ascent +
			     tw->tek.Tfont[c]->descent);

    x = (int) ((tekscr->cur_X * TekScale(tekscr)) + screen->border);
    y = (int) (((TEKHEIGHT + TEKTOPPAD - tekscr->cur_Y) * TekScale(tekscr))
	       + screen->border - tw->tek.tobaseline[c]);

    if (toggle == TOGGLE) {
	if (screen->select || screen->always_highlight)
	    XFillRectangle(XtDisplay(tw), TWindow(tekscr),
			   tekscr->TcursorGC, x, y,
			   cellwidth, cellheight);
	else {			/* fix to use different GC! */
	    XDrawRectangle(XtDisplay(tw), TWindow(tekscr),
			   tekscr->TcursorGC, x, y,
			   cellwidth - 1, cellheight - 1);
	}
    } else {
	/* Clear the entire rectangle, even though we may only
	 * have drawn an outline.  This fits with our refresh
	 * scheme of redrawing the entire window on any expose
	 * event and is easier than trying to figure out exactly
	 * which part of the cursor needs to be erased.
	 */
	XClearArea(XtDisplay(tw), TWindow(tekscr), x, y,
		   cellwidth, cellheight, False);
    }
}

void
TekSimulatePageButton(TekWidget tw, Bool reset)
{
    if (tw != 0) {
	TekScreen *tekscr = TekScreenOf(tw);

	if (reset) {
	    memset(&tekscr->cur, 0, sizeof tekscr->cur);
	}
	tekRefreshList = (TekLink *) 0;
	TekPage(tw);
	tekscr->cur_X = 0;
	tekscr->cur_Y = TEKHOME;
    }
}

/* write copy of screen to a file */

void
TekCopy(TekWidget tw)
{
    if (tw != 0) {
	TekScreen *tekscr = TekScreenOf(tw);
	TScreen *screen = TScreenOf(term);

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

	tekcopyfd = open_userfile(screen->uid, screen->gid, buf, False);
	if (tekcopyfd >= 0) {
	    sprintf(initbuf, "%c%c%c%c",
		    ANSI_ESC, (char) (tekscr->page.fontsize + '8'),
		    ANSI_ESC, (char) (tekscr->page.linetype + '`'));
	    IGNORE_RC(write(tekcopyfd, initbuf, (size_t) 4));
	    Tp = &Tek0;
	    do {
		IGNORE_RC(write(tekcopyfd, Tp->data, (size_t) Tp->count));
		Tp = Tp->next;
	    } while (Tp);
	    close(tekcopyfd);
	}
    }
}

/*ARGSUSED*/
void
HandleGINInput(Widget w,
	       XEvent * event GCC_UNUSED,
	       String * param_list,
	       Cardinal *nparamsp)
{
    TekWidget tw = getTekWidget(w);
    if (tw != 0) {
	TekScreen *tekscr = TekScreenOf(tw);

	if (tekscr->TekGIN && *nparamsp == 1) {
	    int c = param_list[0][0];
	    switch (c) {
	    case 'l':
	    case 'm':
	    case 'r':
	    case 'L':
	    case 'M':
	    case 'R':
		break;
	    default:
		Bell(XkbBI_MinorError, 0);	/* let them know they goofed */
		c = 'l';	/* provide a default */
	    }
	    TekEnqMouse(tw, c | 0x80);
	    TekGINoff(tw);
	} else {
	    Bell(XkbBI_MinorError, 0);
	}
    }
}

/*
 * Check if the current widget, or any parent, is the VT100 "xterm" widget.
 */
TekWidget
getTekWidget(Widget w)
{
    TekWidget xw;

    if (w == 0) {
	xw = (TekWidget) CURRENT_EMU();
	if (!IsTekWidget(xw)) {
	    xw = 0;
	}
    } else if (IsTekWidget(w)) {
	xw = (TekWidget) w;
    } else {
	xw = getTekWidget(XtParent(w));
    }
    TRACE2(("getTekWidget %p -> %p\n", w, xw));
    return xw;
}
