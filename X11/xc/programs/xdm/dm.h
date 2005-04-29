/* $Xorg: dm.h,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xdm/dm.h,v 3.34 2003/12/12 03:20:45 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * dm.h
 *
 * public interfaces for greet/verify functionality
 */

#ifndef _DM_H_
#define _DM_H_ 1

#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xmd.h>
#include <X11/Xauth.h>
#include <X11/Intrinsic.h>

#if defined(X_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#include <setjmp.h>
#include <limits.h>
#undef _POSIX_C_SOURCE
#else
#include <setjmp.h>
#include <limits.h>
#endif
#include <time.h>
#define Time_t time_t

/* If XDMCP symbol defined, compile to run XDMCP protocol */

#define XDMCP

#ifdef XDMCP
#include <X11/Xdmcp.h>
#endif

#ifdef pegasus
#undef dirty		/* Some bozo put a macro called dirty in sys/param.h */
#endif /* pegasus */

#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
#include <sys/wait.h>
#else
#define _POSIX_SOURCE
#ifdef SCO325
#include <sys/procset.h>
#include <sys/siginfo.h>
#endif
#include <sys/wait.h>
#undef _POSIX_SOURCE
#endif
# define waitCode(w)	(WIFEXITED(w) ? WEXITSTATUS(w) : 0)
# define waitSig(w)	(WIFSIGNALED(w) ? WTERMSIG(w) : 0)
# define waitCore(w)    0	/* not in POSIX.  so what? */
typedef int		waitType;
#else /* X_NOT_POSIX */
#ifdef SYSV
# define waitCode(w)	(((w) >> 8) & 0x7f)
# define waitSig(w)	((w) & 0xff)
# define waitCore(w)	(((w) >> 15) & 0x01)
typedef int		waitType;
#else /* SYSV */
# include <sys/wait.h>
# define waitCode(w)	((w).w_T.w_Retcode)
# define waitSig(w)	((w).w_T.w_Termsig)
# define waitCore(w)	((w).w_T.w_Coredump)
typedef union wait	waitType;
#endif
#endif /* X_NOT_POSIX */

#ifdef USE_PAM
#include <security/pam_appl.h>
#endif

#ifdef CSRG_BASED
#include <sys/param.h>
#ifdef HAS_SETUSERCONTEXT
#include <login_cap.h>
#include <pwd.h>
#ifdef USE_BSDAUTH
#include <bsd_auth.h>
#endif
#endif
#endif

# define waitCompose(sig,core,code) ((sig) * 256 + (core) * 128 + (code))
# define waitVal(w)	waitCompose(waitSig(w), waitCore(w), waitCode(w))

typedef enum displayStatus { running, notRunning, zombie, phoenix } DisplayStatus;

#ifndef FD_ZERO
typedef	struct	my_fd_set { int fds_bits[1]; } my_fd_set;
# define FD_ZERO(fdp)	bzero ((fdp), sizeof (*(fdp)))
# define FD_SET(f,fdp)	((fdp)->fds_bits[(f) / (sizeof (int) * 8)] |=  (1 << ((f) % (sizeof (int) * 8))))
# define FD_CLR(f,fdp)	((fdp)->fds_bits[(f) / (sizeof (int) * 8)] &= ~(1 << ((f) % (sizeof (int) * 8))))
# define FD_ISSET(f,fdp)	((fdp)->fds_bits[(f) / (sizeof (int) * 8)] & (1 << ((f) % (sizeof (int) * 8))))
# define FD_TYPE	my_fd_set
#else
# define FD_TYPE	fd_set
#endif

/*
 * local     - server runs on local host
 * foreign   - server runs on remote host
 * permanent - session restarted when it exits
 * transient - session not restarted when it exits
 * fromFile  - started via entry in servers file
 * fromXDMCP - started with XDMCP
 */

typedef struct displayType {
	unsigned int	location:1;
	unsigned int	lifetime:1;
	unsigned int	origin:1;
} DisplayType;

# define Local		1
# define Foreign	0

# define Permanent	1
# define Transient	0

# define FromFile	1
# define FromXDMCP	0

extern DisplayType parseDisplayType (char *string, int *usedDefault);

typedef enum fileState { NewEntry, OldEntry, MissingEntry } FileState;

struct display {
	struct display	*next;
	/* Xservers file / XDMCP information */
	char		*name;		/* DISPLAY name */
	char		*class;		/* display class (may be NULL) */
	DisplayType	displayType;	/* method to handle with */
	char		**argv;		/* program name and arguments */

