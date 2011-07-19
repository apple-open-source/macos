/* $XConsortium: xfontsel.c,v 1.35 94/04/17 20:43:41 rws Exp $ */
/*

Copyright (c) 1985-1989  X Consortium

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

Author:	Ralph R. Swick, DEC/MIT Project Athena
	one weekend in November, 1989
Modified: Mark Leisher <mleisher@crl.nmsu.edu> to deal with UCS sample text.
*/
/* $XFree86: xc/programs/xfontsel/xfontsel.c,v 1.7tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/StdSel.h>
#include <X11/Xfuncs.h>
#include "ULabel.h"

#define MIN_APP_DEFAULTS_VERSION 1
#define FIELD_COUNT 14
#define DELIM '-'

/* number of font names to parse in each background iteration */
#ifndef PARSE_QUANTUM
#define PARSE_QUANTUM 25
#endif

#define NZ NULL,ZERO
#define BACKGROUND 10

void GetFontNames(XtPointer closure);
Boolean Matches(String pattern, String fontName, Boolean fields[], int *maxfields);
Boolean DoWorkPiece(XtPointer closure);
void Quit(Widget w, XtPointer closure, XtPointer callData);
void OwnSelection(Widget w, XtPointer closure, XtPointer callData);
void SelectField(Widget w, XtPointer closure, XtPointer callData);
void ParseFontNames(XtPointer closure);
void SortFields(XtPointer closure);
void FixScalables(XtPointer closure);
void MakeFieldMenu(XtPointer closure);
void SelectValue(Widget w, XtPointer closure, XtPointer callData);
void AnyValue(Widget w, XtPointer closure, XtPointer callData);
void EnableOtherValues(Widget w, XtPointer closure, XtPointer callData);
void EnableMenu(XtPointer closure);
void SetCurrentFont(XtPointer closure);
void QuitAction(Widget w, XEvent *event, String *params, Cardinal *num_params);

static XtActionsRec xfontsel_actions[] = {
    {"Quit",	    QuitAction}
};

static Atom wm_delete_window;

Boolean IsXLFDFontName(String fontName);

typedef void (*XtProc)(XtPointer closure);

static struct _appRes {
    int app_defaults_version;
    Cursor cursor;
    String pattern;
    String pixelSizeList;
    String pointSizeList;
    Boolean print_on_quit;
    String sample_text;
    String sample_text16;
    String sample_textUCS;
    Boolean scaled_fonts;
} AppRes;

#define DEFAULTPATTERN "-*-*-*-*-*-*-*-*-*-*-*-*-*-*"

static XtResource resources[] = {
    { "cursor", "Cursor", XtRCursor, sizeof(Cursor),
		XtOffsetOf( struct _appRes, cursor ),
		XtRImmediate, NULL },
    { "pattern", "Pattern", XtRString, sizeof(String),
		XtOffsetOf( struct _appRes, pattern ),
		XtRString, (XtPointer)DEFAULTPATTERN },
    { "pixelSizeList", "PixelSizeList", XtRString, sizeof(String),
                XtOffsetOf( struct _appRes, pixelSizeList ),
                XtRString, (XtPointer)"" },
    { "pointSizeList", "PointSizeList", XtRString, sizeof(String),
                XtOffsetOf( struct _appRes, pointSizeList ),
                XtRString, (XtPointer)"" },
    { "printOnQuit", "PrintOnQuit", XtRBoolean, sizeof(Boolean),
	  	XtOffsetOf( struct _appRes, print_on_quit ),
      		XtRImmediate, (XtPointer)False },
    { "appDefaultsVersion", "AppDefaultsVersion", XtRInt, sizeof(int),
		XtOffsetOf( struct _appRes, app_defaults_version ),
		XtRImmediate, (XtPointer)0 },
    { "sampleText", "Text", XtRString, sizeof(String),
		XtOffsetOf( struct _appRes, sample_text ),
		XtRString, (XtPointer)"" },
    { "sampleText16", "Text16", XtRString, sizeof(String),
		XtOffsetOf( struct _appRes, sample_text16 ),
		XtRString, (XtPointer)"" },
    { "sampleTextUCS", "TextUCS", XtRString, sizeof(String),
		XtOffsetOf( struct _appRes, sample_textUCS ),
		XtRString, (XtPointer)"" },
    { "scaledFonts", "ScaledFonts", XtRBoolean, sizeof(Boolean),
		XtOffsetOf( struct _appRes, scaled_fonts ),
		XtRImmediate, (XtPointer)False },
};

static XrmOptionDescRec options[] = {
{"-pattern",	"pattern",	XrmoptionSepArg,	NULL},
{"-print",	"printOnQuit",	XrmoptionNoArg,		"True"},
{"-sample",	"sampleText",	XrmoptionSepArg,	NULL},
{"-sample16",	"sampleText16",	XrmoptionSepArg,	NULL},
{"-sampleUCS",	"sampleTextUCS",XrmoptionSepArg,	NULL},
{"-scaled",	"scaledFonts",	XrmoptionNoArg,		"True"},
};

static void Syntax(char *call)
{
    fprintf (stderr, "usage:  %s [-options ...] -fn font\n\n", call);
    fprintf (stderr, "where options include:\n");
    fprintf (stderr,
	"    -display dpy           X server to contact\n");
    fprintf (stderr, 
	"    -geometry geom         size and location of window\n");
    fprintf (stderr, 
	"    -pattern fontspec      font name pattern to match against\n");
    fprintf (stderr, 
	"    -print                 print selected font name on exit\n");
    fprintf (stderr, 
	"    -sample string         sample text to use for 1-byte fonts\n");
    fprintf (stderr, 
	"    -sample16 string       sample text to use for 2-byte fonts\n");
    fprintf (stderr,
	"    -sampleUCS string      sample text to use for ISO10646 fonts\n");
    fprintf (stderr, 
	"    -scaled                use scaled instances of fonts\n");
    fprintf (stderr, "\n");
    exit (1);
}


