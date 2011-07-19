/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/*
 * Copyright © 2006 Sun Microsystems, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * Login.c
 */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/keysym.h>
#include <X11/DECkeysym.h>
#include <X11/Xfuncs.h>

#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <string.h>

#include "dm.h"
#include "dm_error.h"
#include "greet.h"
#include "LoginP.h"

#ifdef XPM
# include <sys/stat.h>
# include <unistd.h>
# include <X11/Xlib.h>
# include <X11/xpm.h>
# include <X11/extensions/shape.h>
#endif /* XPM */

#include <X11/cursorfont.h>

#ifdef USE_XINERAMA
# include <X11/extensions/Xinerama.h>
#endif

#ifndef DEBUG
# define XDM_ASSERT(a)	/* do nothing */
#else
# if defined(__STDC__) && __STDC_VERSION__ - 0 >= 199901L
#  define XDM_ASSERT(a)	if (!(a)) { \
	Debug("Assertion failed in %s() at file %s line %d\n", \
	      __func__, __FILE__, __LINE__); }
# else
#  define XDM_ASSERT(a)	if (!(a)) { \
	Debug("Assertion failed at file %s line %d\n", __FILE__, __LINE__); }
# endif
#endif

static void RedrawFail (LoginWidget w);
static void ResetLogin (LoginWidget w);
static void failTimeout (XtPointer client_data, XtIntervalId * id);
static void EraseCursor (LoginWidget w);
static void XorCursor (LoginWidget w);

#define offset(field) XtOffsetOf(LoginRec, login.field)
#define goffset(field) XtOffsetOf(WidgetRec, core.field)


static XtResource resources[] = {
    {XtNwidth, XtCWidth, XtRDimension, sizeof(Dimension),
	goffset(width), XtRImmediate,	(XtPointer) 0},
    {XtNheight, XtCHeight, XtRDimension, sizeof(Dimension),
	goffset(height), XtRImmediate,	(XtPointer) 0},
    {XtNx, XtCX, XtRPosition, sizeof (Position),
	goffset(x), XtRImmediate,	(XtPointer) -1},
    {XtNy, XtCY, XtRPosition, sizeof (Position),
	goffset(y), XtRImmediate,	(XtPointer) -1},
#ifndef USE_XFT
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
        offset(textpixel), XtRString,	XtDefaultForeground},
    {XtNpromptColor, XtCForeground, XtRPixel, sizeof(Pixel),
        offset(promptpixel), XtRString,	XtDefaultForeground},
    {XtNgreetColor, XtCForeground, XtRPixel, sizeof(Pixel),
        offset(greetpixel), XtRString,	XtDefaultForeground},
    {XtNfailColor, XtCForeground, XtRPixel, sizeof (Pixel),
	offset(failpixel), XtRString,	XtDefaultForeground},
#endif

#ifdef DANCING
/* added by Caolan McNamara */
	{XtNlastEventTime, XtCLastEventTime, XtRInt , sizeof (int),
	offset(lastEventTime), XtRImmediate,	(XtPointer)0},
/* end (caolan) */
#endif /* DANCING */

#ifdef XPM
/* added by Ivan Griffin (ivan.griffin@ul.ie) */
        {XtNlogoFileName, XtCLogoFileName, XtRString, sizeof(char*),
        offset(logoFileName), XtRImmediate, (XtPointer)0},
        {XtNuseShape, XtCUseShape, XtRBoolean, sizeof(Boolean),
        offset(useShape), XtRImmediate, (XtPointer) True},
        {XtNlogoPadding, XtCLogoPadding, XtRInt, sizeof(int),
        offset(logoPadding), XtRImmediate, (XtPointer) 5},
/* end (ivan) */
#endif /* XPM */

/* added by Amit Margalit */
    {XtNhiColor, XtCForeground, XtRPixel, sizeof (Pixel),
	offset(hipixel), XtRString,	XtDefaultForeground},
    {XtNshdColor, XtCForeground, XtRPixel, sizeof (Pixel),
	offset(shdpixel), XtRString,	XtDefaultForeground},
    {XtNframeWidth, XtCFrameWidth, XtRInt, sizeof(int),
        offset(outframewidth), XtRImmediate, (XtPointer) 1},
    {XtNinnerFramesWidth, XtCFrameWidth, XtRInt, sizeof(int),
        offset(inframeswidth), XtRImmediate, (XtPointer) 1},
    {XtNsepWidth, XtCFrameWidth, XtRInt, sizeof(int),
        offset(sepwidth), XtRImmediate, (XtPointer) 1},
/* end (amit) */

#ifndef USE_XFT
    {XtNfont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
    	offset (textFont), XtRString,	"*-new century schoolbook-medium-r-normal-*-180-*"},
    {XtNpromptFont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
    	offset (promptFont), XtRString, "*-new century schoolbook-bold-r-normal-*-180-*"},
    {XtNgreetFont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
    	offset (greetFont), XtRString,	"*-new century schoolbook-bold-i-normal-*-240-*"},
    {XtNfailFont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
	offset (failFont), XtRString,	"*-new century schoolbook-bold-r-normal-*-180-*"},
#else /* USE_XFT */
    {XtNface, XtCFace, XtRXftFont, sizeof (XftFont *),
        offset (textFace), XtRString, "Serif-18"},
    {XtNpromptFace, XtCFace, XtRXftFont, sizeof (XftFont *),
        offset (promptFace), XtRString, "Serif-18:bold"},
    {XtNgreetFace, XtCFace, XtRXftFont, sizeof (XftFont *),
        offset (greetFace), XtRString, "Serif-24:italic"},
    {XtNfailFace, XtCFace, XtRXftFont, sizeof (XftFont *),
        offset (failFace), XtRString, "Serif-18:bold"},
    {XtNforeground, XtCForeground, XtRXftColor, sizeof(XftColor),
        offset(textcolor), XtRString,	XtDefaultForeground},
    {XtNpromptColor, XtCForeground, XtRXftColor, sizeof(XftColor),
        offset(promptcolor), XtRString,	XtDefaultForeground},
    {XtNgreetColor, XtCForeground, XtRXftColor, sizeof(XftColor),
        offset(greetcolor), XtRString,	XtDefaultForeground},
    {XtNfailColor, XtCForeground, XtRXftColor, sizeof (XftColor),
	offset(failcolor), XtRString,	XtDefaultForeground},
#endif
    {XtNgreeting, XtCGreeting, XtRString, sizeof (char *),
    	offset(greeting), XtRString, "X Window System"},
    {XtNunsecureGreeting, XtCGreeting, XtRString, sizeof (char *),
	offset(unsecure_greet), XtRString, "This is an unsecure session"},
    {XtNnamePrompt, XtCNamePrompt, XtRString, sizeof (char *),
	offset(namePrompt), XtRString, "Login:  "},
    {XtNpasswdPrompt, XtCPasswdPrompt, XtRString, sizeof (char *),
	offset(passwdPrompt), XtRString, "Password:  "},
    {XtNfail, XtCFail, XtRString, sizeof (char *),
	offset(failMsg), XtRString,
#if defined(sun) && defined(SVR4)
     "Login incorrect or not on system console if root"
#else
     "Login incorrect"
#endif
    },
    {XtNchangePasswdMessage, XtCChangePasswdMessage, XtRString,
	sizeof (char *), offset(passwdChangeMsg), XtRString,
	(XtPointer) "Password Change Required" },
    {XtNfailTimeout, XtCFailTimeout, XtRInt, sizeof (int),
	offset(failTimeout), XtRImmediate, (XtPointer) 10},
    {XtNnotifyDone, XtCCallback, XtRFunction, sizeof (XtPointer),
	offset(notify_done), XtRFunction, (XtPointer) 0},
    {XtNsessionArgument, XtCSessionArgument, XtRString,	sizeof (char *),
	offset(sessionArg), XtRString, (XtPointer) 0 },
    {XtNsecureSession, XtCSecureSession, XtRBoolean, sizeof (Boolean),
	offset(secure_session), XtRImmediate, (XtPointer) False },
    {XtNallowAccess, XtCAllowAccess, XtRBoolean, sizeof (Boolean),
	offset(allow_access), XtRImmediate, (XtPointer) False },
    {XtNallowNullPasswd, XtCAllowNullPasswd, XtRBoolean, sizeof (Boolean),
	offset(allow_null_passwd), XtRImmediate, (XtPointer) False},
    {XtNallowRootLogin, XtCAllowRootLogin, XtRBoolean, sizeof(Boolean),
	offset(allow_root_login), XtRImmediate, (XtPointer) True},
    {XtNechoPasswd, XtCEchoPasswd, XtRBoolean, sizeof(Boolean),
	offset(echo_passwd), XtRImmediate, (XtPointer) False}
};

#undef offset
#undef goffset

