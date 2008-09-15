/* $XTermId: main.c,v 1.586 2008/02/28 00:28:00 Matthieu.Herrb Exp $ */

/*
 *				 W A R N I N G
 *
 * If you think you know what all of this code is doing, you are
 * probably very mistaken.  There be serious and nasty dragons here.
 *
 * This client is *not* to be taken as an example of how to write X
 * Toolkit applications.  It is in need of a substantial rewrite,
 * ideally to create a generic tty widget with several different parsing
 * widgets so that you can plug 'em together any way you want.  Don't
 * hold your breath, though....
 */

/***********************************************************

Copyright 2002-2007,2008 by Thomas E. Dickey

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

Copyright 1987, 1988  The Open Group

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

Copyright 1987, 1988 by Digital Equipment Corporation, Maynard.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be used in
advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* main.c */

#define RES_OFFSET(field)	XtOffsetOf(XTERM_RESOURCE, field)

#include <xterm.h>

#include <X11/cursorfont.h>
#include <X11/Xlocale.h>

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

#include <pwd.h>
#include <ctype.h>

#include <data.h>
#include <error.h>
#include <menu.h>
#include <main.h>
#include <xstrings.h>
#include <xtermcap.h>
#include <xterm_io.h>

#if OPT_WIDE_CHARS
#include <charclass.h>
#endif

#ifdef __osf__
#define USE_SYSV_SIGNALS
#define WTMP
#include <pty.h>		/* openpty() */
#endif

#ifdef __sgi
#include <grp.h>		/* initgroups() */
#endif

#ifdef USE_ISPTS_FLAG
static Bool IsPts = False;
#endif

#if defined(__SCO__) || defined(SVR4) || defined(_POSIX_SOURCE)
#define USE_POSIX_SIGNALS
#endif

#if defined(SYSV) && !defined(SVR4) && !defined(ISC22) && !defined(ISC30)
/* older SYSV systems cannot ignore SIGHUP.
   Shell hangs, or you get extra shells, or something like that */
#define USE_SYSV_SIGHUP
#endif

#if defined(sony) && defined(bsd43) && !defined(KANJI)
#define KANJI
#endif

#ifdef linux
#define USE_SYSV_PGRP
#define USE_SYSV_SIGNALS
#define WTMP
#ifdef __GLIBC__
#if (__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 1))
#include <pty.h>
#endif
#endif
#endif

#ifdef __MVS__
#define USE_SYSV_PGRP
#define USE_SYSV_SIGNALS
#endif

#ifdef __CYGWIN__
#define WTMP
#endif

#ifdef __SCO__
#ifndef _SVID3
#define _SVID3
#endif
#endif

#if defined(__GLIBC__) && !defined(linux)
#define USE_SYSV_PGRP
#define WTMP
#define HAS_BSD_GROUPS
#endif

#if defined(USE_TTY_GROUP) || defined(USE_UTMP_SETGID)
#include <grp.h>
#endif

#ifndef TTY_GROUP_NAME
#define TTY_GROUP_NAME "tty"
#endif

#include <sys/stat.h>

#ifdef Lynx
#ifndef BSDLY
#define BSDLY	0
#endif
#ifndef VTDLY
#define VTDLY	0
#endif
#ifndef FFDLY
#define FFDLY	0
#endif
#endif

#ifdef SYSV			/* { */

#ifdef USE_USG_PTYS		/* AT&T SYSV has no ptyio.h */
#include <sys/stropts.h>	/* for I_PUSH */
#include <poll.h>		/* for POLLIN */
#endif /* USE_USG_PTYS */

#define USE_SYSV_SIGNALS
#define	USE_SYSV_PGRP

#if !defined(TIOCSWINSZ) || defined(__SCO__) || defined(__UNIXWARE__)
#define USE_SYSV_ENVVARS	/* COLUMNS/LINES vs. TERMCAP */
#endif

/*
 * now get system-specific includes
 */
#ifdef CRAY
#define HAS_BSD_GROUPS
#endif

#ifdef macII
#define HAS_BSD_GROUPS
#include <sys/ttychars.h>
#undef USE_SYSV_ENVVARS
#undef FIOCLEX
#undef FIONCLEX
#define setpgrp2 setpgrp
#include <sgtty.h>
#include <sys/resource.h>
#endif

#ifdef __hpux
#define HAS_BSD_GROUPS
#include <sys/ptyio.h>
#endif /* __hpux */

#ifdef __osf__
#define HAS_BSD_GROUPS
#undef  USE_SYSV_PGRP
#define setpgrp setpgid
#endif

#ifdef __sgi
#define HAS_BSD_GROUPS
#include <sys/sysmacros.h>
#endif /* __sgi */

#ifdef sun
#include <sys/strredir.h>
#endif

#else	/* } !SYSV { */	/* BSD systems */

#ifdef __QNX__

#ifndef __QNXNTO__
#define ttyslot() 1
#else
#define USE_SYSV_PGRP
extern __inline__
int
ttyslot(void)
{
    return 1;			/* yuk */
}
#endif

#else

#if defined(__INTERIX) || defined(__APPLE__)
#define setpgrp setpgid
#endif

#ifndef linux
#ifndef VMS
#ifndef USE_POSIX_TERMIOS
#ifndef USE_ANY_SYSV_TERMIO
#include <sgtty.h>
#endif
#endif /* USE_POSIX_TERMIOS */
#ifdef Lynx
#include <resource.h>
#else
#include <sys/resource.h>
#endif
#ifndef __INTERIX
#define HAS_BSD_GROUPS
#endif
#endif /* !VMS */
#endif /* !linux */

#endif /* __QNX__ */

#endif /* } !SYSV */

/* Xpoll.h and <sys/param.h> on glibc 2.1 systems have colliding NBBY's */
#if defined(__GLIBC__) && ((__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 1)))
#ifndef NOFILE
#define NOFILE OPEN_MAX
#endif
#elif !(defined(VMS) || defined(WIN32) || defined(Lynx) || defined(__GNU__) || defined(__MVS__))
#include <sys/param.h>		/* for NOFILE */
#endif

#if defined(BSD) && (BSD >= 199103)
#define WTMP
#endif

#include <stdio.h>

#ifdef __hpux
#include <sys/utsname.h>
#endif /* __hpux */

#if defined(apollo) && (OSMAJORVERSION == 10) && (OSMINORVERSION < 4)
#define ttyslot() 1
#endif /* apollo */

#if defined(UTMPX_FOR_UTMP)
#define UTMP_STR utmpx
#else
#define UTMP_STR utmp
#endif

#if defined(USE_UTEMPTER)
#include <utempter.h>
#endif

#if defined(UTMPX_FOR_UTMP)

#include <utmpx.h>

#define call_endutent  endutxent
#define call_getutid   getutxid
#define call_pututline pututxline
#define call_setutent  setutxent
#define call_updwtmp   updwtmpx

#elif defined(HAVE_UTMP)

#include <utmp.h>

#if defined(_CRAY) && (OSMAJORVERSION < 8)
extern struct utmp *getutid __((struct utmp * _Id));
#endif

#define call_endutent  endutent
#define call_getutid   getutid
#define call_pututline pututline
#define call_setutent  setutent
#define call_updwtmp   updwtmp

#endif

#if defined(USE_LASTLOG) && defined(HAVE_LASTLOG_H)
#include <lastlog.h>		/* caution: glibc includes utmp.h here */
#endif

#ifndef USE_LASTLOGX
#if defined(_NETBSD_SOURCE) && defined(_PATH_LASTLOGX)
#define USE_LASTLOGX 1
#endif
#endif

#ifdef  PUCC_PTYD
#include <local/openpty.h>
#endif /* PUCC_PTYD */

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <util.h>		/* openpty() */
#endif

#ifdef __FreeBSD__
#include <libutil.h>		/* openpty() */
#endif

#if !defined(UTMP_FILENAME)
#if defined(UTMP_FILE)
#define UTMP_FILENAME UTMP_FILE
#elif defined(_PATH_UTMP)
#define UTMP_FILENAME _PATH_UTMP
#else
#define UTMP_FILENAME "/etc/utmp"
#endif
#endif

#ifndef LASTLOG_FILENAME
#ifdef _PATH_LASTLOG
#define LASTLOG_FILENAME _PATH_LASTLOG
#else
#define LASTLOG_FILENAME "/usr/adm/lastlog"	/* only on BSD systems */
#endif
#endif

#if !defined(WTMP_FILENAME)
#if defined(WTMP_FILE)
#define WTMP_FILENAME WTMP_FILE
#elif defined(_PATH_WTMP)
#define WTMP_FILENAME _PATH_WTMP
#elif defined(SYSV)
#define WTMP_FILENAME "/etc/wtmp"
#else
#define WTMP_FILENAME "/usr/adm/wtmp"
#endif
#endif

#include <signal.h>

#if defined(__SCO__) || (defined(ISC) && !defined(_POSIX_SOURCE))
#undef SIGTSTP			/* defined, but not the BSD way */
#endif

#ifdef SIGTSTP
#include <sys/wait.h>
#endif

#if defined(__SCO__) || defined(__UNIXWARE__)
#undef ECHOKE
#undef ECHOCTL
#endif

#if defined(HAVE_SYS_TTYDEFAULTS_H) && !defined(CEOF)
#include <sys/ttydefaults.h>
#endif

#ifdef X_NOT_POSIX
extern long lseek();
#if defined(USG) || defined(SVR4)
extern unsigned sleep();
#else
extern void sleep();
#endif
extern char *ttyname();
#endif

#if defined(SYSV) && defined(DECL_PTSNAME)
extern char *ptsname(int);
#endif

#ifndef VMS
static SIGNAL_T reapchild(int n);
static int spawnXTerm(XtermWidget /* xw */ );
static void remove_termcap_entry(char *buf, char *str);
#ifdef USE_PTY_SEARCH
static int pty_search(int *pty);
#endif
#endif /* ! VMS */

static int get_pty(int *pty, char *from);
static void resize_termcap(XtermWidget xw, char *newtc);
static void set_owner(char *device, uid_t uid, gid_t gid, mode_t mode);

static Bool added_utmp_entry = False;

#ifdef HAVE_POSIX_SAVED_IDS
static uid_t save_euid;
static gid_t save_egid;
#endif

static uid_t save_ruid;
static gid_t save_rgid;

#if defined(USE_UTMP_SETGID)
static int really_get_pty(int *pty, char *from);
#endif

#if defined(USE_SYSV_UTMP) && !defined(USE_UTEMPTER)
static Bool xterm_exiting = False;
#endif

static char *explicit_shname = NULL;

/*
** Ordinarily it should be okay to omit the assignment in the following
** statement. Apparently the c89 compiler on AIX 4.1.3 has a bug, or does
** it? Without the assignment though the compiler will init command_to_exec
** to 0xffffffff instead of NULL; and subsequent usage, e.g. in spawnXTerm() to
** SEGV.
*/
static char **command_to_exec = NULL;

#if OPT_LUIT_PROG
static char **command_to_exec_with_luit = NULL;
#endif

#define TERMCAP_ERASE "kb"
#define VAL_INITIAL_ERASE A2E(8)

/* choose a nice default value for speed - if we make it too low, users who
 * mistakenly use $TERM set to vt100 will get padding delays.  Setting it to a
 * higher value is not useful since legacy applications (termcap) that care
 * about padding generally store the code in a short, which does not have
 * enough bits for the extended values.
 */
#ifdef B38400			/* everyone should define this */
#define VAL_LINE_SPEED B38400
#else /* ...but xterm's used this for a long time */
#define VAL_LINE_SPEED B9600
#endif

/*
 * Allow use of system default characters if defined and reasonable.
 * These are based on the BSD ttydefaults.h
 */
#ifndef CBRK
#define CBRK     0xff		/* was 0 */
#endif
#ifndef CDISCARD
#define CDISCARD CONTROL('O')
#endif
#ifndef CDSUSP
#define CDSUSP   CONTROL('Y')
#endif
#ifndef CEOF
#define CEOF     CONTROL('D')
#endif
#ifndef CEOL
#define CEOL	 0xff		/* was 0 */
#endif
#ifndef CERASE
#define CERASE   0177
#endif
#ifndef CERASE2
#define	CERASE2  CONTROL('H')
#endif
#ifndef CFLUSH
#define CFLUSH   CONTROL('O')
#endif
#ifndef CINTR
#define CINTR    CONTROL('C')
#endif
#ifndef CKILL
#define CKILL	 CONTROL('U')	/* was '@' */
#endif
#ifndef CLNEXT
#define CLNEXT   CONTROL('V')
#endif
#ifndef CNUL
#define CNUL     0
#endif
#ifndef CQUIT
#define CQUIT    CONTROL('\\')
#endif
#ifndef CRPRNT
#define CRPRNT   CONTROL('R')
#endif
#ifndef CREPRINT
#define CREPRINT CRPRNT
#endif
#ifndef CSTART
#define CSTART   CONTROL('Q')
#endif
#ifndef CSTATUS
#define	CSTATUS  CONTROL('T')
#endif
#ifndef CSTOP
#define CSTOP    CONTROL('S')
#endif
#ifndef CSUSP
#define CSUSP    CONTROL('Z')
#endif
#ifndef CSWTCH
#define CSWTCH   0
#endif
#ifndef CWERASE
#define CWERASE  CONTROL('W')
#endif

#ifdef USE_ANY_SYSV_TERMIO
#define TERMIO_STRUCT struct termio
#define ttySetAttr(fd, datap) ioctl(fd, TCSETA, datap)
#define ttyGetAttr(fd, datap) ioctl(fd, TCGETA, datap)
#define ttyFlush(fd)          ioctl(fd, TCFLSH, 1)
#elif defined(USE_POSIX_TERMIOS)
#define TERMIO_STRUCT struct termios
#define ttySetAttr(fd, datap) tcsetattr(fd, TCSANOW, datap)
#define ttyGetAttr(fd, datap) tcgetattr(fd, datap)
#define ttyFlush(fd)          tcflush(fd, TCOFLUSH)
#endif /* USE_ANY_SYSV_TERMIO */

#ifndef VMS
#ifdef TERMIO_STRUCT
/* The following structures are initialized in main() in order
** to eliminate any assumptions about the internal order of their
** contents.
*/
static TERMIO_STRUCT d_tio;

#ifdef HAS_LTCHARS
static struct ltchars d_ltc;
#endif /* HAS_LTCHARS */

#ifdef TIOCLSET
static unsigned int d_lmode;
#endif /* TIOCLSET */

#else /* !TERMIO_STRUCT */
static struct sgttyb d_sg =
{
    0, 0, 0177, CKILL, (EVENP | ODDP | ECHO | XTABS | CRMOD)
};
static struct tchars d_tc =
{
    CINTR, CQUIT, CSTART,
    CSTOP, CEOF, CBRK
};
static struct ltchars d_ltc =
{
    CSUSP, CDSUSP, CRPRNT,
    CFLUSH, CWERASE, CLNEXT
};
static int d_disipline = NTTYDISC;
static long int d_lmode = LCRTBS | LCRTERA | LCRTKIL | LCTLECH;
#ifdef sony
static long int d_jmode = KM_SYSSJIS | KM_ASCII;
static struct jtchars d_jtc =
{
    'J', 'B'
};
#endif /* sony */
#endif /* TERMIO_STRUCT */
#endif /* ! VMS */

/*
 * SYSV has the termio.c_cc[V] and ltchars; BSD has tchars and ltchars;
 * SVR4 has only termio.c_cc, but it includes everything from ltchars.
 * POSIX termios has termios.c_cc, which is similar to SVR4.
 */
#define TTYMODE(name) { name, sizeof(name)-1, 0, 0 }
static Boolean override_tty_modes = False;
/* *INDENT-OFF* */
static struct _xttymodes {
    char *name;
    size_t len;
    int set;
    int value;
} ttymodelist[] = {
    TTYMODE("intr"),		/* tchars.t_intrc ; VINTR */
#define XTTYMODE_intr	0
    TTYMODE("quit"),		/* tchars.t_quitc ; VQUIT */
#define XTTYMODE_quit	1
    TTYMODE("erase"),		/* sgttyb.sg_erase ; VERASE */
#define XTTYMODE_erase	2
    TTYMODE("kill"),		/* sgttyb.sg_kill ; VKILL */
#define XTTYMODE_kill	3
    TTYMODE("eof"),		/* tchars.t_eofc ; VEOF */
#define XTTYMODE_eof	4
    TTYMODE("eol"),		/* VEOL */
#define XTTYMODE_eol	5
    TTYMODE("swtch"),		/* VSWTCH */
#define XTTYMODE_swtch	6
    TTYMODE("start"),		/* tchars.t_startc ; VSTART */
#define XTTYMODE_start	7
    TTYMODE("stop"),		/* tchars.t_stopc ; VSTOP */
#define XTTYMODE_stop	8
    TTYMODE("brk"),		/* tchars.t_brkc */
#define XTTYMODE_brk	9
    TTYMODE("susp"),		/* ltchars.t_suspc ; VSUSP */
#define XTTYMODE_susp	10
    TTYMODE("dsusp"),		/* ltchars.t_dsuspc ; VDSUSP */
#define XTTYMODE_dsusp	11
    TTYMODE("rprnt"),		/* ltchars.t_rprntc ; VREPRINT */
#define XTTYMODE_rprnt	12
    TTYMODE("flush"),		/* ltchars.t_flushc ; VDISCARD */
#define XTTYMODE_flush	13
    TTYMODE("weras"),		/* ltchars.t_werasc ; VWERASE */
#define XTTYMODE_weras	14
    TTYMODE("lnext"),		/* ltchars.t_lnextc ; VLNEXT */
#define XTTYMODE_lnext	15
    TTYMODE("status"),		/* VSTATUS */
#define XTTYMODE_status	16
    TTYMODE("erase2"),		/* VERASE2 */
#define XTTYMODE_erase2	17
    TTYMODE("eol2"),		/* VEOL2 */
#define XTTYMODE_eol2	18
    { NULL,	0, 0, '\0' },	/* end of data */
};

#define validTtyChar(data, n) \
	    (known_ttyChars[n].sysMode >= 0 && \
	     known_ttyChars[n].sysMode < (int) XtNumber(data.c_cc))

static const struct {
    int sysMode;
    int myMode;
    int myDefault;
} known_ttyChars[] = {
#ifdef VINTR
    { VINTR,    XTTYMODE_intr,   CINTR },
#endif
#ifdef VQUIT
    { VQUIT,    XTTYMODE_quit,   CQUIT },
#endif
#ifdef VERASE
    { VERASE,   XTTYMODE_erase,  CERASE },
#endif
#ifdef VKILL
    { VKILL,    XTTYMODE_kill,   CKILL },
#endif
#ifdef VEOF
    { VEOF,     XTTYMODE_eof,    CEOF },
#endif
#ifdef VEOL
    { VEOL,     XTTYMODE_eol,    CEOL },
#endif
#ifdef VSWTCH
    { VSWTCH,   XTTYMODE_swtch,  CNUL },
#endif
#ifdef VSTART
    { VSTART,   XTTYMODE_start,  CSTART },
#endif
#ifdef VSTOP
    { VSTOP,    XTTYMODE_stop,   CSTOP },
#endif
#ifdef VSUSP
    { VSUSP,    XTTYMODE_susp,   CSUSP },
#endif
#ifdef VDSUSP
    { VDSUSP,   XTTYMODE_dsusp,  CDSUSP },
#endif
#ifdef VREPRINT
    { VREPRINT, XTTYMODE_rprnt,  CREPRINT },
#endif
#ifdef VDISCARD
    { VDISCARD, XTTYMODE_flush,  CDISCARD },
#endif
#ifdef VWERASE
    { VWERASE,  XTTYMODE_weras,  CWERASE },
#endif
#ifdef VLNEXT
    { VLNEXT,   XTTYMODE_lnext,  CLNEXT },
#endif
#ifdef VSTATUS
    { VSTATUS,  XTTYMODE_status, CSTATUS },
#endif
#ifdef VERASE2
    { VERASE2,  XTTYMODE_erase2, CERASE2 },
#endif
#ifdef VEOL2
    { VEOL2,    XTTYMODE_eol2,   CNUL },
#endif
};
/* *INDENT-ON* */

#define TMODE(ind,var) if (ttymodelist[ind].set) var = ttymodelist[ind].value

static int parse_tty_modes(char *s, struct _xttymodes *modelist);

#ifndef USE_UTEMPTER
#ifdef USE_SYSV_UTMP
#if (defined(AIXV3) && (OSMAJORVERSION < 4)) && !(defined(getutid))
extern struct utmp *getutid();
#endif /* AIXV3 */

#else /* not USE_SYSV_UTMP */
static char etc_utmp[] = UTMP_FILENAME;
#endif /* USE_SYSV_UTMP */

#if defined(USE_LASTLOG) && defined(USE_STRUCT_LASTLOG)
static char etc_lastlog[] = LASTLOG_FILENAME;
#else
#undef USE_LASTLOG
#endif

#ifdef WTMP
static char etc_wtmp[] = WTMP_FILENAME;
#endif
#endif /* !USE_UTEMPTER */

/*
 * Some people with 4.3bsd /bin/login seem to like to use login -p -f user
 * to implement xterm -ls.  They can turn on USE_LOGIN_DASH_P and turn off
 * WTMP and USE_LASTLOG.
 */
#ifdef USE_LOGIN_DASH_P
#ifndef LOGIN_FILENAME
#define LOGIN_FILENAME "/bin/login"
#endif
static char bin_login[] = LOGIN_FILENAME;
#endif

static char passedPty[PTYCHARLEN + 1];	/* name if pty if slave */

#if defined(TIOCCONS) || defined(SRIOCSREDIR)
static int Console;
#include <X11/Xmu/SysUtil.h>	/* XmuGetHostname */
#define MIT_CONSOLE_LEN	12
#define MIT_CONSOLE "MIT_CONSOLE_"
static char mit_console_name[255 + MIT_CONSOLE_LEN + 1] = MIT_CONSOLE;
static Atom mit_console;
#endif /* TIOCCONS */