typedef struct FieldValue FieldValue;
struct FieldValue {
    int field;
    String string;
    Widget menu_item;
    int count;			/* of fonts */
    int allocated;
    int *font;
    Boolean enable;
};


typedef struct FieldValueList FieldValueList;
struct FieldValueList {
    int count;			/* of values */
    int allocated;
    Boolean show_unselectable;
    FieldValue value[1];	/* really [allocated] */
};


typedef struct FontValues FontValues;
struct FontValues {
    int value_index[FIELD_COUNT];
};


typedef struct FieldMenuRec FieldMenuRec;
struct FieldMenuRec {
    int field;
    Widget button;
};


typedef struct Choice Choice;
struct Choice {
    Choice *prev;
    FieldValue *value;
};


static XtResource menuResources[] = {
    { "showUnselectable", "ShowUnselectable", XtRBoolean, sizeof(Boolean),
		XtOffsetOf( FieldValueList, show_unselectable ),
		XtRImmediate, (XtPointer)True },
};


typedef enum {ValidateCurrentField, SkipCurrentField} ValidateAction;

static void EnableAllItems(int field);
static void EnableRemainingItems(ValidateAction current_field_action);
static void FlushXqueue(Display *dpy);
static void MarkInvalidFonts(Boolean *set, FieldValue *val);
static void ScheduleWork(XtProc proc, XtPointer closure, int priority);
static void SetCurrentFontCount(void);
static void SetNoFonts(void);
static void SetParsingFontCount(int count);

static XtAppContext appCtx;
static int numFonts;
static int numBadFonts;
static FontValues *fonts;
static int *scaledFonts;
static int numScaledFonts;
static FieldValueList *fieldValues[FIELD_COUNT];
static FontValues currentFont;
static int matchingFontCount;
static Boolean anyDisabled = False;
static Widget ownButton;
static Widget fieldBox;
static Widget countLabel;
static Widget currentFontName;
static String currentFontNameString;
static int currentFontNameSize;
static Widget sampleText;
static int textEncoding = -1;
static XFontStruct *sampleFont = NULL;
static Boolean *fontInSet;
static Choice *choiceList = NULL;
static int enabledMenuIndex;
static Boolean patternFieldSpecified[FIELD_COUNT]; /* = 0 */

int
main(int argc, char **argv)
{
    Widget topLevel, pane;

    XtSetLanguageProc(NULL, (XtLanguageProc) NULL, NULL);

    topLevel = XtAppInitialize(&appCtx, "XFontSel", options, XtNumber(options),
			       &argc, argv, NULL, NULL, 0);

    if (argc != 1) Syntax(argv[0]);

    XtAppAddActions(appCtx, xfontsel_actions, XtNumber(xfontsel_actions));
    XtOverrideTranslations
	(topLevel, XtParseTranslationTable ("<Message>WM_PROTOCOLS: Quit()"));

    XtGetApplicationResources( topLevel, (XtPointer)&AppRes,
			       resources, XtNumber(resources), NZ );
    if (AppRes.app_defaults_version < MIN_APP_DEFAULTS_VERSION) {
	XrmDatabase rdb = XtDatabase(XtDisplay(topLevel));
	XtWarning( "app-defaults file not properly installed." );
	XrmPutLineResource( &rdb,
"*sampleText*UCSLabel:XFontSel app-defaults file not properly installed;\\n\
see 'xfontsel' manual page."
			  );
    }

    ScheduleWork(GetFontNames, (XtPointer)XtDisplay(topLevel), 0);

    pane = XtCreateManagedWidget("pane",panedWidgetClass,topLevel,NZ);
    {
	Widget commandBox, /* fieldBox, currentFontName,*/ viewPort;

	commandBox = XtCreateManagedWidget("commandBox",formWidgetClass,pane,NZ);
	{
	    Widget quitButton /*, ownButton , countLabel*/;

	    quitButton =
		XtCreateManagedWidget("quitButton",commandWidgetClass,commandBox,NZ);

	    ownButton =
		XtCreateManagedWidget("ownButton",toggleWidgetClass,commandBox,NZ);

	    countLabel =
		XtCreateManagedWidget("countLabel",labelWidgetClass,commandBox,NZ);

	    XtAddCallback(quitButton, XtNcallback, Quit, NULL);
	    XtAddCallback(ownButton,XtNcallback,OwnSelection,(XtPointer)True);
	}

	fieldBox = XtCreateManagedWidget("fieldBox", boxWidgetClass, pane, NZ);
	{
	    Widget /*dash,*/ field /*[FIELD_COUNT]*/;
	    int f;

	    for (f = 0; f < FIELD_COUNT; f++) {
		char name[10];
		FieldMenuRec *makeRec = XtNew(FieldMenuRec);
		sprintf( name, "field%d", f );
		XtCreateManagedWidget("dash",labelWidgetClass,fieldBox,NZ);
		field = XtCreateManagedWidget(name, menuButtonWidgetClass,
			fieldBox, NZ);
		XtAddCallback(field, XtNcallback, SelectField,
			(XtPointer)(long)f);
		makeRec->field = f;
		makeRec->button = field;
		ScheduleWork(MakeFieldMenu, (XtPointer)makeRec, 2);
		ScheduleWork((XtProc)XtFree, (XtPointer)makeRec, 2);
	    }
	}

	/* currentFontName = */
	{
	    Arg args[1];
	    currentFontNameSize = strlen(AppRes.pattern);
	    if (currentFontNameSize < 128) currentFontNameSize = 128;
	    currentFontNameString = (String)XtMalloc(currentFontNameSize);
	    strcpy(currentFontNameString, AppRes.pattern);
	    XtSetArg(args[0], XtNlabel, currentFontNameString);
	    currentFontName =
		XtCreateManagedWidget("fontName",labelWidgetClass,pane,args,ONE);
	}

	viewPort =
	    XtCreateManagedWidget("viewPort",viewportWidgetClass,pane,NZ);
	{
#ifdef USE_TEXT_WIDGET
	    Widget text =
		XtCreateManagedWidget("sampleText",asciiTextWidgetClass,viewPort,NZ);
	    Arg args[1];
	    XtSetArg( args[0], XtNtextSink, &sampleText );
	    XtGetValues( text, args, ONE );
#else
	    sampleText =
		XtCreateManagedWidget("sampleText",ucsLabelWidgetClass,viewPort,NZ);
#endif
	}
    }
    
    XtRealizeWidget(topLevel);
    XDefineCursor( XtDisplay(topLevel), XtWindow(topLevel), AppRes.cursor );
    {
	int f;
	for (f = 0; f < FIELD_COUNT; f++) currentFont.value_index[f] = -1;
    }
    wm_delete_window = XInternAtom(XtDisplay(topLevel), "WM_DELETE_WINDOW",
				   False);
    (void) XSetWMProtocols (XtDisplay(topLevel), XtWindow(topLevel),
                            &wm_delete_window, 1);
    XtAppMainLoop(appCtx);

    return 0;
}


