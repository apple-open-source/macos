/* $Xorg: Login.c,v 1.4 2001/02/09 02:05:41 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xdm/greeter/Login.c,v 3.18 2003/07/09 15:27:40 tsi Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * Login.c
 */

# include <X11/IntrinsicP.h>
# include <X11/StringDefs.h>
# include <X11/keysym.h>
# include <X11/DECkeysym.h>
# include <X11/Xfuncs.h>

# include <stdio.h>
#ifdef XPM
# include <time.h>
#endif /* XPM */

# include "dm.h"
# include "dm_error.h"
# include "greet.h"
# include "LoginP.h"

#ifdef XPM
#include <sys/stat.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>
#include <X11/cursorfont.h>
#endif /* XPM */

#ifdef USE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

static void RedrawFail (LoginWidget w);
static void ResetLogin (LoginWidget w);
static void failTimeout (XtPointer client_data, XtIntervalId * id);

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
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
        offset(textpixel), XtRString,	XtDefaultForeground},
    {XtNpromptColor, XtCForeground, XtRPixel, sizeof(Pixel),
        offset(promptpixel), XtRString,	XtDefaultForeground},
    {XtNgreetColor, XtCForeground, XtRPixel, sizeof(Pixel),
        offset(greetpixel), XtRString,	XtDefaultForeground},
    {XtNfailColor, XtCForeground, XtRPixel, sizeof (Pixel),
	offset(failpixel), XtRString,	XtDefaultForeground},

#ifdef XPM
/* added by Caolan McNamara */
	{XtNlastEventTime, XtCLastEventTime, XtRInt , sizeof (int),
	offset(lastEventTime), XtRImmediate,	(XtPointer)0},
/* end (caolan) */

/* added by Ivan Griffin (ivan.griffin@ul.ie) */
        {XtNlogoFileName, XtCLogoFileName, XtRString, sizeof(char*),
        offset(logoFileName), XtRImmediate, (XtPointer)0},
        {XtNuseShape, XtCUseShape, XtRBoolean, sizeof(Boolean),
        offset(useShape), XtRImmediate, (XtPointer) True},
        {XtNlogoPadding, XtCLogoPadding, XtRInt, sizeof(int),
        offset(logoPadding), XtRImmediate, (XtPointer) 5},
/* end (ivan) */


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
#endif /* XPM */

    {XtNfont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
    	offset (font), XtRString,	"*-new century schoolbook-medium-r-normal-*-180-*"},
    {XtNpromptFont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
    	offset (promptFont), XtRString, "*-new century schoolbook-bold-r-normal-*-180-*"},
    {XtNgreetFont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
    	offset (greetFont), XtRString,	"*-new century schoolbook-bold-i-normal-*-240-*"},
    {XtNfailFont, XtCFont, XtRFontStruct, sizeof (XFontStruct *),
	offset (failFont), XtRString,	"*-new century schoolbook-bold-r-normal-*-180-*"},
    {XtNgreeting, XtCGreeting, XtRString, sizeof (char *),
    	offset(greeting), XtRString, "X Window System"},
    {XtNunsecureGreeting, XtCGreeting, XtRString, sizeof (char *),
	offset(unsecure_greet), XtRString, "This is an unsecure session"},
    {XtNnamePrompt, XtCNamePrompt, XtRString, sizeof (char *),
	offset(namePrompt), XtRString, "Login:  "},
    {XtNpasswdPrompt, XtCPasswdPrompt, XtRString, sizeof (char *),
	offset(passwdPrompt), XtRString, "Password:  "},
    {XtNfail, XtCFail, XtRString, sizeof (char *),
	offset(fail), XtRString, 
#if defined(sun) && defined(SVR4)
     "Login incorrect or not on system console if root"},
#else
     "Login incorrect"},