#ifndef USE_SYSV_UTMP
static int tslot;
#endif /* USE_SYSV_UTMP */
static sigjmp_buf env;

#define SetUtmpHost(dst, screen) \
	{ \
	    char host[sizeof(dst) + 1]; \
	    strncpy(host, DisplayString(screen->display), sizeof(host)); \
	    TRACE(("DisplayString(%s)\n", host)); \
	    if (!resource.utmpDisplayId) { \
		char *endptr = strrchr(host, ':'); \
		if (endptr) { \
		    TRACE(("trimming display-id '%s'\n", host)); \
		    *endptr = '\0'; \
		} \
	    } \
	    strncpy(dst, host, sizeof(dst)); \
	}

#ifdef HAVE_UTMP_UT_SYSLEN
#  define SetUtmpSysLen(utmp) 			   \
	{ \
	    utmp.ut_host[sizeof(utmp.ut_host)-1] = '\0'; \
	    utmp.ut_syslen = strlen(utmp.ut_host) + 1; \
	}
#endif

/* used by VT (charproc.c) */

static XtResource application_resources[] =
{
    Sres("name", "Name", xterm_name, DFT_TERMTYPE),
    Sres("iconGeometry", "IconGeometry", icon_geometry, NULL),
    Sres(XtNtitle, XtCTitle, title, NULL),
    Sres(XtNiconName, XtCIconName, icon_name, NULL),
    Sres("termName", "TermName", term_name, NULL),
    Sres("ttyModes", "TtyModes", tty_modes, NULL),
    Bres("hold", "Hold", hold_screen, False),
    Bres("utmpInhibit", "UtmpInhibit", utmpInhibit, False),
    Bres("utmpDisplayId", "UtmpDisplayId", utmpDisplayId, True),
    Bres("messages", "Messages", messages, True),
    Ires("minBufSize", "MinBufSize", minBufSize, 4096),
    Ires("maxBufSize", "MaxBufSize", maxBufSize, 32768),
    Sres("menuLocale", "MenuLocale", menuLocale, ""),
    Sres("keyboardType", "KeyboardType", keyboardType, "unknown"),
#if OPT_SUNPC_KBD
    Bres("sunKeyboard", "SunKeyboard", sunKeyboard, False),
#endif
#if OPT_HP_FUNC_KEYS
    Bres("hpFunctionKeys", "HpFunctionKeys", hpFunctionKeys, False),
#endif
#if OPT_SCO_FUNC_KEYS
    Bres("scoFunctionKeys", "ScoFunctionKeys", scoFunctionKeys, False),
#endif
#if OPT_SUN_FUNC_KEYS
    Bres("sunFunctionKeys", "SunFunctionKeys", sunFunctionKeys, False),
#endif
#if OPT_TCAP_FKEYS
    Bres("tcapFunctionKeys", "TcapFunctionKeys", termcapKeys, False),
#endif
#if OPT_INITIAL_ERASE
    Bres("ptyInitialErase", "PtyInitialErase", ptyInitialErase, DEF_INITIAL_ERASE),
    Bres("backarrowKeyIsErase", "BackarrowKeyIsErase", backarrow_is_erase, DEF_BACKARO_ERASE),
#endif
    Bres("useInsertMode", "UseInsertMode", useInsertMode, False),
#if OPT_ZICONBEEP
    Ires("zIconBeep", "ZIconBeep", zIconBeep, 0),
#endif
#if OPT_PTY_HANDSHAKE
    Bres("waitForMap", "WaitForMap", wait_for_map, False),
    Bres("ptyHandshake", "PtyHandshake", ptyHandshake, True),
    Bres("ptySttySize", "PtySttySize", ptySttySize, DEF_PTY_STTY_SIZE),
#endif
#if OPT_SAME_NAME
    Bres("sameName", "SameName", sameName, True),
#endif
#if OPT_SESSION_MGT
    Bres("sessionMgt", "SessionMgt", sessionMgt, True),
#endif
#if OPT_TOOLBAR
    Bres(XtNtoolBar, XtCToolBar, toolBar, True),
#endif
};

static char *fallback_resources[] =
{
    "*SimpleMenu*menuLabel.vertSpace: 100",
    "*SimpleMenu*HorizontalMargins: 16",
    "*SimpleMenu*Sme.height: 16",
    "*SimpleMenu*Cursor: left_ptr",
    "*mainMenu.Label:  Main Options (no app-defaults)",
    "*vtMenu.Label:  VT Options (no app-defaults)",
    "*fontMenu.Label:  VT Fonts (no app-defaults)",
#if OPT_TEK4014
    "*tekMenu.Label:  Tek Options (no app-defaults)",
#endif
    NULL
};

/* Command line options table.  Only resources are entered here...there is a
   pass over the remaining options after XrmParseCommand is let loose. */
/* *INDENT-OFF* */
static XrmOptionDescRec optionDescList[] = {
{"-geometry",	"*vt100.geometry",XrmoptionSepArg,	(caddr_t) NULL},
{"-132",	"*c132",	XrmoptionNoArg,		(caddr_t) "on"},
{"+132",	"*c132",	XrmoptionNoArg,		(caddr_t) "off"},
{"-ah",		"*alwaysHighlight", XrmoptionNoArg,	(caddr_t) "on"},
{"+ah",		"*alwaysHighlight", XrmoptionNoArg,	(caddr_t) "off"},
{"-aw",		"*autoWrap",	XrmoptionNoArg,		(caddr_t) "on"},
{"+aw",		"*autoWrap",	XrmoptionNoArg,		(caddr_t) "off"},
#ifndef NO_ACTIVE_ICON
{"-ai",		"*activeIcon",	XrmoptionNoArg,		(caddr_t) "off"},
{"+ai",		"*activeIcon",	XrmoptionNoArg,		(caddr_t) "on"},
#endif /* NO_ACTIVE_ICON */
{"-b",		"*internalBorder",XrmoptionSepArg,	(caddr_t) NULL},
{"-bc",		"*cursorBlink",	XrmoptionNoArg,		(caddr_t) "on"},
{"+bc",		"*cursorBlink",	XrmoptionNoArg,		(caddr_t) "off"},
{"-bcf",	"*cursorOffTime",XrmoptionSepArg,	(caddr_t) NULL},
{"-bcn",	"*cursorOnTime",XrmoptionSepArg,	(caddr_t) NULL},
{"-bdc",	"*colorBDMode",	XrmoptionNoArg,		(caddr_t) "off"},
{"+bdc",	"*colorBDMode",	XrmoptionNoArg,		(caddr_t) "on"},
{"-cb",		"*cutToBeginningOfLine", XrmoptionNoArg, (caddr_t) "off"},
{"+cb",		"*cutToBeginningOfLine", XrmoptionNoArg, (caddr_t) "on"},
{"-cc",		"*charClass",	XrmoptionSepArg,	(caddr_t) NULL},
{"-cm",		"*colorMode",	XrmoptionNoArg,		(caddr_t) "off"},
{"+cm",		"*colorMode",	XrmoptionNoArg,		(caddr_t) "on"},
{"-cn",		"*cutNewline",	XrmoptionNoArg,		(caddr_t) "off"},
{"+cn",		"*cutNewline",	XrmoptionNoArg,		(caddr_t) "on"},
{"-cr",		"*cursorColor",	XrmoptionSepArg,	(caddr_t) NULL},
{"-cu",		"*curses",	XrmoptionNoArg,		(caddr_t) "on"},
{"+cu",		"*curses",	XrmoptionNoArg,		(caddr_t) "off"},
{"-dc",		"*dynamicColors",XrmoptionNoArg,	(caddr_t) "off"},
{"+dc",		"*dynamicColors",XrmoptionNoArg,	(caddr_t) "on"},
{"-fb",		"*boldFont",	XrmoptionSepArg,	(caddr_t) NULL},
{"-fbb",	"*freeBoldBox", XrmoptionNoArg,		(caddr_t)"off"},
{"+fbb",	"*freeBoldBox", XrmoptionNoArg,		(caddr_t)"on"},
{"-fbx",	"*forceBoxChars", XrmoptionNoArg,	(caddr_t)"off"},
{"+fbx",	"*forceBoxChars", XrmoptionNoArg,	(caddr_t)"on"},
#ifndef NO_ACTIVE_ICON
{"-fi",		"*iconFont",	XrmoptionSepArg,	(caddr_t) NULL},
#endif /* NO_ACTIVE_ICON */
#if OPT_RENDERFONT
{"-fa",		"*faceName",	XrmoptionSepArg,	(caddr_t) NULL},
{"-fd",		"*faceNameDoublesize", XrmoptionSepArg,	(caddr_t) NULL},
{"-fs",		"*faceSize",	XrmoptionSepArg,	(caddr_t) NULL},
#endif
#if OPT_WIDE_CHARS
{"-fw",		"*wideFont",	XrmoptionSepArg,	(caddr_t) NULL},
{"-fwb",	"*wideBoldFont", XrmoptionSepArg,	(caddr_t) NULL},
#endif
#if OPT_INPUT_METHOD
{"-fx",		"*ximFont",	XrmoptionSepArg,	(caddr_t) NULL},
#endif
#if OPT_HIGHLIGHT_COLOR
{"-hc",		"*highlightColor", XrmoptionSepArg,	(caddr_t) NULL},
{"-hm",		"*highlightColorMode", XrmoptionNoArg,	(caddr_t) "on"},
{"+hm",		"*highlightColorMode", XrmoptionNoArg,	(caddr_t) "off"},
{"-selfg",	"*highlightTextColor", XrmoptionSepArg,	(caddr_t) NULL},
{"-selbg",	"*highlightColor", XrmoptionSepArg,	(caddr_t) NULL},
#endif
#if OPT_HP_FUNC_KEYS
{"-hf",		"*hpFunctionKeys",XrmoptionNoArg,	(caddr_t) "on"},
{"+hf",		"*hpFunctionKeys",XrmoptionNoArg,	(caddr_t) "off"},
#endif
{"-hold",	"*hold",	XrmoptionNoArg,		(caddr_t) "on"},
{"+hold",	"*hold",	XrmoptionNoArg,		(caddr_t) "off"},
#if OPT_INITIAL_ERASE
{"-ie",		"*ptyInitialErase", XrmoptionNoArg,	(caddr_t) "on"},
{"+ie",		"*ptyInitialErase", XrmoptionNoArg,	(caddr_t) "off"},
#endif
{"-j",		"*jumpScroll",	XrmoptionNoArg,		(caddr_t) "on"},
{"+j",		"*jumpScroll",	XrmoptionNoArg,		(caddr_t) "off"},
#if OPT_C1_PRINT
{"-k8",		"*allowC1Printable", XrmoptionNoArg,	(caddr_t) "on"},
{"+k8",		"*allowC1Printable", XrmoptionNoArg,	(caddr_t) "off"},
#endif
{"-kt",		"*keyboardType", XrmoptionSepArg,	(caddr_t) NULL},
{"+kt",		"*keyboardType", XrmoptionSepArg,	(caddr_t) NULL},
/* parse logging options anyway for compatibility */
{"-l",		"*logging",	XrmoptionNoArg,		(caddr_t) "on"},
{"+l",		"*logging",	XrmoptionNoArg,		(caddr_t) "off"},
{"-lf",		"*logFile",	XrmoptionSepArg,	(caddr_t) NULL},
{"-ls",		"*loginShell",	XrmoptionNoArg,		(caddr_t) "on"},
{"+ls",		"*loginShell",	XrmoptionNoArg,		(caddr_t) "off"},
{"-mb",		"*marginBell",	XrmoptionNoArg,		(caddr_t) "on"},
{"+mb",		"*marginBell",	XrmoptionNoArg,		(caddr_t) "off"},
{"-mc",		"*multiClickTime", XrmoptionSepArg,	(caddr_t) NULL},
{"-mesg",	"*messages",	XrmoptionNoArg,		(caddr_t) "off"},
{"+mesg",	"*messages",	XrmoptionNoArg,		(caddr_t) "on"},
{"-ms",		"*pointerColor",XrmoptionSepArg,	(caddr_t) NULL},
{"-nb",		"*nMarginBell",	XrmoptionSepArg,	(caddr_t) NULL},
{"-nul",	"*underLine",	XrmoptionNoArg,		(caddr_t) "off"},
{"+nul",	"*underLine",	XrmoptionNoArg,		(caddr_t) "on"},
{"-pc",		"*boldColors",	XrmoptionNoArg,		(caddr_t) "on"},
{"+pc",		"*boldColors",	XrmoptionNoArg,		(caddr_t) "off"},
{"-rw",		"*reverseWrap",	XrmoptionNoArg,		(caddr_t) "on"},
{"+rw",		"*reverseWrap",	XrmoptionNoArg,		(caddr_t) "off"},
{"-s",		"*multiScroll",	XrmoptionNoArg,		(caddr_t) "on"},
{"+s",		"*multiScroll",	XrmoptionNoArg,		(caddr_t) "off"},
{"-sb",		"*scrollBar",	XrmoptionNoArg,		(caddr_t) "on"},
{"+sb",		"*scrollBar",	XrmoptionNoArg,		(caddr_t) "off"},
#ifdef SCROLLBAR_RIGHT
{"-leftbar",	"*rightScrollBar", XrmoptionNoArg,	(caddr_t) "off"},
{"-rightbar",	"*rightScrollBar", XrmoptionNoArg,	(caddr_t) "on"},
#endif
{"-rvc",	"*colorRVMode",	XrmoptionNoArg,		(caddr_t) "off"},
{"+rvc",	"*colorRVMode",	XrmoptionNoArg,		(caddr_t) "on"},
{"-sf",		"*sunFunctionKeys", XrmoptionNoArg,	(caddr_t) "on"},
{"+sf",		"*sunFunctionKeys", XrmoptionNoArg,	(caddr_t) "off"},
{"-si",		"*scrollTtyOutput", XrmoptionNoArg,	(caddr_t) "off"},
{"+si",		"*scrollTtyOutput", XrmoptionNoArg,	(caddr_t) "on"},
{"-sk",		"*scrollKey",	XrmoptionNoArg,		(caddr_t) "on"},
{"+sk",		"*scrollKey",	XrmoptionNoArg,		(caddr_t) "off"},
{"-sl",		"*saveLines",	XrmoptionSepArg,	(caddr_t) NULL},
#if OPT_SUNPC_KBD
{"-sp",		"*sunKeyboard", XrmoptionNoArg,		(caddr_t) "on"},
{"+sp",		"*sunKeyboard", XrmoptionNoArg,		(caddr_t) "off"},
#endif
#if OPT_TEK4014
{"-t",		"*tekStartup",	XrmoptionNoArg,		(caddr_t) "on"},
{"+t",		"*tekStartup",	XrmoptionNoArg,		(caddr_t) "off"},
#endif
{"-ti",		"*decTerminalID",XrmoptionSepArg,	(caddr_t) NULL},
{"-tm",		"*ttyModes",	XrmoptionSepArg,	(caddr_t) NULL},
{"-tn",		"*termName",	XrmoptionSepArg,	(caddr_t) NULL},
#if OPT_WIDE_CHARS
{"-u8",		"*utf8",	XrmoptionNoArg,		(caddr_t) "2"},
{"+u8",		"*utf8",	XrmoptionNoArg,		(caddr_t) "0"},
#endif
#if OPT_LUIT_PROG
{"-lc",		"*locale",	XrmoptionNoArg,		(caddr_t) "on"},
{"+lc",		"*locale",	XrmoptionNoArg,		(caddr_t) "off"},
{"-lcc",	"*localeFilter",XrmoptionSepArg,	(caddr_t) NULL},
{"-en",		"*locale",	XrmoptionSepArg,	(caddr_t) NULL},
#endif
{"-ulc",	"*colorULMode",	XrmoptionNoArg,		(caddr_t) "off"},
{"+ulc",	"*colorULMode",	XrmoptionNoArg,		(caddr_t) "on"},
{"-ulit",       "*italicULMode", XrmoptionNoArg,        (caddr_t) "off"},
{"+ulit",       "*italicULMode", XrmoptionNoArg,        (caddr_t) "on"},
{"-ut",		"*utmpInhibit",	XrmoptionNoArg,		(caddr_t) "on"},
{"+ut",		"*utmpInhibit",	XrmoptionNoArg,		(caddr_t) "off"},
{"-im",		"*useInsertMode", XrmoptionNoArg,	(caddr_t) "on"},
{"+im",		"*useInsertMode", XrmoptionNoArg,	(caddr_t) "off"},
{"-vb",		"*visualBell",	XrmoptionNoArg,		(caddr_t) "on"},
{"+vb",		"*visualBell",	XrmoptionNoArg,		(caddr_t) "off"},
{"-pob",	"*popOnBell",	XrmoptionNoArg,		(caddr_t) "on"},
{"+pob",	"*popOnBell",	XrmoptionNoArg,		(caddr_t) "off"},
#if OPT_WIDE_CHARS
{"-wc",		"*wideChars",	XrmoptionNoArg,		(caddr_t) "on"},
{"+wc",		"*wideChars",	XrmoptionNoArg,		(caddr_t) "off"},
{"-mk_width",	"*mkWidth",	XrmoptionNoArg,		(caddr_t) "on"},
{"+mk_width",	"*mkWidth",	XrmoptionNoArg,		(caddr_t) "off"},
{"-cjk_width",	"*cjkWidth",	XrmoptionNoArg,		(caddr_t) "on"},
{"+cjk_width",	"*cjkWidth",	XrmoptionNoArg,		(caddr_t) "off"},
#endif
{"-wf",		"*waitForMap",	XrmoptionNoArg,		(caddr_t) "on"},
{"+wf",		"*waitForMap",	XrmoptionNoArg,		(caddr_t) "off"},
#if OPT_ZICONBEEP
{"-ziconbeep",	"*zIconBeep",	XrmoptionSepArg,	(caddr_t) NULL},
#endif
#if OPT_SAME_NAME
{"-samename",	"*sameName",	XrmoptionNoArg,		(caddr_t) "on"},
{"+samename",	"*sameName",	XrmoptionNoArg,		(caddr_t) "off"},
#endif
#if OPT_SESSION_MGT
{"-sm",		"*sessionMgt",	XrmoptionNoArg,		(caddr_t) "on"},
{"+sm",		"*sessionMgt",	XrmoptionNoArg,		(caddr_t) "off"},
#endif
#if OPT_TOOLBAR
{"-tb",		"*"XtNtoolBar,	XrmoptionNoArg,		(caddr_t) "on"},
{"+tb",		"*"XtNtoolBar,	XrmoptionNoArg,		(caddr_t) "off"},
#endif
/* options that we process ourselves */
{"-help",	NULL,		XrmoptionSkipNArgs,	(caddr_t) NULL},
{"-version",	NULL,		XrmoptionSkipNArgs,	(caddr_t) NULL},
{"-class",	NULL,		XrmoptionSkipArg,	(caddr_t) NULL},
{"-e",		NULL,		XrmoptionSkipLine,	(caddr_t) NULL},
{"-into",	NULL,		XrmoptionSkipArg,	(caddr_t) NULL},
/* bogus old compatibility stuff for which there are
   standard XtOpenApplication options now */
{"%",		"*tekGeometry",	XrmoptionStickyArg,	(caddr_t) NULL},
{"#",		".iconGeometry",XrmoptionStickyArg,	(caddr_t) NULL},
{"-T",		".title",	XrmoptionSepArg,	(caddr_t) NULL},
{"-n",		"*iconName",	XrmoptionSepArg,	(caddr_t) NULL},
{"-r",		"*reverseVideo",XrmoptionNoArg,		(caddr_t) "on"},
{"+r",		"*reverseVideo",XrmoptionNoArg,		(caddr_t) "off"},
{"-rv",		"*reverseVideo",XrmoptionNoArg,		(caddr_t) "on"},
{"+rv",		"*reverseVideo",XrmoptionNoArg,		(caddr_t) "off"},
{"-w",		".borderWidth", XrmoptionSepArg,	(caddr_t) NULL},
};