typedef struct WorkPiece WorkPieceRec, *WorkPiece;
struct WorkPiece {
    WorkPiece next;
    int priority;
    XtProc proc;
    XtPointer closure;
};
static WorkPiece workQueue = NULL;


/*
 * ScheduleWork( XtProc proc, XtPointer closure, int priority )
 *
 * Adds a WorkPiece to the workQueue in FIFO order by priority.
 * Lower numbered priority work is completed before higher numbered
 * priorities.
 *
 * If the workQueue was previously empty, then makes sure that
 * Xt knows we have (background) work to do.
 */

static void ScheduleWork(XtProc proc, XtPointer closure, int priority)
{
    WorkPiece piece = XtNew(WorkPieceRec);

    piece->priority = priority;
    piece->proc = proc;
    piece->closure = closure;
    if (workQueue == NULL) {
	piece->next = NULL;
	workQueue = piece;
	XtAppAddWorkProc(appCtx, DoWorkPiece, NULL);
    } else {
	if (workQueue->priority > priority) {
	    piece->next = workQueue;
	    workQueue = piece;
	}
	else {
	    WorkPiece n;
	    for (n = workQueue; n->next && n->next->priority <= priority;)
		n = n->next;
	    piece->next = n->next;
	    n->next = piece;
	}
    }
}

/* ARGSUSED */
Boolean DoWorkPiece(XtPointer closure)
{
    WorkPiece piece = workQueue;

    if (piece) {
	(*piece->proc)(piece->closure);
	workQueue = piece->next;
	XtFree((XtPointer)piece);
	if (workQueue != NULL)
	    return False;
    }
    return True;
}


/*
 * FinishWork()
 *
 * Drains foreground tasks from the workQueue.
 * Foreground == (priority < BACKGROUND)
 */

static void FinishWork(void)
{
    while (workQueue && workQueue->priority < BACKGROUND)
	DoWorkPiece(NULL);
}


typedef struct ParseRec ParseRec;
struct ParseRec {
    char **fontNames;
    int num_fonts;
    int start, end;
    FontValues *fonts;
    FieldValueList **fieldValues;
};


void GetFontNames(XtPointer closure)
{
    Display *dpy = (Display*)closure;
    ParseRec *parseRec = XtNew(ParseRec);
    int f, field, count;
    String *fontNames;
    Boolean *b;
    int work_priority = 0;

    fontNames = parseRec->fontNames =
	XListFonts(dpy, AppRes.pattern, 32767, &numFonts);

    fonts = (FontValues*)XtMalloc( numFonts*sizeof(FontValues) );
    fontInSet = (Boolean*)XtMalloc( numFonts*sizeof(Boolean) );
    for (f = numFonts, b = fontInSet; f; f--, b++) *b = True;
    for (field = 0; field < FIELD_COUNT; field++) {
	fieldValues[field] = (FieldValueList*)XtMalloc(sizeof(FieldValueList));
	fieldValues[field]->allocated = 1;
	fieldValues[field]->count = 0;
    }
    if (numFonts == 0) {
	SetNoFonts();
	return;
    }
    numBadFonts = 0;
    parseRec->fonts = fonts;
    parseRec->num_fonts = count = matchingFontCount = numFonts;
    parseRec->fieldValues = fieldValues;
    parseRec->start = 0;
    /* this is bogus; the task should be responsible for quantizing...*/
    while (count > PARSE_QUANTUM) {
	ParseRec *prevRec = parseRec;
	parseRec->end = parseRec->start + PARSE_QUANTUM;
	ScheduleWork(ParseFontNames, (XtPointer)parseRec, work_priority);
	ScheduleWork((XtProc)XtFree, (XtPointer)parseRec, work_priority);
	parseRec = XtNew(ParseRec);
	*parseRec = *prevRec;
	parseRec->start += PARSE_QUANTUM;
	parseRec->fonts += PARSE_QUANTUM;
	parseRec->fontNames += PARSE_QUANTUM;
	count -= PARSE_QUANTUM;
	work_priority = 1;
    }
    parseRec->end = numFonts;
    ScheduleWork(ParseFontNames,(XtPointer)parseRec,work_priority);
    ScheduleWork((XtProc)XFreeFontNames,(XtPointer)fontNames,work_priority);
    ScheduleWork((XtProc)XtFree, (XtPointer)parseRec, work_priority);
    if (AppRes.scaled_fonts)
	ScheduleWork(FixScalables,(XtPointer)0,work_priority);
    ScheduleWork(SortFields,(XtPointer)0,work_priority);
    SetParsingFontCount(matchingFontCount);
    if (strcmp(AppRes.pattern, DEFAULTPATTERN)) {
	int maxField, f;
	for (f = 0; f < numFonts && !IsXLFDFontName(fontNames[f]); f++);
	if (f != numFonts) {
	    if (Matches(AppRes.pattern, fontNames[f],
			 patternFieldSpecified, &maxField)) {
		for (f = 0; f <= maxField; f++) {
		    if (patternFieldSpecified[f])
			currentFont.value_index[f] = 0;
		}
	    }
	    else
		XtAppWarning( appCtx, 
		    "internal error; pattern didn't match first font" );
	}
	else {
	    SetNoFonts();
	    return;
	}
    }
    ScheduleWork(SetCurrentFont, NULL, 1);
}


