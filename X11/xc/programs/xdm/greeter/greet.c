/* $Xorg: greet.c,v 1.4 2001/02/09 02:05:41 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xdm/greeter/greet.c,v 3.16 2002/10/06 20:42:16 herrb Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, X Consortium
 *
 * widget to get username/password
 *
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/XKBlib.h>

#ifdef USE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "dm.h"
#include "dm_error.h"
#include "greet.h"
#include "Login.h"

#ifdef __OpenBSD__
#include <syslog.h>
#endif

#if defined(SECURE_RPC) && defined(sun)
/* Go figure, there's no getdomainname() prototype available */
extern int getdomainname(char *name, size_t len);
#endif

#ifdef GREET_LIB
/*
 * Function pointers filled in by the initial call ito the library
 */

int     (*__xdm_PingServer)(struct display *d, Display *alternateDpy) = NULL;
void    (*__xdm_SessionPingFailed)(struct display *d) = NULL;
void    (*__xdm_Debug)(char * fmt, ...) = NULL;
void    (*__xdm_RegisterCloseOnFork)(int fd) = NULL;
void    (*__xdm_SecureDisplay)(struct display *d, Display *dpy) = NULL;
void    (*__xdm_UnsecureDisplay)(struct display *d, Display *dpy) = NULL;
void    (*__xdm_ClearCloseOnFork)(int fd) = NULL;
void    (*__xdm_SetupDisplay)(struct display *d) = NULL;
void    (*__xdm_LogError)(char * fmt, ...) = NULL;
void    (*__xdm_SessionExit)(struct display *d, int status, int removeAuth) = NULL;
void    (*__xdm_DeleteXloginResources)(struct display *d, Display *dpy) = NULL;
int     (*__xdm_source)(char **environ, char *file) = NULL;
char    **(*__xdm_defaultEnv)(void) = NULL;
char    **(*__xdm_setEnv)(char **e, char *name, char *value) = NULL;
char    **(*__xdm_putEnv)(const char *string, char **env) = NULL;
char    **(*__xdm_parseArgs)(char **argv, char *string) = NULL;
void    (*__xdm_printEnv)(char **e) = NULL;
char    **(*__xdm_systemEnv)(struct display *d, char *user, char *home) = NULL;
void    (*__xdm_LogOutOfMem)(char * fmt, ...) = NULL;
void    (*__xdm_setgrent)(void) = NULL;
struct group    *(*__xdm_getgrent)(void) = NULL;
void    (*__xdm_endgrent)(void) = NULL;
#ifdef USESHADOW
struct spwd   *(*__xdm_getspnam)(GETSPNAM_ARGS) = NULL;
void   (*__xdm_endspent)(void) = NULL;
#endif
struct passwd   *(*__xdm_getpwnam)(GETPWNAM_ARGS) = NULL;
#ifdef linux
void   (*__xdm_endpwent)(void) = NULL;
#endif
char     *(*__xdm_crypt)(CRYPT_ARGS) = NULL;
#ifdef USE_PAM
pam_handle_t **(*__xdm_thepamhp)(void) = NULL;
#endif

#endif

#ifdef SECURE_RPC
#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#endif

#ifdef K5AUTH
#include <krb5/krb5.h>
#endif

extern Display	*dpy;

static int	done, code;
static char	name[128], password[128];
static Widget		toplevel;
static Widget		login;
static XtAppContext	context;
static XtIntervalId	pingTimeout;

/*ARGSUSED*/
static void
GreetPingServer (
    XtPointer	    closure,
    XtIntervalId    *intervalId)
{
    struct display *d;

    d = (struct display *) closure;
    if (!PingServer (d, XtDisplay (toplevel)))
	SessionPingFailed (d);
    pingTimeout = XtAppAddTimeOut (context, d->pingInterval * 60 * 1000,
				   GreetPingServer, (closure));
}

/*ARGSUSED*/
static void
GreetDone (
    Widget	w,
    LoginData	*data,
    int		status)
{
    Debug ("GreetDone: %s, (password is %d long)\n",
	    data->name, strlen (data->passwd));
    switch (status) {
    case NOTIFY_OK:
	strcpy (name, data->name);
	strcpy (password, data->passwd);
	bzero (data->passwd, PASSWORD_LEN);
	code = 0;
	done = 1;
	break;
    case NOTIFY_ABORT:
	Debug ("RESERVER_DISPLAY\n");
	code = RESERVER_DISPLAY;
	done = 1;
	break;
    case NOTIFY_RESTART:
	Debug ("REMANAGE_DISPLAY\n");
	code = REMANAGE_DISPLAY;
	done = 1;
	break;
    case NOTIFY_ABORT_DISPLAY:
	Debug ("UNMANAGE_DISPLAY\n");
	code = UNMANAGE_DISPLAY;
	done = 1;
	break;
    }
}