#endif
    {XtNfailTimeout, XtCFailTimeout, XtRInt, sizeof (int),
	offset(failTimeout), XtRImmediate, (XtPointer) 10},
    {XtNnotifyDone, XtCCallback, XtRFunction, sizeof (XtPointer),
	offset(notify_done), XtRFunction, (XtPointer) 0},
    {XtNsessionArgument, XtCSessionArgument, XtRString,	sizeof (char *),
	offset(sessionArg), XtRString, (XtPointer) 0 },
    {XtNsecureSession, XtCSecureSession, XtRBoolean, sizeof (Boolean),
	offset(secure_session), XtRImmediate, False },
    {XtNallowAccess, XtCAllowAccess, XtRBoolean, sizeof (Boolean),
	offset(allow_access), XtRImmediate, False },
    {XtNallowNullPasswd, XtCAllowNullPasswd, XtRBoolean, sizeof (Boolean),
	offset(allow_null_passwd), XtRImmediate, False},
    {XtNallowRootLogin, XtCAllowRootLogin, XtRBoolean, sizeof(Boolean),
     offset(allow_root_login), XtRImmediate, (XtPointer) True}
};

#undef offset
#undef goffset

# define TEXT_X_INC(w)	((w)->login.font->max_bounds.width)
# define TEXT_Y_INC(w)	((w)->login.font->max_bounds.ascent +\
			 (w)->login.font->max_bounds.descent)
# define PROMPT_X_INC(w)	((w)->login.promptFont->max_bounds.width)
# define PROMPT_Y_INC(w)	((w)->login.promptFont->max_bounds.ascent +\
			 (w)->login.promptFont->max_bounds.descent)
# define GREET_X_INC(w)	((w)->login.greetFont->max_bounds.width)
# define GREET_Y_INC(w)	((w)->login.greetFont->max_bounds.ascent +\
			 (w)->login.greetFont->max_bounds.descent)
# define FAIL_X_INC(w)	((w)->login.failFont->max_bounds.width)
# define FAIL_Y_INC(w)	((w)->login.failFont->max_bounds.ascent +\
			 (w)->login.failFont->max_bounds.descent)

# define Y_INC(w)	max (TEXT_Y_INC(w), PROMPT_Y_INC(w))

#ifndef XPM
# define LOGIN_PROMPT_W(w) (XTextWidth (w->login.promptFont,\
				 w->login.namePrompt,\
				 strlen (w->login.namePrompt)))
#else
# define LOGIN_PROMPT_W(w) (XTextWidth (w->login.promptFont,\
				 w->login.namePrompt,\
				 strlen (w->login.namePrompt)) + \
				 w->login.inframeswidth)
#endif /* XPM */
#ifndef XPM
# define PASS_PROMPT_W(w) (XTextWidth (w->login.promptFont,\
				 w->login.passwdPrompt,\
				 strlen (w->login.passwdPrompt)))
#else
# define PASS_PROMPT_W(w) (XTextWidth (w->login.promptFont,\
				 w->login.passwdPrompt,\
				 strlen (w->login.passwdPrompt)) + \
				 w->login.inframeswidth)
#endif /* XPM */
# define PROMPT_W(w)	(max(LOGIN_PROMPT_W(w), PASS_PROMPT_W(w)))
# define GREETING(w)	((w)->login.secure_session  && !(w)->login.allow_access ?\
				(w)->login.greeting : (w)->login.unsecure_greet)
# define GREET_X(w)	((int)(w->core.width - XTextWidth (w->login.greetFont,\
			  GREETING(w), strlen (GREETING(w)))) / 2)
# define GREET_Y(w)	(GREETING(w)[0] ? 2 * GREET_Y_INC (w) : 0)
#ifndef XPM
# define GREET_W(w)	(max (XTextWidth (w->login.greetFont,\
			      w->login.greeting, strlen (w->login.greeting)), \
			      XTextWidth (w->login.greetFont,\
			      w->login.unsecure_greet, strlen (w->login.unsecure_greet))))