static OptionHelp xtermOptions[] = {
{ "-version",              "print the version number" },
{ "-help",                 "print out this message" },
{ "-display displayname",  "X server to contact" },
{ "-geometry geom",        "size (in characters) and position" },
{ "-/+rv",                 "turn on/off reverse video" },
{ "-bg color",             "background color" },
{ "-fg color",             "foreground color" },
{ "-bd color",             "border color" },
{ "-bw number",            "border width in pixels" },
{ "-fn fontname",          "normal text font" },
{ "-fb fontname",          "bold text font" },
{ "-/+fbb",                "turn on/off normal/bold font comparison inhibit"},
{ "-/+fbx",                "turn off/on linedrawing characters"},
#if OPT_RENDERFONT
{ "-fa pattern",           "FreeType font-selection pattern" },
{ "-fd pattern",           "FreeType Doublesize font-selection pattern" },
{ "-fs size",              "FreeType font-size" },
#endif
#if OPT_WIDE_CHARS
{ "-fw fontname",          "doublewidth text font" },
{ "-fwb fontname",         "doublewidth bold text font" },
#endif
#if OPT_INPUT_METHOD
{ "-fx fontname",          "XIM fontset" },
#endif
{ "-iconic",               "start iconic" },
{ "-name string",          "client instance, icon, and title strings" },
{ "-class string",         "class string (XTerm)" },
{ "-title string",         "title string" },
{ "-xrm resourcestring",   "additional resource specifications" },
{ "-/+132",                "turn on/off 80/132 column switching" },
{ "-/+ah",                 "turn on/off always highlight" },
#ifndef NO_ACTIVE_ICON
{ "-/+ai",                 "turn off/on active icon" },
{ "-fi fontname",          "icon font for active icon" },
#endif /* NO_ACTIVE_ICON */
{ "-b number",             "internal border in pixels" },
{ "-/+bc",                 "turn on/off text cursor blinking" },
{ "-bcf milliseconds",     "time text cursor is off when blinking"},
{ "-bcn milliseconds",     "time text cursor is on when blinking"},
{ "-/+bdc",                "turn off/on display of bold as color"},
{ "-/+cb",                 "turn on/off cut-to-beginning-of-line inhibit" },
{ "-cc classrange",        "specify additional character classes" },
{ "-/+cm",                 "turn off/on ANSI color mode" },
{ "-/+cn",                 "turn on/off cut newline inhibit" },
{ "-cr color",             "text cursor color" },
{ "-/+cu",                 "turn on/off curses emulation" },
{ "-/+dc",                 "turn off/on dynamic color selection" },
#if OPT_HIGHLIGHT_COLOR
{ "-/+hm",                 "turn on/off selection-color override" },
{ "-selbg color",          "selection background color" },
{ "-selfg color",          "selection foreground color" },
#endif
#if OPT_HP_FUNC_KEYS
{ "-/+hf",                 "turn on/off HP Function Key escape codes" },
#endif
{ "-/+hold",               "turn on/off logic that retains window after exit" },
#if OPT_INITIAL_ERASE
{ "-/+ie",                 "turn on/off initialization of 'erase' from pty" },
#endif
{ "-/+im",                 "use insert mode for TERMCAP" },
{ "-/+j",                  "turn on/off jump scroll" },
#if OPT_C1_PRINT
{ "-/+k8",                 "turn on/off C1-printable classification"},
#endif
{ "-kt keyboardtype",      "set keyboard type:" KEYBOARD_TYPES },
#ifdef ALLOWLOGGING
{ "-/+l",                  "turn on/off logging" },
{ "-lf filename",          "logging filename" },
#else
{ "-/+l",                  "turn on/off logging (not supported)" },
{ "-lf filename",          "logging filename (not supported)" },
#endif
{ "-/+ls",                 "turn on/off login shell" },
{ "-/+mb",                 "turn on/off margin bell" },
{ "-mc milliseconds",      "multiclick time in milliseconds" },
{ "-/+mesg",               "forbid/allow messages" },
{ "-ms color",             "pointer color" },
{ "-nb number",            "margin bell in characters from right end" },
{ "-/+nul",                "turn off/on display of underlining" },
{ "-/+aw",                 "turn on/off auto wraparound" },
{ "-/+pc",                 "turn on/off PC-style bold colors" },
{ "-/+rw",                 "turn on/off reverse wraparound" },
{ "-/+s",                  "turn on/off multiscroll" },
{ "-/+sb",                 "turn on/off scrollbar" },
#ifdef SCROLLBAR_RIGHT
{ "-rightbar",             "force scrollbar right (default left)" },
{ "-leftbar",              "force scrollbar left" },
#endif
{ "-/+rvc",                "turn off/on display of reverse as color" },
{ "-/+sf",                 "turn on/off Sun Function Key escape codes" },
{ "-/+si",                 "turn on/off scroll-on-tty-output inhibit" },
{ "-/+sk",                 "turn on/off scroll-on-keypress" },
{ "-sl number",            "number of scrolled lines to save" },
#if OPT_SUNPC_KBD
{ "-/+sp",                 "turn on/off Sun/PC Function/Keypad mapping" },
#endif
#if OPT_TEK4014
{ "-/+t",                  "turn on/off Tek emulation window" },
#endif
#if OPT_TOOLBAR
{ "-/+tb",                 "turn on/off toolbar" },
#endif
{ "-ti termid",            "terminal identifier" },
{ "-tm string",            "terminal mode keywords and characters" },
{ "-tn name",              "TERM environment variable name" },
#if OPT_WIDE_CHARS
{ "-/+u8",                 "turn on/off UTF-8 mode (implies wide-characters)" },
#endif
#if OPT_LUIT_PROG
{ "-/+lc",                 "turn on/off locale mode using luit" },
{ "-lcc path",             "filename of locale converter (" DEFLOCALEFILTER ")" },
#endif
{ "-/+ulc",                "turn off/on display of underline as color" },
{ "-/+ulit",               "turn off/on display of underline as italics" },
#ifdef HAVE_UTMP
{ "-/+ut",                 "turn on/off utmp support" },
#else
{ "-/+ut",                 "turn on/off utmp support (not available)" },
#endif
{ "-/+vb",                 "turn on/off visual bell" },
{ "-/+pob",                "turn on/off pop on bell" },
#if OPT_WIDE_CHARS
{ "-/+wc",                 "turn on/off wide-character mode" },
{ "-/+mk_width",           "turn on/off simple width convention" },
{ "-/+cjk_width",          "turn on/off legacy CJK width convention" },
#endif
{ "-/+wf",                 "turn on/off wait for map before command exec" },
{ "-e command args ...",   "command to execute" },
#if OPT_TEK4014
{ "%geom",                 "Tek window geometry" },
#endif
{ "#geom",                 "icon window geometry" },
{ "-T string",             "title name for window" },
{ "-n string",             "icon name for window" },
#if defined(TIOCCONS) || defined(SRIOCSREDIR)
{ "-C",                    "intercept console messages" },
#else
{ "-C",                    "intercept console messages (not supported)" },
#endif
{ "-Sccn",                 "slave mode on \"ttycc\", file descriptor \"n\"" },
{ "-into windowId",        "use the window id given to -into as the parent window rather than the default root window" },
#if OPT_ZICONBEEP
{ "-ziconbeep percent",    "beep and flag icon of window having hidden output" },
#endif
#if OPT_SAME_NAME
{ "-/+samename",           "turn on/off the no-flicker option for title and icon name" },
#endif
#if OPT_SESSION_MGT
{ "-/+sm",                 "turn on/off the session-management support" },
#endif
{ NULL, NULL }};
/* *INDENT-ON* */

static char *message[] =
{
    "Fonts should be fixed width and, if both normal and bold are specified, should",
    "have the same size.  If only a normal font is specified, it will be used for",
    "both normal and bold text (by doing overstriking).  The -e option, if given,",
    "must appear at the end of the command line, otherwise the user's default shell",
    "will be started.  Options that start with a plus sign (+) restore the default.",
    NULL};

/*
 * Decode a key-definition.  This combines the termcap and ttyModes, for
 * comparison.  Note that octal escapes in ttyModes are done by the normal
 * resource translation.  Also, ttyModes allows '^-' as a synonym for disabled.
 */
static int
decode_keyvalue(char **ptr, int termcap)
{
    char *string = *ptr;
    int value = -1;

    TRACE(("...decode '%s'\n", string));
    if (*string == '^') {
	switch (*++string) {
	case '?':
	    value = A2E(ANSI_DEL);
	    break;
	case '-':
	    if (!termcap) {
		errno = 0;
#if defined(_POSIX_VDISABLE) && defined(HAVE_UNISTD_H)
		value = _POSIX_VDISABLE;
#endif
#if defined(_PC_VDISABLE)
		if (value == -1) {
		    value = fpathconf(0, _PC_VDISABLE);
		    if (value == -1) {
			if (errno != 0)
			    break;	/* skip this (error) */
			value = 0377;
		    }
		}
#elif defined(VDISABLE)
		if (value == -1)
		    value = VDISABLE;
#endif
		break;
	    }
	    /* FALLTHRU */
	default:
	    value = CONTROL(*string);
	    break;
	}
	++string;
    } else if (termcap && (*string == '\\')) {
	char *d;
	int temp = strtol(string + 1, &d, 8);
	if (temp > 0 && d != string) {
	    value = temp;
	    string = d;
	}
    } else {
	value = CharOf(*string);
	++string;
    }
    *ptr = string;
    return value;
}

static int
abbrev(char *tst, char *cmp, size_t need)
{
    size_t len = strlen(tst);
    return ((len >= need) && (!strncmp(tst, cmp, len)));
}

static void
Syntax(char *badOption)
{
    OptionHelp *opt;
    OptionHelp *list = sortedOpts(xtermOptions, optionDescList, XtNumber(optionDescList));
    int col;

    fprintf(stderr, "%s:  bad command line option \"%s\"\r\n\n",
	    ProgramName, badOption);

    fprintf(stderr, "usage:  %s", ProgramName);
    col = 8 + strlen(ProgramName);
    for (opt = list; opt->opt; opt++) {
	int len = 3 + strlen(opt->opt);		/* space [ string ] */
	if (col + len > 79) {
	    fprintf(stderr, "\r\n   ");		/* 3 spaces */
	    col = 3;
	}
	fprintf(stderr, " [%s]", opt->opt);
	col += len;
    }

    fprintf(stderr, "\r\n\nType %s -help for a full description.\r\n\n",
	    ProgramName);
    exit(1);
}

static void
Version(void)
{
    printf("%s\n", xtermVersion());
    fflush(stdout);
}

static void
Help(void)
{
    OptionHelp *opt;
    OptionHelp *list = sortedOpts(xtermOptions, optionDescList, XtNumber(optionDescList));
    char **cpp;

    printf("%s usage:\n    %s [-options ...] [-e command args]\n\n",
	   xtermVersion(), ProgramName);
    printf("where options include:\n");
    for (opt = list; opt->opt; opt++) {
	printf("    %-28s %s\n", opt->opt, opt->desc);
    }

    putchar('\n');
    for (cpp = message; *cpp; cpp++)
	puts(*cpp);
    putchar('\n');
    fflush(stdout);
}

#if defined(TIOCCONS) || defined(SRIOCSREDIR)
/* ARGSUSED */
static Boolean
ConvertConsoleSelection(Widget w GCC_UNUSED,
			Atom * selection GCC_UNUSED,
			Atom * target GCC_UNUSED,
			Atom * type GCC_UNUSED,
			XtPointer *value GCC_UNUSED,
			unsigned long *length GCC_UNUSED,
			int *format GCC_UNUSED)
{
    /* we don't save console output, so can't offer it */
    return False;
}
#endif /* TIOCCONS */

#if OPT_SESSION_MGT
static void
die_callback(Widget w GCC_UNUSED,
	     XtPointer client_data GCC_UNUSED,
	     XtPointer call_data GCC_UNUSED)
{
    Cleanup(0);
}

static void
save_callback(Widget w GCC_UNUSED,
	      XtPointer client_data GCC_UNUSED,
	      XtPointer call_data)
{
    XtCheckpointToken token = (XtCheckpointToken) call_data;
    /* we have nothing to save */
    token->save_success = True;
}

static void
icewatch(IceConn iceConn,
	 IcePointer clientData GCC_UNUSED,
	 Bool opening,
	 IcePointer * watchData GCC_UNUSED)
{
    if (opening) {
	ice_fd = IceConnectionNumber(iceConn);
	TRACE(("got IceConnectionNumber %d\n", ice_fd));
    } else {
	ice_fd = -1;
	TRACE(("reset IceConnectionNumber\n"));
    }
}

#endif /* OPT_SESSION_MGT */

/*
 * DeleteWindow(): Action proc to implement ICCCM delete_window.
 */
/* ARGSUSED */
static void
DeleteWindow(Widget w,
	     XEvent * event GCC_UNUSED,
	     String * params GCC_UNUSED,
	     Cardinal *num_params GCC_UNUSED)
{
#if OPT_TEK4014
    if (w == toplevel) {
	if (TEK4014_SHOWN(term))
	    hide_vt_window();
	else
	    do_hangup(w, (XtPointer) 0, (XtPointer) 0);
    } else if (term->screen.Vshow)
	hide_tek_window();
    else
#endif
	do_hangup(w, (XtPointer) 0, (XtPointer) 0);
}

/* ARGSUSED */
static void
KeyboardMapping(Widget w GCC_UNUSED,
		XEvent * event,
		String * params GCC_UNUSED,
		Cardinal *num_params GCC_UNUSED)
{
    switch (event->type) {
    case MappingNotify:
	XRefreshKeyboardMapping(&event->xmapping);
	break;
    }
}

static XtActionsRec actionProcs[] =
{
    {"DeleteWindow", DeleteWindow},
    {"KeyboardMapping", KeyboardMapping},
};

/*
 * Some platforms use names such as /dev/tty01, others /dev/pts/1.  Parse off
 * the "tty01" or "pts/1" portion, and return that for use as an identifier for
 * utmp.
 */
static char *
my_pty_name(char *device)
{
    size_t len = strlen(device);
    Bool name = False;

    while (len != 0) {
	int ch = device[len - 1];
	if (isdigit(ch)) {
	    len--;
	} else if (ch == '/') {
	    if (name)
		break;
	    len--;
	} else if (isalpha(ch)) {
	    name = True;
	    len--;
	} else {
	    break;
	}
    }
    TRACE(("my_pty_name(%s) -> '%s'\n", device, device + len));
    return device + len;
}

/*
 * If the name contains a '/', it is a "pts/1" case.  Otherwise, return the
 * last few characters for a utmp identifier.
 */
static char *
my_pty_id(char *device)
{
    char *name = my_pty_name(device);
    char *leaf = x_basename(name);

    if (name == leaf) {		/* no '/' in the name */
	int len = strlen(leaf);
	if (PTYCHARLEN < len)
	    leaf = leaf + (len - PTYCHARLEN);
    }
    TRACE(("my_pty_id  (%s) -> '%s'\n", device, leaf));
    return leaf;
}

/*
 * Set the tty/pty identifier
 */
static void
set_pty_id(char *device, char *id)
{
    char *name = my_pty_name(device);
    char *leaf = x_basename(name);

    if (name == leaf) {
	strcpy(my_pty_id(device), id);
    } else {
	strcpy(leaf, id);
    }
    TRACE(("set_pty_id(%s) -> '%s'\n", id, device));
}

/*
 * The original -S option accepts two characters to identify the pty, and a
 * file-descriptor (assumed to be nonzero).  That is not general enough, so we
 * check first if the option contains a '/' to delimit the two fields, and if
 * not, fall-thru to the original logic.
 */
static Bool
ParseSccn(char *option)
{
    char *leaf = x_basename(option);
    Bool code = False;

    if (leaf != option) {
	if (leaf - option > 0
	    && isdigit(CharOf(*leaf))
	    && sscanf(leaf, "%d", &am_slave) == 1) {
	    size_t len = leaf - option - 1;
	    /*
	     * If we have a slash, we only care about the part after the slash,
	     * which is a file-descriptor.  The part before the slash can be
	     * the /dev/pts/XXX value, but since we do not need to reopen it,
	     * it is useful mainly for display in a "ps -ef".
	     */
	    strncpy(passedPty, option, len);
	    passedPty[len] = 0;
	    code = True;
	}
    } else {
	code = (sscanf(option, "%c%c%d",
		       passedPty, passedPty + 1, &am_slave) == 3);
    }
    TRACE(("ParseSccn(%s) = '%s' %d (%s)\n", option,
	   passedPty, am_slave, code ? "OK" : "ERR"));
    return code;
}

#if defined(USE_SYSV_UTMP) && !defined(USE_UTEMPTER)
/*
 * From "man utmp":
 * xterm and other terminal emulators directly create a USER_PROCESS record
 * and generate the ut_id by using the last two letters of /dev/ttyp%c or by
 * using p%d for /dev/pts/%d.  If they find a DEAD_PROCESS for this id, they
 * recycle it, otherwise they create a new entry.  If they can, they will mark
 * it as DEAD_PROCESS on exiting and it is advised that they null ut_line,
 * ut_time, ut_user and ut_host as well.
 *
 * Generally ut_id allows no more than 3 characters (plus null), even if the
 * pty implementation allows more than 3 digits.
 */
static char *
my_utmp_id(char *device)
{
    typedef struct UTMP_STR UTMP_STRUCT;
#define	UTIDSIZE	(sizeof(((UTMP_STRUCT *)NULL)->ut_id))
    static char result[UTIDSIZE + 1];

#if defined(__SCO__) || defined(__UNIXWARE__)
    /*
     * Legend does not support old-style pty's, has no related compatibility
     * issues, and can use the available space in ut_id differently from the
     * default convention.
     *
     * This scheme is intended to avoid conflicts both with other users of
     * utmpx as well as between multiple xterms.  First, Legend uses all of the
     * characters of ut_id, and adds no terminating NUL is required (the
     * default scheme may add a trailing NUL).  Second, all xterm entries will
     * start with the letter 'x' followed by three digits, which will be the
     * last three digits of the device name, regardless of the format of the
     * device name, with leading 0's added where necessary.  For instance, an
     * xterm on /dev/pts/3 will have a ut_id of x003; an xterm on /dev/pts123
     * will have a ut_id of x123.  Under the other convention, /dev/pts/3 would
     * have a ut_id of p3 and /dev/pts123 would have a ut_id of p123.
     */
    int len, n;

    len = strlen(device);
    n = UTIDSIZE;
    result[n] = '\0';
    while ((n > 0) && (len > 0) && isdigit(device[len - 1]))
	result[--n] = device[--len];
    while (n > 0)
	result[--n] = '0';
    result[0] = 'x';
#else
    char *name = my_pty_name(device);
    char *leaf = x_basename(name);
    size_t len = strlen(leaf);

    if ((UTIDSIZE - 1) < len)
	leaf = leaf + (len - (UTIDSIZE - 1));
    sprintf(result, "p%s", leaf);
#endif

    TRACE(("my_utmp_id (%s) -> '%s'\n", device, result));
    return result;
}
#endif /* USE_SYSV_UTMP */

#ifdef USE_POSIX_SIGNALS

typedef void (*sigfunc) (int);

/* make sure we sure we ignore SIGCHLD for the cases parent
   has just been stopped and not actually killed */

static sigfunc
posix_signal(int signo, sigfunc func)
{
    struct sigaction act, oact;

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
#ifdef SA_RESTART
    act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
#else
    act.sa_flags = SA_NOCLDSTOP;
#endif
    if (sigaction(signo, &act, &oact) < 0)
	return (SIG_ERR);
    return (oact.sa_handler);
}

#endif /* linux && _POSIX_SOURCE */

#if defined(DISABLE_SETUID) || defined(USE_UTMP_SETGID)
static void
disableSetUid(void)
{
    TRACE(("process %d disableSetUid\n", (int) getpid()));
    if (setuid(save_ruid) == -1) {
	fprintf(stderr, "%s: unable to reset uid\n", ProgramName);
	exit(1);
    }
    TRACE_IDS;
}
#else
#define disableSetUid()		/* nothing */
#endif /* DISABLE_SETUID */

#if defined(DISABLE_SETGID) || defined(USE_UTMP_SETGID)
static void
disableSetGid(void)
{
    TRACE(("process %d disableSetGid\n", (int) getpid()));
    if (setegid(save_rgid) == -1) {
	fprintf(stderr, "%s: unable to reset effective gid\n", ProgramName);
	exit(1);
    }
    TRACE_IDS;
}
#else
#define disableSetGid()		/* nothing */
#endif /* DISABLE_SETGID */

#if defined(HAVE_POSIX_SAVED_IDS)
#if (!defined(USE_UTEMPTER) || !defined(DISABLE_SETGID))
static void
setEffectiveGroup(gid_t group)
{
    TRACE(("process %d setEffectiveGroup(%d)\n", (int) getpid(), (int) group));
    if (setegid(group) == -1) {
#ifdef __MVS__
	if (!(errno == EMVSERR))	/* could happen if _BPX_SHAREAS=REUSE */
#endif
	{
	    (void) fprintf(stderr, "setegid(%d): %s\n",
			   (int) group, strerror(errno));
	}
    }
    TRACE_IDS;
}
#endif

#if !defined(USE_UTMP_SETGID) && (!defined(USE_UTEMPTER) || !defined(DISABLE_SETUID))
static void
setEffectiveUser(uid_t user)
{
    TRACE(("process %d setEffectiveUser(%d)\n", (int) getpid(), (int) user));
    if (seteuid(user) == -1) {
#ifdef __MVS__
	if (!(errno == EMVSERR))
#endif
	{
	    (void) fprintf(stderr, "seteuid(%d): %s\n",
			   (int) user, strerror(errno));
	}
    }
    TRACE_IDS;
}
#endif
#endif /* HAVE_POSIX_SAVED_IDS */