static Display *
InitGreet (struct display *d)
{
    Arg		arglist[10];
    int		i;
    static int	argc;
    Screen		*scrn;
    static char	*argv[] = { "xlogin", 0 };
    Display		*dpy;
#ifdef USE_XINERAMA
    XineramaScreenInfo *screens;
    int                 s_num;
#endif

    Debug ("greet %s\n", d->name);
    argc = 1;
    XtToolkitInitialize ();
    context = XtCreateApplicationContext();
    dpy = XtOpenDisplay (context, d->name, "xlogin", "Xlogin", 0,0,
			 &argc, argv);

    if (!dpy)
	return 0;

#ifdef XKB
    {
    int opcode, evbase, errbase, majret, minret;
    unsigned int value = XkbPCF_GrabsUseXKBStateMask;
    if (XkbQueryExtension (dpy, &opcode, &evbase, &errbase, &majret, &minret)) {
	if (!XkbSetPerClientControls (dpy, XkbPCF_GrabsUseXKBStateMask, &value))
	    LogError ("%s\n", "SetPerClientControls failed");
    }
    }
#endif
    RegisterCloseOnFork (ConnectionNumber (dpy));

    SecureDisplay (d, dpy);

    i = 0;
    scrn = XDefaultScreenOfDisplay(dpy);
    XtSetArg(arglist[i], XtNscreen, scrn);	i++;
    XtSetArg(arglist[i], XtNargc, argc);	i++;
    XtSetArg(arglist[i], XtNargv, argv);	i++;

    toplevel = XtAppCreateShell ((String) NULL, "Xlogin",
		    applicationShellWidgetClass, dpy, arglist, i);

    i = 0;
    XtSetArg (arglist[i], XtNnotifyDone, (XtPointer)GreetDone); i++;
    if (!d->authorize || d->authorizations || !d->authComplain)
    {
	XtSetArg (arglist[i], XtNsecureSession, True); i++;
    }
    login = XtCreateManagedWidget ("login", loginWidgetClass, toplevel,
				    arglist, i);
    XtRealizeWidget (toplevel);

#ifdef USE_XINERAMA
    if (
	XineramaIsActive(dpy) &&
	(screens = XineramaQueryScreens(dpy, &s_num)) != NULL
       )
    {
	XWarpPointer(dpy, None, XRootWindowOfScreen (scrn),
			0, 0, 0, 0,
			screens[0].x_org + screens[0].width / 2,
			screens[0].y_org + screens[0].height / 2);

	XFree(screens);
    }
    else
#endif
    XWarpPointer(dpy, None, XRootWindowOfScreen (scrn),
		    0, 0, 0, 0,
		    XWidthOfScreen(scrn) / 2,
		    XHeightOfScreen(scrn) / 2);

    if (d->pingInterval)
    {
    	pingTimeout = XtAppAddTimeOut (context, d->pingInterval * 60 * 1000,
				       GreetPingServer, (XtPointer) d);
    }
    return dpy;
}

static void
CloseGreet (struct display *d)
{
    Boolean	    allow;
    Arg	    arglist[1];
    Display *dpy = XtDisplay(toplevel);

    if (pingTimeout)
    {
	XtRemoveTimeOut (pingTimeout);
	pingTimeout = 0;
    }
    UnsecureDisplay (d, dpy);
    XtSetArg (arglist[0], XtNallowAccess, (char *) &allow);
    XtGetValues (login, arglist, 1);
    if (allow)
    {
	Debug ("Disabling access control\n");
	XSetAccessControl (dpy, DisableAccess);
    }
    XtDestroyWidget (toplevel);
    ClearCloseOnFork (XConnectionNumber (dpy));
    XCloseDisplay (dpy);
    Debug ("Greet connection closed\n");
}

static int
Greet (struct display *d, struct greet_info *greet)
{
    XEvent		event;
    Arg		arglist[3];

    XtSetArg (arglist[0], XtNallowAccess, False);
    XtSetValues (login, arglist, 1);

    Debug ("dispatching %s\n", d->name);
    done = 0;
    while (!done) {
	XtAppNextEvent (context, &event);
	switch (event.type) {
	case MappingNotify:
	    XRefreshKeyboardMapping(&event.xmapping);
	    break;
	default:
	    XtDispatchEvent (&event);
	    break;
	}
    }
    XFlush (XtDisplay (toplevel));
    Debug ("Done dispatch %s\n", d->name);
    if (code == 0)
    {
	greet->name = name;
	greet->password = password;
	XtSetArg (arglist[0], XtNsessionArgument, (char *) &(greet->string));
	XtSetArg (arglist[1], XtNallowNullPasswd, (char *) &(greet->allow_null_passwd));
	XtSetArg (arglist[2], XtNallowRootLogin, (char *) &(greet->allow_root_login));
	XtGetValues (login, arglist, 3);
	Debug ("sessionArgument: %s\n", greet->string ? greet->string : "<NULL>");
    }
    return code;
}


static void
FailedLogin (struct display *d, struct greet_info *greet)
{
#ifdef __OpenBSD__
    syslog(LOG_NOTICE, "LOGIN FAILURE ON %s",
	   d->name);
    syslog(LOG_AUTHPRIV|LOG_NOTICE,
	   "LOGIN FAILURE ON %s, %s",
	   d->name, greet->name);
#endif
    DrawFail (login);
    bzero (greet->name, strlen(greet->name));
    bzero (greet->password, strlen(greet->password));
}


