/* $XdotOrg: $ */
/* $Xorg: Login.h,v 1.4 2001/02/09 02:05:41 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xdm/greeter/Login.h,v 3.7 2002/10/06 20:42:16 herrb Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 */


#ifndef _XtLogin_h
#define _XtLogin_h

/***********************************************************************
 *
 * Login Widget
 *
 ***********************************************************************/

/* Parameters:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 background	     Background		pixel		White
 border		     BorderColor	pixel		Black
 borderWidth	     BorderWidth	int		1
 foreground	     Foreground		Pixel		Black
 height		     Height		int		120
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 width		     Width		int		120
 x		     Position		int		0
 y		     Position		int		0

*/

# define XtNgreeting		"greeting"
# define XtNunsecureGreeting	"unsecureGreeting"
# define XtNnamePrompt		"namePrompt"
# define XtNpasswdPrompt	"passwdPrompt"
# define XtNfail		"fail"
# define XtNnotifyDone		"notifyDone"
# define XtNpromptColor		"promptColor"
# define XtNgreetColor		"greetColor"
# define XtNfailColor		"failColor"
# define XtNpromptFont		"promptFont"
# define XtNgreetFont		"greetFont"
# define XtNfailFont		"failFont"
# define XtNpromptFace		"promptFace"
# define XtNgreetFace		"greetFace"
# define XtNfailFace		"failFace"
# define XtNfailTimeout		"failTimeout"
# define XtNsessionArgument	"sessionArgument"
# define XtNsecureSession	"secureSession"
# define XtNallowAccess		"allowAccess"
# define XtNallowNullPasswd	"allowNullPasswd"
# define XtNallowRootLogin	"allowRootLogin"

# define XtNface 		"face"
# define XtCFace		"Face"
# define XtRXftFont		"XftFont"
# define XtRXftColor		"XftColor"

/* added by Amit Margalit Oct 1996 */
# define XtNhiColor		"hiColor"
# define XtNshdColor		"shdColor"
# define XtNframeWidth		"frameWidth"
# define XtCFrameWidth		"FrameWidth"
# define XtNinnerFramesWidth	"innerFramesWidth"
# define XtNsepWidth		"sepWidth"

#ifdef DANCING
/* caolan begin */
#define XtNlastEventTime "lastEventTime"
#define XtCLastEventTime "LastEventTime"
/* caolan end */
#endif /* DANCING */

#ifdef XPM
#define XtNuseShape "useShape"
#define XtCUseShape "UseShape"
#define XtNlogoFileName "logoFileName"
#define XtCLogoFileName "LogoFileName"
#define XtNlogoPadding "logoPadding"
#define XtCLogoPadding "LogoPadding"
#endif /* XPM */

# define XtCGreeting		"Greeting"
# define XtCNamePrompt		"NamePrompt"
# define XtCPasswdPrompt	"PasswdPrompt"
# define XtCFail		"Fail"
# define XtCFailTimeout		"FailTimeout"
# define XtCSessionArgument	"SessionArgument"
# define XtCSecureSession	"SecureSession"
# define XtCAllowAccess		"AllowAccess"
# define XtCAllowNullPasswd	"AllowNullPasswd"
# define XtCAllowRootLogin	"AllowRootLogin"

# define XtNchangePasswdMessage	"changePasswdMessage"
# define XtCChangePasswdMessage	"ChangePasswdMessage"

/* notifyDone interface definition */

#ifdef __OpenBSD__
# include <sys/param.h>
#endif

#include <pwd.h>
#include <limits.h>

#ifdef USE_PAM
# define NAME_LEN	PAM_MAX_RESP_SIZE
# define PASSWORD_LEN	PAM_MAX_RESP_SIZE
#endif

/* Defined to be in <limits.h> by SUSv2 */
#if !defined(PASSWORD_LEN) && defined(PASS_MAX)
# define PASSWORD_LEN PASS_MAX
#endif

/* _PW_NAME_LEN is found in <pwd.h> on OpenBSD > 2.8 (200012) */
#if !defined(NAME_LEN) && defined(_PW_NAME_LEN)
# define NAME_LEN	(_PW_NAME_LEN + 2)
#endif

/* _PASSWORD_LEN appears to come from 4.4BSD-Lite <pwd.h> */
#if !defined(PASSWORD_LEN) && defined(_PASSWORD_LEN)
# define PASSWORD_LEN	(_PASSWORD_LEN + 2)
#endif

/* Fallbacks if no other definition found */
#ifndef NAME_LEN
# define NAME_LEN	32
#endif

#ifndef PASSWORD_LEN
# define PASSWORD_LEN	32
#endif

typedef struct _LoginData { 
	char	name[NAME_LEN], passwd[PASSWORD_LEN];
} LoginData;

# define NOTIFY_OK	0
# define NOTIFY_ABORT	1
# define NOTIFY_RESTART	2
# define NOTIFY_ABORT_DISPLAY	3

typedef struct _LoginRec *LoginWidget;  /* completely defined in LoginPrivate.h */
typedef struct _LoginClassRec *LoginWidgetClass;    /* completely defined in LoginPrivate.h */

extern WidgetClass loginWidgetClass;

extern void ErrorMessage(Widget ctx, const char *message, Bool timeout);
extern void ShowChangePasswdMessage(Widget ctx);

typedef enum {
    LOGIN_PROMPT_NOT_SHOWN,	/* Neither prompt nor input shown */
    LOGIN_PROMPT_ECHO_ON,	/* Both prompt and input shown */
    LOGIN_PROMPT_ECHO_OFF,	/* Prompt shown, input accepted but not
				   shown (bullets may be shown instead) */
    LOGIN_TEXT_INFO		/* Prompt shown, no input area */
} loginPromptState;

/* Default prompt meanings for simple username/password auth systems */
#define LOGIN_PROMPT_USERNAME 0
#define LOGIN_PROMPT_PASSWORD 1

extern int SetPrompt(Widget ctx, int promptId, const char *message,
		     loginPromptState state, Boolean minimumTime);
extern const char *GetPrompt(Widget ctx, int promptId);

extern int SetValue(Widget ctx, int promptId, char *value);
extern const char *GetValue(Widget ctx, int promptId);

#endif /* _XtLogin_h */
/* DON'T ADD STUFF AFTER THIS #endif */