#ifdef USE_XFT
# define F_MAX_WIDTH(f)	((w)->login.f##Face->max_advance_width)
# define F_ASCENT(f)	((w)->login.f##Face->ascent)
# define F_DESCENT(f)	((w)->login.f##Face->descent)
#else
# define F_MAX_WIDTH(f)	((w)->login.f##Font->max_bounds.width)
# define F_ASCENT(f)	((w)->login.f##Font->max_bounds.ascent)
# define F_DESCENT(f)	((w)->login.f##Font->max_bounds.descent)
#endif

#define TEXT_X_INC(w)		F_MAX_WIDTH(text)
#define TEXT_Y_INC(w)		(F_ASCENT(text) + F_DESCENT(text))

#define PROMPT_X_INC(w)	F_MAX_WIDTH(prompt)
#define PROMPT_Y_INC(w)	(F_ASCENT(prompt) + F_DESCENT(prompt))

#define GREET_X_INC(w)		F_MAX_WIDTH(greet)
#define GREET_Y_INC(w)		(F_ASCENT(greet) + F_DESCENT(greet))

#define FAIL_X_INC(w)		F_MAX_WIDTH(fail)
#define FAIL_Y_INC(w)		(F_ASCENT(fail) + F_DESCENT(fail))

#define Y_INC(w)	max (TEXT_Y_INC(w), PROMPT_Y_INC(w))


#define PROMPT_TEXT(w,n) 	((w)->login.prompts[n].promptText)
#define DEF_PROMPT_TEXT(w,n) 	((w)->login.prompts[n].defaultPrompt)
#define VALUE_TEXT(w,n) 	((w)->login.prompts[n].valueText)
#define VALUE_TEXT_MAX(w,n)	((w)->login.prompts[n].valueTextMax)
#define VALUE_SHOW_START(w,n)	((w)->login.prompts[n].valueShownStart)
#define VALUE_SHOW_END(w,n)	((w)->login.prompts[n].valueShownEnd)
#define PROMPT_STATE(w,n) 	((w)->login.prompts[n].state)
#define PROMPT_CURSOR(w,n)	((w)->login.prompts[n].cursor)

#define CUR_PROMPT_CURSOR(w)	PROMPT_CURSOR(w,w->login.activePrompt)

#define CUR_PROMPT_TEXT(w, n)	(PROMPT_TEXT(w,n) != NULL ? \
				 PROMPT_TEXT(w,n) : DEF_PROMPT_TEXT(w,n))

#ifdef USE_XFT

# define TEXT_COLOR(f)		(w->login.f##color.pixel)

# define TEXT_WIDTH(f, m, l) 	XmuXftTextWidth(XtDisplay (w), \
					w->login.f##Face, (FcChar8 *) m, l)
static int
XmuXftTextWidth(Display *dpy, XftFont *font, FcChar8 *string, int len);

# define DRAW_STRING(f, x, y, m, l) \
	/* Debug("DRAW_STRING(%s, %d, %d, %s, %d)\n", #f, x, y, m, l); */ \
	XftDrawString8 (w->login.draw, &w->login.f##color, w->login.f##Face, \
			x, y, (FcChar8 *) m, l)

#else

# define TEXT_COLOR(f)		(w->login.f##pixel)

# define TEXT_WIDTH(f, m, l) 	(XTextWidth (w->login.f##Font, m, l))

# define DRAW_STRING(f, x, y, m, l) \
	XDrawString (XtDisplay (w), XtWindow (w), w->login.f##GC, x, y, m, l)

#endif


#define STRING_WIDTH(f, s) 	TEXT_WIDTH (f, s, strlen(s))



#define TEXT_PROMPT_W(w, m) (STRING_WIDTH(prompt, m) + w->login.inframeswidth)

#define DEF_PROMPT_W(w,n) TEXT_PROMPT_W(w, w->login.prompts[n].defaultPrompt)
#define CUR_PROMPT_W(w,n)  (max(MAX_DEF_PROMPT_W(w), PROMPT_TEXT(w,n) ? \
		     TEXT_PROMPT_W(w, PROMPT_TEXT(w,n)) : 0))

#define MAX_DEF_PROMPT_W(w) (max(DEF_PROMPT_W(w,0), DEF_PROMPT_W(w,1)))

#define GREETING(w)	((w)->login.secure_session  && !(w)->login.allow_access ?\
				(w)->login.greeting : (w)->login.unsecure_greet)
#define GREET_X(w)	((int)((w->core.width - \
			     	STRING_WIDTH (greet, GREETING(w))) / 2))
#define GREET_Y(w)	(GREETING(w)[0] ? 2 * GREET_Y_INC (w) : 0)
#ifndef XPM
# define GREET_W(w)	(max (STRING_WIDTH (greet, w->login.greeting), \
			      STRING_WIDTH (greet, w->login.unsecure_greet)))
#else
# define GREET_W(w)	(max (STRING_WIDTH (greet, w->login.greeting), \
			      STRING_WIDTH (greet, w->login.unsecure_greet)) \
			 + w->login.logoWidth + (2*w->login.logoPadding))
#endif /* XPM */
#define PROMPT_X(w)	(2 * PROMPT_X_INC(w))
#define PROMPT_Y(w,n)	((GREET_Y(w) + GREET_Y_INC(w) +\
			  F_ASCENT(greet) + Y_INC(w)) + \
			 (n * PROMPT_SPACE_Y(w)))
#define PROMPT_W(w)	(w->core.width - (2 * TEXT_X_INC(w)))
#define PROMPT_H(w)	(3 * Y_INC(w) / 2)
#define VALUE_X(w,n)	(PROMPT_X(w) + CUR_PROMPT_W(w,n))
#define PROMPT_SPACE_Y(w)	(10 * Y_INC(w) / 5)

#define ERROR_X(w,m)	((int)(w->core.width - STRING_WIDTH (fail, m)) / 2)
#define FAIL_X(w)	ERROR_X(w, w->login.fail)
#define FAIL_Y(w)	(PROMPT_Y(w,1) + 2 * FAIL_Y_INC (w) + F_ASCENT(fail))

#ifndef XPM
# define ERROR_W(w,m)	STRING_WIDTH (fail, m)
#else
# define ERROR_W(w,m)	(STRING_WIDTH (fail, m) \
			 + w->login.logoWidth + (2*w->login.logoPadding))
#endif /* XPM */

#define FAIL_W(w)	max(ERROR_W(w, w->login.failMsg), \
			    ERROR_W(w, w->login.passwdChangeMsg))

#define PAD_X(w)	(2 * (PROMPT_X(w) + max (GREET_X_INC(w), FAIL_X_INC(w))))

#define PAD_Y(w)	(max (max (Y_INC(w), GREET_Y_INC(w)),\
			     FAIL_Y_INC(w)))

#ifndef max
static inline int max (int a, int b) { return a > b ? a : b; }
#endif

static void
realizeValue (LoginWidget w, int cursor, int promptNum, GC gc)
{
    loginPromptState state = PROMPT_STATE(w, promptNum);
    char *text = VALUE_TEXT(w, promptNum);
    int	x, y, height, width, curoff;

    XDM_ASSERT(promptNum >= 0 && promptNum <= LAST_PROMPT);

    /* replace all password characters with asterisks */
    if ((state == LOGIN_PROMPT_ECHO_OFF) && (w->login.echo_passwd == True))
    {
	Cardinal length = strlen(text);
	Cardinal i = 0;

	text = XtMalloc(length + 1);

	if (text == NULL)
	{
	    LogOutOfMem("realizeValue");
	    return;
	}

	while (i < length)
	{
	    text[i++] = '*';
	}

	text[i] = 0;
    }

    x = VALUE_X (w,promptNum);
    y = PROMPT_Y (w,promptNum);

    height = PROMPT_H(w);
    width = PROMPT_W(w) - x - 3;

    height -= (w->login.inframeswidth * 2);
    width -= (w->login.inframeswidth * 2);
#ifdef XPM
    width -= (w->login.logoWidth + (w->login.logoPadding * 2));
#endif
    if (cursor > VALUE_SHOW_START(w, promptNum))
	curoff = TEXT_WIDTH (text, text, cursor);
    else
	curoff = 0;


    if (gc == w->login.bgGC) {
	if (curoff < width) {
	    XFillRectangle (XtDisplay (w), XtWindow (w), gc,
			    x + curoff, y - TEXT_Y_INC(w),
			    width - curoff, height);
	}
    } else if ((state == LOGIN_PROMPT_ECHO_ON) || (state == LOGIN_TEXT_INFO) ||
	       ((state == LOGIN_PROMPT_ECHO_OFF) && (w->login.echo_passwd == True)))
    {
	int textwidth;
	int offset = max(cursor, VALUE_SHOW_START(w, promptNum));
	int textlen = strlen (text + offset);

	textwidth = TEXT_WIDTH (text, text + offset, textlen);

	if (textwidth > (width - curoff)) {
	    /* Recalculate amount of text that can fit in field */
	    offset = VALUE_SHOW_START(w, promptNum);
	    textlen = strlen (text + offset);

	    while ((textlen > 0) && (textwidth > width))
	    {
		if (offset < PROMPT_CURSOR(w, promptNum)) {
		    offset++;
		}
		textlen--;
		textwidth = TEXT_WIDTH (text, text + offset, textlen);
	    }

	    VALUE_SHOW_START(w, promptNum) = offset;
	    VALUE_SHOW_END(w, promptNum) = offset + textlen;

	    /* Erase old string */
	    XFillRectangle (XtDisplay (w), XtWindow (w), w->login.bgGC,
			    x, y - TEXT_Y_INC(w), width, height);

	    DRAW_STRING(text, x, y, text + offset, textlen);
	} else {
	    DRAW_STRING(text, x + curoff, y, text + offset, textlen);
	}
    }
    /* free memory */
    if ((state == LOGIN_PROMPT_ECHO_OFF) && (w->login.echo_passwd == True))
    {
	XtFree(text);
    }
}