void ParseFontNames(XtPointer closure)
{
    ParseRec *parseRec = (ParseRec*)closure;
    char **fontNames = parseRec->fontNames;
    int num_fonts = parseRec->end;
    FieldValueList **fieldValues = parseRec->fieldValues;
    FontValues *fontValues = parseRec->fonts - numBadFonts;
    int i, font;

    for (font = parseRec->start; font < num_fonts; font++) {
	char *p;
	int f, len;
	FieldValue *v;

	if (!IsXLFDFontName(*fontNames)) {
	    numFonts--;
	    numBadFonts++;
	    continue;
	}

	for (f = 0, p = *fontNames++; f < FIELD_COUNT; f++) {
	    char *fieldP;

	    if (*p) ++p;
	    if (*p == DELIM || *p == '\0') {
		fieldP = "";
		len = 0;
	    } else {
		fieldP = p;
		while (*p && *++p != DELIM);
		len = p - fieldP;
	    }
	    for (i=fieldValues[f]->count,v=fieldValues[f]->value; i;i--,v++) {
		if (len == 0) {
		    if (v->string == NULL) break;
		}
		else
		    if (v->string &&
			strncmp( v->string, fieldP, len ) == 0 &&
			(v->string)[len] == '\0')
			break;
	    }
	    if (i == 0) {
		int count = fieldValues[f]->count++;
		if (count == fieldValues[f]->allocated) {
		    int allocated = (fieldValues[f]->allocated += 10);
		    fieldValues[f] = (FieldValueList*)
			XtRealloc( (char *) fieldValues[f],
				   sizeof(FieldValueList) +
					(allocated-1) * sizeof(FieldValue) );
		}
		v = &fieldValues[f]->value[count];
		v->field = f;
		if (len == 0)
		    v->string = NULL;
		else {
		    v->string = (String)XtMalloc( len+1 );
		    strncpy( v->string, fieldP, len );
		    v->string[len] = '\0';
		}
		v->font = (int*)XtMalloc( 10*sizeof(int) );
		v->allocated = 10;
		v->count = 0;
		v->enable = True;
		i = 1;
	    }
	    fontValues->value_index[f] = fieldValues[f]->count - i;
	    if ((i = v->count++) == v->allocated) {
		int allocated = (v->allocated += 10);
		v->font = (int*)XtRealloc( (char *) v->font, 
					  allocated * sizeof(int) );
	    }
	    v->font[i] = font - numBadFonts;
	}
	fontValues++;
    }
    SetParsingFontCount(numFonts - num_fonts);
}


/* Add the list of scalable fonts to the match-list of every value instance
 * for field f.  Must produce sorted order.  Must deal with duplicates
 * since we need to do this for resolution fields which can be nonzero in
 * the scalable fonts.
 */
static void AddScalables(int f)
{
    int i;
    int max = fieldValues[f]->count;
    FieldValue *fval = fieldValues[f]->value;

    for (i = 0; i < max; i++, fval++) {
	int *oofonts, *ofonts, *nfonts, *fonts;
	int ocount, ncount, count;

	if (fval->string && !strcmp(fval->string, "0"))
	    continue;
	count = numScaledFonts;
	fonts = scaledFonts;
	ocount = fval->count;
	ncount = ocount + count;
	nfonts = (int *)XtMalloc( ncount * sizeof(int) );
	oofonts = ofonts = fval->font;
	fval->font = nfonts;
	fval->count = ncount;
	fval->allocated = ncount;
	while (count && ocount) {
	    if (*fonts < *ofonts) {
		*nfonts++ = *fonts++;
		count--;
	    } else if (*fonts == *ofonts) {
		*nfonts++ = *fonts++;
		count--;
		ofonts++;
		ocount--;
		fval->count--;
	    } else {
		*nfonts++ = *ofonts++;
		ocount--;
	    }
	}
	while (ocount) {
	    *nfonts++ = *ofonts++;
	    ocount--;
	}
	while (count) {
	    *nfonts++ = *fonts++;
	    count--;
	}
	XtFree((char *)oofonts);
    }
}


/* Merge in specific scaled sizes (specified in a comma-separated string)
 * for field f.  Weed out duplicates.  The set of matching fonts is just
 * the set of scalable fonts.
 */
static void NewScalables(int f, char *slist)
{
    char endc = 1;
    char *str;
    int i, count;
    FieldValue *v;

    while (endc) {
	while (*slist == ' ' || *slist == ',')
	    slist++;
	if (!*slist)
	    break;
	str = slist;
	while ((endc = *slist) && endc != ' ' && endc != ',')
	    slist++;
	*slist++ = '\0';
	for (i=fieldValues[f]->count,v=fieldValues[f]->value; --i >= 0; v++) {
	    if (v->string && !strcmp(v->string, str))
		break;
	}
	if (i >= 0)
	    continue;
	count = fieldValues[f]->count++;
	if (count == fieldValues[f]->allocated) {
	    int allocated = (fieldValues[f]->allocated += 10);
	    fieldValues[f] = (FieldValueList*)
		XtRealloc( (char *) fieldValues[f],
			   sizeof(FieldValueList) +
				(allocated-1) * sizeof(FieldValue) );
	}
	v = &fieldValues[f]->value[count];
	v->field = f;
	v->string = str;
	v->count = numScaledFonts;
	v->font = scaledFonts;
	v->allocated = 0;
	v->enable = True;
    }
}


