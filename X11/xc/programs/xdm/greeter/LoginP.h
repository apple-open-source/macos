/* $Xorg: LoginP.h,v 1.4 2001/02/09 02:05:41 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xdm/greeter/LoginP.h,v 3.8 2001/12/14 20:01:29 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 */

#ifndef _LoginP_h
#define _LoginP_h

#include "Login.h"
#include <X11/CoreP.h>
#ifdef XPM
#include <X11/Xlib.h>
#endif /* XPM */

#define GET_NAME	0
#define GET_PASSWD	1
#define DONE		2

typedef void (*LoginFunc)(LoginWidget, LoginData *, int);

/* New fields for the login widget instance record */
typedef struct {
	Pixel		textpixel;	/* foreground pixel */
	Pixel		promptpixel;	/* prompt pixel */
	Pixel		greetpixel;	/* greeting pixel */
	Pixel		failpixel;	/* failure pixel */
#ifdef XPM
	Pixel		hipixel;	/* frame hilite pixel */
	Pixel		shdpixel;	/* shadow frame pixel */
#endif /* XPM */
	GC		textGC;		/* pointer to GraphicsContext */
	GC		bgGC;		/* pointer to GraphicsContext */
	GC		xorGC;		/* pointer to GraphicsContext */
	GC		promptGC;
	GC		greetGC;
	GC		failGC;
#ifdef XPM
	GC		hiGC;		/* for hilight part of frame */
	GC		shdGC;		/* for shaded part of frame */
#endif /* XPM */
	char		*greeting;	/* greeting */
	char		*unsecure_greet;/* message displayed when insecure */
	char		*namePrompt;	/* name prompt */
	char		*passwdPrompt;	/* password prompt */
	char		*fail;		/* failure message */
	XFontStruct	*font;		/* font for text */
	XFontStruct	*promptFont;	/* font for prompts */
	XFontStruct	*greetFont;	/* font for greeting */
	XFontStruct	*failFont;	/* font for failure message */
	int		state;		/* state */
	int		cursor;		/* current cursor position */
	int		failUp;		/* failure message displayed */
	LoginData	data;		/* name/passwd */
	char		*sessionArg;	/* argument passed to session */
	LoginFunc	notify_done;	/* proc to call when done */
	int		failTimeout;	/* seconds til drop fail msg */
	XtIntervalId	interval_id;	/* drop fail message note */
	Boolean		secure_session;	/* session is secured */
	Boolean		allow_access;	/* disable access control on login */
	Boolean		allow_null_passwd; /* allow null password on login */
	Boolean		allow_root_login; /* allow root login */
	XIC		xic;		/* input method of input context */
#ifdef XPM
	/*caolan begin*/
	int lastEventTime;
	/*caolan end*/
	int		outframewidth;	/* outer frame thickness */
	int		inframeswidth;	/* inner frames thickness */
	int		sepwidth;	/* width of separator line */

        char *logoFileName;
        unsigned int logoWidth, logoHeight, logoPadding, logoBorderWidth;
        int logoX, logoY;
        Window logoWindow;
        Boolean useShape, logoValid;
        Pixmap logoPixmap, logoMask;
#endif /* XPM */
   } LoginPart;

/* Full instance record declaration */
typedef struct _LoginRec {
   CorePart core;
   LoginPart login;
   } LoginRec;

/* New fields for the Login widget class record */
typedef struct {int dummy;} LoginClassPart;

/* Full class record declaration. */
typedef struct _LoginClassRec {
   CoreClassPart core_class;
   LoginClassPart login_class;
   } LoginClassRec;

/* Class pointer. */
extern LoginClassRec loginClassRec;

#endif /* _LoginP_h */