static void
EraseValue (LoginWidget w, int cursor, int promptNum)
{
    realizeValue(w, cursor, promptNum, w->login.bgGC);
}

static void
DrawValue (LoginWidget w, int cursor, int promptNum)
{
    realizeValue(w, cursor, promptNum, w->login.textGC);
#ifdef DANCING
    /*as good a place as any Caolan begin*/
    w->login.lastEventTime = time(NULL);
    /*as good a place as any Caolan end*/
#endif /* DANCING */
}

static void
realizeCursor (LoginWidget w, GC gc)
{
    int	x, y;
    int height, width;

    if (w->login.state != PROMPTING) {
	return;
    }

    x = VALUE_X (w, w->login.activePrompt);
    y = PROMPT_Y (w, w->login.activePrompt);
    height = (F_ASCENT(text) + F_DESCENT(text));
    width = 1;

    switch (PROMPT_STATE(w, w->login.activePrompt)) {
    case LOGIN_PROMPT_NOT_SHOWN:
    case LOGIN_TEXT_INFO:
	return;
    case LOGIN_PROMPT_ECHO_ON:
	if (CUR_PROMPT_CURSOR(w) > 0) {
	    x += TEXT_WIDTH (text,
			     VALUE_TEXT(w, w->login.activePrompt)
			     + VALUE_SHOW_START(w, w->login.activePrompt),
			     PROMPT_CURSOR(w, w->login.activePrompt)
			     - VALUE_SHOW_START(w, w->login.activePrompt) );
	}
	break;
    case LOGIN_PROMPT_ECHO_OFF:
	if (w->login.echo_passwd == True) {
	    int len = PROMPT_CURSOR(w, w->login.activePrompt) -
		VALUE_SHOW_START(w, w->login.activePrompt);

	    x += len*TEXT_WIDTH(text, "*", 1);
	}
	else
	{
	    /* Move cursor one pixel per character to give some feedback
	       without giving away the password length */
	    x += PROMPT_CURSOR(w, w->login.activePrompt);
	}
	break;
    }

    XFillRectangle (XtDisplay (w), XtWindow (w), gc,
		    x, y+1 - F_ASCENT(text), width, height-1);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x-1 , y - F_ASCENT(text));
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x+1 , y - F_ASCENT(text));
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x-1 , y - F_ASCENT(text)+height);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x+1 , y - F_ASCENT(text)+height);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x-2 , y - F_ASCENT(text));
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x+2 , y - F_ASCENT(text));
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x-2 , y - F_ASCENT(text)+height);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x+2 , y - F_ASCENT(text)+height);

    XFlush (XtDisplay(w));
}

static void
EraseFail (LoginWidget w)
{
#ifdef USE_XFT
    w->login.failUp = 0;
    RedrawFail(w);
#else
    XSetForeground (XtDisplay (w), w->login.failGC,
			w->core.background_pixel);
    RedrawFail(w);
    w->login.failUp = 0;
    XSetForeground (XtDisplay (w), w->login.failGC,
			TEXT_COLOR(fail));
#endif
}

static void
XorCursor (LoginWidget w)
{
    realizeCursor (w, w->login.xorGC);
}

static void
RemoveFail (LoginWidget w)
{
    if (w->login.failUp)
	EraseFail (w);
}

static void
EraseCursor (LoginWidget w)
{
    realizeCursor (w, w->login.bgGC);
}

/*ARGSUSED*/
static void failTimeout (XtPointer client_data, XtIntervalId * id)
{
    LoginWidget	w = (LoginWidget)client_data;

    Debug ("failTimeout\n");
    w->login.interval_id = 0;
    EraseFail (w);
}

_X_INTERNAL
void
DrawFail (Widget ctx)
{
    LoginWidget	w;

    w = (LoginWidget) ctx;
    XorCursor (w);
    ResetLogin (w);
    XorCursor (w);
    ErrorMessage(ctx, w->login.failMsg, True);
}

static void
RedrawFail (LoginWidget w)
{
    int x = FAIL_X(w);
    int y = FAIL_Y(w);
    int maxw = w->core.width - PAD_X(w);

#ifndef USE_XFT
    if (w->login.failUp)
#endif
    {
	Debug("RedrawFail('%s', %d)\n", w->login.fail, w->login.failUp);
	if (ERROR_W(w, w->login.fail) > maxw) {
	    /* Too long to fit on one line, break into multiple lines */
	    char *tempCopy = strdup(w->login.fail);
	    if (tempCopy != NULL) {
		char *start, *next;
		char lastspace = ' ';

		y = PROMPT_Y(w,LAST_PROMPT) + (2 * PROMPT_Y_INC(w));

		for (start = next = tempCopy; start != NULL ; start = next) {
		    /* search for longest string broken by whitespace that
		       will fit on a single line */
		    do {
			if (next != start) {
			    *next = lastspace;
			}
			for (next = next + 1;
			     (*next != '\0') && !isspace(*next) ; next++)
			{
			    /* this loop intentionally left blank */
			}
			if (*next != '\0') {
			    lastspace = *next;
			    *next = '\0';
			} else {
			    next = NULL;
			}
		    } while ((next != NULL) && ERROR_W(w, start) < maxw);

		    x = ERROR_X(w, start);
#ifdef USE_XFT
		    if (w->login.failUp == 0) {
			XClearArea(XtDisplay(w), XtWindow(w), x, y - F_ASCENT(fail),
				   ERROR_W(w, start), FAIL_Y_INC(w), False);
		    } else
#endif
			DRAW_STRING (fail, x, y, start, strlen(start));

		    if (next != NULL) {
			next++;
			y += FAIL_Y_INC(w);
		    }
		}
		free(tempCopy);
		return;
	    }
	    /* if strdup failed, fall through to draw all at once, even
	       though we know it can't all fit */
	    LogOutOfMem("RedrawFail");
	}

#ifdef USE_XFT
	if (w->login.failUp == 0) {
	    XClearArea(XtDisplay(w), XtWindow(w), x, y - F_ASCENT(fail),
		       ERROR_W(w, w->login.fail), FAIL_Y_INC(w), False);
	} else
#endif
	    DRAW_STRING (fail, x, y, w->login.fail, strlen (w->login.fail));
    }
}

_X_INTERNAL
void
ErrorMessage(Widget ctx, const char *message, Bool timeout)
{
    LoginWidget	w = (LoginWidget) ctx;

/*  Debug("ErrorMessage: %s\n", message);   */
    if (w->login.interval_id != 0) {
	XtRemoveTimeOut(w->login.interval_id);
	w->login.interval_id = 0;
    }
    RemoveFail(w);
    if (w->login.fail != w->login.failMsg)
	free(w->login.fail);
    w->login.fail = strdup(message);
    if (w->login.fail == NULL)
	w->login.fail = (char *) w->login.failMsg;
    w->login.failUp = 1;
    RedrawFail (w);
    if (timeout && (w->login.failTimeout > 0)) {
	Debug ("failTimeout: %d\n", w->login.failTimeout);
	w->login.interval_id =
	    XtAppAddTimeOut(XtWidgetToApplicationContext ((Widget)w),
			    w->login.failTimeout * 1000,
			    failTimeout, (XtPointer) w);
    }
}

_X_INTERNAL
void
ShowChangePasswdMessage(Widget ctx)
{
    LoginWidget	w = (LoginWidget) ctx;

    ErrorMessage(ctx, w->login.passwdChangeMsg, False);
}