/* Find all scalable fonts, defined as the set matching "0" in the pixel
 * size field (field 6).  Augment the match-lists for all other fields
 * that are scalable.  Add in new scalable pixel and point sizes given
 * in resources.
 */
/*ARGSUSED*/
void FixScalables(XtPointer closure)
{
    int i;
    FieldValue *fval = fieldValues[6]->value;

    for (i = fieldValues[6]->count; --i >= 0; fval++) {
	if (fval->string && !strcmp(fval->string, "0")) {
	    scaledFonts = fval->font;
	    numScaledFonts = fval->count;
	    AddScalables(6);
	    NewScalables(6, AppRes.pixelSizeList);
	    AddScalables(7);
	    NewScalables(7, AppRes.pointSizeList);
	    AddScalables(8);
	    AddScalables(9);
	    AddScalables(11);
	    break;
	}
    }
}


/* A verbatim copy from xc/lib/font/fontfile/fontdir.c */

/*
 * Compare two strings just like strcmp, but preserve decimal integer
 * sorting order, i.e. "2" < "10" or "iso8859-2" < "iso8859-10" <
 * "iso10646-1". Strings are sorted as if sequences of digits were
 * prefixed by a length indicator (i.e., does not ignore leading zeroes).
 *
 * Markus Kuhn <Markus.Kuhn@cl.cam.ac.uk>
 */
#define Xisdigit(c) ('\060' <= (c) && (c) <= '\071')

static int strcmpn(const char *s1, const char *s2)
{
    int digits, predigits = 0;
    const char *ss1, *ss2;

    while (1) {
	if (*s1 == 0 && *s2 == 0)
	    return 0;
	digits = Xisdigit(*s1) && Xisdigit(*s2);
	if (digits && !predigits) {
	    ss1 = s1;
	    ss2 = s2;
	    while (Xisdigit(*ss1) && Xisdigit(*ss2))
		ss1++, ss2++;
	    if (!Xisdigit(*ss1) && Xisdigit(*ss2))
		return -1;
	    if (Xisdigit(*ss1) && !Xisdigit(*ss2))
		return 1;
	}
	if ((unsigned char)*s1 < (unsigned char)*s2)
	    return -1;
	if ((unsigned char)*s1 > (unsigned char)*s2)
	    return 1;
	predigits = digits;
	s1++, s2++;
    }
}


/* Order is *, (nil), rest */
static int AlphabeticSort(_Xconst void *fval1, _Xconst void *fval2)
{
#   define fval1 ((_Xconst FieldValue *)fval1)
#   define fval2 ((_Xconst FieldValue *)fval2)

    if (fval1->string && !strcmp(fval1->string, "*"))
	return -1;
    if (fval2->string && !strcmp(fval2->string, "*"))
	return 1;
    if (!fval1->string)
	return -1;
    if (!fval2->string)
	return 1;

    return strcmpn(fval1->string, fval2->string);

#   undef fval1
#   undef fval2
}


/* Order is *, (nil), rest */
static int NumericSort(_Xconst void *fval1, _Xconst void *fval2)
{
#   define fval1 ((_Xconst FieldValue *)fval1)
#   define fval2 ((_Xconst FieldValue *)fval2)

    if (fval1->string && !strcmp(fval1->string, "*"))
	return -1;
    if (fval2->string && !strcmp(fval2->string, "*"))
	return 1;
    if (!fval1->string)
	return -1;
    if (!fval2->string)
	return 1;

    return atoi(fval1->string) - atoi(fval2->string);

#   undef fval1
#   undef fval2
}


/* Resort each field, to get reasonable menus.  Sort alphabetically or
 * numerically, depending on the field.  Since the fonts have indexes
 * into the fields, we need to deal with updating those indexes after the
 * sort.
 */
/*ARGSUSED*/
void SortFields(XtPointer closure)
{
    int i, j, count;
    FieldValue *vals;
    int *indexes;
    int *idx;

    for (i = 0; i < FIELD_COUNT; i++) {
	count = fieldValues[i]->count;
	vals = fieldValues[i]->value;
	indexes = (int *)XtMalloc(count * sizeof(int));
	/* temporarily use the field component, will restore it below */
	for (j = 0; j < count; j++)
	    vals[j].field = j;
	switch (i) {
	case 6: case 7: case 8: case 9: case 11:
	    qsort((char *)vals, count, sizeof(FieldValue), NumericSort);
	    break;
	default:
	    qsort((char *)vals, count, sizeof(FieldValue), AlphabeticSort);
	    break;
	}
	for (j = 0; j < count; j++) {
	    indexes[vals[j].field] = j;
	    vals[j].field = i;
	}
	for (j = 0; j < numFonts; j++) {
	    idx = &fonts[j].value_index[i];
	    if (*idx >= 0)
		*idx = indexes[*idx];
	}
	XtFree((char *)indexes);
    }
}


Boolean IsXLFDFontName(String fontName)
{
    int f;
    for (f = 0; *fontName;) if (*fontName++ == DELIM) f++;
    return (f == FIELD_COUNT);
}


void MakeFieldMenu(XtPointer closure)
{
    FieldMenuRec *makeRec = (FieldMenuRec*)closure;
    Widget menu;
    FieldValueList *values = fieldValues[makeRec->field];
    FieldValue *val = values->value;
    int i;
    Arg args[1];
    register Widget item;

    if (numFonts)
	menu =
	  XtCreatePopupShell("menu",simpleMenuWidgetClass,makeRec->button,NZ);
    else {
	SetNoFonts();
	return;
    }
    XtGetSubresources(menu, (XtPointer) values, "options", "Options",
		      menuResources, XtNumber(menuResources), NZ);
    XtAddCallback(menu, XtNpopupCallback, EnableOtherValues,
		  (XtPointer)(long)makeRec->field );

    if (!patternFieldSpecified[val->field]) {
	XtSetArg( args[0], XtNlabel, "*" );
	item = XtCreateManagedWidget("any",smeBSBObjectClass,menu,args,ONE);
	XtAddCallback(item, XtNcallback, AnyValue, (XtPointer)(long)val->field);
    }

    for (i = values->count; i; i--, val++) {
	XtSetArg( args[0], XtNlabel, val->string ? val->string : "(nil)" );
	item =
	    XtCreateManagedWidget(val->string ? val->string : "nil",
				  smeBSBObjectClass, menu, args, ONE);
	XtAddCallback(item, XtNcallback, SelectValue, (XtPointer)val);
	val->menu_item = item;
    }
}