#else
# define GREET_W(w)	(max (XTextWidth (w->login.greetFont,\
			      w->login.greeting, strlen (w->login.greeting)), \
			      XTextWidth (w->login.greetFont,\
			      w->login.unsecure_greet, strlen (w->login.unsecure_greet)))) + w->login.logoWidth + (2*w->login.logoPadding)
#endif /* XPM */
# define LOGIN_X(w)	(2 * PROMPT_X_INC(w))
# define LOGIN_Y(w)	(GREET_Y(w) + GREET_Y_INC(w) +\
			 w->login.greetFont->max_bounds.ascent + Y_INC(w))
# define LOGIN_W(w)	(w->core.width - 6 * TEXT_X_INC(w))
# define LOGIN_H(w)	(3 * Y_INC(w) / 2)
# define LOGIN_TEXT_X(w)(LOGIN_X(w) + PROMPT_W(w))
# define PASS_X(w)	(LOGIN_X(w))
#ifndef XPM
# define PASS_Y(w)	(LOGIN_Y(w) + 8 * Y_INC(w) / 5)
#else
# define PASS_Y(w)	(LOGIN_Y(w) + 10 * Y_INC(w) / 5)
#endif /* XPM */
# define PASS_W(w)	(LOGIN_W(w))
# define PASS_H(w)	(LOGIN_H(w))
# define PASS_TEXT_X(w)	(PASS_X(w) + PROMPT_W(w))
# define FAIL_X(w)	((int)(w->core.width - XTextWidth (w->login.failFont,\
				w->login.fail, strlen (w->login.fail))) / 2)
# define FAIL_Y(w)	(PASS_Y(w) + 2 * FAIL_Y_INC (w) +\
			w->login.failFont->max_bounds.ascent)
#ifndef XPM
# define FAIL_W(w)	(XTextWidth (w->login.failFont,\
			 w->login.fail, strlen (w->login.fail)))
#else
# define FAIL_W(w)	(XTextWidth (w->login.failFont,\
			 w->login.fail, strlen (w->login.fail))) + w->login.logoWidth + (2*w->login.logoPadding)
#endif /* XPM */

# define PAD_X(w)	(2 * (LOGIN_X(w) + max (GREET_X_INC(w), FAIL_X_INC(w))))

# define PAD_Y(w)	(max (max (Y_INC(w), GREET_Y_INC(w)),\
			     FAIL_Y_INC(w)))
	
#ifndef max
static int max (int a, int b) { return a > b ? a : b; }
#endif

static void
EraseName (LoginWidget w, int cursor)
{
    int	x;

    x = LOGIN_TEXT_X (w);
    if (cursor > 0)
	x += XTextWidth (w->login.font, w->login.data.name, cursor);
    XDrawString (XtDisplay(w), XtWindow (w), w->login.bgGC, x, LOGIN_Y(w),
		w->login.data.name + cursor, strlen (w->login.data.name + cursor));
}

static void
DrawName (LoginWidget w, int cursor)
{
    int	x;

    x = LOGIN_TEXT_X (w);
    if (cursor > 0)
	x += XTextWidth (w->login.font, w->login.data.name, cursor);
    XDrawString (XtDisplay(w), XtWindow (w), w->login.textGC, x, LOGIN_Y(w),
		w->login.data.name + cursor, strlen (w->login.data.name + cursor));

#ifdef XPM
	/*as good a place as any Caolan begin*/
	w->login.lastEventTime = time(NULL);
	/*as good a place as any Caolan end*/
#endif /* XPM */
}