static void
draw_it (LoginWidget w)
{
    int p;
    int i;
    int gr_line_x, gr_line_y, gr_line_w;

    EraseCursor (w);

    /* draw window borders */
    for(i=1;i<=(w->login.outframewidth);i++)
    {
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.hiGC,
    		i-1,i-1,w->core.width-i,i-1);
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.hiGC,
    		i-1,i-1,i-1,w->core.height-i);
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.shdGC,
    		w->core.width-i,i-1,w->core.width-i,w->core.height-i);
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.shdGC,
    		i-1,w->core.height-i,w->core.width-i,w->core.height-i);
    }

    /* make separator line */
    gr_line_x = w->login.outframewidth;
    gr_line_y = GREET_Y(w) + GREET_Y_INC(w);
    gr_line_w = w->core.width - 2*(w->login.outframewidth);

#ifdef XPM
    gr_line_x += w->login.logoPadding;
    gr_line_w -= w->login.logoWidth + (3 * (w->login.logoPadding));
#endif /* XPM */

    for(i=1;i<=(w->login.sepwidth);i++)
    {
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.shdGC,
        gr_line_x,           gr_line_y + i-1,
        gr_line_x+gr_line_w, gr_line_y + i-1);
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.hiGC,
        gr_line_x,           gr_line_y + 2*(w->login.inframeswidth) -i,
        gr_line_x+gr_line_w, gr_line_y + 2*(w->login.inframeswidth) -i);
    }

    for (p = 0; p < NUM_PROMPTS ; p++)
    {
	int in_frame_x = VALUE_X(w,p) - w->login.inframeswidth - 3;
	int in_frame_y
	    = PROMPT_Y(w,p) - w->login.inframeswidth - 1 - TEXT_Y_INC(w);

	int in_width = PROMPT_W(w) - VALUE_X(w,p);
	int in_height = PROMPT_H(w) + w->login.inframeswidth + 2;

	GC topLeftGC, botRightGC;

#ifdef XPM
	in_width -= (w->login.logoWidth + 2*(w->login.logoPadding));
#endif /* XPM */

	if ((PROMPT_STATE(w, p) == LOGIN_PROMPT_ECHO_ON) ||
	    (PROMPT_STATE(w, p) == LOGIN_PROMPT_ECHO_OFF)) {
	    topLeftGC = w->login.shdGC;
	    botRightGC = w->login.hiGC;
	} else {
	    topLeftGC = botRightGC = w->login.bgGC;
	}

	/* draw borders of editboxes */
	for (i=1; i<=(w->login.inframeswidth); i++)
	{
	    /* Make top/left sides */
	    XDrawLine(XtDisplay (w), XtWindow (w), topLeftGC,
		      in_frame_x + i-1,         in_frame_y + i-1,
		      in_frame_x + in_width-i,  in_frame_y + i-1);

	    XDrawLine(XtDisplay (w), XtWindow (w), topLeftGC,
		      in_frame_x + i-1,         in_frame_y + i-1,
		      in_frame_x + i-1,         in_frame_y + in_height-i);

	    /* Make bottom/right sides */
	    XDrawLine(XtDisplay (w), XtWindow (w), botRightGC,
		      in_frame_x + in_width-i,  in_frame_y + i-1,
		      in_frame_x + in_width-i,  in_frame_y + in_height-i);

	    XDrawLine(XtDisplay (w), XtWindow (w), botRightGC,
		      in_frame_x + i-1,         in_frame_y + in_height-i,
		      in_frame_x + in_width-i,  in_frame_y + in_height-i);
	}
    }

    if (GREETING(w)[0]) {
	int gx = GREET_X(w);

#ifdef XPM
	gx -= ((w->login.logoWidth/2) + w->login.logoPadding);
#endif
	DRAW_STRING (greet, gx, GREET_Y(w), GREETING(w), strlen (GREETING(w)));
    }
    for (p = 0; p < NUM_PROMPTS ; p++) {
	if (PROMPT_STATE(w, p) != LOGIN_PROMPT_NOT_SHOWN) {
	    DRAW_STRING (prompt, PROMPT_X(w), PROMPT_Y(w,p),
			 CUR_PROMPT_TEXT(w,p), strlen (CUR_PROMPT_TEXT(w,p)));
	    DrawValue (w, 0, p);
	}
    }
    RedrawFail (w);
    XorCursor (w);
    XSetInputFocus (XtDisplay (w), XtWindow (w),
		    RevertToPointerRoot, CurrentTime);
}

/* Returns 0 on success, -1 on failure */
_X_INTERNAL
int
SetPrompt (Widget ctx, int promptNum, const char *message,
	   loginPromptState state, Boolean minimumTime)
{
    LoginWidget	w = (LoginWidget) ctx;
    char *prompt;
    int messageLen, e;
    const char *stateNames[4] = {
	"LOGIN_PROMPT_NOT_SHOWN", "LOGIN_PROMPT_ECHO_ON",
	"LOGIN_PROMPT_ECHO_OFF", "LOGIN_TEXT_INFO" };
    loginPromptState priorState;

    Debug("SetPrompt(%d, %s, %s(%d))\n", promptNum,
	  message ? message : "<NULL>", stateNames[state], state);

    XDM_ASSERT(promptNum >= 0 && promptNum <= LAST_PROMPT);

    if (PROMPT_TEXT(w, promptNum) != NULL) {
	XtFree(PROMPT_TEXT(w, promptNum));
	PROMPT_TEXT(w, promptNum) = NULL;
    }

    priorState = PROMPT_STATE(w, promptNum);
    PROMPT_STATE(w, promptNum) = state;

    if (state == LOGIN_PROMPT_NOT_SHOWN) {
	return 0;
    }

    if (message == NULL) {
	message = DEF_PROMPT_TEXT(w, promptNum);
    }

    messageLen = strlen(message);

    prompt = XtMalloc(messageLen + 3);
    if (prompt == NULL) {
	LogOutOfMem ("SetPrompt");
	return -1;
    }

    strncpy(prompt, message, messageLen);

    /* Make sure text prompts have at least two spaces at end */
    e = messageLen;

    if (!isspace(message[messageLen - 2])) {
	prompt[e] = ' ';
	e++;
    }
    if (!isspace(message[messageLen - 1])) {
	prompt[e] = ' ';
	e++;
    }
    prompt[e] = '\0';

    PROMPT_TEXT(w, promptNum) = prompt;

    if (w->login.state == INITIALIZING) {
	return 0;
    }

    if ((priorState == LOGIN_TEXT_INFO) && (w->login.msgTimeout != 0)) {
	time_t now = time(NULL);
	int timeleft = w->login.msgTimeout - now;

	if (timeleft > 0) {
	    sleep(timeleft);
	}
	w->login.msgTimeout = 0;
    }

    if (state == LOGIN_TEXT_INFO) {
	if (minimumTime) {
	    time_t now = time(NULL);
	    w->login.msgTimeout = now + w->login.failTimeout;
	}
	w->login.state = SHOW_MESSAGE;
    } else {
	w->login.activePrompt = promptNum;
	w->login.state = PROMPTING;
    }

    PROMPT_CURSOR(w, promptNum) = 0;
    XClearArea (XtDisplay(w), XtWindow(w), 0, 0, 0, 0, FALSE);
    draw_it(w);
    return 0;
}

_X_INTERNAL
const char *
GetPrompt(Widget ctx, int promptNum)
{
    LoginWidget	w = (LoginWidget) ctx;

    XDM_ASSERT(promptNum >= 0 && promptNum <= LAST_PROMPT);

    return CUR_PROMPT_TEXT(w,promptNum);
}

_X_INTERNAL
int
SetValue(Widget ctx, int promptNum, char *value)
{
    LoginWidget	w = (LoginWidget) ctx;

    XDM_ASSERT(promptNum >= 0 && promptNum <= LAST_PROMPT);

    if ((promptNum < 0) || (promptNum > LAST_PROMPT))
	return -1;

    XDM_ASSERT(VALUE_TEXT(w, promptNum) != NULL);

    if (VALUE_TEXT(w, promptNum) == NULL)
	return -1;

    if (value == NULL) {
	bzero(VALUE_TEXT(w, promptNum), VALUE_TEXT_MAX(w, promptNum));
    } else {
	strncpy(VALUE_TEXT(w, promptNum), value, VALUE_TEXT_MAX(w, promptNum));
	VALUE_TEXT(w, promptNum)[VALUE_TEXT_MAX(w, promptNum)] = '\0';
    }

    VALUE_SHOW_START(w, promptNum) = 0;
    VALUE_SHOW_END(w, promptNum) = 0;
    PROMPT_CURSOR(w, promptNum) = 0;

    return 0;
}

