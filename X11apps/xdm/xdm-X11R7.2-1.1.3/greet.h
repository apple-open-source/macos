/* $Xorg: greet.h,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
/*

Copyright 1994, 1998  The Open Group

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
/* $XFree86: xc/programs/xdm/greet.h,v 1.9 2003/07/09 15:27:38 tsi Exp $ */

/*
 * greet.h - interface to xdm's dynamically-loadable modular greeter
 */

#include <X11/Xlib.h>

/*
 * Do this rather than break a build over a const-mismatch
 */
#if defined(__linux__) || defined(CSRG_BASED) || (defined(sun) && defined(SVR4))
#define CRYPT_ARGS    const char *s1, const char *s2
#define GETSPNAM_ARGS const char *name
#define GETPWNAM_ARGS const char *name
#else
#define CRYPT_ARGS    /*unknown*/
#define GETSPNAM_ARGS /*unknown*/
#define GETPWNAM_ARGS /*unknown*/
#endif

#if defined(__FreeBSD__) || defined(__bsdi__) || defined(__osf__)
#define SETGRENT_TYPE int
#else
#define SETGRENT_TYPE void
#endif

struct dlfuncs {
    int (*_PingServer)(struct display *d, Display *alternateDpy);
    void (*_SessionPingFailed)(struct display *d);
    void (*_Debug)(char * fmt, ...);
    void (*_RegisterCloseOnFork)(int fd);
    void (*_SecureDisplay)(struct display *d, Display *dpy);
    void (*_UnsecureDisplay)(struct display *d, Display *dpy);
    void (*_ClearCloseOnFork)(int fd);
    void (*_SetupDisplay)(struct display *d);
    void (*_LogError)(char * fmt, ...);
    void (*_SessionExit)(struct display *d, int status, int removeAuth);
    void (*_DeleteXloginResources)(struct display *d, Display *dpy);
    int (*_source)(char **environ, char *file);
    char **(*_defaultEnv)(void);
    char **(*_setEnv)(char **e, char *name, char *value);
    char **(*_putEnv)(const char *string, char **env);
    char **(*_parseArgs)(char **argv, char *string);
    void (*_printEnv)(char **e);
    char **(*_systemEnv)(struct display *d, char *user, char *home);
    void (*_LogOutOfMem)(char * fmt, ...);
    SETGRENT_TYPE (*_setgrent)(void);		/* no longer used */
    struct group *(*_getgrent)(void);	/* no longer used */
    void (*_endgrent)(void);		/* no longer used */
#ifdef USESHADOW
    struct spwd *(*_getspnam)(GETSPNAM_ARGS);
# ifndef QNX4
    void (*_endspent)(void);
# endif /* QNX4 doesn't use endspent */
#endif
    struct passwd *(*_getpwnam)(GETPWNAM_ARGS);
#if defined(linux) || defined(__GLIBC__)
    void (*_endpwent)(void);
#endif
    char *(*_crypt)(CRYPT_ARGS);
#ifdef USE_PAM
    pam_handle_t **(*_thepamhp)(void);
#endif
};

/*
 * Return values for GreetUser();
 * Values must be explictly defined because the greet library
 * may come from a different vendor.
 * Negative values indicate an error.
 */
typedef enum {
    Greet_Session_Over = 0,	/* session managed and over */
    Greet_Success = 1,		/* greet succeeded, session not managed */
    Greet_Failure = -1		/* greet failed */
} greet_user_rtn;

/*
 * GreetUser can either handle the user's session or allow xdm to do it.
 * The return or exit status of GreetUser indicates to xdm whether it
 * should start a session.
 * 
 * GreetUser is passed the xdm struct display pointer, a pointer to a
 * Display, and pointers to greet and verify structs.  If it expectes xdm
 * to run the session, it fills in the Display pointer and the fields
 * of the greet and verify structs.
 * 
 * The verify struct includes the uid, gid, arguments to run the session,
 * environment for the session, and environment for startup/reset.
 * 
 * The greet struct includes the user's name and password but these are
 * really only needed if xdm is compiled with a user-based authorization
 * option such as SECURE_RPC or K5AUTH.
 */

extern greet_user_rtn GreetUser(struct display *, Display **,
    struct verify_info *, struct greet_info *, struct dlfuncs *);

typedef greet_user_rtn (*GreetUserProc)(struct display *, Display **,
    struct verify_info *, struct greet_info *, struct dlfuncs *dlfcns);