static void
realizeCursor (LoginWidget w, GC gc)
{
    int	x, y;
    int height, width;

    switch (w->login.state) {
    case GET_NAME:
	x = LOGIN_TEXT_X (w);
	y = LOGIN_Y (w);
	height = w->login.font->max_bounds.ascent + w->login.font->max_bounds.descent;
	width = 1;
	if (w->login.cursor > 0)
	    x += XTextWidth (w->login.font, w->login.data.name, w->login.cursor);
	break;
    case GET_PASSWD:
	x = PASS_TEXT_X (w);
	y = PASS_Y (w);
	height = w->login.font->max_bounds.ascent + w->login.font->max_bounds.descent;
	width = 1;
	break;
    default:
	return;
    }
    XFillRectangle (XtDisplay (w), XtWindow (w), gc,
#ifndef XPM
		    x, y - w->login.font->max_bounds.ascent, width, height);
#else
		    x, y+1 - w->login.font->max_bounds.ascent, width, height-1);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x-1 , y - w->login.font->max_bounds.ascent);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x+1 , y - w->login.font->max_bounds.ascent);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x-1 , y - w->login.font->max_bounds.ascent+height);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x+1 , y - w->login.font->max_bounds.ascent+height);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x-2 , y - w->login.font->max_bounds.ascent);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x+2 , y - w->login.font->max_bounds.ascent);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x-2 , y - w->login.font->max_bounds.ascent+height);
    XDrawPoint     (XtDisplay (w), XtWindow (w), gc,
    		    x+2 , y - w->login.font->max_bounds.ascent+height);
#endif /* XPM */
}

static void
EraseFail (LoginWidget w)
{
    int x = FAIL_X(w);
    int y = FAIL_Y(w);

    XSetForeground (XtDisplay (w), w->login.failGC,
			w->core.background_pixel);
    XDrawString (XtDisplay (w), XtWindow (w), w->login.failGC,
		x, y,
		w->login.fail, strlen (w->login.fail));
    w->login.failUp = 0;
    XSetForeground (XtDisplay (w), w->login.failGC,
			w->login.failpixel);
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
    EraseFail (w);
}

void
DrawFail (Widget ctx)
{
    LoginWidget	w;

    w = (LoginWidget) ctx;
    XorCursor (w);
    ResetLogin (w);
    XorCursor (w);
    w->login.failUp = 1;
    RedrawFail (w);
    if (w->login.failTimeout > 0) {
	Debug ("failTimeout: %d\n", w->login.failTimeout);
	XtAppAddTimeOut(XtWidgetToApplicationContext ((Widget)w),
			w->login.failTimeout * 1000,
		        failTimeout, (XtPointer) w);
    }
}

static void
RedrawFail (LoginWidget w)
{
    int x = FAIL_X(w);
    int y = FAIL_Y(w);

    if (w->login.failUp)
        XDrawString (XtDisplay (w), XtWindow (w), w->login.failGC,
		    x, y,
		    w->login.fail, strlen (w->login.fail));
}