static void SetNoFonts(void)
{
    matchingFontCount = 0;
    SetCurrentFontCount();
    XtSetSensitive(fieldBox, False);
    XtSetSensitive(ownButton, False);
    if (AppRes.app_defaults_version >= MIN_APP_DEFAULTS_VERSION) {
#ifdef USE_TEXT_WIDGET
	XtUnmapWidget(XtParent(sampleText));
#else
	XtUnmapWidget(sampleText);
#endif
    }
}


Boolean Matches(register String pattern, register String fontName,
		Boolean fields[/*FIELD_COUNT*/], int *maxField)
{
    register int field = (*fontName == DELIM) ? -1 : 0;
    register Boolean marked_this_field = False;

    while (*pattern) {
	if (*pattern == *fontName || *pattern == '?') {
	    pattern++;
	    if (*fontName++ == DELIM) {
		field++;
		marked_this_field = False;
	    }
	    else if (!marked_this_field)
		fields[field] = marked_this_field = True; 
	    continue;
	}
	if (*pattern == '*') {
	    if (*++pattern == '\0') {
		*maxField = field;
		return True;
	    }
	    while (*fontName) {
		Boolean field_bits[FIELD_COUNT];
		int max_field;
		if (*fontName == DELIM) field++;
		bzero( field_bits, sizeof(field_bits) );
		if (Matches(pattern, fontName++, field_bits, &max_field)) {
		    int f;
		    *maxField = field + max_field;
		    for (f = 0; f <= max_field; field++, f++)
			fields[field] = field_bits[f];
		    return True;
		}
	    }
	    return False;
	}
	else /* (*pattern != '*') */
	    return False;
    }
    if (*fontName)
	return False;

    *maxField = field;
    return True;
}


/* ARGSUSED */
void SelectValue(Widget w, XtPointer closure, XtPointer callData)
{
    FieldValue *val = (FieldValue*)closure;
#ifdef LOG_CHOICES
    Choice *choice = XtNew(Choice);
#else
    static Choice pChoice;
    Choice *choice = &pChoice;
#endif

#ifdef notdef
    Widget button = XtParent(XtParent(w));
    Arg args[1];

    XtSetArg(args[0], XtNlabel, val->string);
    XtSetValues( button, args, ONE );
#endif

    currentFont.value_index[val->field] = val - fieldValues[val->field]->value;

    choice->prev = choiceList;
    choice->value = val;
    choiceList = choice;
	
    SetCurrentFont(NULL);
    EnableRemainingItems(SkipCurrentField);
}


/* ARGSUSED */
void AnyValue(Widget w, XtPointer closure, XtPointer callData)
{
    int field = (long)closure;
    currentFont.value_index[field] = -1;
    SetCurrentFont(NULL);
    EnableAllItems(field);
    EnableRemainingItems(ValidateCurrentField);
}


static void SetCurrentFontCount(void)
{
    char label[80];
    Arg args[1];
    if (matchingFontCount == 1)
	strcpy( label, "1 name matches" );
    else if (matchingFontCount)
	sprintf( label, "%d names match", matchingFontCount );
    else
	strcpy( label, "no names match" );
    XtSetArg( args[0], XtNlabel, label );
    XtSetValues( countLabel, args, ONE );
}


static void SetParsingFontCount(int count)
{
    char label[80];
    Arg args[1];
    if (count == 1)
	strcpy( label, "1 name to parse" );
    else
	sprintf( label, "%d names to parse", count );
    XtSetArg( args[0], XtNlabel, label );
    XtSetValues( countLabel, args, ONE );
    FlushXqueue(XtDisplay(countLabel));
}

/* ARGSUSED */
static Boolean IsISO10646(Display *dpy, XFontStruct *font)
{
    Boolean ok;
    int i;
    char *regname;
    Atom registry;
    XFontProp *xfp;

    ok = False;
    registry = XInternAtom(dpy, "CHARSET_REGISTRY", False);

    for (i = 0, xfp = font->properties;
	 ok == False && i < font->n_properties; xfp++, i++) {
	if (xfp->name == registry) {
	    regname = XGetAtomName(dpy, (Atom) xfp->card32);
	    if (strcmp(regname, "ISO10646") == 0 ||
		strcmp(regname, "iso10646") == 0)
	      ok = True;
	    XFree(regname);
	}
    }
    return ok;
}