int
main(int argc, char *argv[]ENVP_ARG)
{
    Widget form_top, menu_top;
    Dimension menu_high;
    TScreen *screen;
    int mode;
    char *my_class = DEFCLASS;
    Window winToEmbedInto = None;
#if OPT_COLOR_RES
    Bool reversed = False;
#endif

    ProgramName = argv[0];

#ifdef HAVE_POSIX_SAVED_IDS
    save_euid = geteuid();
    save_egid = getegid();
#endif

    save_ruid = getuid();
    save_rgid = getgid();

#if defined(DISABLE_SETUID) || defined(DISABLE_SETGID)
#if defined(DISABLE_SETUID)
    disableSetUid();
#endif
#if defined(DISABLE_SETGID)
    disableSetGid();
#endif
    TRACE_IDS;
#endif

    /* extra length in case longer tty name like /dev/ttyq255 */
    ttydev = TypeMallocN(char, sizeof(TTYDEV) + 80);
#ifdef USE_PTY_DEVICE
    ptydev = TypeMallocN(char, sizeof(PTYDEV) + 80);
    if (!ttydev || !ptydev)
#else
    if (!ttydev)
#endif
    {
	fprintf(stderr,
		"%s: unable to allocate memory for ttydev or ptydev\n",
		ProgramName);
	exit(1);
    }
    strcpy(ttydev, TTYDEV);
#ifdef USE_PTY_DEVICE
    strcpy(ptydev, PTYDEV);
#endif

#if defined(USE_UTMP_SETGID)
    get_pty(NULL, NULL);
    disableSetUid();
    disableSetGid();
    TRACE_IDS;
#define get_pty(pty, from) really_get_pty(pty, from)
#endif

    /* Do these first, since we may not be able to open the display */
    TRACE_OPTS(xtermOptions, optionDescList, XtNumber(optionDescList));
    TRACE_ARGV("Before XtOpenApplication", argv);
    if (argc > 1) {
	int n;
	unsigned unique = 2;
	Bool quit = True;

	for (n = 1; n < argc; n++) {
	    TRACE(("parsing %s\n", argv[n]));
	    if (abbrev(argv[n], "-version", unique)) {
		Version();
	    } else if (abbrev(argv[n], "-help", unique)) {
		Help();
	    } else if (abbrev(argv[n], "-class", 3)) {
		if ((my_class = argv[++n]) == 0) {
		    Help();
		} else {
		    quit = False;
		}
		unique = 3;
	    } else {
#if OPT_COLOR_RES
		if (abbrev(argv[n], "-reverse", 2)
		    || !strcmp("-rv", argv[n])) {
		    reversed = True;
		} else if (!strcmp("+rv", argv[n])) {
		    reversed = False;
		}
#endif
		quit = False;
		unique = 3;
	    }
	}
	if (quit)
	    exit(0);
    }

    /* This dumps core on HP-UX 9.05 with X11R5 */
#if OPT_I18N_SUPPORT
    XtSetLanguageProc(NULL, NULL, NULL);
#endif

#ifdef TERMIO_STRUCT		/* { */
    /* Initialization is done here rather than above in order
     * to prevent any assumptions about the order of the contents
     * of the various terminal structures (which may change from
     * implementation to implementation).
     */
    memset(&d_tio, 0, sizeof(d_tio));
    d_tio.c_iflag = ICRNL | IXON;
#ifdef TAB3
    d_tio.c_oflag = OPOST | ONLCR | TAB3;
#else
#ifdef ONLCR
    d_tio.c_oflag = OPOST | ONLCR;
#else
    d_tio.c_oflag = OPOST;
#endif
#endif
    {
	Cardinal nn;

	/* fill in default-values */
	for (nn = 0; nn < XtNumber(known_ttyChars); ++nn) {
	    if (validTtyChar(d_tio, nn)) {
		d_tio.c_cc[known_ttyChars[nn].sysMode] =
		    known_ttyChars[nn].myDefault;
	    }
	}
    }
#if defined(macII) || defined(ATT) || defined(CRAY)	/* { */
    d_tio.c_cflag = VAL_LINE_SPEED | CS8 | CREAD | PARENB | HUPCL;
    d_tio.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
#ifdef ECHOKE
    d_tio.c_lflag |= ECHOKE | IEXTEN;
#endif
#ifdef ECHOCTL
    d_tio.c_lflag |= ECHOCTL | IEXTEN;
#endif
#ifndef USE_TERMIOS		/* { */
    d_tio.c_line = 0;
#endif /* } */
#ifdef HAS_LTCHARS		/* { */
    d_ltc.t_suspc = CSUSP;	/* t_suspc */
    d_ltc.t_dsuspc = CDSUSP;	/* t_dsuspc */
    d_ltc.t_rprntc = CRPRNT;
    d_ltc.t_flushc = CFLUSH;
    d_ltc.t_werasc = CWERASE;
    d_ltc.t_lnextc = CLNEXT;
#endif /* } HAS_LTCHARS */
#ifdef TIOCLSET			/* { */
    d_lmode = 0;
#endif /* } TIOCLSET */
#else /* }{ else !macII, ATT, CRAY */
#ifndef USE_POSIX_TERMIOS
#ifdef BAUD_0			/* { */
    d_tio.c_cflag = CS8 | CREAD | PARENB | HUPCL;
#else /* }{ !BAUD_0 */
    d_tio.c_cflag = VAL_LINE_SPEED | CS8 | CREAD | PARENB | HUPCL;
#endif /* } !BAUD_0 */
#else /* USE_POSIX_TERMIOS */
    d_tio.c_cflag = CS8 | CREAD | PARENB | HUPCL;
    cfsetispeed(&d_tio, VAL_LINE_SPEED);
    cfsetospeed(&d_tio, VAL_LINE_SPEED);
#endif
    d_tio.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
#ifdef ECHOKE
    d_tio.c_lflag |= ECHOKE | IEXTEN;
#endif
#ifdef ECHOCTL
    d_tio.c_lflag |= ECHOCTL | IEXTEN;
#endif
#ifndef USE_POSIX_TERMIOS
#ifdef NTTYDISC
    d_tio.c_line = NTTYDISC;
#else
    d_tio.c_line = 0;
#endif
#endif /* USE_POSIX_TERMIOS */
#ifdef __sgi
    d_tio.c_cflag &= ~(HUPCL | PARENB);
    d_tio.c_iflag |= BRKINT | ISTRIP | IGNPAR;
#endif
#ifdef __MVS__
    d_tio.c_cflag &= ~(HUPCL | PARENB);
#endif
    {
	Cardinal nn;
	int i;

	/* try to inherit tty settings */
	for (i = 0; i <= 2; i++) {
	    TERMIO_STRUCT deftio;
	    if (ttyGetAttr(i, &deftio) == 0) {
		for (nn = 0; nn < XtNumber(known_ttyChars); ++nn) {
		    if (validTtyChar(d_tio, nn)) {
			d_tio.c_cc[known_ttyChars[nn].sysMode] =
			    deftio.c_cc[known_ttyChars[nn].sysMode];
		    }
		}
		break;
	    }
	}
    }
#if defined(USE_TERMIOS) || defined(USE_POSIX_TERMIOS)	/* { */
    d_tio.c_cc[VMIN] = 1;
    d_tio.c_cc[VTIME] = 0;
#endif /* } */
#ifdef HAS_LTCHARS		/* { */
    d_ltc.t_suspc = CharOf('\000');	/* t_suspc */
    d_ltc.t_dsuspc = CharOf('\000');	/* t_dsuspc */
    d_ltc.t_rprntc = CharOf('\377');	/* reserved... */
    d_ltc.t_flushc = CharOf('\377');
    d_ltc.t_werasc = CharOf('\377');
    d_ltc.t_lnextc = CharOf('\377');
#endif /* } HAS_LTCHARS */

#ifdef TIOCLSET			/* { */
    d_lmode = 0;
#endif /* } TIOCLSET */
#endif /* } macII, ATT, CRAY */
#endif /* } TERMIO_STRUCT */

    /* Init the Toolkit. */
    {
#if defined(HAVE_POSIX_SAVED_IDS) && !defined(USE_UTMP_SETGID) && !defined(USE_UTEMPTER)
	setEffectiveGroup(save_rgid);
	setEffectiveUser(save_ruid);
	TRACE_IDS;
#endif

	XtSetErrorHandler(xt_error);
#if OPT_SESSION_MGT
	toplevel = XtOpenApplication(&app_con, my_class,
				     optionDescList,
				     XtNumber(optionDescList),
				     &argc, argv, fallback_resources,
				     sessionShellWidgetClass,
				     NULL, 0);
	IceAddConnectionWatch(icewatch, NULL);
#else
	toplevel = XtAppInitialize(&app_con, my_class,
				   optionDescList,
				   XtNumber(optionDescList),
				   &argc, argv, fallback_resources,
				   NULL, 0);
#endif /* OPT_SESSION_MGT */
	XtSetErrorHandler((XtErrorHandler) 0);

	XtGetApplicationResources(toplevel, (XtPointer) &resource,
				  application_resources,
				  XtNumber(application_resources), NULL, 0);
	TRACE_XRES();
#if OPT_PTY_HANDSHAKE
	resource.wait_for_map0 = resource.wait_for_map;
#endif

#if defined(HAVE_POSIX_SAVED_IDS) && !defined(USE_UTMP_SETGID)
#if !defined(DISABLE_SETUID) || !defined(DISABLE_SETGID)
#if !defined(DISABLE_SETUID)
	setEffectiveUser(save_euid);
#endif
#if !defined(DISABLE_SETGID)
	setEffectiveGroup(save_egid);
#endif
	TRACE_IDS;
#endif
#endif
    }

    /*
     * ICCCM delete_window.
     */
    XtAppAddActions(app_con, actionProcs, XtNumber(actionProcs));

    /*
     * fill in terminal modes
     */
    if (resource.tty_modes) {
	int n = parse_tty_modes(resource.tty_modes, ttymodelist);
	if (n < 0) {
	    fprintf(stderr, "%s:  bad tty modes \"%s\"\n",
		    ProgramName, resource.tty_modes);
	} else if (n > 0) {
	    override_tty_modes = True;
	}
    }
#if OPT_ZICONBEEP
    if (resource.zIconBeep > 100 || resource.zIconBeep < -100) {
	resource.zIconBeep = 0;	/* was 100, but I prefer to defaulting off. */
	fprintf(stderr,
		"a number between -100 and 100 is required for zIconBeep.  0 used by default\n");
    }
#endif /* OPT_ZICONBEEP */
    hold_screen = resource.hold_screen ? 1 : 0;
    xterm_name = resource.xterm_name;
    if (strcmp(xterm_name, "-") == 0)
	xterm_name = DFT_TERMTYPE;
    if (resource.icon_geometry != NULL) {
	int scr, junk;
	int ix, iy;
	Arg args[2];

	for (scr = 0;		/* yyuucchh */
	     XtScreen(toplevel) != ScreenOfDisplay(XtDisplay(toplevel), scr);
	     scr++) ;

	args[0].name = XtNiconX;
	args[1].name = XtNiconY;
	XGeometry(XtDisplay(toplevel), scr, resource.icon_geometry, "",
		  0, 0, 0, 0, 0, &ix, &iy, &junk, &junk);
	args[0].value = (XtArgVal) ix;
	args[1].value = (XtArgVal) iy;
	XtSetValues(toplevel, args, 2);
    }

    XtSetValues(toplevel, ourTopLevelShellArgs,
		number_ourTopLevelShellArgs);

#if OPT_WIDE_CHARS
    /* seems as good a place as any */
    init_classtab();
#endif

    /* Parse the rest of the command line */
    TRACE_ARGV("After XtOpenApplication", argv);
    for (argc--, argv++; argc > 0; argc--, argv++) {
#ifdef VMS
	if (**argv != '-')
	    Syntax(*argv);
#else
	if (**argv != '-') {
	    if (argc > 1)
		Syntax(*argv);
	    if (command_to_exec == 0)	/* if no "-e" option */
		explicit_shname = xtermFindShell(*argv, True);
	    continue;
	}
#endif

	TRACE(("parsing %s\n", argv[0]));
	switch (argv[0][1]) {
	case 'h':		/* -help */
	    Help();
	    continue;
	case 'v':		/* -version */
	    Version();
	    continue;
	case 'C':
#if defined(TIOCCONS) || defined(SRIOCSREDIR)
#ifndef __sgi
	    {
		struct stat sbuf;

		/* Must be owner and have read/write permission.
		   xdm cooperates to give the console the right user. */
		if (!stat("/dev/console", &sbuf) &&
		    (sbuf.st_uid == save_ruid) &&
		    !access("/dev/console", R_OK | W_OK)) {
		    Console = True;
		} else
		    Console = False;
	    }
#else /* __sgi */
	    Console = True;
#endif /* __sgi */
#endif /* TIOCCONS */
	    continue;
	case 'S':
	    if (!ParseSccn(*argv + 2))
		Syntax(*argv);
	    continue;
#ifdef DEBUG
	case 'D':
	    debug = True;
	    continue;
#endif /* DEBUG */
	case 'c':		/* -class param */
	    if (strcmp(argv[0] + 1, "class") == 0)
		argc--, argv++;
	    else
		Syntax(*argv);
	    continue;
	case 'e':
	    if (argc <= 1)
		Syntax(*argv);
	    command_to_exec = ++argv;
	    break;
	case 'i':
	    if (argc <= 1) {
		Syntax(*argv);
	    } else {
		char *endPtr;
		--argc;
		++argv;
		winToEmbedInto = (Window) strtol(argv[0], &endPtr, 10);
	    }
	    continue;

	default:
	    Syntax(*argv);
	}
	break;
    }

    SetupMenus(toplevel, &form_top, &menu_top, &menu_high);

    term = (XtermWidget) XtVaCreateManagedWidget("vt100", xtermWidgetClass,
						 form_top,
#if OPT_TOOLBAR
						 XtNmenuBar, menu_top,
						 XtNresizable, True,
						 XtNfromVert, menu_top,
						 XtNleft, XawChainLeft,
						 XtNright, XawChainRight,
						 XtNtop, XawChainTop,
						 XtNbottom, XawChainBottom,
						 XtNmenuHeight, menu_high,
#endif
						 (XtPointer) 0);
    decode_keyboard_type(term, &resource);

    screen = TScreenOf(term);
    screen->inhibit = 0;

#ifdef ALLOWLOGGING
    if (term->misc.logInhibit)
	screen->inhibit |= I_LOG;
#endif
    if (term->misc.signalInhibit)
	screen->inhibit |= I_SIGNAL;
#if OPT_TEK4014
    if (term->misc.tekInhibit)
	screen->inhibit |= I_TEK;
#endif

    /*
     * We might start by showing the tek4014 window.
     */
#if OPT_TEK4014
    if (screen->inhibit & I_TEK)
	TEK4014_ACTIVE(term) = False;

    if (TEK4014_ACTIVE(term) && !TekInit())
	SysError(ERROR_INIT);
#endif

    /*
     * Start the toolbar at this point, after the first window has been setup.
     */
#if OPT_TOOLBAR
    ShowToolbar(resource.toolBar);
#endif

#if OPT_SESSION_MGT
    if (resource.sessionMgt) {
	TRACE(("Enabling session-management callbacks\n"));
	XtAddCallback(toplevel, XtNdieCallback, die_callback, NULL);
	XtAddCallback(toplevel, XtNsaveCallback, save_callback, NULL);
    }
#endif

    /*
     * Set title and icon name if not specified
     */
    if (command_to_exec) {
	Arg args[2];

	if (!resource.title) {
	    if (command_to_exec) {
		resource.title = x_basename(command_to_exec[0]);
	    }			/* else not reached */
	}

	if (!resource.icon_name)
	    resource.icon_name = resource.title;
	XtSetArg(args[0], XtNtitle, resource.title);
	XtSetArg(args[1], XtNiconName, resource.icon_name);

	TRACE(("setting:\n\ttitle \"%s\"\n\ticon \"%s\"\n\tbased on command \"%s\"\n",
	       resource.title,
	       resource.icon_name,
	       *command_to_exec));

	XtSetValues(toplevel, args, 2);
    }
#if OPT_LUIT_PROG
    if (term->misc.callfilter) {
	int u = (term->misc.use_encoding ? 2 : 0);
	if (command_to_exec) {
	    int n;
	    char **c;
	    for (n = 0, c = command_to_exec; *c; n++, c++) ;
	    c = TypeMallocN(char *, n + 3 + u);
	    if (c == NULL)
		SysError(ERROR_LUMALLOC);
	    memcpy(c + 2 + u, command_to_exec, (n + 1) * sizeof(char *));
	    c[0] = term->misc.localefilter;
	    if (u) {
		c[1] = "-encoding";
		c[2] = term->misc.locale_str;
	    }
	    c[1 + u] = "--";
	    command_to_exec_with_luit = c;
	} else {
	    static char *luit[4];
	    luit[0] = term->misc.localefilter;
	    if (u) {
		luit[1] = "-encoding";
		luit[2] = term->misc.locale_str;
		luit[3] = NULL;
	    } else
		luit[1] = NULL;
	    command_to_exec_with_luit = luit;
	}
    }
#endif

#ifdef DEBUG
    {
	/* Set up stderr properly.  Opening this log file cannot be
	   done securely by a privileged xterm process (although we try),
	   so the debug feature is disabled by default. */
	char dbglogfile[45];
	int i = -1;
	if (debug) {
	    timestamp_filename(dbglogfile, "xterm.debug.log.");
	    if (creat_as(save_ruid, save_rgid, False, dbglogfile, 0666) > 0) {
		i = open(dbglogfile, O_WRONLY | O_TRUNC);
	    }
	}
	if (i >= 0) {
	    dup2(i, 2);

	    /* mark this file as close on exec */
	    (void) fcntl(i, F_SETFD, 1);
	}
    }
#endif /* DEBUG */

    spawnXTerm(term);

#ifndef VMS
    /* Child process is out there, let's catch its termination */

#ifdef USE_POSIX_SIGNALS
    (void) posix_signal(SIGCHLD, reapchild);
#else
    (void) signal(SIGCHLD, reapchild);
#endif
    /* Realize procs have now been executed */

    if (am_slave >= 0) {	/* Write window id so master end can read and use */
	char buf[80];

	buf[0] = '\0';
	sprintf(buf, "%lx\n", XtWindow(SHELL_OF(CURRENT_EMU())));
	write(screen->respond, buf, strlen(buf));
    }
#ifdef AIXV3
#if (OSMAJORVERSION < 4)
    /* In AIXV3, xterms started from /dev/console have CLOCAL set.
     * This means we need to clear CLOCAL so that SIGHUP gets sent
     * to the slave-pty process when xterm exits.
     */

    {
	TERMIO_STRUCT tio;

	if (ttyGetAttr(screen->respond, &tio) == -1)
	    SysError(ERROR_TIOCGETP);

	tio.c_cflag &= ~(CLOCAL);

	if (ttySetAttr(screen->respond, &tio) == -1)
	    SysError(ERROR_TIOCSETP);
    }
#endif
#endif
#if defined(USE_ANY_SYSV_TERMIO) || defined(__MVS__)
    if (0 > (mode = fcntl(screen->respond, F_GETFL, 0)))
	SysError(ERROR_F_GETFL);
#ifdef O_NDELAY
    mode |= O_NDELAY;
#else
    mode |= O_NONBLOCK;
#endif /* O_NDELAY */
    if (fcntl(screen->respond, F_SETFL, mode))
	SysError(ERROR_F_SETFL);
#else /* !USE_ANY_SYSV_TERMIO */
    mode = 1;
    if (ioctl(screen->respond, FIONBIO, (char *) &mode) == -1)
	SysError(ERROR_FIONBIO);
#endif /* USE_ANY_SYSV_TERMIO, etc */

    /* The erase character is used to delete the current completion */
#if OPT_DABBREV
#ifdef TERMIO_STRUCT
    screen->dabbrev_erase_char = d_tio.c_cc[VERASE];
#else
    screen->dabbrev_erase_char = d_sg.sg_erase;
#endif
    TRACE(("set dabbrev erase_char %#x\n", screen->dabbrev_erase_char));
#endif

    FD_ZERO(&pty_mask);
    FD_ZERO(&X_mask);
    FD_ZERO(&Select_mask);
    FD_SET(screen->respond, &pty_mask);
    FD_SET(ConnectionNumber(screen->display), &X_mask);
    FD_SET(screen->respond, &Select_mask);
    FD_SET(ConnectionNumber(screen->display), &Select_mask);
    max_plus1 = ((screen->respond < ConnectionNumber(screen->display))
		 ? (1 + ConnectionNumber(screen->display))
		 : (1 + screen->respond));

#endif /* !VMS */
#ifdef DEBUG
    if (debug)
	printf("debugging on\n");
#endif /* DEBUG */
    XSetErrorHandler(xerror);
    XSetIOErrorHandler(xioerror);

    initPtyData(&VTbuffer);
#ifdef ALLOWLOGGING
    if (term->misc.log_on) {
	StartLog(screen);
    }
#endif

    if (winToEmbedInto != None) {
	XtRealizeWidget(toplevel);
	/*
	 * This should probably query the tree or check the attributes of
	 * winToEmbedInto in order to verify that it exists, but I'm still not
	 * certain what is the best way to do it -GPS
	 */
	XReparentWindow(XtDisplay(toplevel),
			XtWindow(toplevel),
			winToEmbedInto, 0, 0);
    }
#if OPT_COLOR_RES
    TRACE(("checking resource values rv %s fg %s, bg %s\n",
	   BtoS(term->misc.re_verse0),
	   NonNull(term->screen.Tcolors[TEXT_FG].resource),
	   NonNull(term->screen.Tcolors[TEXT_BG].resource)));

    if ((reversed && term->misc.re_verse0)
	&& ((term->screen.Tcolors[TEXT_FG].resource
	     && !isDefaultForeground(term->screen.Tcolors[TEXT_FG].resource))
	    || (term->screen.Tcolors[TEXT_BG].resource
		&& !isDefaultBackground(term->screen.Tcolors[TEXT_BG].resource))
	))
	ReverseVideo(term);
#endif /* OPT_COLOR_RES */

    for (;;) {
#if OPT_TEK4014
	if (TEK4014_ACTIVE(term))
	    TekRun();
	else
#endif
	    VTRun();
    }
}

#if defined(__osf__) || (defined(__GLIBC__) && !defined(USE_USG_PTYS)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
#define USE_OPENPTY 1
static int opened_tty = -1;
#endif

/*
 * This function opens up a pty master and stuffs its value into pty.
 *
 * If it finds one, it returns a value of 0.  If it does not find one,
 * it returns a value of !0.  This routine is designed to be re-entrant,
 * so that if a pty master is found and later, we find that the slave
 * has problems, we can re-enter this function and get another one.
 */