static void
draw_it (LoginWidget w)
{
#ifdef XPM
    int i,in_frame_x,in_login_y,in_pass_y,in_width,in_height;
    int gr_line_x, gr_line_y, gr_line_w;
#endif /* XPM */

    EraseCursor (w);

#ifdef XPM
    if( (w->login.outframewidth) < 1 )
      w->login.outframewidth = 1;
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
    gr_line_x = w->login.outframewidth + w->login.logoPadding;
    gr_line_y = GREET_Y(w) + GREET_Y_INC(w);
    gr_line_w = w->core.width - 2*(w->login.outframewidth) -
        (w->login.logoWidth + 3*(w->login.logoPadding));
 
    for(i=1;i<=(w->login.sepwidth);i++)
    {
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.shdGC,
        gr_line_x,           gr_line_y + i-1,
        gr_line_x+gr_line_w, gr_line_y + i-1);
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.hiGC,
        gr_line_x,           gr_line_y + 2*(w->login.inframeswidth) -i,
        gr_line_x+gr_line_w, gr_line_y + 2*(w->login.inframeswidth) -i);
    }
    
    in_frame_x = LOGIN_TEXT_X(w) - w->login.inframeswidth - 3;
    in_login_y = LOGIN_Y(w) - w->login.inframeswidth - 1 - TEXT_Y_INC(w);
    in_pass_y  = PASS_Y(w) - w->login.inframeswidth - 1 - TEXT_Y_INC(w);
 
    in_width = LOGIN_W(w) - PROMPT_W(w) -
        (w->login.logoWidth + 2*(w->login.logoPadding));
    in_height = LOGIN_H(w) + w->login.inframeswidth + 2;

    for(i=1;i<=(w->login.inframeswidth);i++)
    {
      /* Make top/left sides */
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.shdGC,
	in_frame_x + i-1,             in_login_y + i-1,
	in_frame_x + in_width-i,      in_login_y + i-1); 

      XDrawLine(XtDisplay (w), XtWindow (w), w->login.shdGC,
	in_frame_x + i-1,             in_login_y + i-1,
	in_frame_x + i-1,             in_login_y + in_height-i); 

      XDrawLine(XtDisplay (w), XtWindow (w), w->login.hiGC,
        in_frame_x + in_width-i,      in_login_y + i-1,
        in_frame_x + in_width-i,      in_login_y + in_height-i); 
                
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.hiGC,
	in_frame_x + i-1,             in_login_y + in_height-i,
	in_frame_x + in_width-i,      in_login_y + in_height-i);

      /* Make bottom/right sides */
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.shdGC,
	in_frame_x + i-1,             in_pass_y + i-1,
	in_frame_x + in_width-i,      in_pass_y + i-1); 

      XDrawLine(XtDisplay (w), XtWindow (w), w->login.shdGC,
	in_frame_x + i-1,             in_pass_y + i-1,
	in_frame_x + i-1,             in_pass_y + in_height-i); 

      XDrawLine(XtDisplay (w), XtWindow (w), w->login.hiGC,
        in_frame_x + in_width-i,      in_pass_y + i-1,
        in_frame_x + in_width-i,      in_pass_y + in_height-i); 
                
      XDrawLine(XtDisplay (w), XtWindow (w), w->login.hiGC,
	in_frame_x + i-1,             in_pass_y + in_height-i,
	in_frame_x + in_width-i,      in_pass_y + in_height-i);
    }
#endif /* XPM */

    if (GREETING(w)[0])
	    XDrawString (XtDisplay (w), XtWindow (w), w->login.greetGC,
#ifndef XPM
			GREET_X(w), GREET_Y(w),
#else
			GREET_X(w) -
                            ((w->login.logoWidth/2) + w->login.logoPadding),
                        GREET_Y(w),
#endif /* XPM */
			GREETING(w), strlen (GREETING(w)));
    XDrawString (XtDisplay (w), XtWindow (w), w->login.promptGC,
		LOGIN_X(w), LOGIN_Y(w),
		w->login.namePrompt, strlen (w->login.namePrompt));
    XDrawString (XtDisplay (w), XtWindow (w), w->login.promptGC,
		PASS_X(w), PASS_Y(w),
		w->login.passwdPrompt, strlen (w->login.passwdPrompt));
    RedrawFail (w);
    DrawName (w, 0);
    XorCursor (w);
    /*
     * The GrabKeyboard here is needed only because of
     * a bug in the R3 server -- the keyboard is grabbed on
     * the root window, and the server won't dispatch events
     * to the focus window unless the focus window is a ancestor
     * of the grab window.  Bug in server already found and fixed,
     * compatibility until at least R4.
     */
    if (XGrabKeyboard (XtDisplay (w), XtWindow (w), False, GrabModeAsync,
		       GrabModeAsync, CurrentTime) != GrabSuccess)
    {
	XSetInputFocus (XtDisplay (w), XtWindow (w),
			RevertToPointerRoot, CurrentTime);
    }
}