#ifdef GREET_LIB
/*
 * The greeter uses some symbols from the main xdm executable.  Since some
 * dynamic linkers are broken, we need to fix things up so that the symbols
 * are referenced indirectly through function pointers.  The definitions
 * here, are used to hold the pointers to the functions in the main xdm
 * executable.  The pointers are filled in when the GreetUser function is
 * called, with the pointer values passed as a paramter.
 */

extern	int     (*__xdm_PingServer)(struct display *d, Display *alternateDpy);
extern	void    (*__xdm_SessionPingFailed)(struct display *d);
extern	void    (*__xdm_Debug)(char * fmt, ...);
extern	void    (*__xdm_RegisterCloseOnFork)(int fd);
extern	void    (*__xdm_SecureDisplay)(struct display *d, Display *dpy);
extern	void    (*__xdm_UnsecureDisplay)(struct display *d, Display *dpy);
extern	void    (*__xdm_ClearCloseOnFork)(int fd);
extern	void    (*__xdm_SetupDisplay)(struct display *d);
extern	void    (*__xdm_LogError)(char * fmt, ...);
extern	void    (*__xdm_SessionExit)(struct display *d, int status, int removeAuth);
extern	void    (*__xdm_DeleteXloginResources)(struct display *d, Display *dpy);
extern	int     (*__xdm_source)(char **environ, char *file);
extern	char    **(*__xdm_defaultEnv)(void);
extern	char    **(*__xdm_setEnv)(char **e, char *name, char *value);
extern	char    **(*__xdm_putEnv)(const char *string, char **env);
extern	char    **(*__xdm_parseArgs)(char **argv, char *string);
extern	void    (*__xdm_printEnv)(char **e);
extern	char    **(*__xdm_systemEnv)(struct display *d, char *user, char *home);
extern	void    (*__xdm_LogOutOfMem)(char * fmt, ...);
extern	void    (*__xdm_setgrent)(void);
extern	struct group    *(*__xdm_getgrent)(void);
extern	void    (*__xdm_endgrent)(void);
#ifdef USESHADOW
extern	struct spwd   *(*__xdm_getspnam)(GETSPNAM_ARGS);
# ifndef QNX4
extern	void    (*__xdm_endspent)(void);
# endif /* QNX4 doesn't use endspent */
#endif
extern	struct passwd   *(*__xdm_getpwnam)(GETPWNAM_ARGS);
#if defined(linux) || defined(__GLIBC__)
extern  void    (*__xdm_endpwent)(void);
#endif
extern	char    *(*__xdm_crypt)(CRYPT_ARGS);
#ifdef USE_PAM
extern  pam_handle_t    **(*__xdm_thepamhp)(void);
#endif

/*
 * Force the shared library to call through the function pointer
 * initialized during the initial call into the library.
 */

#define	PingServer	(*__xdm_PingServer)
#define	SessionPingFailed	(*__xdm_SessionPingFailed)
#define	Debug	(*__xdm_Debug)
#define	RegisterCloseOnFork	(*__xdm_RegisterCloseOnFork)
#define	SecureDisplay	(*__xdm_SecureDisplay)
#define	UnsecureDisplay	(*__xdm_UnsecureDisplay)
#define	ClearCloseOnFork	(*__xdm_ClearCloseOnFork)
#define	SetupDisplay	(*__xdm_SetupDisplay)
#define	LogError	(*__xdm_LogError)
#define	SessionExit	(*__xdm_SessionExit)
#define	DeleteXloginResources	(*__xdm_DeleteXloginResources)
#define	source		(*__xdm_source)
#define	defaultEnv	(*__xdm_defaultEnv)
#define	setEnv		(*__xdm_setEnv)
#define putEnv		(*__xdm_putEnv)
#define	parseArgs	(*__xdm_parseArgs)
#define	printEnv	(*__xdm_printEnv)
#define	systemEnv	(*__xdm_systemEnv)
#define	LogOutOfMem	(*__xdm_LogOutOfMem)
#define	setgrent	(*__xdm_setgrent)
#define	getgrent	(*__xdm_getgrent)
#define	endgrent	(*__xdm_endgrent)
#ifdef USESHADOW
# define getspnam	(*__xdm_getspnam)
# ifndef QNX4
#  define endspent	(*__xdm_endspent)
# endif /* QNX4 doesn't use endspent */
#endif
#define	getpwnam	(*__xdm_getpwnam)
#if defined(linux) || defined(__GLIBC__)
# define endpwent	(*__xdm_endpwent)
#endif
#define	crypt		(*__xdm_crypt)
#ifdef USE_PAM
# define thepamhp	(*__xdm_thepamhp)
#endif

#endif /* GREET_LIB */