/* ARGSUSED */
void SetCurrentFont(XtPointer closure)
{
    int f;
    Boolean *b;

    if (numFonts == 0) {
	SetNoFonts();
	return;
    }
    for (f = numFonts, b = fontInSet; f; f--, b++) *b = True;

    {
	int bytesLeft = currentFontNameSize;
	int pos = 0;

	for (f = 0; f < FIELD_COUNT; f++) {
	    int len, i;
	    String str;

	    currentFontNameString[pos++] = DELIM;
	    if ((i = currentFont.value_index[f]) != -1) {
		FieldValue *val = &fieldValues[f]->value[i];
		if ((str = val->string))
		    len = strlen(str);
		else {
		    str = "";
		    len = 0;
		}
		MarkInvalidFonts(fontInSet, val);
	    } else {
		str = "*";
		len = 1;
	    }
	    if (len+1 > --bytesLeft) {
		currentFontNameString = (String)
		    XtRealloc(currentFontNameString, currentFontNameSize+=128);
		bytesLeft += 128;
	    }
	    strcpy( &currentFontNameString[pos], str );
	    pos += len;
	    bytesLeft -= len;
	}
    }
    {
	Arg args[1];
	XtSetArg( args[0], XtNlabel, currentFontNameString );
	XtSetValues( currentFontName, args, ONE );
    }
    matchingFontCount = 0;
    for (f = numFonts, b = fontInSet; f; f--, b++) {
	if (*b) matchingFontCount++;
    }

    SetCurrentFontCount();

    {
#ifdef USE_TEXT_WIDGET
	Widget mapWidget = XtParent(sampleText);
#else
	Widget mapWidget = sampleText;
#endif
	Display *dpy = XtDisplay(mapWidget);
	XFontStruct *font = XLoadQueryFont(dpy, currentFontNameString);
	String sample_text;
	if (font == NULL)
	    XtUnmapWidget(mapWidget);
	else {
	    int nargs = 1;
	    Arg args[3];
	    int encoding;
	    if (font->min_byte1 || font->max_byte1) {
		if (IsISO10646(dpy, font) == True) {
		    encoding = XawTextEncodingUCS;
		    sample_text = AppRes.sample_textUCS;
		} else {
		    encoding = XawTextEncodingChar2b;
		    sample_text = AppRes.sample_text16;
		}
	    } else {
		encoding = XawTextEncoding8bit;
		sample_text = AppRes.sample_text;
	    }
	    XtSetArg( args[0], XtNfont, font );
	    if (encoding != textEncoding) {
		XtSetArg(args[1], XtNencoding, encoding);
		XtSetArg(args[2], XtNlabel, sample_text);
		textEncoding = encoding;
		nargs = 3;
	    }
	    XtSetValues( sampleText, args, nargs );
	    XtMapWidget(mapWidget);
	    if (sampleFont) XFreeFont( dpy, sampleFont );
	    sampleFont = font;
	    OwnSelection( sampleText, (XtPointer)False, (XtPointer)True );
	}
	FlushXqueue(dpy);
    }
}


static void MarkInvalidFonts(Boolean *set, FieldValue *val)
{
    int fi = 0, vi;
    int *fp = val->font;
    for (vi = val->count; vi; vi--, fp++) {
	while (fi < *fp) {
	    set[fi] = False;
	    fi++;
	}
	fi++;
    }
    while (fi < numFonts) {
	set[fi] = False;
	fi++;
    }
}


static void EnableRemainingItems(ValidateAction current_field_action)
{
    if (matchingFontCount == 0 || matchingFontCount == numFonts) {
	if (anyDisabled) {
	    int field;
	    for (field = 0; field < FIELD_COUNT; field++) {
		EnableAllItems(field);
	    }
	    anyDisabled = False;
	}
    }
    else {
	int field;
	for (field = 0; field < FIELD_COUNT; field++) {
	    FieldValue *value = fieldValues[field]->value;
	    int count;
	    if (current_field_action == SkipCurrentField &&
		field == choiceList->value->field)
		continue;
	    for (count = fieldValues[field]->count; count; count--, value++) {
		int *fp = value->font;
		int fontCount;
		for (fontCount = value->count; fontCount; fontCount--, fp++) {
		    if (fontInSet[*fp]) {
			value->enable = True;
			goto NextValue;
		    }
		}
		value->enable = False;
	      NextValue:;
	    }
	}
	anyDisabled = True;
    }
    enabledMenuIndex = -1;
    {
	int f;
	for (f = 0; f < FIELD_COUNT; f++)
	    ScheduleWork(EnableMenu, (XtPointer)(long)f, BACKGROUND);
    }
}


static void EnableAllItems(int field)
{
    FieldValue *value = fieldValues[field]->value;
    int count;
    for (count = fieldValues[field]->count; count; count--, value++) {
	value->enable = True;
    }
}


/* ARGSUSED */
void SelectField(Widget w, XtPointer closure, XtPointer callData)
{
    int field = (long)closure;
    FieldValue *values = fieldValues[field]->value;
    int count = fieldValues[field]->count;
    printf( "field %d:\n", field );
    while (count--) {
	printf( " %s: %d fonts\n", values->string, values->count );
	values++;
    }
    printf( "\n" );
}


/* When 2 out of 3 y-related scalable fields are set, we need to restrict
 * the third set to only match on exact matches, that is, ignore the
 * matching to scalable fonts.  Because choosing a random third value
 * will almost always produce an illegal font name, and it isn't worth
 * trying to compute which choices might be legal to the font scaler.
 */
static void DisableScaled(int f, int f1, int f2)
{
    int i, j;
    FieldValue *v;
    int *font;

    for (i = fieldValues[f]->count, v = fieldValues[f]->value; --i >= 0; v++) {
	if (!v->enable || !v->string || !strcmp(v->string, "0"))
	    continue;
	for (j = v->count, font = v->font; --j >= 0; font++) {
	    if (fontInSet[*font] &&
		fonts[*font].value_index[f1] == currentFont.value_index[f1] &&
		fonts[*font].value_index[f2] == currentFont.value_index[f2])
		break;
	}
	if (j < 0) {
	    v->enable = False;
	    XtSetSensitive(v->menu_item, False);
	}
    }
}