/*ARGSUSED*/
static void
DeleteBackwardChar (Widget ctxw, XEvent *event, String *params, Cardinal *num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    XorCursor (ctx);
    RemoveFail (ctx);
    if (ctx->login.cursor > 0) {
	ctx->login.cursor--;
	switch (ctx->login.state) {
	case GET_NAME:
	    EraseName (ctx, ctx->login.cursor);
	    strcpy (ctx->login.data.name + ctx->login.cursor,
		    ctx->login.data.name + ctx->login.cursor + 1);
	    DrawName (ctx, ctx->login.cursor);
	    break;
	case GET_PASSWD:
	    strcpy (ctx->login.data.passwd + ctx->login.cursor,
		    ctx->login.data.passwd + ctx->login.cursor + 1);
	    break;
	}
    }
    XorCursor (ctx);	
}

/*ARGSUSED*/
static void
DeleteForwardChar (Widget ctxw, XEvent *event, String *params, Cardinal *num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    XorCursor (ctx);
    RemoveFail (ctx);
    switch (ctx->login.state) {
    case GET_NAME:
	if (ctx->login.cursor < (int)strlen (ctx->login.data.name)) {
	    EraseName (ctx, ctx->login.cursor);
	    strcpy (ctx->login.data.name + ctx->login.cursor,
		    ctx->login.data.name + ctx->login.cursor + 1);
	    DrawName (ctx, ctx->login.cursor);
	}
	break;
    case GET_PASSWD:
    	if (ctx->login.cursor < (int)strlen (ctx->login.data.passwd)) {
	    strcpy (ctx->login.data.passwd + ctx->login.cursor,
		    ctx->login.data.passwd + ctx->login.cursor + 1);
	}
	break;
    }
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

    XorCursor (ctx);
    RemoveFail (ctx);
    if (ctx->login.cursor > 0)
    	ctx->login.cursor--;
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

    XorCursor (ctx);
    RemoveFail (ctx);
    switch (ctx->login.state) {
    case GET_NAME:
    	if (ctx->login.cursor < (int)strlen(ctx->login.data.name))
	    ++ctx->login.cursor;
	break;
    case GET_PASSWD:
    	if (ctx->login.cursor < (int)strlen(ctx->login.data.passwd))
	    ++ctx->login.cursor;
	break;
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

    XorCursor (ctx);
    RemoveFail (ctx);
    ctx->login.cursor = 0;
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

    XorCursor (ctx);
    RemoveFail (ctx);
    switch (ctx->login.state) {
    case GET_NAME:
    	ctx->login.cursor = strlen (ctx->login.data.name);
	break;
    case GET_PASSWD:
    	ctx->login.cursor = strlen (ctx->login.data.passwd);
	break;
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

    XorCursor (ctx);
    RemoveFail (ctx);
    switch (ctx->login.state) {
    case GET_NAME:
	EraseName (ctx, ctx->login.cursor);
	ctx->login.data.name[ctx->login.cursor] = '\0';
	break;
    case GET_PASSWD:
	ctx->login.data.passwd[ctx->login.cursor] = '\0';
	break;
    }
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

    XorCursor (ctx);
    RemoveFail (ctx);
    switch (ctx->login.state) {
    case GET_NAME:
	ctx->login.state = GET_PASSWD;
	ctx->login.cursor = 0;
	break;
    case GET_PASSWD:
	ctx->login.state = DONE;
	ctx->login.cursor = 0;
	(*ctx->login.notify_done) (ctx, &ctx->login.data, NOTIFY_OK);
	break;
    }
    XorCursor (ctx);
}

#ifdef XPM
/*ARGSUSED*/
static void
TabField(Widget ctxw, XEvent *event, String *params, Cardinal *num_params)
{
    LoginWidget ctx = (LoginWidget)ctxw;

    XorCursor (ctx);
    RemoveFail (ctx);
    switch (ctx->login.state) {
    case GET_NAME:
	ctx->login.state = GET_PASSWD;
	ctx->login.cursor = 0;
	break;
    case GET_PASSWD:
	ctx->login.state = GET_NAME;
	ctx->login.cursor = 0;
	break;
    }
    XorCursor (ctx);
}
#endif /* XPM */

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
    ctx->login.sessionArg = 0;
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
    ctx->login.cursor = 0;
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
    ctx->login.cursor = 0;
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
    ctx->login.cursor = 0;
    (*ctx->login.notify_done) (ctx, &ctx->login.data, NOTIFY_ABORT_DISPLAY);
    XorCursor (ctx);
}

static void
ResetLogin (LoginWidget w)
{
    EraseName (w, 0);
    w->login.cursor = 0;
    w->login.data.name[0] = '\0';
    w->login.data.passwd[0] = '\0';
    w->login.state = GET_NAME;
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
#ifndef XPM
    int  len;
#else
    int  len,pixels;
#endif /* XPM */
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

#ifdef XPM
    pixels = 3 + ctx->login.font->max_bounds.width * len +
    	     XTextWidth(ctx->login.font,
    	     		ctx->login.data.name,
    	     		strlen(ctx->login.data.name));
    	     	/* pixels to be added */
#endif /* XPM */

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

    switch (ctx->login.state) {
    case GET_NAME:
#ifndef XPM
	if (len + (int)strlen(ctx->login.data.name) >= NAME_LEN - 1)
#else
	if (
	        (len + (int)strlen(ctx->login.data.name) >= NAME_LEN - 1)/* &&
		(pixels <= LOGIN_W(ctx) - PROMPT_W(ctx))*/
	   )
#endif /* XPM */
	    len = NAME_LEN - strlen(ctx->login.data.name) - 2;
    case GET_PASSWD:
	if (len + (int)strlen(ctx->login.data.passwd) >= PASSWORD_LEN - 1)
	    len = PASSWORD_LEN - strlen(ctx->login.data.passwd) - 2;
    }
#ifndef XPM
    if (len == 0)
#else
    if (len == 0 || pixels >= LOGIN_W(ctx) - PROMPT_W(ctx))
#endif /* XPM */
	return;
    XorCursor (ctx);
    RemoveFail (ctx);
    switch (ctx->login.state) {
    case GET_NAME:
	EraseName (ctx, ctx->login.cursor);
	memmove( ctx->login.data.name + ctx->login.cursor + len,
	       ctx->login.data.name + ctx->login.cursor,
	       strlen (ctx->login.data.name + ctx->login.cursor) + 1);
	memmove( ctx->login.data.name + ctx->login.cursor, strbuf, len);
	DrawName (ctx, ctx->login.cursor);
	ctx->login.cursor += len;
	break;
    case GET_PASSWD:
	memmove( ctx->login.data.passwd + ctx->login.cursor + len,
	       ctx->login.data.passwd + ctx->login.cursor,
	       strlen (ctx->login.data.passwd + ctx->login.cursor) + 1);
	memmove( ctx->login.data.passwd + ctx->login.cursor, strbuf, len);
	ctx->login.cursor += len;

#ifdef XPM
	/*as good a place as any Caolan begin*/
	ctx->login.lastEventTime = time(NULL);
	/*as good a place as any Caolan end*/
#endif /* XPM */
	break;
    }
    XorCursor (ctx);
}

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

#ifdef XPM
    myXGCV.foreground = w->login.hipixel;
    myXGCV.background = w->core.background_pixel;
    valuemask = GCForeground | GCBackground;
    w->login.hiGC = XtGetGC(gnew, valuemask, &myXGCV);

    myXGCV.foreground = w->login.shdpixel;
    myXGCV.background = w->core.background_pixel;
    valuemask = GCForeground | GCBackground;
    w->login.shdGC = XtGetGC(gnew, valuemask, &myXGCV);
#endif /* XPM */

    myXGCV.foreground = w->login.textpixel;
    myXGCV.background = w->core.background_pixel;
    valuemask = GCForeground | GCBackground;
    if (w->login.font) {
	myXGCV.font = w->login.font->fid;
	valuemask |= GCFont;
    }
    w->login.textGC = XtGetGC(gnew, valuemask, &myXGCV);
    myXGCV.foreground = w->core.background_pixel;
    w->login.bgGC = XtGetGC(gnew, valuemask, &myXGCV);

    myXGCV.foreground = w->login.textpixel ^ w->core.background_pixel;
    myXGCV.function = GXxor;
    xvaluemask = valuemask | GCFunction;
    w->login.xorGC = XtGetGC (gnew, xvaluemask, &myXGCV);

    /*
     * Note that the second argument is a GCid -- QueryFont accepts a GCid and
     * returns the curently contained font.
     */

    if (w->login.font == NULL)
	w->login.font = XQueryFont (XtDisplay (w),
		XGContextFromGC (XDefaultGCOfScreen (XtScreen (w))));

    xvaluemask = valuemask;
    if (w->login.promptFont == NULL)
        w->login.promptFont = w->login.font;
    else
	xvaluemask |= GCFont;

    myXGCV.foreground = w->login.promptpixel;
    myXGCV.font = w->login.promptFont->fid;
    w->login.promptGC = XtGetGC (gnew, xvaluemask, &myXGCV);

    xvaluemask = valuemask;
    if (w->login.greetFont == NULL)
    	w->login.greetFont = w->login.font;
    else
	xvaluemask |= GCFont;

    myXGCV.foreground = w->login.greetpixel;
    myXGCV.font = w->login.greetFont->fid;
    w->login.greetGC = XtGetGC (gnew, xvaluemask, &myXGCV);

    xvaluemask = valuemask;
    if (w->login.failFont == NULL)
	w->login.failFont = w->login.font;
    else
	xvaluemask |= GCFont;

    myXGCV.foreground = w->login.failpixel;
    myXGCV.font = w->login.failFont->fid;
    w->login.failGC = XtGetGC (gnew, xvaluemask, &myXGCV);

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

        XpmReadFileToPixmap(XtDisplay(w),            /* display */
            RootWindowOfScreen(XtScreen(w)),         /* window */
            w->login.logoFileName,                   /* XPM filename */
            &(w->login.logoPixmap),                  /* pixmap */
            &(w->login.logoMask),                    /* pixmap mask */
            &myAttributes);                          /* XPM attributes */
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
    w->login.state = GET_NAME;
    w->login.cursor = 0;
    w->login.failUp = 0;
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
}

 
static void Realize (
     Widget gw,
     XtValueMask *valueMask,
     XSetWindowAttributes *attrs)
{
#ifdef XPM
    LoginWidget	w = (LoginWidget) gw;
    Cursor cursor;
#endif /* XPM */
    XtCreateWindow( gw, (unsigned)InputOutput, (Visual *)CopyFromParent,
		     *valueMask, attrs );
    InitI18N(gw);
#ifdef XPM
    cursor = XCreateFontCursor(XtDisplay(gw), XC_left_ptr);
    XDefineCursor(XtDisplay(gw), XtWindow(gw), cursor);

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
    XtReleaseGC(gw, w->login.textGC);
    XtReleaseGC(gw, w->login.bgGC);
    XtReleaseGC(gw, w->login.xorGC);
    XtReleaseGC(gw, w->login.promptGC);
    XtReleaseGC(gw, w->login.greetGC);
    XtReleaseGC(gw, w->login.failGC);
#ifdef XPM
    XtReleaseGC(gw, w->login.hiGC);
    XtReleaseGC(gw, w->login.shdGC);

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
#ifndef XPM
"<KeyPress>:	insert-char()"
#else
"<Key>Tab:	tab-field() \n"
"<KeyPress>:	insert-char()"
#endif /* XPM */
;

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
#ifdef XPM
  {"tab-field", 		TabField},
#endif /* XPM */
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
    /* class_initialize		*/	NULL,
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
#ifndef XPM
    /* set_values_almost	*/	NULL,
#else
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
#endif /* XPM */
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
