/* $XdotOrg: $ */
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
/* Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder
 * shall not be used in advertising or otherwise to promote the sale, use
 * or other dealings in this Software without prior written authorization
 * of the copyright holder.
 */
/* $XFree86: xc/programs/xdm/greeter/LoginP.h,v 3.8 2001/12/14 20:01:29 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 */

#ifndef _LoginP_h
#define _LoginP_h

#include "Login.h"
#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/Xlib.h>
#ifdef USE_XFT
# include <X11/Xft/Xft.h>
#endif

#define INITIALIZING	0
#define PROMPTING	1
#define SHOW_MESSAGE	2
#define DONE		3

typedef void (*LoginFunc)(LoginWidget, LoginData *, int);

typedef struct {
    	char *		promptText;	/* Prompt displayed */
    	const char *	defaultPrompt;	/* Default text for prompt */
    	char *		valueText;	/* Value entered for prompt */
   	size_t		valueTextMax;	/* Size of valueText buffer */
    	int		valueShownStart;/* Amount of string shown if too */
    	int		valueShownEnd;	/*  long to fit in field */
	int		cursor;		/* current cursor position */	
	loginPromptState state;
} loginPromptData;

#define NUM_PROMPTS	2	/* Currently only 2 prompt fields supported */
#define LAST_PROMPT	(NUM_PROMPTS - 1)

/* New fields for the login widget instance record */
typedef struct {
#ifndef USE_XFT    
	Pixel		textpixel;	/* foreground pixel */
	Pixel		promptpixel;	/* prompt pixel */
	Pixel		greetpixel;	/* greeting pixel */
	Pixel		failpixel;	/* failure pixel */
#endif
	Pixel		hipixel;	/* frame hilite pixel */
	Pixel		shdpixel;	/* shadow frame pixel */
	GC		textGC;		/* pointer to GraphicsContext */
	GC		bgGC;		/* pointer to GraphicsContext */
	GC		xorGC;		/* pointer to GraphicsContext */
#ifndef USE_XFT
	GC		promptGC;
	GC		greetGC;
	GC		failGC;
#endif
	GC		hiGC;		/* for hilight part of frame */
	GC		shdGC;		/* for shaded part of frame */
	char		*greeting;	/* greeting */
	char		*unsecure_greet;/* message displayed when insecure */
	char		*namePrompt;	/* name prompt */
	char		*passwdPrompt;	/* password prompt */
    	char		*failMsg;	/* failure message */
	char		*fail;		/* current error message */
    	char		*passwdChangeMsg; /* message when passwd expires */
#ifndef USE_XFT    
	XFontStruct	*textFont;	/* font for text */
	XFontStruct	*promptFont;	/* font for prompts */
	XFontStruct	*greetFont;	/* font for greeting */
	XFontStruct	*failFont;	/* font for failure message */
#endif /* USE_XFT */
	int		state;		/* state */
    	int		activePrompt;	/* which prompt is active */
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
	loginPromptData	prompts[NUM_PROMPTS];
    	time_t 		msgTimeout;

#ifdef DANCING
	/*caolan begin*/
	int 		lastEventTime;
	/*caolan end*/
#endif /* DANCING */
    
	int		outframewidth;	/* outer frame thickness */
	int		inframeswidth;	/* inner frames thickness */
	int		sepwidth;	/* width of separator line */

#ifdef XPM    
        char *logoFileName;
        unsigned int logoWidth, logoHeight, logoPadding, logoBorderWidth;
        int logoX, logoY;
        Window logoWindow;
        Boolean useShape, logoValid;
        Pixmap logoPixmap, logoMask;
#endif /* XPM */
#ifdef USE_XFT
	XftDraw	       *draw;
	XftFont        *textFace;	/* font for text */
	XftFont        *promptFace;	/* font for prompts */
	XftFont        *greetFace;	/* font for greeting */
	XftFont        *failFace;  	/* font for failure message */  
	XftColor	textcolor;	/* foreground color */
	XftColor	promptcolor;	/* prompt color */
	XftColor	greetcolor;	/* greeting color */
	XftColor	failcolor;	/* failure color */
#endif
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