/* ARGSUSED */
void EnableOtherValues(Widget w, XtPointer closure, XtPointer callData)
{
    int field = (long)closure;
    Boolean *font_in_set = (Boolean*)XtMalloc(numFonts*sizeof(Boolean));
    Boolean *b;
    int f, count;

    FinishWork();
    for (f = numFonts, b = font_in_set; f; f--, b++) *b = True;
    for (f = 0; f < FIELD_COUNT; f++) {
	int i;
	if (f != field && (i = currentFont.value_index[f]) != -1) {
	    MarkInvalidFonts( font_in_set, &fieldValues[f]->value[i] );
	}
    }
    if (scaledFonts)
    {
	/* Check for 2 out of 3 scalable y fields being set */
	char *str;
	Bool specificPxl, specificPt, specificY;

	f = currentFont.value_index[6];
	specificPxl = (f >= 0 &&
		       (str = fieldValues[6]->value[f].string) &&
		       strcmp(str, "0"));
	f = currentFont.value_index[7];
	specificPt = (f >= 0 &&
		      (str = fieldValues[7]->value[f].string) &&
		      strcmp(str, "0"));
	f = currentFont.value_index[9];
	specificY = (f >= 0 &&
		     (str = fieldValues[9]->value[f].string) &&
		     strcmp(str, "0"));
	if (specificPt && specificY)
	    DisableScaled(6, 7, 9);
	if (specificPxl && specificY)
	    DisableScaled(7, 6, 9);
	if (specificPxl && specificPt)
	    DisableScaled(9, 6, 7);
    }
    count = 0;
    for (f = numFonts, b = font_in_set; f; f--, b++) {
	if (*b) count++;
    }
    if (count != matchingFontCount) {
	Boolean *sp = fontInSet;
	FieldValueList *fieldValue = fieldValues[field];
	for (b = font_in_set, f = 0; f < numFonts; f++, b++, sp++) {
	    if (*b != *sp) {
		int i = fonts[f].value_index[field];
		FieldValue *val = &fieldValue->value[i];
		val->enable = True;
		XtSetSensitive(val->menu_item, True);
		if (++count == matchingFontCount) break;
	    }
	}
    }
    XtFree((char *)font_in_set);
    if (enabledMenuIndex < field)
	EnableMenu((XtPointer)(long)field);
}


void EnableMenu(XtPointer closure)
{
    int field = (long)closure;
    FieldValue *val = fieldValues[field]->value;
    int f;
    Widget *managed = NULL, *pManaged = NULL;
    Widget *unmanaged = NULL, *pUnmanaged = NULL;
    Boolean showUnselectable = fieldValues[field]->show_unselectable;

    for (f = fieldValues[field]->count; f; f--, val++) {
	if (showUnselectable) {
	    if (val->enable != XtIsSensitive(val->menu_item))
		XtSetSensitive(val->menu_item, val->enable);
	}
	else {
	    if (val->enable != XtIsManaged(val->menu_item)) {
		if (val->enable) {
		    if (managed == NULL) {
			managed = (Widget*)
			    XtMalloc(fieldValues[field]->count*sizeof(Widget));
			pManaged = managed;
		    }
		    *pManaged++ = val->menu_item;
		}
		else {
		    if (unmanaged == NULL) {
			unmanaged = (Widget*)
			    XtMalloc(fieldValues[field]->count*sizeof(Widget));
			pUnmanaged = unmanaged;
		    }
		    *pUnmanaged++ = val->menu_item;
		}
	    }
	}
    }
    if (pManaged != managed) {
	XtManageChildren(managed, pManaged - managed);
	XtFree((char *) managed);
    }
    if (pUnmanaged != unmanaged) {
	XtUnmanageChildren(unmanaged, pUnmanaged - unmanaged);
	XtFree((char *) unmanaged);
    }
    enabledMenuIndex = field;
}


static void FlushXqueue(Display *dpy)
{
    XSync(dpy, False);
    while (XtAppPending(appCtx)) XtAppProcessEvent(appCtx, XtIMAll);
}


/* ARGSUSED */
void Quit(Widget w, XtPointer closure, XtPointer callData)
{
    XtCloseDisplay(XtDisplay(w));
    if (AppRes.print_on_quit) printf( "%s", currentFontNameString );
    exit(0);
}


static Boolean
ConvertSelection(Widget w, Atom *selection, Atom *target, Atom *type,
		 XtPointer *value, unsigned long *length, int *format)
{
    /* XmuConvertStandardSelection will use the second parameter only when
     * converting to the target TIMESTAMP.  However, it will never be
     * called upon to perform this conversion, because Xt will handle it
     * internally.  CurrentTime will never be used.
     */
    if (XmuConvertStandardSelection(w, CurrentTime, selection, target, type,
				    (XPointer *) value, length, format))
	return True;

    if (*target == XA_STRING) {
	*type = XA_STRING;
	*value = currentFontNameString;
	*length = strlen(*value);
	*format = 8;
	return True;
    }
    else {
	return False;
    }
}

static AtomPtr _XA_PRIMARY_FONT = NULL;
#define XA_PRIMARY_FONT XmuInternAtom(XtDisplay(w),_XA_PRIMARY_FONT)

/* ARGSUSED */
static void LoseSelection(Widget w, Atom *selection)
{
    Arg args[1];
    XtSetArg( args[0], XtNstate, False );
    XtSetValues( w, args, ONE );
    if (*selection == XA_PRIMARY_FONT) {
	XtSetSensitive(currentFontName, False);
    }
}


/* ARGSUSED */
static void DoneSelection(Widget w, Atom *selection, Atom *target)
{
    /* do nothing */
}


/* ARGSUSED */
void OwnSelection(Widget w, XtPointer closure, XtPointer callData)
{
    Time time = XtLastTimestampProcessed(XtDisplay(w));
    Boolean primary = (Boolean) (long) closure;
    Boolean own = (Boolean) (long) callData;

    if (_XA_PRIMARY_FONT == NULL)
	_XA_PRIMARY_FONT = XmuMakeAtom("PRIMARY_FONT");

    if (own) {
	XtOwnSelection( w, XA_PRIMARY_FONT, time,
			ConvertSelection, LoseSelection, DoneSelection );
	if (primary)
	    XtOwnSelection( w, XA_PRIMARY, time,
			   ConvertSelection, LoseSelection, DoneSelection );
	if (!XtIsSensitive(currentFontName)) {
	    XtSetSensitive(currentFontName, True);
	}
    }
    else {
	XtDisownSelection(w, XA_PRIMARY_FONT, time);
	if (primary)
	    XtDisownSelection(w, XA_PRIMARY, time);
	XtSetSensitive(currentFontName, False);
    }
}

/*ARGSUSED*/
void
QuitAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    exit (0);
}