	/* display state */
	DisplayStatus	status;		/* current status */
	int		pid;		/* process id of child */
	int		serverPid;	/* process id of server (-1 if none) */
	FileState	state;		/* state during HUP processing */
	int		startTries;	/* current start try */
        Time_t          lastCrash;      /* time of last crash */
#ifdef XDMCP
	/* XDMCP state */
	CARD32		sessionID;	/* ID of active session */
	XdmcpNetaddr    peer;		/* display peer address */
	int		peerlen;	/* length of peer address */
	XdmcpNetaddr    from;		/* XDMCP port of display */
	int		fromlen;
	CARD16		displayNumber;
	int		useChooser;	/* Run the chooser for this display */
	ARRAY8		clientAddr;	/* for chooser picking */
	CARD16		connectionType;	/* ... */
	int		xdmcpFd;
#endif
	/* server management resources */
	int		serverAttempts;	/* number of attempts at running X */
	int		openDelay;	/* open delay time */
	int		openRepeat;	/* open attempts to make */
	int		openTimeout;	/* abort open attempt timeout */
	int		startAttempts;	/* number of attempts at starting */
	int		pingInterval;	/* interval between XSync */
	int		pingTimeout;	/* timeout for XSync */
	int		terminateServer;/* restart for each session */
	int		grabServer;	/* keep server grabbed for Login */
	int		grabTimeout;	/* time to wait for grab */
	int		resetSignal;	/* signal to reset server */
	int		termSignal;	/* signal to terminate server */
	int		resetForAuth;	/* server reads auth file at reset */
	char            *keymaps;       /* binary compat with DEC */
	char		*greeterLib;	/* greeter shared library name */

	/* session resources */
	char		*resources;	/* resource file */
	char		*xrdb;		/* xrdb program */
	char		*setup;		/* Xsetup program */
	char		*startup;	/* Xstartup program */
	char		*reset;		/* Xreset program */
	char		*session;	/* Xsession program */
	char		*userPath;	/* path set for session */
	char		*systemPath;	/* path set for startup/reset */
	char		*systemShell;	/* interpreter for startup/reset */
	char		*failsafeClient;/* a client to start when the session fails */
	char		*chooser;	/* chooser program */

	/* authorization resources */
	int		authorize;	/* enable authorization */
	char		**authNames;	/* authorization protocol names */
	unsigned short	*authNameLens;	/* authorization protocol name lens */
	char		*clientAuthFile;/* client specified auth file */
	char		*userAuthDir;	/* backup directory for tickets */
	int		authComplain;	/* complain when no auth for XDMCP */

	/* information potentially derived from resources */
	int		authNameNum;	/* number of protocol names */
	Xauth		**authorizations;/* authorization data */
	int		authNum;	/* number of authorizations */
	char		*authFile;	/* file to store authorization in */

	int		version;	/* to keep dynamic greeter clued in */
	/* add new fields only after here.  And preferably at the end. */

	/* Hack for making "Willing to manage" configurable */
	char		*willing;	/* "Willing to manage" program */
};

#ifdef XDMCP

#define PROTO_TIMEOUT	(30 * 60)   /* 30 minutes should be long enough */
#define XDM_BROKEN_INTERVAL (120)   /* server crashing more than once within */
                                    /* two minutes is assumed to be broken!  */
struct protoDisplay {
	struct protoDisplay	*next;
	XdmcpNetaddr		address;   /* UDP address */
	int			addrlen;    /* UDP address length */
	unsigned long		date;	    /* creation date */
	CARD16			displayNumber;
	CARD16			connectionType;
	ARRAY8			connectionAddress;
	CARD32			sessionID;
	Xauth			*fileAuthorization;
	Xauth			*xdmcpAuthorization;
	ARRAY8			authenticationName;
	ARRAY8			authenticationData;
	XdmAuthKeyRec		key;
};
#endif /* XDMCP */

struct greet_info {
	char		*name;		/* user name */
	char		*password;	/* user password */
	char		*string;	/* random string */
	char            *passwd;        /* binary compat with DEC */
	int		version;	/* for dynamic greeter to see */
	/* add new fields below this line, and preferably at the end */
	Boolean		allow_null_passwd; /* allow null password on login */
	Boolean		allow_root_login; /* allow direct root login */
};

/* setgroups is not covered by POSIX, arg type varies */
#if defined(SYSV) || defined(SVR4) || defined(__osf__) || defined(linux) || defined(__GNU__)
#define GID_T gid_t
#else
#define GID_T int
#endif

typedef void (*ChooserFunc)(CARD16 connectionType, ARRAY8Ptr addr, char *closure);
typedef void (*ListenFunc)(ARRAY8Ptr addr, void **closure);