_X_INTERNAL
const char *
GetValue(Widget ctx, int promptNum)
{
    LoginWidget	w = (LoginWidget) ctx;

    XDM_ASSERT(promptNum >= 0 && promptNum <= LAST_PROMPT);

    if ((promptNum < 0) || (promptNum > LAST_PROMPT))
	return NULL;

    XDM_ASSERT(VALUE_TEXT(w, promptNum) != NULL);

    return VALUE_TEXT(w, promptNum);
}


static void
realizeDeleteChar (LoginWidget ctx)
{
    if (ctx->login.state == PROMPTING) {
	int promptNum = ctx->login.activePrompt;
	int redrawFrom = PROMPT_CURSOR(ctx, promptNum);

	if (PROMPT_CURSOR(ctx,promptNum) <  (int)strlen(VALUE_TEXT(ctx,promptNum))) {
	    if (redrawFrom < VALUE_SHOW_START(ctx, ctx->login.activePrompt)) {
		redrawFrom = 0;
		EraseValue (ctx, redrawFrom, promptNum);
		VALUE_SHOW_START(ctx, ctx->login.activePrompt)
		    = PROMPT_CURSOR(ctx,promptNum);
	    } else {
		EraseValue (ctx, redrawFrom, promptNum);
	    }
	    strcpy(VALUE_TEXT(ctx, promptNum) + PROMPT_CURSOR(ctx, promptNum),
		   VALUE_TEXT(ctx, promptNum) + PROMPT_CURSOR(ctx, promptNum) + 1);
	    DrawValue (ctx, redrawFrom, promptNum);
	}
    }
}