greet_user_rtn GreetUser(
    struct display          *d,
    Display                 ** dpy,
    struct verify_info      *verify,
    struct greet_info       *greet,
    struct dlfuncs        *dlfuncs)
{
    int i;

#ifdef GREET_LIB
/*
 * These must be set before they are used.
 */
    __xdm_PingServer = dlfuncs->_PingServer;
    __xdm_SessionPingFailed = dlfuncs->_SessionPingFailed;
    __xdm_Debug = dlfuncs->_Debug;
    __xdm_RegisterCloseOnFork = dlfuncs->_RegisterCloseOnFork;
    __xdm_SecureDisplay = dlfuncs->_SecureDisplay;
    __xdm_UnsecureDisplay = dlfuncs->_UnsecureDisplay;
    __xdm_ClearCloseOnFork = dlfuncs->_ClearCloseOnFork;
    __xdm_SetupDisplay = dlfuncs->_SetupDisplay;
    __xdm_LogError = dlfuncs->_LogError;
    __xdm_SessionExit = dlfuncs->_SessionExit;
    __xdm_DeleteXloginResources = dlfuncs->_DeleteXloginResources;
    __xdm_source = dlfuncs->_source;
    __xdm_defaultEnv = dlfuncs->_defaultEnv;
    __xdm_setEnv = dlfuncs->_setEnv;
    __xdm_putEnv = dlfuncs->_putEnv;
    __xdm_parseArgs = dlfuncs->_parseArgs;
    __xdm_printEnv = dlfuncs->_printEnv;
    __xdm_systemEnv = dlfuncs->_systemEnv;
    __xdm_LogOutOfMem = dlfuncs->_LogOutOfMem;
    __xdm_setgrent = dlfuncs->_setgrent;
    __xdm_getgrent = dlfuncs->_getgrent;
    __xdm_endgrent = dlfuncs->_endgrent;
#ifdef USESHADOW
    __xdm_getspnam = dlfuncs->_getspnam;
    __xdm_endspent = dlfuncs->_endspent;
#endif
    __xdm_getpwnam = dlfuncs->_getpwnam;
#ifdef linux
    __xdm_endpwent = dlfuncs->_endpwent;
#endif
    __xdm_crypt = dlfuncs->_crypt;
#ifdef USE_PAM
    __xdm_thepamhp = dlfuncs->_thepamhp;
#endif
#endif

    *dpy = InitGreet (d);
    /*
     * Run the setup script - note this usually will not work when
     * the server is grabbed, so we don't even bother trying.
     */
    if (!d->grabServer)
	SetupDisplay (d);
    if (!*dpy) {
	LogError ("Cannot reopen display %s for greet window\n", d->name);
	exit (RESERVER_DISPLAY);
    }
#ifdef __OpenBSD__
    openlog("xdm", LOG_ODELAY, LOG_AUTH);
#endif
    for (;;) {
	/*
	 * Greet user, requesting name/password
	 */
	code = Greet (d, greet);
	if (code != 0)
	{
	    CloseGreet (d);
	    SessionExit (d, code, FALSE);
	}
	/*
	 * Verify user
	 */
	if (Verify (d, greet, verify))
	    break;
	else
	    FailedLogin (d, greet);
    }
    DeleteXloginResources (d, *dpy);
    CloseGreet (d);
    Debug ("Greet loop finished\n");
    /*
     * Run system-wide initialization file
     */
    if (source (verify->systemEnviron, d->startup) != 0)
    {
	Debug ("Startup program %s exited with non-zero status\n",
		d->startup);
	SessionExit (d, OBEYSESS_DISPLAY, FALSE);
    }
    /*
     * for user-based authorization schemes,
     * add the user to the server's allowed "hosts" list.
     */
    for (i = 0; i < d->authNum; i++)
    {
#ifdef SECURE_RPC
	if (d->authorizations[i]->name_length == 9 &&
	    memcmp(d->authorizations[i]->name, "SUN-DES-1", 9) == 0)
	{
	    XHostAddress	addr;
	    char		netname[MAXNETNAMELEN+1];
	    char		domainname[MAXNETNAMELEN+1];
    
	    getdomainname(domainname, sizeof domainname);
	    user2netname (netname, verify->uid, domainname);
	    addr.family = FamilyNetname;
	    addr.length = strlen (netname);
	    addr.address = netname;
	    XAddHost (*dpy, &addr);
	}
#endif
#ifdef K5AUTH
	if (d->authorizations[i]->name_length == 14 &&
	    memcmp(d->authorizations[i]->name, "MIT-KERBEROS-5", 14) == 0)
	{
	    /* Update server's auth file with user-specific info.
	     * Don't need to AddHost because X server will do that
	     * automatically when it reads the cache we are about
	     * to point it at.
	     */
	    extern Xauth *Krb5GetAuthFor();

	    XauDisposeAuth (d->authorizations[i]);
	    d->authorizations[i] =
		Krb5GetAuthFor(14, "MIT-KERBEROS-5", d->name);
	    SaveServerAuthorizations (d, d->authorizations, d->authNum);
	} 
#endif
    }

    return Greet_Success;
}