static int
get_pty(int *pty, char *from GCC_UNUSED)
{
    int result = 1;

#if defined(PUCC_PTYD)

    result = ((*pty = openrpty(ttydev, ptydev,
			       (resource.utmpInhibit ? OPTY_NOP : OPTY_LOGIN),
			       save_ruid, from)) < 0);

#elif defined(USE_OPENPTY)

    result = openpty(pty, &opened_tty, ttydev, NULL, NULL);

#elif defined(__QNXNTO__)

    result = pty_search(pty);

#else
#if defined(USE_ISPTS_FLAG)

    /*
       The order of this code is *important*.  On SYSV/386 we want to open
       a /dev/ttyp? first if at all possible.  If none are available, then
       we'll try to open a /dev/pts??? device.

       The reason for this is because /dev/ttyp? works correctly, where
       as /dev/pts??? devices have a number of bugs, (won't update
       screen correcly, will hang -- it more or less works, but you
       really don't want to use it).

       Most importantly, for boxes of this nature, one of the major
       "features" is that you can emulate a 8086 by spawning off a UNIX
       program on 80386/80486 in v86 mode.  In other words, you can spawn
       off multiple MS-DOS environments.  On ISC the program that does
       this is named "vpix."  The catcher is that "vpix" will *not* work
       with a /dev/pts??? device, will only work with a /dev/ttyp? device.

       Since we can open either a /dev/ttyp? or a /dev/pts??? device,
       the flag "IsPts" is set here so that we know which type of
       device we're dealing with in routine spawnXTerm().  That's the reason
       for the "if (IsPts)" statement in spawnXTerm(); we have two different
       device types which need to be handled differently.
     */
    result = pty_search(pty);
    if (!result)
	IsPts = 0;

#endif
#if defined(USE_USG_PTYS) || defined(__CYGWIN__)
#ifdef __GLIBC__		/* if __GLIBC__ and USE_USG_PTYS, we know glibc >= 2.1 */
    /* GNU libc 2 allows us to abstract away from having to know the
       master pty device name. */
    if ((*pty = getpt()) >= 0) {
	char *name = ptsname(*pty);
	if (name != 0) {	/* if filesystem is trashed, this may be null */
	    strcpy(ttydev, name);
	    result = 0;
	}
    }
#elif defined(__MVS__)
    result = pty_search(pty);
#else
#if defined(USE_ISPTS_FLAG)
    if (result) {
#endif
	result = ((*pty = open("/dev/ptmx", O_RDWR)) < 0);
#endif
#if defined(SVR4) || defined(__SCO__) || defined(USE_ISPTS_FLAG)
	if (!result)
	    strcpy(ttydev, ptsname(*pty));
#ifdef USE_ISPTS_FLAG
	IsPts = !result;	/* true if we're successful */
    }
#endif
#endif

#elif defined(AIXV3)

    if ((*pty = open("/dev/ptc", O_RDWR)) >= 0) {
	strcpy(ttydev, ttyname(*pty));
	result = 0;
    }
#elif defined(__convex__)

    char *pty_name;
    extern char *getpty(void);

    while ((pty_name = getpty()) != NULL) {
	if ((*pty = open(pty_name, O_RDWR)) >= 0) {
	    strcpy(ptydev, pty_name);
	    strcpy(ttydev, pty_name);
	    *x_basename(ttydev) = 't';
	    result = 0;
	    break;
	}
    }

#elif defined(sequent)

    result = ((*pty = getpseudotty(&ttydev, &ptydev)) < 0);

#elif defined(__sgi) && (OSMAJORVERSION >= 4)

    char *tty_name;

    tty_name = _getpty(pty, O_RDWR, 0622, 0);
    if (tty_name != 0) {
	strcpy(ttydev, tty_name);
	result = 0;
    }
#elif (defined(__sgi) && (OSMAJORVERSION < 4)) || (defined(umips) && defined (SYSTYPE_SYSV))

    struct stat fstat_buf;

    *pty = open("/dev/ptc", O_RDWR);
    if (*pty >= 0 && (fstat(*pty, &fstat_buf)) >= 0) {
	result = 0;
	sprintf(ttydev, "/dev/ttyq%d", minor(fstat_buf.st_rdev));
    }
#elif defined(__hpux)

    /*
     * Use the clone device if it works, otherwise use pty_search logic.
     */
    if ((*pty = open("/dev/ptym/clone", O_RDWR)) >= 0) {
	char *name = ptsname(*pty);
	if (name != 0) {
	    strcpy(ttydev, name);
	    result = 0;
	} else {		/* permissions, or other unexpected problem */
	    close(*pty);
	    *pty = -1;
	    result = pty_search(pty);
	}
    } else {
	result = pty_search(pty);
    }

#else

    result = pty_search(pty);

#endif
#endif

    TRACE(("get_pty(ttydev=%s, ptydev=%s) %s fd=%d\n",
	   ttydev != 0 ? ttydev : "?",
	   ptydev != 0 ? ptydev : "?",
	   result ? "FAIL" : "OK",
	   pty != 0 ? *pty : -1));
    return result;
}

static void
set_pty_permissions(uid_t uid, gid_t gid, mode_t mode)
{
#ifdef USE_TTY_GROUP
    struct group *ttygrp;

    if ((ttygrp = getgrnam(TTY_GROUP_NAME)) != 0) {
	gid = ttygrp->gr_gid;
	mode &= 0660U;
    }
    endgrent();
#endif /* USE_TTY_GROUP */

    TRACE_IDS;
    set_owner(ttydev, uid, gid, mode);
}

#ifdef get_pty			/* USE_UTMP_SETGID */
#undef get_pty
/*
 * Call the real get_pty() before relinquishing root-setuid, caching the
 * result.
 */
static int
get_pty(int *pty, char *from)
{
    static int m_pty = -1;
    int result = -1;

    if (pty == NULL) {
	result = really_get_pty(&m_pty, from);

	seteuid(0);
	set_pty_permissions(save_ruid, save_rgid, 0600U);
	seteuid(save_ruid);
	TRACE_IDS;

#ifdef USE_OPENPTY
	if (opened_tty >= 0) {
	    close(opened_tty);
	    opened_tty = -1;
	}
#endif
    } else if (m_pty != -1) {
	*pty = m_pty;
	result = 0;
    } else {
	result = -1;
    }
    return result;
}
#endif

/*
 * Called from get_pty to iterate over likely pseudo terminals
 * we might allocate.  Used on those systems that do not have
 * a functional interface for allocating a pty.
 * Returns 0 if found a pty, 1 if fails.
 */
#ifdef USE_PTY_SEARCH
static int
pty_search(int *pty)
{
    static int devindex = 0, letter = 0;

#if defined(CRAY) || defined(__MVS__)
    while (devindex < MAXPTTYS) {
	sprintf(ttydev, TTYFORMAT, devindex);
	sprintf(ptydev, PTYFORMAT, devindex);
	devindex++;

	TRACE(("pty_search(ttydev=%s, ptydev=%s)\n", ttydev, ptydev));
	if ((*pty = open(ptydev, O_RDWR)) >= 0) {
	    return 0;
	}
    }
#else /* CRAY || __MVS__ */
    while (PTYCHAR1[letter]) {
	ttydev[strlen(ttydev) - 2] =
	    ptydev[strlen(ptydev) - 2] = PTYCHAR1[letter];

	while (PTYCHAR2[devindex]) {
	    ttydev[strlen(ttydev) - 1] =
		ptydev[strlen(ptydev) - 1] = PTYCHAR2[devindex];
	    devindex++;

	    TRACE(("pty_search(ttydev=%s, ptydev=%s)\n", ttydev, ptydev));
	    if ((*pty = open(ptydev, O_RDWR)) >= 0) {
#ifdef sun
		/* Need to check the process group of the pty.
		 * If it exists, then the slave pty is in use,
		 * and we need to get another one.
		 */
		int pgrp_rtn;
		if (ioctl(*pty, TIOCGPGRP, &pgrp_rtn) == 0 || errno != EIO) {
		    close(*pty);
		    continue;
		}
#endif /* sun */
		return 0;
	    }
	}
	devindex = 0;
	letter++;
    }
#endif /* CRAY else */
    /*
     * We were unable to allocate a pty master!  Return an error
     * condition and let our caller terminate cleanly.
     */
    return 1;
}
#endif /* USE_PTY_SEARCH */

/*
 * The only difference in /etc/termcap between 4014 and 4015 is that
 * the latter has support for switching character sets.  We support the
 * 4015 protocol, but ignore the character switches.  Therefore, we
 * choose 4014 over 4015.
 *
 * Features of the 4014 over the 4012: larger (19") screen, 12-bit
 * graphics addressing (compatible with 4012 10-bit addressing),
 * special point plot mode, incremental plot mode (not implemented in
 * later Tektronix terminals), and 4 character sizes.
 * All of these are supported by xterm.
 */

#if OPT_TEK4014
static char *tekterm[] =
{
    "tek4014",
    "tek4015",			/* 4014 with APL character set support */
    "tek4012",			/* 4010 with lower case */
    "tek4013",			/* 4012 with APL character set support */
    "tek4010",			/* small screen, upper-case only */
    "dumb",
    0
};
#endif

/* The VT102 is a VT100 with the Advanced Video Option included standard.
 * It also adds Escape sequences for insert/delete character/line.
 * The VT220 adds 8-bit character sets, selective erase.
 * The VT320 adds a 25th status line, terminal state interrogation.
 * The VT420 has up to 48 lines on the screen.
 */

static char *vtterm[] =
{
#ifdef USE_X11TERM
    "x11term",			/* for people who want special term name */
#endif
    DFT_TERMTYPE,		/* for people who want special term name */
    "xterm",			/* the prefered name, should be fastest */
    "vt102",
    "vt100",
    "ansi",
    "dumb",
    0
};

/* ARGSUSED */
static SIGNAL_T
hungtty(int i GCC_UNUSED)
{
    siglongjmp(env, 1);
    SIGNAL_RETURN;
}

#if OPT_PTY_HANDSHAKE
#define NO_FDS {-1, -1}

static int cp_pipe[2] = NO_FDS;	/* this pipe is used for child to parent transfer */
static int pc_pipe[2] = NO_FDS;	/* this pipe is used for parent to child transfer */

typedef enum {			/* c == child, p == parent                        */
    PTY_BAD,			/* c->p: can't open pty slave for some reason     */
    PTY_FATALERROR,		/* c->p: we had a fatal error with the pty        */
    PTY_GOOD,			/* c->p: we have a good pty, let's go on          */
    PTY_NEW,			/* p->c: here is a new pty slave, try this        */
    PTY_NOMORE,			/* p->c; no more pty's, terminate                 */
    UTMP_ADDED,			/* c->p: utmp entry has been added                */
    UTMP_TTYSLOT,		/* c->p: here is my ttyslot                       */
    PTY_EXEC			/* p->c: window has been mapped the first time    */
} status_t;

typedef struct {
    status_t status;
    int error;
    int fatal_error;
    int tty_slot;
    int rows;
    int cols;
    char buffer[1024];
} handshake_t;

#if OPT_TRACE
static void
trace_handshake(const char *tag, handshake_t * data)
{
    const char *status = "?";
    switch (data->status) {
    case PTY_BAD:
	status = "PTY_BAD";
	break;
    case PTY_FATALERROR:
	status = "PTY_FATALERROR";
	break;
    case PTY_GOOD:
	status = "PTY_GOOD";
	break;
    case PTY_NEW:
	status = "PTY_NEW";
	break;
    case PTY_NOMORE:
	status = "PTY_NOMORE";
	break;
    case UTMP_ADDED:
	status = "UTMP_ADDED";
	break;
    case UTMP_TTYSLOT:
	status = "UTMP_TTYSLOT";
	break;
    case PTY_EXEC:
	status = "PTY_EXEC";
	break;
    }
    TRACE(("handshake %s %s errno=%d, error=%d device \"%s\"\n",
	   tag,
	   status,
	   data->error,
	   data->fatal_error,
	   data->buffer));
}
#define TRACE_HANDSHAKE(tag, data) trace_handshake(tag, data)
#else
#define TRACE_HANDSHAKE(tag, data)	/* nothing */
#endif

/* HsSysError()
 *
 * This routine does the equivalent of a SysError but it handshakes
 * over the errno and error exit to the master process so that it can
 * display our error message and exit with our exit code so that the
 * user can see it.
 */

static void
HsSysError(int error)
{
    handshake_t handshake;

    memset(&handshake, 0, sizeof(handshake));
    handshake.status = PTY_FATALERROR;
    handshake.error = errno;
    handshake.fatal_error = error;
    strcpy(handshake.buffer, ttydev);

    if (resource.ptyHandshake && (cp_pipe[1] >= 0)) {
	TRACE(("HsSysError errno=%d, error=%d device \"%s\"\n",
	       handshake.error,
	       handshake.fatal_error,
	       handshake.buffer));
	TRACE_HANDSHAKE("writing", &handshake);
	write(cp_pipe[1], (char *) &handshake, sizeof(handshake));
    } else {
	fprintf(stderr,
		"%s: fatal pty error errno=%d, error=%d device \"%s\"\n",
		ProgramName,
		handshake.error,
		handshake.fatal_error,
		handshake.buffer);
	fprintf(stderr, "%s\n", SysErrorMsg(handshake.error));
	fprintf(stderr, "Reason: %s\n", SysReasonMsg(handshake.fatal_error));
    }
    exit(error);
}

void
first_map_occurred(void)
{
    if (resource.wait_for_map) {
	handshake_t handshake;
	TScreen *screen = TScreenOf(term);

	memset(&handshake, 0, sizeof(handshake));
	handshake.status = PTY_EXEC;
	handshake.rows = screen->max_row;
	handshake.cols = screen->max_col;

	if (pc_pipe[1] >= 0) {
	    TRACE(("first_map_occurred: %dx%d\n", handshake.rows, handshake.cols));
	    TRACE_HANDSHAKE("writing", &handshake);
	    write(pc_pipe[1], (char *) &handshake, sizeof(handshake));
	    close(cp_pipe[0]);
	    close(pc_pipe[1]);
	}
	resource.wait_for_map = False;
    }
}
#else
/*
 * temporary hack to get xterm working on att ptys
 */
static void
HsSysError(int error)
{
    fprintf(stderr, "%s: fatal pty error %d (errno=%d) on tty %s\n",
	    ProgramName, error, errno, ttydev);
    exit(error);
}
#endif /* OPT_PTY_HANDSHAKE else !OPT_PTY_HANDSHAKE */

#ifndef VMS
static void
set_owner(char *device, uid_t uid, gid_t gid, mode_t mode)
{
    int why;

    TRACE_IDS;
    TRACE(("set_owner(%s, uid=%d, gid=%d, mode=%#o\n", device, uid, gid, mode));

    if (chown(device, uid, gid) < 0) {
	why = errno;
	if (why != ENOENT
	    && save_ruid == 0) {
	    fprintf(stderr, "Cannot chown %s to %ld,%ld: %s\n",
		    device, (long) uid, (long) gid,
		    strerror(why));
	}
	TRACE(("...chown failed: %s\n", strerror(why)));
    }
    if (chmod(device, mode) < 0) {
	why = errno;
	if (why != ENOENT) {
	    struct stat sb;
	    if (stat(device, &sb) < 0) {
		fprintf(stderr, "Cannot chmod %s to %03o: %s\n",
			device, (unsigned) mode,
			strerror(why));
	    } else if (mode != (sb.st_mode & 0777U)) {
		fprintf(stderr,
			"Cannot chmod %s to %03lo currently %03lo: %s\n",
			device,
			(unsigned long) mode,
			(unsigned long) (sb.st_mode & 0777U),
			strerror(why));
		TRACE(("...stat uid=%d, gid=%d, mode=%#o\n",
		       sb.st_uid, sb.st_gid, sb.st_mode));
	    }
	}
	TRACE(("...chmod failed: %s\n", strerror(why)));
    }
}

#if defined(HAVE_UTMP) && defined(USE_SYSV_UTMP) && !defined(USE_UTEMPTER)
/*
 * getutid() only looks at ut_type and ut_id.
 * But we'll also check ut_line in find_utmp().
 */
static void
init_utmp(int type, struct UTMP_STR *tofind)
{
    memset(tofind, 0, sizeof(*tofind));
    tofind->ut_type = type;
    (void) strncpy(tofind->ut_id, my_utmp_id(ttydev), sizeof(tofind->ut_id));
    (void) strncpy(tofind->ut_line, my_pty_name(ttydev), sizeof(tofind->ut_line));
}

/*
 * We could use getutline() if we didn't support old systems.
 */
static struct UTMP_STR *
find_utmp(struct UTMP_STR *tofind)
{
    struct UTMP_STR *result;
    struct UTMP_STR working;

    for (;;) {
	memset(&working, 0, sizeof(working));
	working.ut_type = tofind->ut_type;
	memcpy(working.ut_id, tofind->ut_id, sizeof(tofind->ut_id));
#if defined(__digital__) && defined(__unix__) && (defined(OSMAJORVERSION) && OSMAJORVERSION < 5)
	working.ut_type = 0;
#endif
	if ((result = call_getutid(&working)) == 0)
	    break;
	if (!strcmp(result->ut_line, tofind->ut_line))
	    break;
	/*
	 * Solaris, IRIX64 and HPUX manpages say to fill the static area
	 * pointed to by the return-value to zeros if searching for multiple
	 * occurrences.  Otherwise it will continue to return the same value.
	 */
	memset(result, 0, sizeof(*result));
    }
    return result;
}
#endif /* HAVE_UTMP... */

#define close_fd(fd) close(fd), fd = -1

/*
 *  Inits pty and tty and forks a login process.
 *  Does not close fd Xsocket.
 *  If slave, the pty named in passedPty is already open for use
 */
static int
spawnXTerm(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    Cardinal nn;
#if OPT_PTY_HANDSHAKE
    Bool got_handshake_size = False;
    handshake_t handshake;
    int done;
#endif
#if OPT_INITIAL_ERASE
    int initial_erase = VAL_INITIAL_ERASE;
    Bool setInitialErase;
#endif
    int rc = 0;
    int ttyfd = -1;
    Bool ok_termcap;
    char *newtc;

#ifdef TERMIO_STRUCT
    TERMIO_STRUCT tio;
#ifdef __MVS__
    TERMIO_STRUCT gio;
#endif /* __MVS__ */
#ifdef TIOCLSET
    unsigned lmode;
#endif /* TIOCLSET */
#ifdef HAS_LTCHARS
    struct ltchars ltc;
#endif /* HAS_LTCHARS */
#else /* !TERMIO_STRUCT */
    int ldisc = 0;
    int discipline;
    unsigned lmode;
    struct tchars tc;
    struct ltchars ltc;
    struct sgttyb sg;
#ifdef sony
    int jmode;
    struct jtchars jtc;
#endif /* sony */
#endif /* TERMIO_STRUCT */

    char *ptr, *shname, *shname_minus;
    int i, no_dev_tty = False;
    char **envnew;		/* new environment */
    char buf[64];
    char *TermName = NULL;
#ifdef TTYSIZE_STRUCT
    TTYSIZE_STRUCT ts;
#endif
    struct passwd *pw = NULL;
    char *login_name = NULL;
#ifndef USE_UTEMPTER
#ifdef HAVE_UTMP
    struct UTMP_STR utmp;
#ifdef USE_SYSV_UTMP
    struct UTMP_STR *utret = NULL;
#endif
#ifdef USE_LASTLOG
    struct lastlog lastlog;
#endif
#ifdef USE_LASTLOGX
    struct lastlogx lastlogx;
#endif /* USE_LASTLOG */
#endif /* HAVE_UTMP */
#endif /* !USE_UTEMPTER */

    /* Noisy compilers (suppress some unused-variable warnings) */
    (void) rc;
#if defined(HAVE_UTMP) && defined(USE_SYSV_UTMP) && !defined(USE_UTEMPTER)
    (void) utret;
#endif

    screen->uid = save_ruid;
    screen->gid = save_rgid;

#ifdef SIGTTOU
    /* so that TIOCSWINSZ || TIOCSIZE doesn't block */
    signal(SIGTTOU, SIG_IGN);
#endif

#if OPT_PTY_HANDSHAKE
    memset(&handshake, 0, sizeof(handshake));
#endif

    if (am_slave >= 0) {
	screen->respond = am_slave;
	set_pty_id(ttydev, passedPty);
#ifdef USE_PTY_DEVICE
	set_pty_id(ptydev, passedPty);
#endif
	if (xtermResetIds(screen) < 0)
	    exit(1);
    } else {
	Bool tty_got_hung;

	/*
	 * Sometimes /dev/tty hangs on open (as in the case of a pty
	 * that has gone away).  Simply make up some reasonable
	 * defaults.
	 */

	signal(SIGALRM, hungtty);
	alarm(2);		/* alarm(1) might return too soon */
	if (!sigsetjmp(env, 1)) {
	    ttyfd = open("/dev/tty", O_RDWR);
	    alarm(0);
	    tty_got_hung = False;
	} else {
	    tty_got_hung = True;
	    ttyfd = -1;
	    errno = ENXIO;
	}
	pw = NULL;
#if OPT_PTY_HANDSHAKE
	got_handshake_size = False;
#endif /* OPT_PTY_HANDSHAKE */
#if OPT_INITIAL_ERASE
	initial_erase = VAL_INITIAL_ERASE;
#endif
	signal(SIGALRM, SIG_DFL);

	/*
	 * Check results and ignore current control terminal if
	 * necessary.  ENXIO is what is normally returned if there is
	 * no controlling terminal, but some systems (e.g. SunOS 4.0)
	 * seem to return EIO.  Solaris 2.3 is said to return EINVAL.
	 * Cygwin returns ENOENT.
	 */
	no_dev_tty = False;
	if (ttyfd < 0) {
	    if (tty_got_hung || errno == ENXIO || errno == EIO ||
#ifdef ENODEV
		errno == ENODEV ||
#endif
#ifdef __CYGWIN__
		errno == ENOENT ||
#endif
		errno == EINVAL || errno == ENOTTY || errno == EACCES) {
		no_dev_tty = True;
#ifdef HAS_LTCHARS
		ltc = d_ltc;
#endif /* HAS_LTCHARS */
#ifdef TIOCLSET
		lmode = d_lmode;
#endif /* TIOCLSET */
#ifdef TERMIO_STRUCT
		tio = d_tio;
#else /* !TERMIO_STRUCT */
		sg = d_sg;
		tc = d_tc;
		discipline = d_disipline;
#ifdef sony
		jmode = d_jmode;
		jtc = d_jtc;
#endif /* sony */
#endif /* TERMIO_STRUCT */
	    } else {
		SysError(ERROR_OPDEVTTY);
	    }
	} else {

	    /* Get a copy of the current terminal's state,
	     * if we can.  Some systems (e.g., SVR4 and MacII)
	     * may not have a controlling terminal at this point
	     * if started directly from xdm or xinit,
	     * in which case we just use the defaults as above.
	     */
#ifdef HAS_LTCHARS
	    if (ioctl(ttyfd, TIOCGLTC, &ltc) == -1)
		ltc = d_ltc;
#endif /* HAS_LTCHARS */
#ifdef TIOCLSET
	    if (ioctl(ttyfd, TIOCLGET, &lmode) == -1)
		lmode = d_lmode;
#endif /* TIOCLSET */
#ifdef TERMIO_STRUCT
	    if ((rc = ttyGetAttr(ttyfd, &tio)) == -1)
		tio = d_tio;
#else /* !TERMIO_STRUCT */
	    if ((rc = ioctl(ttyfd, TIOCGETP, (char *) &sg)) == -1)
		sg = d_sg;
	    if (ioctl(ttyfd, TIOCGETC, (char *) &tc) == -1)
		tc = d_tc;
	    if (ioctl(ttyfd, TIOCGETD, (char *) &discipline) == -1)
		discipline = d_disipline;
#ifdef sony
	    if (ioctl(ttyfd, TIOCKGET, (char *) &jmode) == -1)
		jmode = d_jmode;
	    if (ioctl(ttyfd, TIOCKGETC, (char *) &jtc) == -1)
		jtc = d_jtc;
#endif /* sony */
#endif /* TERMIO_STRUCT */

	    /*
	     * If ptyInitialErase is set, we want to get the pty's
	     * erase value.  Just in case that will fail, first get
	     * the value from /dev/tty, so we will have something
	     * at least.
	     */
#if OPT_INITIAL_ERASE
	    if (resource.ptyInitialErase) {
#ifdef TERMIO_STRUCT
		initial_erase = tio.c_cc[VERASE];
#else /* !TERMIO_STRUCT */
		initial_erase = sg.sg_erase;
#endif /* TERMIO_STRUCT */
		TRACE(("%s initial_erase:%d (from /dev/tty)\n",
		       rc == 0 ? "OK" : "FAIL",
		       initial_erase));
	    }
#endif
#ifdef __MVS__
	    if (ttyGetAttr(ttyfd, &gio) == 0) {
		gio.c_cflag &= ~(HUPCL | PARENB);
		ttySetAttr(ttyfd, &gio);
	    }
#endif /* __MVS__ */

	    close_fd(ttyfd);
	}

	if (get_pty(&screen->respond, XDisplayString(screen->display))) {
	    SysError(ERROR_PTYS);
	}
#if OPT_INITIAL_ERASE
	if (resource.ptyInitialErase) {
#ifdef TERMIO_STRUCT
	    TERMIO_STRUCT my_tio;
	    if ((rc = ttyGetAttr(screen->respond, &my_tio)) == 0)
		initial_erase = my_tio.c_cc[VERASE];
#else /* !TERMIO_STRUCT */
	    struct sgttyb my_sg;
	    if ((rc = ioctl(screen->respond, TIOCGETP, (char *) &my_sg)) == 0)
		initial_erase = my_sg.sg_erase;
#endif /* TERMIO_STRUCT */
	    TRACE(("%s initial_erase:%d (from pty)\n",
		   (rc == 0) ? "OK" : "FAIL",
		   initial_erase));
	}
#endif /* OPT_INITIAL_ERASE */
    }

    /* avoid double MapWindow requests */
    XtSetMappedWhenManaged(SHELL_OF(CURRENT_EMU()), False);

    wm_delete_window = XInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW",
				   False);

    if (!TEK4014_ACTIVE(xw))
	VTInit();		/* realize now so know window size for tty driver */