struct verify_info {
	int		uid;		/* user id */
	int		gid;		/* group id */
	char		**argv;		/* arguments to session */
	char		**userEnviron;	/* environment for session */
	char		**systemEnviron;/* environment for startup/reset */
	int		version;	/* for dynamic greeter to see */
	/* add new fields below this line, and preferably at the end */
};

/* display manager exit status definitions */

# define OBEYSESS_DISPLAY	0	/* obey multipleSessions resource */
# define REMANAGE_DISPLAY	1	/* force remanage */
# define UNMANAGE_DISPLAY	2	/* force deletion */
# define RESERVER_DISPLAY	3	/* force server termination */
# define OPENFAILED_DISPLAY	4	/* XOpenDisplay failed, retry */

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

extern char	*config;

extern char	*servers;
extern int	request_port;
extern int	debugLevel;
extern char	*errorLogFile;
extern int	daemonMode;
extern char	*pidFile;
extern int	lockPidFile;
extern char	*authDir;
extern int	autoRescan;
extern int	removeDomainname;
extern char	*keyFile;
extern char	*accessFile;
extern char	**exportList;
#if !defined(ARC4_RANDOM)
extern char	*randomFile;
extern char	*prngdSocket;
extern int	prngdPort;
#endif

extern char	*greeterLib;
extern char	*willing;
extern int	choiceTimeout;	/* chooser choice timeout */

extern struct display	*FindDisplayByName (char *name),
			*FindDisplayBySessionID (CARD32 sessionID),
			*FindDisplayByAddress (XdmcpNetaddr addr, int addrlen, CARD16 displayNumber),
			*FindDisplayByPid (int pid),
			*FindDisplayByServerPid (int serverPid),
			*NewDisplay (char *name, char *class);

extern struct protoDisplay	*FindProtoDisplay (
					XdmcpNetaddr address,
					int          addrlen,
					CARD16       displayNumber);
extern struct protoDisplay	*NewProtoDisplay (
					XdmcpNetaddr address,
					int	     addrlen,
					CARD16	     displayNumber,
					CARD16	     connectionType,
					ARRAY8Ptr    connectionAddress,
					CARD32	     sessionID);

/* in Login.c */
extern void DrawFail (Widget ctx);

/* in access.c */
extern ARRAY8Ptr getLocalAddress (void);
extern int AcceptableDisplayAddress (ARRAY8Ptr clientAddress, CARD16 connectionType, xdmOpCode type);
extern int ForEachMatchingIndirectHost (ARRAY8Ptr clientAddress, CARD16 connectionType, ChooserFunc function, char *closure);
extern int ScanAccessDatabase (void);
extern int UseChooser (ARRAY8Ptr clientAddress, CARD16 connectionType);
extern void ForEachChooserHost (ARRAY8Ptr clientAddress, CARD16 connectionType, ChooserFunc function, char *closure);
extern void ForEachListenAddr(ListenFunc listenfunction,
  ListenFunc mcastfcuntion, void **closure);

/* in choose.c */
extern ARRAY8Ptr IndirectChoice (ARRAY8Ptr clientAddress, CARD16 connectionType);
extern int IsIndirectClient (ARRAY8Ptr clientAddress, CARD16 connectionType);
extern int RememberIndirectClient (ARRAY8Ptr clientAddress, CARD16 connectionType);
extern void ForgetIndirectClient ( ARRAY8Ptr clientAddress, CARD16 connectionType);
extern void ProcessChooserSocket (int fd);

/* in chooser.c */
extern void RunChooser (struct display *d);

/* in daemon.c */
extern void BecomeDaemon (void);
extern void BecomeOrphan (void);

/* in dm.c */
extern void CloseOnFork (void);
extern void RegisterCloseOnFork (int fd);
extern void StartDisplay (struct display *d);
#ifndef HAS_SETPROCTITLE
extern void SetTitle (char *name, ...);
#endif

/* in dpylist.c */
extern int AnyDisplaysLeft (void);
extern void ForEachDisplay (void (*f)(struct display *));
extern void RemoveDisplay (struct display *old);

/* in file.c */
extern void ParseDisplay (char *source, DisplayType *acceptableTypes, int numAcceptable);

/* in netaddr.c */
extern char *NetaddrAddress(XdmcpNetaddr netaddrp, int *lenp);
extern char *NetaddrPort(XdmcpNetaddr netaddrp, int *lenp);
extern int ConvertAddr (XdmcpNetaddr saddr, int *len, char **addr);
extern int NetaddrFamily (XdmcpNetaddr netaddrp);
extern int addressEqual (XdmcpNetaddr a1, int len1, XdmcpNetaddr a2, int len2);