/*ARGSUSED*/
static void
DeleteBackwardChar (Widget ctxw, XEvent *event, String *params, Cardinal *num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    RemoveFail (ctx);

    if (ctx->login.state != PROMPTING) {
	return;
    }

    XorCursor (ctx);
    if (CUR_PROMPT_CURSOR(ctx) > 0) {
	CUR_PROMPT_CURSOR(ctx) -= 1;
	realizeDeleteChar(ctx);
    }
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
DeleteForwardChar (Widget ctxw, XEvent *event, String *params, Cardinal *num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    RemoveFail (ctx);

    if (ctx->login.state != PROMPTING) {
	return;
    }

    XorCursor (ctx);
    realizeDeleteChar(ctx);
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
MoveBackwardChar (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget	ctx = (LoginWidget)ctxw;

    RemoveFail (ctx);

    if (ctx->login.state != PROMPTING) {
	return;
    }

    XorCursor (ctx);
    if (CUR_PROMPT_CURSOR(ctx) > 0)
	CUR_PROMPT_CURSOR(ctx) -= 1;
    if (CUR_PROMPT_CURSOR(ctx) < VALUE_SHOW_START(ctx, ctx->login.activePrompt)) {
	EraseValue(ctx, 0, ctx->login.activePrompt);
	VALUE_SHOW_START(ctx, ctx->login.activePrompt)
	    = CUR_PROMPT_CURSOR(ctx);
	DrawValue(ctx, 0, ctx->login.activePrompt);
    }
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
MoveForwardChar (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    RemoveFail (ctx);

    if (ctx->login.state != PROMPTING) {
	return;
    }

    XorCursor (ctx);
    if (CUR_PROMPT_CURSOR(ctx) <
	(int)strlen(VALUE_TEXT(ctx,ctx->login.activePrompt))) {
	CUR_PROMPT_CURSOR(ctx) += 1;
	if (VALUE_SHOW_END(ctx, ctx->login.activePrompt)
	    < CUR_PROMPT_CURSOR(ctx)) {
	    EraseValue(ctx, 0, ctx->login.activePrompt);
	    DrawValue(ctx, 0, ctx->login.activePrompt);
	}
    }
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
MoveToBegining (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    RemoveFail (ctx);

    if (ctx->login.state != PROMPTING) {
	return;
    }

    XorCursor (ctx);
    CUR_PROMPT_CURSOR(ctx) = 0;
    if (VALUE_SHOW_START(ctx, ctx->login.activePrompt) > 0) {
	EraseValue(ctx, 0, ctx->login.activePrompt);
	VALUE_SHOW_START(ctx, ctx->login.activePrompt) = 0;
	DrawValue(ctx, 0, ctx->login.activePrompt);
    }
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
MoveToEnd (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    RemoveFail (ctx);

    if (ctx->login.state != PROMPTING) {
	return;
    }

    XorCursor (ctx);
    CUR_PROMPT_CURSOR(ctx) = strlen (VALUE_TEXT(ctx, ctx->login.activePrompt));
    if (VALUE_SHOW_END(ctx, ctx->login.activePrompt) < CUR_PROMPT_CURSOR(ctx)) {
	EraseValue(ctx, 0, ctx->login.activePrompt);
	DrawValue(ctx, 0, ctx->login.activePrompt);
    }
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
EraseToEndOfLine (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    RemoveFail (ctx);

    if (ctx->login.state != PROMPTING) {
	return;
    }

    XorCursor (ctx);
    EraseValue (ctx, CUR_PROMPT_CURSOR(ctx), ctx->login.activePrompt);
    bzero(VALUE_TEXT(ctx, ctx->login.activePrompt) +
	  CUR_PROMPT_CURSOR(ctx),
	  VALUE_TEXT_MAX(ctx, ctx->login.activePrompt) -
	  CUR_PROMPT_CURSOR(ctx));
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
EraseLine (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    MoveToBegining (ctxw, event, params, num_params);
    EraseToEndOfLine (ctxw, event, params, num_params);
}

/*ARGSUSED*/
static void
FinishField (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;
    int promptNum = ctx->login.activePrompt;
    int nextPrompt;

    RemoveFail (ctx);

    if (ctx->login.state != PROMPTING) {
	return;
    }

    XorCursor (ctx);

    for (nextPrompt = promptNum + 1; nextPrompt <= LAST_PROMPT; nextPrompt++) {
	if ((PROMPT_STATE(ctx, nextPrompt) == LOGIN_PROMPT_ECHO_ON) ||
	    (PROMPT_STATE(ctx, nextPrompt) == LOGIN_PROMPT_ECHO_OFF)) {
	    ctx->login.activePrompt = nextPrompt;
	    break;
	}
    }
    if (nextPrompt > LAST_PROMPT) {
	ctx->login.state = DONE;
	(*ctx->login.notify_done) (ctx, &ctx->login.data, NOTIFY_OK);
	Debug("FinishField #%d: now DONE\n", promptNum);
    } else {
	Debug("FinishField #%d: %d next\n", promptNum, nextPrompt);
    }

    XorCursor (ctx);
}

/*ARGSUSED*/
static void
TabField(Widget ctxw, XEvent *event, String *params, Cardinal *num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;
    int promptNum = ctx->login.activePrompt;
    int nextPrompt;

    RemoveFail (ctx);

    if (ctx->login.state != PROMPTING) {
	return;
    }

    XorCursor (ctx);

    for (nextPrompt = promptNum + 1; nextPrompt != promptNum; nextPrompt++) {
	if (nextPrompt > LAST_PROMPT) {
	    nextPrompt = 0;
	}

	if ((PROMPT_STATE(ctx, nextPrompt) == LOGIN_PROMPT_ECHO_ON) ||
	    (PROMPT_STATE(ctx, nextPrompt) == LOGIN_PROMPT_ECHO_OFF)) {
	    ctx->login.activePrompt = nextPrompt;
	    break;
	}
    }
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
AllowAccess (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;
    Arg	arglist[1];
    Boolean allow;

    RemoveFail (ctx);
    XtSetArg (arglist[0], XtNallowAccess, (char *) &allow);
    XtGetValues ((Widget) ctx, arglist, 1);
    XtSetArg (arglist[0], XtNallowAccess, !allow);
    XtSetValues ((Widget) ctx, arglist, 1);
}

/*ARGSUSED*/
static void
SetSessionArgument (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    RemoveFail (ctx);
    if (ctx->login.sessionArg)
	XtFree (ctx->login.sessionArg);
    ctx->login.sessionArg = NULL;
    if (*num_params > 0) {
	ctx->login.sessionArg = XtMalloc (strlen (params[0]) + 1);
	if (ctx->login.sessionArg)
	    strcpy (ctx->login.sessionArg, params[0]);
	else
	    LogOutOfMem ("set session argument");
    }
}

/*ARGSUSED*/
static void
RestartSession (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    XorCursor (ctx);
    RemoveFail (ctx);
    ctx->login.state = DONE;
    (*ctx->login.notify_done) (ctx, &ctx->login.data, NOTIFY_RESTART);
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
AbortSession (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    XorCursor (ctx);
    RemoveFail (ctx);
    ctx->login.state = DONE;
    (*ctx->login.notify_done) (ctx, &ctx->login.data, NOTIFY_ABORT);
    XorCursor (ctx);
}

/*ARGSUSED*/
static void
AbortDisplay (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    XorCursor (ctx);
    RemoveFail (ctx);
    ctx->login.state = DONE;
    (*ctx->login.notify_done) (ctx, &ctx->login.data, NOTIFY_ABORT_DISPLAY);
    XorCursor (ctx);
}

static void
ResetLogin (LoginWidget w)
{
    int i;

    for (i = 0; i < NUM_PROMPTS ; i++) {
	EraseValue(w, 0, i);
	bzero(VALUE_TEXT(w, i), VALUE_TEXT_MAX(w, i));
	VALUE_SHOW_START(w, i) = 0;
	PROMPT_CURSOR(w, i) = 0;
    }
    w->login.state = PROMPTING;
    w->login.activePrompt = 0;
}

static void
InitI18N(Widget ctxw)
{
    LoginWidget ctx = (LoginWidget)ctxw;
    XIM         xim = (XIM) NULL;
    char *p;

    ctx->login.xic = (XIC) NULL;

    if ((p = XSetLocaleModifiers("@im=none")) != NULL && *p)
	xim = XOpenIM(XtDisplay(ctx), NULL, NULL, NULL);

    if (!xim) {
	LogError("Failed to open input method\n");
	return;
    }

    ctx->login.xic = XCreateIC(xim,
	XNInputStyle, (XIMPreeditNothing|XIMStatusNothing),
	XNClientWindow, ctx->core.window,
	XNFocusWindow,  ctx->core.window, NULL);

    if (!ctx->login.xic) {
	LogError("Failed to create input context\n");
	XCloseIM(xim);
    }
    return;
}

/* ARGSUSED */
static void
InsertChar (
    Widget	ctxw,
    XEvent	*event,
    String	*params,
    Cardinal	*num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    char strbuf[128];
    int  len, promptNum = ctx->login.activePrompt;
    KeySym  keysym = 0;

    if (ctx->login.xic) {
	static Status status;
	len = XmbLookupString(ctx->login.xic, &event->xkey, strbuf,
			      sizeof (strbuf), &keysym, &status);
    } else {
	static XComposeStatus compose_status = {NULL, 0};
	len = XLookupString (&event->xkey, strbuf, sizeof (strbuf),
			     &keysym, &compose_status);
    }
    strbuf[len] = '\0';

    /*
     * Note: You can override this default key handling
     * by the settings in the translation table
     * loginActionsTable at the end of this file.
     */
    switch (keysym) {
    case XK_Return:
    case XK_KP_Enter:
    case XK_Linefeed:
    case XK_Execute:
	FinishField(ctxw, event, params, num_params);
	return;
    case XK_BackSpace:
	DeleteBackwardChar(ctxw, event, params, num_params);
	return;
    case XK_Delete:
    case XK_KP_Delete:
    case DXK_Remove:
	/* Sorry, it's not a telex machine, it's a terminal */
	DeleteForwardChar(ctxw, event, params, num_params);
	return;
    case XK_Left:
    case XK_KP_Left:
	MoveBackwardChar(ctxw, event, params, num_params);
	return;
    case XK_Right:
    case XK_KP_Right:
	MoveForwardChar(ctxw, event, params, num_params);
	return;
    case XK_End:
    case XK_KP_End:
	MoveToEnd(ctxw, event, params, num_params);
	return;
    case XK_Home:
    case XK_KP_Home:
	MoveToBegining(ctxw, event, params, num_params);
	return;
    default:
	if (len == 0) {
	    if (!IsModifierKey(keysym))  /* it's not a modifier */
		XBell(XtDisplay(ctxw), 60);
	    return;
	} else
	    break;
    }

    if (ctx->login.state == PROMPTING) {
	if ((len + (int)strlen(VALUE_TEXT(ctx, promptNum)) >=
	     (VALUE_TEXT_MAX(ctx,promptNum) - 1))) {
	    len = VALUE_TEXT_MAX(ctx,promptNum) -
		strlen(VALUE_TEXT(ctx, promptNum)) - 2;
	}
    }
    EraseCursor (ctx);
    RemoveFail (ctx);
    if (len != 0)
    {
	if (ctx->login.state == PROMPTING) {
	    EraseValue (ctx, PROMPT_CURSOR(ctx, promptNum), promptNum);
	    memmove(VALUE_TEXT(ctx, promptNum) + PROMPT_CURSOR(ctx, promptNum) + len,
		    VALUE_TEXT(ctx, promptNum) + PROMPT_CURSOR(ctx, promptNum),
		    strlen (VALUE_TEXT(ctx, promptNum) + PROMPT_CURSOR(ctx, promptNum))+1);
	    memmove(VALUE_TEXT(ctx, promptNum) + PROMPT_CURSOR(ctx, promptNum),
		     strbuf, len);
	    DrawValue (ctx, PROMPT_CURSOR(ctx, promptNum), promptNum);
	    PROMPT_CURSOR(ctx, promptNum) += len;
	}
    }
    XorCursor (ctx);
}


/**** Copied from xclock.c - original author: Keith Packard ****/
#ifdef USE_XFT
static XtConvertArgRec xftColorConvertArgs[] = {
    {XtWidgetBaseOffset, (XtPointer)XtOffsetOf(WidgetRec, core.screen),
     sizeof(Screen *)},
    {XtWidgetBaseOffset, (XtPointer)XtOffsetOf(WidgetRec, core.colormap),
     sizeof(Colormap)}
};

# define	donestr(type, value, tstr) \
	{							\
	    if (toVal->addr != NULL) {				\
		if (toVal->size < sizeof(type)) {		\
		    toVal->size = sizeof(type);			\
		    XtDisplayStringConversionWarning(dpy, 	\
			(char*) fromVal->addr, tstr);		\
		    return False;				\
		}						\
		*(type*)(toVal->addr) = (value);		\
	    }							\
	    else {						\
		static type static_val;				\
		static_val = (value);				\
		toVal->addr = (XPointer)&static_val;		\
	    }							\
	    toVal->size = sizeof(type);				\
	    return True;					\
	}

static void
XmuFreeXftColor (XtAppContext app, XrmValuePtr toVal, XtPointer closure,
		 XrmValuePtr args, Cardinal *num_args)
{
    Screen	*screen;
    Colormap	colormap;
    XftColor	*color;

    if (*num_args != 2)
    {
	XtAppErrorMsg (app,
		       "freeXftColor", "wrongParameters",
		       "XtToolkitError",
		       "Freeing an XftColor requires screen and colormap arguments",
		       (String *) NULL, (Cardinal *)NULL);
	return;
    }

    screen = *((Screen **) args[0].addr);
    colormap = *((Colormap *) args[1].addr);
    color = (XftColor *) toVal->addr;
    XftColorFree (DisplayOfScreen (screen),
		  DefaultVisual (DisplayOfScreen (screen),
				 XScreenNumberOfScreen (screen)),
		  colormap, color);
}

static Boolean
XmuCvtStringToXftColor(Display *dpy,
		       XrmValue *args, Cardinal *num_args,
		       XrmValue *fromVal, XrmValue *toVal,
		       XtPointer *converter_data)
{
    char	    *spec;
    XRenderColor    renderColor;
    XftColor	    xftColor;
    Screen	    *screen;
    Colormap	    colormap;

    if (*num_args != 2)
    {
	XtAppErrorMsg (XtDisplayToApplicationContext (dpy),
		       "cvtStringToXftColor", "wrongParameters",
		       "XtToolkitError",
		       "String to render color conversion needs screen and colormap arguments",
		       (String *) NULL, (Cardinal *)NULL);
	return False;
    }

    screen = *((Screen **) args[0].addr);
    colormap = *((Colormap *) args[1].addr);

    spec = (char *) fromVal->addr;
    if (strcasecmp (spec, XtDefaultForeground) == 0)
    {
	renderColor.red = 0;
	renderColor.green = 0;
	renderColor.blue = 0;
	renderColor.alpha = 0xffff;
    }
    else if (strcasecmp (spec, XtDefaultBackground) == 0)
    {
	renderColor.red = 0xffff;
	renderColor.green = 0xffff;
	renderColor.blue = 0xffff;
	renderColor.alpha = 0xffff;
    }
    else if (!XRenderParseColor (dpy, spec, &renderColor))
	return False;
    if (!XftColorAllocValue (dpy,
			     DefaultVisual (dpy,
					    XScreenNumberOfScreen (screen)),
			     colormap,
			     &renderColor,
			     &xftColor))
	return False;

    donestr (XftColor, xftColor, XtRXftColor);
}

static void
XmuFreeXftFont (XtAppContext app, XrmValuePtr toVal, XtPointer closure,
		XrmValuePtr args, Cardinal *num_args)
{
    Screen  *screen;
    XftFont *font;

    if (*num_args != 1)
    {
	XtAppErrorMsg (app,
		       "freeXftFont", "wrongParameters",
		       "XtToolkitError",
		       "Freeing an XftFont requires screen argument",
		       (String *) NULL, (Cardinal *)NULL);
	return;
    }

    screen = *((Screen **) args[0].addr);
    font = *((XftFont **) toVal->addr);
    if (font)
	XftFontClose (DisplayOfScreen (screen), font);
}

static Boolean
XmuCvtStringToXftFont(Display *dpy,
		      XrmValue *args, Cardinal *num_args,
		      XrmValue *fromVal, XrmValue *toVal,
		      XtPointer *converter_data)
{
    char    *name;
    XftFont *font;
    Screen  *screen;

    if (*num_args != 1)
    {
	XtAppErrorMsg (XtDisplayToApplicationContext (dpy),
		       "cvtStringToXftFont", "wrongParameters",
		       "XtToolkitError",
		       "String to XftFont conversion needs screen argument",
		       (String *) NULL, (Cardinal *)NULL);
	return False;
    }

    screen = *((Screen **) args[0].addr);
    name = (char *) fromVal->addr;

    font = XftFontOpenName (dpy,
			    XScreenNumberOfScreen (screen),
			    name);
    if (font)
    {
	donestr (XftFont *, font, XtRXftFont);
    }
    XtDisplayStringConversionWarning(dpy, (char *) fromVal->addr, XtRXftFont);
    return False;
}

static XtConvertArgRec xftFontConvertArgs[] = {
    {XtWidgetBaseOffset, (XtPointer)XtOffsetOf(WidgetRec, core.screen),
     sizeof(Screen *)},
};


static int
XmuXftTextWidth(Display *dpy, XftFont *font, FcChar8 *string, int len)
{
    XGlyphInfo  extents;

    XftTextExtents8 (dpy, font, string, len, &extents);

    return extents.xOff;
}

#endif /* USE_XFT */

static void
ClassInitialize(void)
{
#ifdef USE_XFT
    XtSetTypeConverter (XtRString, XtRXftColor,
			XmuCvtStringToXftColor,
			xftColorConvertArgs, XtNumber(xftColorConvertArgs),
			XtCacheByDisplay, XmuFreeXftColor);
    XtSetTypeConverter (XtRString, XtRXftFont,
			XmuCvtStringToXftFont,
			xftFontConvertArgs, XtNumber(xftFontConvertArgs),
			XtCacheByDisplay, XmuFreeXftFont);
#endif /* USE_XFT */
}
/**** End of portion borrowed from xclock ****/

/* ARGSUSED */
static void Initialize (
    Widget greq,
    Widget gnew,
    ArgList args,
    Cardinal *num_args)
{
    LoginWidget w = (LoginWidget)gnew;
    XtGCMask	valuemask, xvaluemask;
    XGCValues	myXGCV;
    Arg		position[2];
    Position	x, y;
#ifdef USE_XINERAMA
    XineramaScreenInfo *screens;
    int                 s_num;
#endif
    int 	rv = 0;

    myXGCV.foreground = w->login.hipixel;
    myXGCV.background = w->core.background_pixel;
    valuemask = GCForeground | GCBackground;
    w->login.hiGC = XtGetGC(gnew, valuemask, &myXGCV);

    myXGCV.foreground = w->login.shdpixel;
    myXGCV.background = w->core.background_pixel;
    valuemask = GCForeground | GCBackground;
    w->login.shdGC = XtGetGC(gnew, valuemask, &myXGCV);

    myXGCV.foreground = TEXT_COLOR(text);
    myXGCV.background = w->core.background_pixel;
    valuemask = GCForeground | GCBackground;
#ifndef USE_XFT
    if (w->login.textFont) {
	myXGCV.font = w->login.textFont->fid;
	valuemask |= GCFont;
    }
#endif
    w->login.textGC = XtGetGC(gnew, valuemask, &myXGCV);
    myXGCV.foreground = w->core.background_pixel;
    w->login.bgGC = XtGetGC(gnew, valuemask, &myXGCV);

    myXGCV.foreground = TEXT_COLOR(text) ^ w->core.background_pixel;
    myXGCV.function = GXxor;
    xvaluemask = valuemask | GCFunction;
    w->login.xorGC = XtGetGC (gnew, xvaluemask, &myXGCV);

#ifndef USE_XFT
    /*
     * Note that the second argument is a GCid -- QueryFont accepts a GCid and
     * returns the curently contained font.
     */

    if (w->login.textFont == NULL)
	w->login.textFont = XQueryFont (XtDisplay (w),
		XGContextFromGC (XDefaultGCOfScreen (XtScreen (w))));

    xvaluemask = valuemask;
    if (w->login.promptFont == NULL)
        w->login.promptFont = w->login.textFont;
    else
	xvaluemask |= GCFont;

    myXGCV.foreground = TEXT_COLOR(prompt);
    myXGCV.font = w->login.promptFont->fid;
    w->login.promptGC = XtGetGC (gnew, xvaluemask, &myXGCV);

    xvaluemask = valuemask;
    if (w->login.greetFont == NULL)
    	w->login.greetFont = w->login.textFont;
    else
	xvaluemask |= GCFont;

    myXGCV.foreground = TEXT_COLOR(greet);
    myXGCV.font = w->login.greetFont->fid;
    w->login.greetGC = XtGetGC (gnew, xvaluemask, &myXGCV);

    xvaluemask = valuemask;
    if (w->login.failFont == NULL)
	w->login.failFont = w->login.textFont;
    else
	xvaluemask |= GCFont;
    myXGCV.foreground = TEXT_COLOR(fail);
    myXGCV.font = w->login.failFont->fid;
    w->login.failGC = XtGetGC (gnew, xvaluemask, &myXGCV);
#endif /* USE_XFT */

#ifdef XPM
    w->login.logoValid = False;

    if (NULL != w->login.logoFileName)
    {
        XpmAttributes myAttributes = { 0 };
        Window tmpWindow = { 0 };
        struct stat myBuffer = { 0 };
        unsigned int myPixmapDepth = 0;

        if (0 != stat(w->login.logoFileName, &myBuffer))
        {
            LogError("Unable to stat() pixmap file %s\n",
                w->login.logoFileName);
            w->login.logoValid = False;
            goto SkipXpmLoad;
        }

        myAttributes.valuemask |= XpmReturnPixels;
        myAttributes.valuemask |= XpmReturnExtensions;

        rv = XpmReadFileToPixmap(XtDisplay(w),		/* display */
	     RootWindowOfScreen(XtScreen(w)),		/* window */
	     w->login.logoFileName,			/* XPM filename */
	     &(w->login.logoPixmap),			/* pixmap */
	     &(w->login.logoMask),			/* pixmap mask */
	     &myAttributes);				/* XPM attributes */

	if ( rv < 0 )
	{
	    LogError("Cannot load xpm file %s: %s.\n", w->login.logoFileName,
		     XpmGetErrorString(rv));
	    goto SkipXpmLoad;
	}

	w->login.logoValid = True;

        XGetGeometry(XtDisplay(w), w->login.logoPixmap,
            &tmpWindow,
            &(w->login.logoX),
            &(w->login.logoY),
            &(w->login.logoWidth),
            &(w->login.logoHeight),
            &(w->login.logoBorderWidth),
            &myPixmapDepth);
    } else {
	w->login.logoX = 0;
	w->login.logoY = 0;
	w->login.logoWidth = 0;
	w->login.logoHeight = 0;
	w->login.logoBorderWidth = 0;
    }


SkipXpmLoad:
#endif /* XPM */
    w->login.data.name[0] = '\0';
    w->login.data.passwd[0] = '\0';
    w->login.state = INITIALIZING;
    w->login.activePrompt = LOGIN_PROMPT_USERNAME;
    w->login.failUp = 0;
    w->login.fail = (char *) w->login.failMsg;

    /* Set prompt defaults */
    PROMPT_TEXT(w, LOGIN_PROMPT_USERNAME) 	= NULL;
    DEF_PROMPT_TEXT(w, LOGIN_PROMPT_USERNAME) 	= w->login.namePrompt;
    VALUE_TEXT(w, LOGIN_PROMPT_USERNAME) 	= w->login.data.name;
    VALUE_TEXT_MAX(w, LOGIN_PROMPT_USERNAME)	= sizeof(w->login.data.name);
    VALUE_SHOW_START(w, LOGIN_PROMPT_USERNAME)	= 0;

    PROMPT_TEXT(w, LOGIN_PROMPT_PASSWORD) 	= NULL;
    DEF_PROMPT_TEXT(w, LOGIN_PROMPT_PASSWORD) 	= w->login.passwdPrompt;
    VALUE_TEXT(w, LOGIN_PROMPT_PASSWORD) 	= w->login.data.passwd;
    VALUE_TEXT_MAX(w, LOGIN_PROMPT_PASSWORD)	= sizeof(w->login.data.passwd);
    VALUE_SHOW_START(w, LOGIN_PROMPT_PASSWORD)	= 0;

    SetPrompt(gnew, LOGIN_PROMPT_PASSWORD, NULL, LOGIN_PROMPT_ECHO_OFF, False);
    SetPrompt(gnew, LOGIN_PROMPT_USERNAME, NULL, LOGIN_PROMPT_ECHO_ON, False);

    if (w->core.width == 0)
	w->core.width = max (GREET_W(w), FAIL_W(w)) + PAD_X(w);
    if (w->core.height == 0) {
	int fy = FAIL_Y(w);
	int pady = PAD_Y(w);

#ifndef XPM
	w->core.height = fy + pady;	/* for stupid compilers */
#else
/*	w->core.height = fy + pady;	* for stupid compilers */

        w->core.height = max(fy + pady,
            (w->login.logoHeight + (2*w->login.logoPadding)) + pady);

#endif /* XPM */
    }
#ifdef USE_XINERAMA
    if (
	XineramaIsActive(XtDisplay(w)) &&
	(screens = XineramaQueryScreens(XtDisplay(w), &s_num)) != NULL
       )
    {
	if ((x = w->core.x) == -1)
	    x = screens[0].x_org + (int)(screens[0].width - w->core.width) / 2;
	if ((y = w->core.y) == -1)
	    y = screens[0].y_org + (int)(screens[0].height - w->core.height) / 3;

	XFree(screens);
    }
    else
#endif
    {
	if ((x = w->core.x) == -1)
	    x = (int)(XWidthOfScreen (XtScreen (w)) - w->core.width) / 2;
	if ((y = w->core.y) == -1)
	    y = (int)(XHeightOfScreen (XtScreen (w)) - w->core.height) / 3;
    }
    XtSetArg (position[0], XtNx, x);
    XtSetArg (position[1], XtNy, y);
    XtSetValues (XtParent (w), position, (Cardinal) 2);

    w->login.state = PROMPTING;
}


static void Realize (
     Widget gw,
     XtValueMask *valueMask,
     XSetWindowAttributes *attrs)
{
    LoginWidget	w = (LoginWidget) gw;
    Cursor cursor;

    XtCreateWindow( gw, (unsigned)InputOutput, (Visual *)CopyFromParent,
		     *valueMask, attrs );
    InitI18N(gw);

#ifdef USE_XFT
    w->login.draw = XftDrawCreate (XtDisplay (w), XtWindow(w),
	   DefaultVisual (XtDisplay (w), DefaultScreen(XtDisplay (w))),
				   w->core.colormap);

#endif

    cursor = XCreateFontCursor(XtDisplay(gw), XC_left_ptr);
    XDefineCursor(XtDisplay(gw), DefaultRootWindow(XtDisplay(gw)), cursor);

#ifdef XPM
    /*
     * Check if Pixmap was valid
     */
    if (True == w->login.logoValid)
    {
        /*
         * Create pixmap window
         */
        {
            unsigned long valueMask = CWBackPixel | CWBackPixmap;
            XSetWindowAttributes windowAttributes = { 0 };

            windowAttributes.background_pixel = w->core.background_pixel;
            windowAttributes.background_pixmap = None;

            w->login.logoWindow = XCreateWindow(XtDisplay(w),
                XtWindow(w),
                w->core.width - w->login.outframewidth -
                    w->login.logoWidth - w->login.logoPadding,
                (w->core.height - w->login.logoHeight) /2,
                w->login.logoWidth, w->login.logoHeight, 0,
                CopyFromParent, InputOutput, CopyFromParent,
                valueMask, &windowAttributes);
        }

        /*
         * check if we can use shape extension
         */
        if (True == w->login.useShape)
        {
            int foo, bar;

            if (XShapeQueryExtension(XtDisplay(w), &foo, &bar) == TRUE)
            {
                XShapeCombineMask(XtDisplay(w), w->login.logoWindow,
                    ShapeBounding, w->login.logoX, w->login.logoY,
                    w->login.logoMask, ShapeSet);
            }
        }

        XSetWindowBackgroundPixmap(XtDisplay(w), w->login.logoWindow,
            w->login.logoPixmap);
        XMapWindow(XtDisplay(w), w->login.logoWindow);
    }
#endif /* XPM */
}

static void Destroy (Widget gw)
{
    LoginWidget w = (LoginWidget)gw;
    bzero (w->login.data.name, NAME_LEN);
    bzero (w->login.data.passwd, PASSWORD_LEN);

    if (PROMPT_TEXT(w,0) != NULL)
	XtFree(PROMPT_TEXT(w,0));
    if (PROMPT_TEXT(w,1) != NULL)
	XtFree(PROMPT_TEXT(w,1));

#ifdef USE_XFT
    if (w->login.draw) {
	XftDrawDestroy(w->login.draw);
	w->login.draw = NULL;
    }
#endif

    XtReleaseGC(gw, w->login.textGC);
    XtReleaseGC(gw, w->login.bgGC);
    XtReleaseGC(gw, w->login.xorGC);
#ifndef USE_XFT
    XtReleaseGC(gw, w->login.promptGC);
    XtReleaseGC(gw, w->login.greetGC);
    XtReleaseGC(gw, w->login.failGC);
#endif
    XtReleaseGC(gw, w->login.hiGC);
    XtReleaseGC(gw, w->login.shdGC);

#ifdef XPM
    if (True == w->login.logoValid)
    {
        if (w->login.logoPixmap != 0)
            XFreePixmap(XtDisplay(w), w->login.logoPixmap);

        if (w->login.logoMask != 0)
            XFreePixmap(XtDisplay(w), w->login.logoMask);
    }
#endif /* XPM */
}

/* ARGSUSED */
static void Redisplay(
     Widget gw,
     XEvent *event,
     Region region)
{
    draw_it ((LoginWidget) gw);
}

/*ARGSUSED*/
static Boolean SetValues (
    Widget  current,
    Widget  request,
    Widget  new,
    ArgList args,
    Cardinal *num_args)
{
    LoginWidget currentL, newL;

    currentL = (LoginWidget) current;
    newL = (LoginWidget) new;
    if (GREETING (currentL) != GREETING (newL))
	return True;
    return False;
}

static
char defaultLoginTranslations [] =
"Ctrl<Key>H:	delete-previous-character() \n"
"Ctrl<Key>D:	delete-character() \n"
"Ctrl<Key>B:	move-backward-character() \n"
"Ctrl<Key>F:	move-forward-character() \n"
"Ctrl<Key>A:	move-to-begining() \n"
"Ctrl<Key>E:	move-to-end() \n"
"Ctrl<Key>K:	erase-to-end-of-line() \n"
"Ctrl<Key>U:	erase-line() \n"
"Ctrl<Key>X:	erase-line() \n"
"Ctrl<Key>C:	restart-session() \n"
"Ctrl<Key>\\\\:	abort-session() \n"
":Ctrl<Key>plus:	allow-all-access() \n"
"<Key>BackSpace:	delete-previous-character() \n"
#ifdef linux
"<Key>Delete:	delete-character() \n"
#else
"<Key>Delete:	delete-previous-character() \n"
#endif
"<Key>Return:	finish-field() \n"
"<Key>Tab:	tab-field() \n"
"<KeyPress>:	insert-char()"
;

static
XtActionsRec loginActionsTable [] = {
  {"delete-previous-character",	DeleteBackwardChar},
  {"delete-character",		DeleteForwardChar},
  {"move-backward-character",	MoveBackwardChar},
  {"move-forward-character",	MoveForwardChar},
  {"move-to-begining",		MoveToBegining},
  {"move-to-end",		MoveToEnd},
  {"erase-to-end-of-line",	EraseToEndOfLine},
  {"erase-line",		EraseLine},
  {"finish-field", 		FinishField},
  {"tab-field", 		TabField},
  {"abort-session",		AbortSession},
  {"abort-display",		AbortDisplay},
  {"restart-session",		RestartSession},
  {"insert-char", 		InsertChar},
  {"set-session-argument",	SetSessionArgument},
  {"allow-all-access",		AllowAccess},
};

LoginClassRec loginClassRec = {
    { /* core fields */
    /* superclass		*/	&widgetClassRec,
    /* class_name		*/	"Login",
    /* size			*/	sizeof(LoginRec),
    /* class_initialize		*/	ClassInitialize,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	Realize,
    /* actions			*/	loginActionsTable,
    /* num_actions		*/	XtNumber (loginActionsTable),
    /* resources		*/	resources,
    /* num_resources		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	NULL,
    /* expose			*/	Redisplay,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	defaultLoginTranslations,
    /* query_geometry		*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
    }
};

WidgetClass loginWidgetClass = (WidgetClass) &loginClassRec;