#if defined(TIOCCONS) || defined(SRIOCSREDIR)
    if (Console) {
	/*
	 * Inform any running xconsole program
	 * that we are going to steal the console.
	 */
	XmuGetHostname(mit_console_name + MIT_CONSOLE_LEN, 255);
	mit_console = XInternAtom(screen->display, mit_console_name, False);
	/* the user told us to be the console, so we can use CurrentTime */
	XtOwnSelection(SHELL_OF(CURRENT_EMU()),
		       mit_console, CurrentTime,
		       ConvertConsoleSelection, NULL, NULL);
    }
#endif
#if OPT_TEK4014
    if (TEK4014_ACTIVE(xw)) {
	envnew = tekterm;
	newtc = TekScreenOf(tekWidget)->tcapbuf;
    } else
#endif
    {
	envnew = vtterm;
	newtc = screen->tcapbuf;
    }

    /*
     * This used to exit if no termcap entry was found for the specified
     * terminal name.  That's a little unfriendly, so instead we'll allow
     * the program to proceed (but not to set $TERMCAP) if the termcap
     * entry is not found.
     */
    ok_termcap = True;
    if (!get_termcap(TermName = resource.term_name, newtc)) {
	char *last = NULL;
	TermName = *envnew;
	ok_termcap = False;
	while (*envnew != NULL) {
	    if ((last == NULL || strcmp(last, *envnew))
		&& get_termcap(*envnew, newtc)) {
		TermName = *envnew;
		ok_termcap = True;
		break;
	    }
	    last = *envnew;
	    envnew++;
	}
    }
    if (ok_termcap) {
	resize_termcap(xw, newtc);
    }

    /*
     * Check if ptyInitialErase is not set.  If so, we rely on the termcap
     * (or terminfo) to tell us what the erase mode should be set to.
     */
#if OPT_INITIAL_ERASE
    TRACE(("resource ptyInitialErase is %sset\n",
	   resource.ptyInitialErase ? "" : "not "));
    setInitialErase = False;
    if (override_tty_modes && ttymodelist[XTTYMODE_erase].set) {
	initial_erase = ttymodelist[XTTYMODE_erase].value;
	setInitialErase = True;
    } else if (resource.ptyInitialErase) {
	;
    } else if (ok_termcap) {
	char temp[1024], *p = temp;
	char *s = tgetstr(TERMCAP_ERASE, &p);
	TRACE(("...extracting initial_erase value from termcap\n"));
	if (s != 0) {
	    initial_erase = decode_keyvalue(&s, True);
	    setInitialErase = True;
	}
    }
    TRACE(("...initial_erase:%d\n", initial_erase));

    TRACE(("resource backarrowKeyIsErase is %sset\n",
	   resource.backarrow_is_erase ? "" : "not "));
    if (resource.backarrow_is_erase) {	/* see input.c */
	if (initial_erase == ANSI_DEL) {
	    xw->keyboard.flags &= ~MODE_DECBKM;
	} else {
	    xw->keyboard.flags |= MODE_DECBKM;
	    xw->keyboard.reset_DECBKM = 1;
	}
	TRACE(("...sets DECBKM %s\n",
	       (xw->keyboard.flags & MODE_DECBKM) ? "on" : "off"));
    } else {
	xw->keyboard.reset_DECBKM = 2;
    }
#endif /* OPT_INITIAL_ERASE */

#ifdef TTYSIZE_STRUCT
    /* tell tty how big window is */
#if OPT_TEK4014
    if (TEK4014_ACTIVE(xw)) {
	TTYSIZE_ROWS(ts) = 38;
	TTYSIZE_COLS(ts) = 81;
#if defined(USE_STRUCT_WINSIZE)
	ts.ws_xpixel = TFullWidth(&(tekWidget->screen));
	ts.ws_ypixel = TFullHeight(&(tekWidget->screen));
#endif
    } else
#endif
    {
	TTYSIZE_ROWS(ts) = MaxRows(screen);
	TTYSIZE_COLS(ts) = MaxCols(screen);
#if defined(USE_STRUCT_WINSIZE)
	ts.ws_xpixel = FullWidth(screen);
	ts.ws_ypixel = FullHeight(screen);
#endif
    }
    i = SET_TTYSIZE(screen->respond, ts);
    TRACE(("spawn SET_TTYSIZE %dx%d return %d\n",
	   TTYSIZE_ROWS(ts),
	   TTYSIZE_COLS(ts), i));
#endif /* TTYSIZE_STRUCT */

    added_utmp_entry = False;
#if defined(USE_UTEMPTER)
#undef UTMP
    if (!resource.utmpInhibit) {
	struct UTMP_STR dummy;

	/* Note: utempter may trim it anyway */
	SetUtmpHost(dummy.ut_host, screen);
	addToUtmp(ttydev, dummy.ut_host, screen->respond);
	added_utmp_entry = True;
    }