/* in policy.c */
#if 0
extern ARRAY8Ptr Accept (/* struct sockaddr *from, int fromlen, CARD16 displayNumber */);
#endif
extern ARRAY8Ptr ChooseAuthentication (ARRAYofARRAY8Ptr authenticationNames);
extern int CheckAuthentication (struct protoDisplay *pdpy, ARRAY8Ptr displayID, ARRAY8Ptr name, ARRAY8Ptr data);
extern int SelectAuthorizationTypeIndex (ARRAY8Ptr authenticationName, ARRAYofARRAY8Ptr authorizationNames);
extern int SelectConnectionTypeIndex (ARRAY16Ptr connectionTypes, ARRAYofARRAY8Ptr connectionAddresses);
extern int Willing (ARRAY8Ptr addr, CARD16 connectionType, ARRAY8Ptr authenticationName, ARRAY8Ptr status, xdmOpCode type);

/* in protodpy.c */
extern void DisposeProtoDisplay(struct protoDisplay *pdpy);

/* in reset.c */
extern void pseudoReset (Display *dpy);

/* in resource.c */
extern void InitResources (int argc, char **argv);
extern void LoadDMResources (void);
extern void LoadServerResources (struct display *d);
extern void LoadSessionResources (struct display *d);
extern void ReinitResources (void);

/* in session.c */
#ifdef USE_PAM
extern pam_handle_t **thepamhp(void);
extern pam_handle_t *thepamh(void);
#endif
extern char **defaultEnv (void);
extern char **systemEnv (struct display *d, char *user, char *home);
extern int PingServer(struct display *d, Display *alternateDpy);
extern int source (char **environ, char *file);
extern void ClearCloseOnFork (int fd);
extern void DeleteXloginResources (struct display *d, Display *dpy);
extern void LoadXloginResources (struct display *d);
extern void ManageSession (struct display *d);
extern void SecureDisplay (struct display *d, Display *dpy);
extern void SessionExit (struct display *d, int status, int removeAuth);
extern void SessionPingFailed (struct display *d);
extern void SetupDisplay (struct display *d);
extern void UnsecureDisplay (struct display *d, Display *dpy);
extern void execute(char **argv, char **environ);

/* server.c */
extern char *_SysErrorMsg (int n);
extern int StartServer (struct display *d);
extern int WaitForServer (struct display *d);
extern void ResetServer (struct display *d);

/* socket.c or streams.c */
extern int GetChooserAddr (char *addr, int *lenp);
extern void CreateWellKnownSockets (void);
extern void UpdateListenSockets (void);
extern void CloseListenSockets (void);
extern void ProcessListenSockets (fd_set *readmask);

/* in util.c */
extern char *localHostname (void);
extern char **parseArgs (char **argv, char *string);
extern char **setEnv (char **e, char *name, char *value);
extern char **putEnv(const char *string, char **env);
extern char *getEnv (char **e, char *name);
extern void CleanUpChild (void);
extern void freeArgs (char **argv);
extern void freeEnv (char **env);
extern void printEnv (char **e);

/* in verify.c */
extern int Verify (struct display *d, struct greet_info *greet, struct verify_info *verify);

/* in xdmcp.c */
extern char *NetworkAddressToHostname (CARD16 connectionType, ARRAY8Ptr connectionAddress);
extern int AnyWellKnownSockets (void);
extern void DestroyWellKnownSockets (void);
extern void SendFailed (struct display *d, char *reason);
extern void StopDisplay (struct display *d);
extern void WaitForChild (void);
extern void WaitForSomething (void);
extern void init_session_id(void);
extern void registerHostname(char *name, int namelen);
extern void ProcessRequestSocket(int fd);

/*
 * CloseOnFork flags
 */

# define CLOSE_ALWAYS	    0
# define LEAVE_FOR_DISPLAY  1

#include <stdlib.h>

#if defined(X_NOT_POSIX) && defined(SIGNALRETURNSINT)
#define SIGVAL int
#else
#define SIGVAL void
#endif

#if defined(X_NOT_POSIX) || defined(__UNIXOS2__) || defined(__NetBSD__) && defined(__sparc__)
#if defined(SYSV) || defined(__UNIXOS2__)
#define SIGNALS_RESET_WHEN_CAUGHT
#define UNRELIABLE_SIGNALS
#endif
#define Setjmp(e)	setjmp(e)
#define Longjmp(e,v)	longjmp(e,v)
#define Jmp_buf		jmp_buf
#else
#define Setjmp(e)   sigsetjmp(e,1)
#define Longjmp(e,v)	siglongjmp(e,v)
#define Jmp_buf		sigjmp_buf
#endif

typedef SIGVAL (*SIGFUNC)(int);

SIGVAL (*Signal(int, SIGFUNC Handler))(int);

#endif /* _DM_H_ */