#endif

    if (am_slave < 0) {
#if OPT_PTY_HANDSHAKE
	if (resource.ptyHandshake && (pipe(pc_pipe) || pipe(cp_pipe)))
	    SysError(ERROR_FORK);
#endif
	TRACE(("Forking...\n"));
	if ((screen->pid = fork()) == -1)
	    SysError(ERROR_FORK);

	if (screen->pid == 0) {
#ifdef USE_USG_PTYS
	    int ptyfd;
	    char *pty_name;
#endif
	    /*
	     * now in child process
	     */
#if defined(_POSIX_SOURCE) || defined(SVR4) || defined(__convex__) || defined(__SCO__) || defined(__QNX__)
	    int pgrp = setsid();	/* variable may not be used... */
#else
	    int pgrp = getpid();
#endif
	    TRACE_CHILD

#ifdef USE_USG_PTYS
#ifdef USE_ISPTS_FLAG
		if (IsPts) {	/* SYSV386 supports both, which did we open? */
#endif
		ptyfd = 0;
		pty_name = 0;

		setpgrp();
		grantpt(screen->respond);
		unlockpt(screen->respond);
		if ((pty_name = ptsname(screen->respond)) == 0) {
		    SysError(ERROR_PTSNAME);
		}
		if ((ptyfd = open(pty_name, O_RDWR)) < 0) {
		    SysError(ERROR_OPPTSNAME);
		}
#ifdef I_PUSH
		if (ioctl(ptyfd, I_PUSH, "ptem") < 0) {
		    SysError(ERROR_PTEM);
		}
#if !defined(SVR4) && !(defined(SYSV) && defined(i386))
		if (!x_getenv("CONSEM") && ioctl(ptyfd, I_PUSH, "consem") < 0) {
		    SysError(ERROR_CONSEM);
		}
#endif /* !SVR4 */
		if (ioctl(ptyfd, I_PUSH, "ldterm") < 0) {
		    SysError(ERROR_LDTERM);
		}
#ifdef SVR4			/* from Sony */
		if (ioctl(ptyfd, I_PUSH, "ttcompat") < 0) {
		    SysError(ERROR_TTCOMPAT);
		}
#endif /* SVR4 */
#endif /* I_PUSH */
		ttyfd = ptyfd;
#ifndef __MVS__
		close_fd(screen->respond);
#endif /* __MVS__ */

#ifdef TTYSIZE_STRUCT
		/* tell tty how big window is */
#if OPT_TEK4014
		if (TEK4014_ACTIVE(xw)) {
		    TTYSIZE_ROWS(ts) = 24;
		    TTYSIZE_COLS(ts) = 80;
#ifdef USE_STRUCT_WINSIZE
		    ts.ws_xpixel = TFullWidth(&(tekWidget->screen));
		    ts.ws_ypixel = TFullHeight(&(tekWidget->screen));
#endif
		} else
#endif /* OPT_TEK4014 */
		{
		    TTYSIZE_ROWS(ts) = MaxRows(screen);
		    TTYSIZE_COLS(ts) = MaxCols(screen);
#ifdef USE_STRUCT_WINSIZE
		    ts.ws_xpixel = FullWidth(screen);
		    ts.ws_ypixel = FullHeight(screen);
#endif
		}
#endif /* TTYSIZE_STRUCT */

#ifdef USE_ISPTS_FLAG
	    } else {		/* else pty, not pts */
#endif
#endif /* USE_USG_PTYS */

		(void) pgrp;	/* not all branches use this variable */

#if OPT_PTY_HANDSHAKE		/* warning, goes for a long ways */
		if (resource.ptyHandshake) {
		    /* close parent's sides of the pipes */
		    close(cp_pipe[0]);
		    close(pc_pipe[1]);

		    /* Make sure that our sides of the pipes are not in the
		     * 0, 1, 2 range so that we don't fight with stdin, out
		     * or err.
		     */
		    if (cp_pipe[1] <= 2) {
			if ((i = fcntl(cp_pipe[1], F_DUPFD, 3)) >= 0) {
			    (void) close(cp_pipe[1]);
			    cp_pipe[1] = i;
			}
		    }
		    if (pc_pipe[0] <= 2) {
			if ((i = fcntl(pc_pipe[0], F_DUPFD, 3)) >= 0) {
			    (void) close(pc_pipe[0]);
			    pc_pipe[0] = i;
			}
		    }

		    /* we don't need the socket, or the pty master anymore */
		    close(ConnectionNumber(screen->display));
#ifndef __MVS__
		    close(screen->respond);
#endif /* __MVS__ */

		    /* Now is the time to set up our process group and
		     * open up the pty slave.
		     */
#ifdef USE_SYSV_PGRP
#if defined(CRAY) && (OSMAJORVERSION > 5)
		    (void) setsid();
#else
		    (void) setpgrp();
#endif
#endif /* USE_SYSV_PGRP */

#if defined(__QNX__) && !defined(__QNXNTO__)
		    qsetlogin(getlogin(), ttydev);
#endif
		    if (ttyfd >= 0) {
#ifdef __MVS__
			if (ttyGetAttr(ttyfd, &gio) == 0) {
			    gio.c_cflag &= ~(HUPCL | PARENB);
			    ttySetAttr(ttyfd, &gio);
			}
#else /* !__MVS__ */
			close_fd(ttyfd);
#endif /* __MVS__ */
		    }

		    while (1) {
#if defined(TIOCNOTTY) && (!defined(__GLIBC__) || (__GLIBC__ < 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ < 1)))
			if (!no_dev_tty
			    && (ttyfd = open("/dev/tty", O_RDWR)) >= 0) {
			    ioctl(ttyfd, TIOCNOTTY, (char *) NULL);
			    close_fd(ttyfd);
			}
#endif /* TIOCNOTTY && !glibc >= 2.1 */
#ifdef CSRG_BASED
			(void) revoke(ttydev);
#endif
			if ((ttyfd = open(ttydev, O_RDWR)) >= 0) {
#if defined(CRAY) && defined(TCSETCTTY)
			    /* make /dev/tty work */
			    ioctl(ttyfd, TCSETCTTY, 0);
#endif
#if ((defined(__GLIBC__) && defined(__FreeBSD_kernel__)) || defined(__GNU__)) && defined(TIOCSCTTY)
			    /* make /dev/tty work */
			    ioctl(ttyfd, TIOCSCTTY, 0);
#endif
#ifdef USE_SYSV_PGRP
			    /* We need to make sure that we are actually
			     * the process group leader for the pty.  If
			     * we are, then we should now be able to open
			     * /dev/tty.
			     */
			    if ((i = open("/dev/tty", O_RDWR)) >= 0) {
				/* success! */
				close(i);
				break;
			    }
#else /* USE_SYSV_PGRP */
			    break;
#endif /* USE_SYSV_PGRP */
			}
			perror("open ttydev");
#ifdef TIOCSCTTY
			ioctl(ttyfd, TIOCSCTTY, 0);
#endif
			/* let our master know that the open failed */
			handshake.status = PTY_BAD;
			handshake.error = errno;
			strcpy(handshake.buffer, ttydev);
			TRACE_HANDSHAKE("writing", &handshake);
			write(cp_pipe[1], (char *) &handshake,
			      sizeof(handshake));

			/* get reply from parent */
			i = read(pc_pipe[0], (char *) &handshake,
				 sizeof(handshake));
			if (i <= 0) {
			    /* parent terminated */
			    exit(1);
			}

			if (handshake.status == PTY_NOMORE) {
			    /* No more ptys, let's shutdown. */
			    exit(1);
			}

			/* We have a new pty to try */
			free(ttydev);
			ttydev = CastMallocN(char, strlen(handshake.buffer));
			if (ttydev == NULL) {
			    SysError(ERROR_SPREALLOC);
			}
			strcpy(ttydev, handshake.buffer);
		    }

		    /* use the same tty name that everyone else will use
		     * (from ttyname)
		     */
		    if ((ptr = ttyname(ttyfd)) != 0) {
			/* it may be bigger */
			ttydev = TypeRealloc(char, strlen(ptr) + 1, ttydev);
			if (ttydev == NULL) {
			    SysError(ERROR_SPREALLOC);
			}
			(void) strcpy(ttydev, ptr);
		    }
		}
#endif /* OPT_PTY_HANDSHAKE -- from near fork */

#ifdef USE_ISPTS_FLAG
	    }			/* end of IsPts else clause */
#endif

	    set_pty_permissions(screen->uid,
				screen->gid,
				(resource.messages
				 ? 0622U
				 : 0600U));

	    /*
	     * set up the tty modes
	     */
	    {
#ifdef TERMIO_STRUCT
#if defined(umips) || defined(CRAY) || defined(linux)
		/* If the control tty had its modes screwed around with,
		   eg. by lineedit in the shell, or emacs, etc. then tio
		   will have bad values.  Let's just get termio from the
		   new tty and tailor it.  */
		if (ttyGetAttr(ttyfd, &tio) == -1)
		    SysError(ERROR_TIOCGETP);
		tio.c_lflag |= ECHOE;
#endif /* umips */
		/* Now is also the time to change the modes of the
		 * child pty.
		 */
		/* input: nl->nl, don't ignore cr, cr->nl */
		tio.c_iflag &= ~(INLCR | IGNCR);
		tio.c_iflag |= ICRNL;
#if OPT_WIDE_CHARS && defined(linux) && defined(IUTF8)
#if OPT_LUIT_PROG
		if (command_to_exec_with_luit == 0)
#endif
		    if (screen->utf8_mode)
			tio.c_iflag |= IUTF8;
#endif
		/* ouput: cr->cr, nl is not return, no delays, ln->cr/nl */
#ifndef USE_POSIX_TERMIOS
		tio.c_oflag &=
		    ~(OCRNL
		      | ONLRET
		      | NLDLY
		      | CRDLY
		      | TABDLY
		      | BSDLY
		      | VTDLY
		      | FFDLY);
#endif /* USE_POSIX_TERMIOS */
#ifdef ONLCR
		tio.c_oflag |= ONLCR;
#endif /* ONLCR */
#ifdef OPOST
		tio.c_oflag |= OPOST;
#endif /* OPOST */
#ifndef USE_POSIX_TERMIOS
# if defined(Lynx) && !defined(CBAUD)
#  define CBAUD V_CBAUD
# endif
		tio.c_cflag &= ~(CBAUD);
#ifdef BAUD_0
		/* baud rate is 0 (don't care) */
#elif defined(HAVE_TERMIO_C_ISPEED)
		tio.c_ispeed = tio.c_ospeed = VAL_LINE_SPEED;
#else /* !BAUD_0 */
		tio.c_cflag |= VAL_LINE_SPEED;
#endif /* !BAUD_0 */
#else /* USE_POSIX_TERMIOS */
		cfsetispeed(&tio, VAL_LINE_SPEED);
		cfsetospeed(&tio, VAL_LINE_SPEED);
#ifdef __MVS__
		/* turn off bits that can't be set from the slave side */
		tio.c_cflag &= ~(PACKET | PKT3270 | PTU3270 | PKTXTND);
#endif /* __MVS__ */
		/* Clear CLOCAL so that SIGHUP is sent to us
		   when the xterm ends */
		tio.c_cflag &= ~CLOCAL;
#endif /* USE_POSIX_TERMIOS */
		/* enable signals, canonical processing (erase, kill, etc),
		 * echo
		 */
		tio.c_lflag |= ISIG | ICANON | ECHO | ECHOE | ECHOK;
#ifdef ECHOKE
		tio.c_lflag |= ECHOKE | IEXTEN;
#endif
#ifdef ECHOCTL
		tio.c_lflag |= ECHOCTL | IEXTEN;
#endif
		for (nn = 0; nn < XtNumber(known_ttyChars); ++nn) {
		    if (validTtyChar(tio, nn)) {
			int sysMode = known_ttyChars[nn].sysMode;
#ifdef __MVS__
			if (tio.c_cc[sysMode] != 0) {
			    switch (sysMode) {
			    case VEOL:
			    case VEOF:
				continue;
			    }
			}
#endif
			tio.c_cc[sysMode] = known_ttyChars[nn].myDefault;
		    }
		}

		if (override_tty_modes) {
		    for (nn = 0; nn < XtNumber(known_ttyChars); ++nn) {
			if (validTtyChar(tio, nn)) {
			    TMODE(known_ttyChars[nn].myMode,
				  tio.c_cc[known_ttyChars[nn].sysMode]);
			}
		    }
#ifdef HAS_LTCHARS
		    /* both SYSV and BSD have ltchars */
		    TMODE(XTTYMODE_susp, ltc.t_suspc);
		    TMODE(XTTYMODE_dsusp, ltc.t_dsuspc);
		    TMODE(XTTYMODE_rprnt, ltc.t_rprntc);
		    TMODE(XTTYMODE_flush, ltc.t_flushc);
		    TMODE(XTTYMODE_weras, ltc.t_werasc);
		    TMODE(XTTYMODE_lnext, ltc.t_lnextc);
#endif
		}
#ifdef HAS_LTCHARS
#ifdef __hpux
		/* ioctl chokes when the "reserved" process group controls
		 * are not set to _POSIX_VDISABLE */
		ltc.t_rprntc = ltc.t_rprntc = ltc.t_flushc =
		    ltc.t_werasc = ltc.t_lnextc = _POSIX_VDISABLE;
#endif /* __hpux */
		if (ioctl(ttyfd, TIOCSLTC, &ltc) == -1)
		    HsSysError(ERROR_TIOCSETC);
#endif /* HAS_LTCHARS */
#ifdef TIOCLSET
		if (ioctl(ttyfd, TIOCLSET, (char *) &lmode) == -1)
		    HsSysError(ERROR_TIOCLSET);
#endif /* TIOCLSET */
		if (ttySetAttr(ttyfd, &tio) == -1)
		    HsSysError(ERROR_TIOCSETP);

		/* ignore errors here - some platforms don't work */
		tio.c_cflag &= ~CSIZE;
		if (screen->input_eight_bits)
		    tio.c_cflag |= CS8;
		else
		    tio.c_cflag |= CS7;
		(void) ttySetAttr(ttyfd, &tio);

#else /* !TERMIO_STRUCT */
		sg.sg_flags &= ~(ALLDELAY | XTABS | CBREAK | RAW);
		sg.sg_flags |= ECHO | CRMOD;
		/* make sure speed is set on pty so that editors work right */
		sg.sg_ispeed = VAL_LINE_SPEED;
		sg.sg_ospeed = VAL_LINE_SPEED;
		/* reset t_brkc to default value */
		tc.t_brkc = -1;
#ifdef LPASS8
		if (screen->input_eight_bits)
		    lmode |= LPASS8;
		else
		    lmode &= ~(LPASS8);
#endif
#ifdef sony
		jmode &= ~KM_KANJI;
#endif /* sony */

		ltc = d_ltc;

		if (override_tty_modes) {
		    TMODE(XTTYMODE_intr, tc.t_intrc);
		    TMODE(XTTYMODE_quit, tc.t_quitc);
		    TMODE(XTTYMODE_erase, sg.sg_erase);
		    TMODE(XTTYMODE_kill, sg.sg_kill);
		    TMODE(XTTYMODE_eof, tc.t_eofc);
		    TMODE(XTTYMODE_start, tc.t_startc);
		    TMODE(XTTYMODE_stop, tc.t_stopc);
		    TMODE(XTTYMODE_brk, tc.t_brkc);
		    /* both SYSV and BSD have ltchars */
		    TMODE(XTTYMODE_susp, ltc.t_suspc);
		    TMODE(XTTYMODE_dsusp, ltc.t_dsuspc);
		    TMODE(XTTYMODE_rprnt, ltc.t_rprntc);
		    TMODE(XTTYMODE_flush, ltc.t_flushc);
		    TMODE(XTTYMODE_weras, ltc.t_werasc);
		    TMODE(XTTYMODE_lnext, ltc.t_lnextc);
		}

		if (ioctl(ttyfd, TIOCSETP, (char *) &sg) == -1)
		    HsSysError(ERROR_TIOCSETP);
		if (ioctl(ttyfd, TIOCSETC, (char *) &tc) == -1)
		    HsSysError(ERROR_TIOCSETC);
		if (ioctl(ttyfd, TIOCSETD, (char *) &discipline) == -1)
		    HsSysError(ERROR_TIOCSETD);
		if (ioctl(ttyfd, TIOCSLTC, (char *) &ltc) == -1)
		    HsSysError(ERROR_TIOCSLTC);
		if (ioctl(ttyfd, TIOCLSET, (char *) &lmode) == -1)
		    HsSysError(ERROR_TIOCLSET);
#ifdef sony
		if (ioctl(ttyfd, TIOCKSET, (char *) &jmode) == -1)
		    HsSysError(ERROR_TIOCKSET);
		if (ioctl(ttyfd, TIOCKSETC, (char *) &jtc) == -1)
		    HsSysError(ERROR_TIOCKSETC);
#endif /* sony */
#endif /* TERMIO_STRUCT */
#if defined(TIOCCONS) || defined(SRIOCSREDIR)
		if (Console) {
#ifdef TIOCCONS
		    int on = 1;
		    if (ioctl(ttyfd, TIOCCONS, (char *) &on) == -1)
			fprintf(stderr, "%s: cannot open console: %s\n",
				ProgramName, strerror(errno));
#endif
#ifdef SRIOCSREDIR
		    int fd = open("/dev/console", O_RDWR);
		    if (fd == -1 || ioctl(fd, SRIOCSREDIR, ttyfd) == -1)
			fprintf(stderr, "%s: cannot open console: %s\n",
				ProgramName, strerror(errno));
		    (void) close(fd);
#endif
		}
#endif /* TIOCCONS */
	    }

	    signal(SIGCHLD, SIG_DFL);
#ifdef USE_SYSV_SIGHUP
	    /* watch out for extra shells (I don't understand either) */
	    signal(SIGHUP, SIG_DFL);
#else
	    signal(SIGHUP, SIG_IGN);
#endif
	    /* restore various signals to their defaults */
	    signal(SIGINT, SIG_DFL);
	    signal(SIGQUIT, SIG_DFL);
	    signal(SIGTERM, SIG_DFL);

	    /*
	     * If we're not asked to let the parent process set the terminal's
	     * erase mode, or if we had the ttyModes erase resource, then set
	     * the terminal's erase mode from our best guess.
	     */
#if OPT_INITIAL_ERASE
	    TRACE(("check if we should set erase to %d:%s\n\tptyInitialErase:%d,\n\toveride_tty_modes:%d,\n\tXTTYMODE_erase:%d\n",
		   initial_erase,
		   setInitialErase ? "YES" : "NO",
		   resource.ptyInitialErase,
		   override_tty_modes,
		   ttymodelist[XTTYMODE_erase].set));
	    if (setInitialErase) {
#if OPT_TRACE
		int old_erase;
#endif
#ifdef TERMIO_STRUCT
		if (ttyGetAttr(ttyfd, &tio) == -1)
		    tio = d_tio;
#if OPT_TRACE
		old_erase = tio.c_cc[VERASE];
#endif
		tio.c_cc[VERASE] = initial_erase;
		rc = ttySetAttr(ttyfd, &tio);
#else /* !TERMIO_STRUCT */
		if (ioctl(ttyfd, TIOCGETP, (char *) &sg) == -1)
		    sg = d_sg;
#if OPT_TRACE
		old_erase = sg.sg_erase;
#endif
		sg.sg_erase = initial_erase;
		rc = ioctl(ttyfd, TIOCSETP, (char *) &sg);
#endif /* TERMIO_STRUCT */
		TRACE(("%s setting erase to %d (was %d)\n",
		       rc ? "FAIL" : "OK", initial_erase, old_erase));
	    }
#endif

	    xtermCopyEnv(environ);

	    xtermSetenv("TERM", TermName);
	    if (!TermName)
		*newtc = 0;

	    sprintf(buf, "%lu",
		    ((unsigned long) XtWindow(SHELL_OF(CURRENT_EMU()))));
	    xtermSetenv("WINDOWID", buf);

	    /* put the display into the environment of the shell */
	    xtermSetenv("DISPLAY", XDisplayString(screen->display));

	    xtermSetenv("XTERM_VERSION", xtermVersion());
	    xtermSetenv("XTERM_LOCALE", xtermEnvLocale());

	    signal(SIGTERM, SIG_DFL);

	    /* this is the time to go and set up stdin, out, and err
	     */
	    {
#if defined(CRAY) && (OSMAJORVERSION >= 6)
		close_fd(ttyfd);

		(void) close(0);

		if (open("/dev/tty", O_RDWR)) {
		    SysError(ERROR_OPDEVTTY);
		}
		(void) close(1);
		(void) close(2);
		dup(0);
		dup(0);
#else
		/* dup the tty */
		for (i = 0; i <= 2; i++)
		    if (i != ttyfd) {
			(void) close(i);
			(void) dup(ttyfd);
		    }
#ifndef ATT
		/* and close the tty */
		if (ttyfd > 2)
		    close_fd(ttyfd);
#endif
#endif /* CRAY */
	    }

#if !defined(USE_SYSV_PGRP)
#ifdef TIOCSCTTY
	    setsid();
	    ioctl(0, TIOCSCTTY, 0);
#endif
	    ioctl(0, TIOCSPGRP, (char *) &pgrp);
	    setpgrp(0, 0);
	    close(open(ttydev, O_WRONLY));
	    setpgrp(0, pgrp);
#if defined(__QNX__)
	    tcsetpgrp(0, pgrp /*setsid() */ );
#endif
#endif /* !USE_SYSV_PGRP */

#ifdef Lynx
	    {
		TERMIO_STRUCT t;
		if (ttyGetAttr(0, &t) >= 0) {
		    /* this gets lost somewhere on our way... */
		    t.c_oflag |= OPOST;
		    ttySetAttr(0, &t);
		}
	    }
#endif

#ifdef HAVE_UTMP
	    pw = getpwuid(screen->uid);
	    login_name = NULL;
	    if (pw && pw->pw_name) {
#ifdef HAVE_GETLOGIN
		/*
		 * If the value from getlogin() differs from the value we
		 * get by looking in the password file, check if it does
		 * correspond to the same uid.  If so, allow that as an
		 * alias for the uid.
		 *
		 * Of course getlogin() will fail if we're started from
		 * a window-manager, since there's no controlling terminal
		 * to fuss with.  In that case, try to get something useful
		 * from the user's $LOGNAME or $USER environment variables.
		 */
		if (((login_name = getlogin()) != NULL
		     || (login_name = x_getenv("LOGNAME")) != NULL
		     || (login_name = x_getenv("USER")) != NULL)
		    && strcmp(login_name, pw->pw_name)) {
		    struct passwd *pw2 = getpwnam(login_name);
		    if (pw2 != 0) {
			uid_t uid2 = pw2->pw_uid;
			pw = getpwuid(screen->uid);
			if ((uid_t) pw->pw_uid != uid2)
			    login_name = NULL;
		    } else {
			pw = getpwuid(screen->uid);
		    }
		}
#endif
		if (login_name == NULL)
		    login_name = pw->pw_name;
		if (login_name != NULL)
		    login_name = x_strdup(login_name);
	    }
	    if (login_name != NULL) {
		xtermSetenv("LOGNAME", login_name);	/* for POSIX */
	    }
#ifndef USE_UTEMPTER
#ifdef USE_UTMP_SETGID
	    setEffectiveGroup(save_egid);
	    TRACE_IDS;
#endif
#ifdef USE_SYSV_UTMP
	    /* Set up our utmp entry now.  We need to do it here
	     * for the following reasons:
	     *   - It needs to have our correct process id (for
	     *     login).
	     *   - If our parent was to set it after the fork(),
	     *     it might make it out before we need it.
	     *   - We need to do it before we go and change our
	     *     user and group id's.
	     */
	    (void) call_setutent();
	    init_utmp(DEAD_PROCESS, &utmp);

	    /* position to entry in utmp file */
	    /* Test return value: beware of entries left behind: PSz 9 Mar 00 */
	    if (!(utret = find_utmp(&utmp))) {
		(void) call_setutent();
		init_utmp(USER_PROCESS, &utmp);
		if (!(utret = find_utmp(&utmp))) {
		    (void) call_setutent();
		}
	    }
#if OPT_TRACE
	    if (!utret)
		TRACE(("getutid: NULL\n"));
	    else
		TRACE(("getutid: pid=%d type=%d user=%s line=%s id=%s\n",
		       utret->ut_pid, utret->ut_type, utret->ut_user,
		       utret->ut_line, utret->ut_id));
#endif

	    /* set up the new entry */
	    utmp.ut_type = USER_PROCESS;
#ifdef HAVE_UTMP_UT_XSTATUS
	    utmp.ut_xstatus = 2;
#endif
	    (void) strncpy(utmp.ut_user,
			   (login_name != NULL) ? login_name : "????",
			   sizeof(utmp.ut_user));
	    /* why are we copying this string again?  (see above) */
	    (void) strncpy(utmp.ut_id, my_utmp_id(ttydev), sizeof(utmp.ut_id));
	    (void) strncpy(utmp.ut_line,
			   my_pty_name(ttydev), sizeof(utmp.ut_line));

#ifdef HAVE_UTMP_UT_HOST
	    SetUtmpHost(utmp.ut_host, screen);
#endif
#ifdef HAVE_UTMP_UT_SYSLEN
	    SetUtmpSysLen(utmp);
#endif

	    (void) strncpy(utmp.ut_name,
			   (login_name) ? login_name : "????",
			   sizeof(utmp.ut_name));

	    utmp.ut_pid = getpid();
#if defined(HAVE_UTMP_UT_XTIME)
#if defined(HAVE_UTMP_UT_SESSION)
	    utmp.ut_session = getsid(0);
#endif
	    utmp.ut_xtime = time((time_t *) 0);
	    utmp.ut_tv.tv_usec = 0;
#else
	    utmp.ut_time = time((time_t *) 0);
#endif

	    /* write out the entry */
	    if (!resource.utmpInhibit) {
		errno = 0;
		call_pututline(&utmp);
		TRACE(("pututline: id %s, line %s, pid %ld, errno %d %s\n",
		       utmp.ut_id,
		       utmp.ut_line,
		       (long) utmp.ut_pid,
		       errno, (errno != 0) ? strerror(errno) : ""));
	    }
#ifdef WTMP
#if defined(WTMPX_FILE) && (defined(SVR4) || defined(__SCO__))
	    if (xw->misc.login_shell)
		updwtmpx(WTMPX_FILE, &utmp);
#elif defined(linux) && defined(__GLIBC__) && (__GLIBC__ >= 2) && !(defined(__powerpc__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 0))
	    if (xw->misc.login_shell)
		call_updwtmp(etc_wtmp, &utmp);
#else
	    if (xw->misc.login_shell &&
		(i = open(etc_wtmp, O_WRONLY | O_APPEND)) >= 0) {
		write(i, (char *) &utmp, sizeof(utmp));
		close(i);
	    }
#endif
#endif
	    /* close the file */
	    (void) call_endutent();

#else /* USE_SYSV_UTMP */
	    /* We can now get our ttyslot!  We can also set the initial
	     * utmp entry.
	     */
	    tslot = ttyslot();
	    added_utmp_entry = False;
	    {
		if (tslot > 0 && pw && !resource.utmpInhibit &&
		    (i = open(etc_utmp, O_WRONLY)) >= 0) {
		    bzero((char *) &utmp, sizeof(utmp));
		    (void) strncpy(utmp.ut_line,
				   my_pty_name(ttydev),
				   sizeof(utmp.ut_line));
		    (void) strncpy(utmp.ut_name, login_name,
				   sizeof(utmp.ut_name));
#ifdef HAVE_UTMP_UT_HOST
		    SetUtmpHost(utmp.ut_host, screen);
#endif
#ifdef HAVE_UTMP_UT_SYSLEN
		    SetUtmpSysLen(utmp);
#endif

		    utmp.ut_time = time((time_t *) 0);
		    lseek(i, (long) (tslot * sizeof(utmp)), 0);
		    write(i, (char *) &utmp, sizeof(utmp));
		    close(i);
		    added_utmp_entry = True;
#if defined(WTMP)
		    if (xw->misc.login_shell &&
			(i = open(etc_wtmp, O_WRONLY | O_APPEND)) >= 0) {
			int status;
			status = write(i, (char *) &utmp, sizeof(utmp));
			status = close(i);
		    }
#elif defined(MNX_LASTLOG)
		    if (xw->misc.login_shell &&
			(i = open(_U_LASTLOG, O_WRONLY)) >= 0) {
			lseek(i, (long) (screen->uid *
					 sizeof(utmp)), 0);
			write(i, (char *) &utmp, sizeof(utmp));
			close(i);
		    }
#endif /* WTMP or MNX_LASTLOG */
		} else
		    tslot = -tslot;
	    }

	    /* Let's pass our ttyslot to our parent so that it can
	     * clean up after us.
	     */
#if OPT_PTY_HANDSHAKE
	    if (resource.ptyHandshake) {
		handshake.tty_slot = tslot;
	    }
#endif /* OPT_PTY_HANDSHAKE */
#endif /* USE_SYSV_UTMP */

#ifdef USE_LASTLOGX
	    if (xw->misc.login_shell) {
		bzero((char *) &lastlogx, sizeof(lastlogx));
		(void) strncpy(lastlogx.ll_line,
			       my_pty_name(ttydev),
			       sizeof(lastlogx.ll_line));
		X_GETTIMEOFDAY(&lastlogx.ll_tv);
		SetUtmpHost(lastlogx.ll_host, screen);
		updlastlogx(_PATH_LASTLOGX, screen->uid, &lastlogx);
	    }
#endif

#ifdef USE_LASTLOG
	    if (xw->misc.login_shell &&
		(i = open(etc_lastlog, O_WRONLY)) >= 0) {
		size_t size = sizeof(struct lastlog);
		off_t offset = (screen->uid * size);

		bzero((char *) &lastlog, size);
		(void) strncpy(lastlog.ll_line,
			       my_pty_name(ttydev),
			       sizeof(lastlog.ll_line));
		SetUtmpHost(lastlog.ll_host, screen);
		lastlog.ll_time = time((time_t *) 0);
		if (lseek(i, offset, 0) != (off_t) (-1)) {
		    write(i, (char *) &lastlog, size);
		}
		close(i);
	    }
#endif /* USE_LASTLOG */

#if defined(USE_UTMP_SETGID)
	    disableSetGid();
	    TRACE_IDS;
#endif

#if OPT_PTY_HANDSHAKE
	    /* Let our parent know that we set up our utmp entry
	     * so that it can clean up after us.
	     */
	    if (resource.ptyHandshake) {
		handshake.status = UTMP_ADDED;
		handshake.error = 0;
		strcpy(handshake.buffer, ttydev);
		TRACE_HANDSHAKE("writing", &handshake);
		(void) write(cp_pipe[1], (char *) &handshake, sizeof(handshake));
	    }
#endif /* OPT_PTY_HANDSHAKE */
#endif /* USE_UTEMPTER */
#endif /* HAVE_UTMP */

	    (void) setgid(screen->gid);
	    TRACE_IDS;
#ifdef HAS_BSD_GROUPS
	    if (geteuid() == 0 && pw) {
		if (initgroups(login_name, pw->pw_gid)) {
		    perror("initgroups failed");
		    SysError(ERROR_INIGROUPS);
		}
	    }
#endif
	    if (setuid(screen->uid)) {
		SysError(ERROR_SETUID);
	    }
	    TRACE_IDS;
#if OPT_PTY_HANDSHAKE
	    if (resource.ptyHandshake) {
		/* mark the pipes as close on exec */
		fcntl(cp_pipe[1], F_SETFD, 1);
		fcntl(pc_pipe[0], F_SETFD, 1);

		/* We are at the point where we are going to
		 * exec our shell (or whatever).  Let our parent
		 * know we arrived safely.
		 */
		handshake.status = PTY_GOOD;
		handshake.error = 0;
		(void) strcpy(handshake.buffer, ttydev);
		TRACE_HANDSHAKE("writing", &handshake);
		(void) write(cp_pipe[1], (char *) &handshake, sizeof(handshake));

		if (resource.wait_for_map) {
		    i = read(pc_pipe[0], (char *) &handshake,
			     sizeof(handshake));
		    if (i != sizeof(handshake) ||
			handshake.status != PTY_EXEC) {
			/* some very bad problem occurred */
			exit(ERROR_PTY_EXEC);
		    }
		    if (handshake.rows > 0 && handshake.cols > 0) {
			TRACE(("handshake ttysize: %dx%d\n",
			       handshake.rows, handshake.cols));
			set_max_row(screen, handshake.rows);
			set_max_col(screen, handshake.cols);
#ifdef TTYSIZE_STRUCT
			got_handshake_size = True;
			TTYSIZE_ROWS(ts) = MaxRows(screen);
			TTYSIZE_COLS(ts) = MaxCols(screen);
#if defined(USE_STRUCT_WINSIZE)
			ts.ws_xpixel = FullWidth(screen);
			ts.ws_ypixel = FullHeight(screen);
#endif
#endif /* TTYSIZE_STRUCT */
		    }
		}
	    }
#endif /* OPT_PTY_HANDSHAKE */

#ifdef USE_SYSV_ENVVARS
	    {
		char numbuf[12];
		sprintf(numbuf, "%d", MaxCols(screen));
		xtermSetenv("COLUMNS", numbuf);
		sprintf(numbuf, "%d", MaxRows(screen));
		xtermSetenv("LINES", numbuf);
	    }
#ifdef HAVE_UTMP
	    if (pw) {		/* SVR4 doesn't provide these */
		if (!x_getenv("HOME"))
		    xtermSetenv("HOME", pw->pw_dir);
		if (!x_getenv("SHELL"))
		    xtermSetenv("SHELL", pw->pw_shell);
	    }
#endif /* HAVE_UTMP */
#ifdef OWN_TERMINFO_DIR
	    xtermSetenv("TERMINFO", OWN_TERMINFO_DIR);
#endif
#else /* USE_SYSV_ENVVARS */
	    resize_termcap(xw, newtc);
	    if (xw->misc.titeInhibit && !xw->misc.tiXtraScroll) {
		remove_termcap_entry(newtc, "ti=");
		remove_termcap_entry(newtc, "te=");
	    }
	    /*
	     * work around broken termcap entries */
	    if (resource.useInsertMode) {
		remove_termcap_entry(newtc, "ic=");
		/* don't get duplicates */
		remove_termcap_entry(newtc, "im=");
		remove_termcap_entry(newtc, "ei=");
		remove_termcap_entry(newtc, "mi");
		if (*newtc)
		    strcat(newtc, ":im=\\E[4h:ei=\\E[4l:mi:");
	    }
	    if (*newtc) {
#if OPT_INITIAL_ERASE
		unsigned len;
		remove_termcap_entry(newtc, TERMCAP_ERASE "=");
		len = strlen(newtc);
		if (len != 0 && newtc[len - 1] == ':')
		    len--;
		sprintf(newtc + len, ":%s=\\%03o:",
			TERMCAP_ERASE,
			CharOf(initial_erase));
#endif
		xtermSetenv("TERMCAP", newtc);
	    }
#endif /* USE_SYSV_ENVVARS */

#if OPT_PTY_HANDSHAKE
	    /*
	     * Need to reset after all the ioctl bashing we did above.
	     *
	     * If we expect the waitForMap logic to set the handshake-size,
	     * use that to prevent races.
	     */
	    if (resource.ptyHandshake
		&& resource.ptySttySize
		&& (got_handshake_size || !resource.wait_for_map0)) {
#ifdef TTYSIZE_STRUCT
		i = SET_TTYSIZE(0, ts);
		TRACE(("ptyHandshake SET_TTYSIZE %dx%d return %d\n",
		       TTYSIZE_ROWS(ts),
		       TTYSIZE_COLS(ts), i));
#endif /* TTYSIZE_STRUCT */
	    }
#endif /* OPT_PTY_HANDSHAKE */
	    signal(SIGHUP, SIG_DFL);

	    if ((ptr = explicit_shname) == NULL) {
		if (((ptr = x_getenv("SHELL")) == NULL) &&
		    ((pw == NULL && (pw = getpwuid(screen->uid)) == NULL) ||
		     *(ptr = pw->pw_shell) == 0)) {
		    ptr = "/bin/sh";
		}
	    } else {
		xtermSetenv("SHELL", explicit_shname);
	    }
	    xtermSetenv("XTERM_SHELL", ptr);

	    shname = x_basename(ptr);
	    TRACE(("shell path '%s' leaf '%s'\n", ptr, shname));

#if OPT_LUIT_PROG
	    /*
	     * Use two copies of command_to_exec, in case luit is not actually
	     * there, or refuses to run.  In that case we will fall-through to
	     * to command that the user gave anyway.
	     */
	    if (command_to_exec_with_luit) {
		xtermSetenv("XTERM_SHELL",
			    xtermFindShell(*command_to_exec_with_luit, False));
		TRACE(("spawning command \"%s\"\n", *command_to_exec_with_luit));
		execvp(*command_to_exec_with_luit, command_to_exec_with_luit);
		/* print error message on screen */
		fprintf(stderr, "%s: Can't execvp %s: %s\n",
			ProgramName, *command_to_exec_with_luit, strerror(errno));
		fprintf(stderr, "%s: cannot support your locale.\n",
			ProgramName);
	    }
#endif
	    if (command_to_exec) {
		xtermSetenv("XTERM_SHELL",
			    xtermFindShell(*command_to_exec, False));
		TRACE(("spawning command \"%s\"\n", *command_to_exec));
		execvp(*command_to_exec, command_to_exec);
		if (command_to_exec[1] == 0)
		    execlp(ptr, shname, "-c", command_to_exec[0], (void *) 0);
		/* print error message on screen */
		fprintf(stderr, "%s: Can't execvp %s: %s\n",
			ProgramName, *command_to_exec, strerror(errno));
	    }
#ifdef USE_SYSV_SIGHUP
	    /* fix pts sh hanging around */
	    signal(SIGHUP, SIG_DFL);
#endif

	    shname_minus = CastMallocN(char, strlen(shname) + 2);
	    (void) strcpy(shname_minus, "-");
	    (void) strcat(shname_minus, shname);
#ifndef TERMIO_STRUCT
	    ldisc = XStrCmp("csh", shname + strlen(shname) - 3) == 0 ?
		NTTYDISC : 0;
	    ioctl(0, TIOCSETD, (char *) &ldisc);
#endif /* !TERMIO_STRUCT */

#ifdef USE_LOGIN_DASH_P
	    if (xw->misc.login_shell && pw && added_utmp_entry)
		execl(bin_login, "login", "-p", "-f", login_name, (void *) 0);
#endif
	    execlp(ptr,
		   (xw->misc.login_shell ? shname_minus : shname),
		   (void *) 0);

	    /* Exec failed. */
	    fprintf(stderr, "%s: Could not exec %s: %s\n", ProgramName,
		    ptr, strerror(errno));
	    (void) sleep(5);
	    exit(ERROR_EXEC);
	}
	/* end if in child after fork */
#if OPT_PTY_HANDSHAKE
	if (resource.ptyHandshake) {
	    /* Parent process.  Let's handle handshaked requests to our
	     * child process.
	     */

	    /* close childs's sides of the pipes */
	    close(cp_pipe[1]);
	    close(pc_pipe[0]);

	    for (done = 0; !done;) {
		if (read(cp_pipe[0],
			 (char *) &handshake,
			 sizeof(handshake)) <= 0) {
		    /* Our child is done talking to us.  If it terminated
		     * due to an error, we will catch the death of child
		     * and clean up.
		     */
		    break;
		}

		TRACE_HANDSHAKE("read", &handshake);
		switch (handshake.status) {
		case PTY_GOOD:
		    /* Success!  Let's free up resources and
		     * continue.
		     */
		    done = 1;
		    break;

		case PTY_BAD:
		    /* The open of the pty failed!  Let's get
		     * another one.
		     */
		    (void) close(screen->respond);
		    if (get_pty(&screen->respond, XDisplayString(screen->display))) {
			/* no more ptys! */
			fprintf(stderr,
				"%s: child process can find no available ptys: %s\n",
				ProgramName, strerror(errno));
			handshake.status = PTY_NOMORE;
			TRACE_HANDSHAKE("writing", &handshake);
			write(pc_pipe[1], (char *) &handshake, sizeof(handshake));
			exit(ERROR_PTYS);
		    }
		    handshake.status = PTY_NEW;
		    (void) strcpy(handshake.buffer, ttydev);
		    TRACE_HANDSHAKE("writing", &handshake);
		    write(pc_pipe[1], (char *) &handshake, sizeof(handshake));
		    break;

		case PTY_FATALERROR:
		    errno = handshake.error;
		    close(cp_pipe[0]);
		    close(pc_pipe[1]);
		    SysError(handshake.fatal_error);
		    /*NOTREACHED */

		case UTMP_ADDED:
		    /* The utmp entry was set by our slave.  Remember
		     * this so that we can reset it later.
		     */
		    added_utmp_entry = True;
#ifndef	USE_SYSV_UTMP
		    tslot = handshake.tty_slot;
#endif /* USE_SYSV_UTMP */
		    free(ttydev);
		    ttydev = x_strdup(handshake.buffer);
		    break;
		case PTY_NEW:
		case PTY_NOMORE:
		case UTMP_TTYSLOT:
		case PTY_EXEC:
		default:
		    fprintf(stderr, "%s: unexpected handshake status %d\n",
			    ProgramName,
			    (int) handshake.status);
		}
	    }
	    /* close our sides of the pipes */
	    if (!resource.wait_for_map) {
		close(cp_pipe[0]);
		close(pc_pipe[1]);
	    }
	}
#endif /* OPT_PTY_HANDSHAKE */
    }

    /* end if no slave */
    /*
     * still in parent (xterm process)
     */
#ifdef USE_SYSV_SIGHUP
    /* hung sh problem? */
    signal(SIGHUP, SIG_DFL);
#else
    signal(SIGHUP, SIG_IGN);
#endif

/*
 * Unfortunately, System V seems to have trouble divorcing the child process
 * from the process group of xterm.  This is a problem because hitting the
 * INTR or QUIT characters on the keyboard will cause xterm to go away if we
 * don't ignore the signals.  This is annoying.
 */

#if defined(USE_SYSV_SIGNALS) && !defined(SIGTSTP)
    signal(SIGINT, SIG_IGN);

#ifndef SYSV
    /* hung shell problem */
    signal(SIGQUIT, SIG_IGN);
#endif
    signal(SIGTERM, SIG_IGN);
#elif defined(SYSV) || defined(__osf__)
    /* if we were spawned by a jobcontrol smart shell (like ksh or csh),
     * then our pgrp and pid will be the same.  If we were spawned by
     * a jobcontrol dumb shell (like /bin/sh), then we will be in our
     * parent's pgrp, and we must ignore keyboard signals, or we will
     * tank on everything.
     */
    if (getpid() == getpgrp()) {
	(void) signal(SIGINT, Exit);
	(void) signal(SIGQUIT, Exit);
	(void) signal(SIGTERM, Exit);
    } else {
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGTERM, SIG_IGN);
    }
    (void) signal(SIGPIPE, Exit);
#else /* SYSV */
    signal(SIGINT, Exit);
    signal(SIGQUIT, Exit);
    signal(SIGTERM, Exit);
    signal(SIGPIPE, Exit);
#endif /* USE_SYSV_SIGNALS and not SIGTSTP */

    return 0;
}				/* end spawnXTerm */

SIGNAL_T
Exit(int n)
{
    TScreen *screen = TScreenOf(term);

#ifdef USE_UTEMPTER
    if (!resource.utmpInhibit && added_utmp_entry)
	removeFromUtmp();
#elif defined(HAVE_UTMP)
#ifdef USE_SYSV_UTMP
    struct UTMP_STR utmp;
    struct UTMP_STR *utptr;

    /* don't do this more than once */
    if (xterm_exiting)
	SIGNAL_RETURN;
    xterm_exiting = True;

#ifdef PUCC_PTYD
    closepty(ttydev, ptydev, (resource.utmpInhibit ? OPTY_NOP : OPTY_LOGIN), screen->respond);
#endif /* PUCC_PTYD */

    /* cleanup the utmp entry we forged earlier */
    if (!resource.utmpInhibit
#if OPT_PTY_HANDSHAKE		/* without handshake, no way to know */
	&& (resource.ptyHandshake && added_utmp_entry)
#endif /* OPT_PTY_HANDSHAKE */
	) {
#if defined(USE_UTMP_SETGID)
	setEffectiveGroup(save_egid);
	TRACE_IDS;
#endif
	init_utmp(USER_PROCESS, &utmp);
	(void) call_setutent();

	/*
	 * We could use getutline() if we didn't support old systems.
	 */
	while ((utptr = find_utmp(&utmp)) != 0) {
	    if (utptr->ut_pid == screen->pid) {
		utptr->ut_type = DEAD_PROCESS;
#if defined(HAVE_UTMP_UT_XTIME)
#if defined(HAVE_UTMP_UT_SESSION)
		utptr->ut_session = getsid(0);
#endif
		utptr->ut_xtime = time((time_t *) 0);
		utptr->ut_tv.tv_usec = 0;
#else
		*utptr->ut_user = 0;
		utptr->ut_time = time((time_t *) 0);
#endif
		(void) call_pututline(utptr);
#ifdef WTMP
#if defined(WTMPX_FILE) && (defined(SVR4) || defined(__SCO__))
		if (term->misc.login_shell)
		    updwtmpx(WTMPX_FILE, utptr);
#elif defined(linux) && defined(__GLIBC__) && (__GLIBC__ >= 2) && !(defined(__powerpc__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 0))
		strncpy(utmp.ut_line, utptr->ut_line, sizeof(utmp.ut_line));
		if (term->misc.login_shell)
		    call_updwtmp(etc_wtmp, utptr);
#else
		/* set wtmp entry if wtmp file exists */
		if (term->misc.login_shell) {
		    int fd;
		    if ((fd = open(etc_wtmp, O_WRONLY | O_APPEND)) >= 0) {
			write(fd, utptr, sizeof(*utptr));
			close(fd);
		    }
		}
#endif
#endif
		break;
	    }
	    memset(utptr, 0, sizeof(*utptr));	/* keep searching */
	}
	(void) call_endutent();
#ifdef USE_UTMP_SETGID
	disableSetGid();
	TRACE_IDS;
#endif
    }
#else /* not USE_SYSV_UTMP */
    int wfd;
    struct utmp utmp;

    if (!resource.utmpInhibit && added_utmp_entry &&
	(am_slave < 0 && tslot > 0)) {
#if defined(USE_UTMP_SETGID)
	setEffectiveGroup(save_egid);
	TRACE_IDS;
#endif
	if ((wfd = open(etc_utmp, O_WRONLY)) >= 0) {
	    bzero((char *) &utmp, sizeof(utmp));
	    lseek(wfd, (long) (tslot * sizeof(utmp)), 0);
	    write(wfd, (char *) &utmp, sizeof(utmp));
	    close(wfd);
	}
#ifdef WTMP
	if (term->misc.login_shell &&
	    (wfd = open(etc_wtmp, O_WRONLY | O_APPEND)) >= 0) {
	    (void) strncpy(utmp.ut_line,
			   my_pty_name(ttydev),
			   sizeof(utmp.ut_line));
	    utmp.ut_time = time((time_t *) 0);
	    write(wfd, (char *) &utmp, sizeof(utmp));
	    close(wfd);
	}
#endif /* WTMP */
#ifdef USE_UTMP_SETGID
	disableSetGid();
	TRACE_IDS;
#endif
    }
#endif /* USE_SYSV_UTMP */
#endif /* HAVE_UTMP */

    /*
     * Flush pending data before releasing ownership, so nobody else can write
     * in the middle of the data.
     */
    ttyFlush(screen->respond);

    if (am_slave < 0) {
	TRACE_IDS;
	/* restore ownership of tty and pty */
	set_owner(ttydev, 0, 0, 0666U);
#if (defined(USE_PTY_DEVICE) && !defined(__sgi) && !defined(__hpux))
	set_owner(ptydev, 0, 0, 0666U);
#endif
    }

    /*
     * Close after releasing ownership to avoid race condition: other programs 
     * grabbing it, and *then* having us release ownership....
     */
    close(screen->respond);	/* close explicitly to avoid race with slave side */
#ifdef ALLOWLOGGING
    if (screen->logging)
	CloseLog(screen);
#endif

#ifdef NO_LEAKS
    if (n == 0) {
	TRACE(("Freeing memory leaks\n"));
	if (term != 0) {
	    Display *dpy = term->screen.display;

	    if (toplevel) {
		XtDestroyWidget(toplevel);
		TRACE(("destroyed top-level widget\n"));
	    }
	    sortedOpts(0, 0, 0);
	    noleaks_charproc();
	    noleaks_ptydata();
#if OPT_WIDE_CHARS
	    noleaks_CharacterClass();
#endif
	    /* XrmSetDatabase(dpy, 0); increases leaks ;-) */
	    XtCloseDisplay(dpy);
	    XtDestroyApplicationContext(app_con);
#if OPT_SESSION_MGT
	    IceRemoveConnectionWatch(icewatch, NULL);
#endif
	    TRACE(("closed display\n"));
	}
	TRACE((0));
    }
#endif

    exit(n);
    SIGNAL_RETURN;
}

/* ARGSUSED */
static void
resize_termcap(XtermWidget xw, char *newtc)
{
#ifndef USE_SYSV_ENVVARS
    if (!TEK4014_ACTIVE(xw) && *newtc) {
	TScreen *screen = TScreenOf(xw);
	char *ptr1, *ptr2;
	size_t i;
	int li_first = 0;
	char *temp;
	char oldtc[TERMCAP_SIZE];

	strcpy(oldtc, newtc);
	TRACE(("resize %s\n", oldtc));
	if ((ptr1 = x_strindex(oldtc, "co#")) == NULL) {
	    strcat(oldtc, "co#80:");
	    ptr1 = x_strindex(oldtc, "co#");
	}
	if ((ptr2 = x_strindex(oldtc, "li#")) == NULL) {
	    strcat(oldtc, "li#24:");
	    ptr2 = x_strindex(oldtc, "li#");
	}
	if (ptr1 > ptr2) {
	    li_first++;
	    temp = ptr1;
	    ptr1 = ptr2;
	    ptr2 = temp;
	}
	ptr1 += 3;
	ptr2 += 3;
	strncpy(newtc, oldtc, i = ptr1 - oldtc);
	temp = newtc + i;
	sprintf(temp, "%d", (li_first
			     ? MaxRows(screen)
			     : MaxCols(screen)));
	temp += strlen(temp);
	ptr1 = strchr(ptr1, ':');
	strncpy(temp, ptr1, i = ptr2 - ptr1);
	temp += i;
	sprintf(temp, "%d", (li_first
			     ? MaxCols(screen)
			     : MaxRows(screen)));
	ptr2 = strchr(ptr2, ':');
	strcat(temp, ptr2);
	TRACE(("   ==> %s\n", newtc));
	TRACE(("   new size %dx%d\n", MaxRows(screen), MaxCols(screen)));
    }
#endif /* USE_SYSV_ENVVARS */
}

#endif /* ! VMS */

/*
 * Does a non-blocking wait for a child process.  If the system
 * doesn't support non-blocking wait, do nothing.
 * Returns the pid of the child, or 0 or -1 if none or error.
 */
int
nonblocking_wait(void)
{
#ifdef USE_POSIX_WAIT
    pid_t pid;

    pid = waitpid(-1, NULL, WNOHANG);
#elif defined(USE_SYSV_SIGNALS) && (defined(CRAY) || !defined(SIGTSTP))
    /* cannot do non-blocking wait */
    int pid = 0;
#else /* defined(USE_SYSV_SIGNALS) && (defined(CRAY) || !defined(SIGTSTP)) */
#if defined(Lynx)
    int status;
#else
    union wait status;
#endif
    int pid;

    pid = wait3(&status, WNOHANG, (struct rusage *) NULL);
#endif /* USE_POSIX_WAIT else */
    return pid;
}

#ifndef VMS

/* ARGSUSED */
static SIGNAL_T
reapchild(int n GCC_UNUSED)
{
    int olderrno = errno;
    int pid;

    pid = wait(NULL);

#ifdef USE_SYSV_SIGNALS
    /* cannot re-enable signal before waiting for child
     * because then SVR4 loops.  Sigh.  HP-UX 9.01 too.
     */
    (void) signal(SIGCHLD, reapchild);
#endif

    do {
	if (pid == term->screen.pid) {
#ifdef DEBUG
	    if (debug)
		fputs("Exiting\n", stderr);
#endif
	    if (!hold_screen)
		need_cleanup = True;
	}
    } while ((pid = nonblocking_wait()) > 0);

    errno = olderrno;
    SIGNAL_RETURN;
}
#endif /* !VMS */

static void
remove_termcap_entry(char *buf, char *str)
{
    char *base = buf;
    char *first = base;
    int count = 0;
    size_t len = strlen(str);

    TRACE(("*** remove_termcap_entry('%s', '%s')\n", str, buf));

    while (*buf != 0) {
	if (!count && !strncmp(buf, str, len)) {
	    while (*buf != 0) {
		if (*buf == '\\')
		    buf++;
		else if (*buf == ':')
		    break;
		if (*buf != 0)
		    buf++;
	    }
	    while ((*first++ = *buf++) != 0) ;
	    TRACE(("...removed_termcap_entry('%s', '%s')\n", str, base));
	    return;
	} else if (*buf == '\\') {
	    buf++;
	} else if (*buf == ':') {
	    first = buf;
	    count = 0;
	} else if (!isspace(CharOf(*buf))) {
	    count++;
	}
	if (*buf != 0)
	    buf++;
    }
    TRACE(("...cannot remove\n"));
}

/*
 * parse_tty_modes accepts lines of the following form:
 *
 *         [SETTING] ...
 *
 * where setting consists of the words in the modelist followed by a character
 * or ^char.
 */
static int
parse_tty_modes(char *s, struct _xttymodes *modelist)
{
    struct _xttymodes *mp;
    int c;
    int count = 0;

    TRACE(("parse_tty_modes\n"));
    while (1) {
	size_t len;

	while (*s && isascii(CharOf(*s)) && isspace(CharOf(*s)))
	    s++;
	if (!*s)
	    return count;

	for (len = 0; isalnum(CharOf(s[len])); ++len) ;
	for (mp = modelist; mp->name; mp++) {
	    if (len == mp->len
		&& strncmp(s, mp->name, mp->len) == 0)
		break;
	}
	if (!mp->name)
	    return -1;

	s += mp->len;
	while (*s && isascii(CharOf(*s)) && isspace(CharOf(*s)))
	    s++;
	if (!*s)
	    return -1;

	if ((c = decode_keyvalue(&s, False)) != -1) {
	    mp->value = c;
	    mp->set = 1;
	    count++;
	    TRACE(("...parsed #%d: %s=%#x\n", count, mp->name, c));
	}
    }
}

#ifndef VMS			/* don't use pipes on OpenVMS */
int
GetBytesAvailable(int fd)
{
#if defined(FIONREAD)
    int arg;
    ioctl(fd, FIONREAD, (char *) &arg);
    return (int) arg;
#elif defined(__CYGWIN__)
    fd_set set;
    struct timeval timeout =
    {0, 0};

    FD_ZERO(&set);
    FD_SET(fd, &set);
    if (Select(fd + 1, &set, NULL, NULL, &timeout) > 0)
	return 1;
    else
	return 0;
#elif defined(FIORDCK)
    return (ioctl(fd, FIORDCHK, NULL));
#else /* !FIORDCK */
    struct pollfd pollfds[1];

    pollfds[0].fd = fd;
    pollfds[0].events = POLLIN;
    return poll(pollfds, 1, 0);
#endif
}
#endif /* !VMS */

/* Utility function to try to hide system differences from
   everybody who used to call killpg() */

int
kill_process_group(int pid, int sig)
{
    TRACE(("kill_process_group(pid=%d, sig=%d)\n", pid, sig));
#if defined(SVR4) || defined(SYSV) || !defined(X_NOT_POSIX)
    return kill(-pid, sig);
#else
    return killpg(pid, sig);
#endif
}

#if OPT_EBCDIC
int
A2E(int x)
{
    char c;
    c = x;
    __atoe_l(&c, 1);
    return c;
}

int
E2A(int x)
{
    char c;
    c = x;
    __etoa_l(&c, 1);
    return c;
}
#endif

#if defined(__QNX__) && !defined(__QNXNTO__)
#include <sys/types.h>
#include <sys/proc_msg.h>
#include <sys/kernel.h>
#include <string.h>
#include <errno.h>

struct _proc_session ps;
struct _proc_session_reply rps;

int
qsetlogin(char *login, char *ttyname)
{
    int v = getsid(getpid());

    memset(&ps, 0, sizeof(ps));
    memset(&rps, 0, sizeof(rps));

    ps.type = _PROC_SESSION;
    ps.subtype = _PROC_SUB_ACTION1;
    ps.sid = v;
    strcpy(ps.name, login);

    Send(1, &ps, &rps, sizeof(ps), sizeof(rps));

    if (rps.status < 0)
	return (rps.status);

    ps.type = _PROC_SESSION;
    ps.subtype = _PROC_SUB_ACTION2;
    ps.sid = v;
    sprintf(ps.name, "//%d%s", getnid(), ttyname);
    Send(1, &ps, &rps, sizeof(ps), sizeof(rps));

    return (rps.status);
}
#endif
