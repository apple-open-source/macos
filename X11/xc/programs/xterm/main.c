#if !defined(lint) && 0
static char *rid = "$Xorg: main.c,v 1.7 2001/02/09 02:06:02 xorgcvs Exp $";
#endif /* lint */

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

Copyright 2002 by Thomas E. Dickey

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

/* $XFree86: xc/programs/xterm/main.c,v 3.162 2002/12/27 21:05:22 dickey Exp $ */

/* main.c */

#include <version.h>
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
#endif

#endif /* OPT_TOOLBAR */

#include <pwd.h>
#include <ctype.h>

#include <data.h>
#include <error.h>
#include <menu.h>
#include <main.h>
#include <xstrings.h>
#include <xterm_io.h>

#if OPT_WIDE_CHARS
#include <charclass.h>
#include <wcwidth.h>
#endif

#ifdef __osf__
#define USE_SYSV_SIGNALS
#define WTMP
#endif

#ifdef USE_ISPTS_FLAG
static Bool IsPts = False;
#endif

#if defined(SCO) || defined(SVR4) || defined(_POSIX_SOURCE)
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
#define LASTLOG
#define WTMP
#endif

#ifdef SCO325
#define _SVID3
#endif

#ifdef __GNU__
#define USE_SYSV_PGRP
#define WTMP
#define HAS_BSD_GROUPS
#endif

#ifdef USE_TTY_GROUP
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

#if !defined(TIOCSWINSZ)
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
ttyslot()
{
    return 1;			/* yuk */
}
#endif

#else

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
#define HAS_BSD_GROUPS
#endif /* !VMS */
#endif /* !linux */

#endif /* __QNX__ */

#endif /* } !SYSV */

#if defined(SVR4) && !defined(__CYGWIN__)
#define HAS_SAVED_IDS_AND_SETEUID
#endif

#ifdef linux
#define HAS_SAVED_IDS_AND_SETEUID
#endif

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
#define HAS_SAVED_IDS_AND_SETEUID
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

#elif defined(UTMPX_FOR_UTMP)

#include <utmpx.h>
#define setutent setutxent
#define getutid getutxid
#define endutent endutxent
#define pututline pututxline

#elif defined(HAVE_UTMP)

#include <utmp.h>
#if defined(_CRAY) && (OSMAJORVERSION < 8)
extern struct utmp *getutid __((struct utmp * _Id));
#endif

#endif

#if defined(USE_LASTLOG) && defined(HAVE_LASTLOG_H)
#include <lastlog.h>
#endif

#ifdef  PUCC_PTYD
#include <local/openpty.h>
#endif /* PUCC_PTYD */

#ifdef __OpenBSD__
#include <util.h>
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

#if defined(sco) || (defined(ISC) && !defined(_POSIX_SOURCE))
#undef SIGTSTP			/* defined, but not the BSD way */
#endif

#ifdef SIGTSTP
#include <sys/wait.h>
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

#ifdef SYSV
extern char *ptsname(int);
#endif

#ifdef __cplusplus
extern "C" {
#endif

    extern int tgetent(char *ptr, char *name);
    extern char *tgetstr(char *name, char **ptr);

#ifdef __cplusplus
}
#endif
#ifndef VMS
static SIGNAL_T reapchild(int n);
static int spawn(void);
static void remove_termcap_entry(char *buf, char *str);
#ifdef USE_PTY_SEARCH
static int pty_search(int *pty);
#endif
#endif /* ! VMS */

static int get_pty(int *pty, char *from);
static void get_terminal(void);
static void resize(TScreen * s, char *oldtc, char *newtc);
static void set_owner(char *device, int uid, int gid, int mode);

static Bool added_utmp_entry = False;

#ifdef __OpenBSD__
static gid_t utmpGid = -1;
#endif

#ifdef USE_SYSV_UTMP
static Bool xterm_exiting = False;
#endif

/*
** Ordinarily it should be okay to omit the assignment in the following
** statement. Apparently the c89 compiler on AIX 4.1.3 has a bug, or does
** it? Without the assignment though the compiler will init command_to_exec
** to 0xffffffff instead of NULL; and subsequent usage, e.g. in spawn() to
** SEGV.
*/
static char **command_to_exec = NULL;

#if OPT_LUIT_PROG
static char **command_to_exec_with_luit = NULL;
#endif

#define TERMCAP_ERASE "kb"
#define VAL_INITIAL_ERASE A2E(8)

/* choose a nice default value for speed - if we make it too low, users who
 * mistakenly use $TERM set to vt100 will get padding delays
 */
#ifdef B38400			/* everyone should define this */
#define VAL_LINE_SPEED B38400
#else /* ...but xterm's used this for a long time */
#define VAL_LINE_SPEED B9600
#endif

/* allow use of system default characters if defined and reasonable */
#ifndef CBRK
#define CBRK 0
#endif
#ifndef CDSUSP
#define CDSUSP CONTROL('Y')
#endif
#ifndef CEOF
#define CEOF CONTROL('D')
#endif
#ifndef CEOL
#define CEOL 0
#endif
#ifndef CFLUSH
#define CFLUSH CONTROL('O')
#endif
#ifndef CINTR
#define CINTR 0177
#endif
#ifndef CKILL
#define CKILL '@'
#endif
#ifndef CLNEXT
#define CLNEXT CONTROL('V')
#endif
#ifndef CNUL
#define CNUL 0
#endif
#ifndef CQUIT
#define CQUIT CONTROL('\\')
#endif
#ifndef CRPRNT
#define CRPRNT CONTROL('R')
#endif
#ifndef CSTART
#define CSTART CONTROL('Q')
#endif
#ifndef CSTOP
#define CSTOP CONTROL('S')
#endif
#ifndef CSUSP
#define CSUSP CONTROL('Z')
#endif
#ifndef CSWTCH
#define CSWTCH 0
#endif
#ifndef CWERASE
#define CWERASE CONTROL('W')
#endif

#ifndef VMS
#ifdef USE_ANY_SYSV_TERMIO
/* The following structures are initialized in main() in order
** to eliminate any assumptions about the internal order of their
** contents.
*/
static struct termio d_tio;
#ifdef HAS_LTCHARS
static struct ltchars d_ltc;
#endif /* HAS_LTCHARS */

#ifdef TIOCLSET
static unsigned int d_lmode;
#endif /* TIOCLSET */

#elif defined(USE_POSIX_TERMIOS)
static struct termios d_tio;
#else /* !USE_ANY_SYSV_TERMIO && !USE_POSIX_TERMIOS */
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
#endif /* USE_ANY_SYSV_TERMIO */
#endif /* ! VMS */

/*
 * SYSV has the termio.c_cc[V] and ltchars; BSD has tchars and ltchars;
 * SVR4 has only termio.c_cc, but it includes everything from ltchars.
 * POSIX termios has termios.c_cc, which is similar to SVR4.
 */
static int override_tty_modes = 0;
/* *INDENT-OFF* */
struct _xttymodes {
    char *name;
    size_t len;
    int set;
    char value;
} ttymodelist[] = {
    { "intr",	4, 0, '\0' },	/* tchars.t_intrc ; VINTR */
#define XTTYMODE_intr 0
    { "quit",	4, 0, '\0' },	/* tchars.t_quitc ; VQUIT */
#define XTTYMODE_quit 1
    { "erase",	5, 0, '\0' },	/* sgttyb.sg_erase ; VERASE */
#define XTTYMODE_erase 2
    { "kill",	4, 0, '\0' },	/* sgttyb.sg_kill ; VKILL */
#define XTTYMODE_kill 3
    { "eof",	3, 0, '\0' },	/* tchars.t_eofc ; VEOF */
#define XTTYMODE_eof 4
    { "eol",	3, 0, '\0' },	/* VEOL */
#define XTTYMODE_eol 5
    { "swtch",	5, 0, '\0' },	/* VSWTCH */
#define XTTYMODE_swtch 6
    { "start",	5, 0, '\0' },	/* tchars.t_startc */
#define XTTYMODE_start 7
    { "stop",	4, 0, '\0' },	/* tchars.t_stopc */
#define XTTYMODE_stop 8
    { "brk",	3, 0, '\0' },	/* tchars.t_brkc */
#define XTTYMODE_brk 9
    { "susp",	4, 0, '\0' },	/* ltchars.t_suspc ; VSUSP */
#define XTTYMODE_susp 10
    { "dsusp",	5, 0, '\0' },	/* ltchars.t_dsuspc ; VDSUSP */
#define XTTYMODE_dsusp 11
    { "rprnt",	5, 0, '\0' },	/* ltchars.t_rprntc ; VREPRINT */
#define XTTYMODE_rprnt 12
    { "flush",	5, 0, '\0' },	/* ltchars.t_flushc ; VDISCARD */
#define XTTYMODE_flush 13
    { "weras",	5, 0, '\0' },	/* ltchars.t_werasc ; VWERASE */
#define XTTYMODE_weras 14
    { "lnext",	5, 0, '\0' },	/* ltchars.t_lnextc ; VLNEXT */
#define XTTYMODE_lnext 15
    { "status", 6, 0, '\0' },	/* VSTATUS */
#define XTTYMODE_status 16
    { NULL,	0, 0, '\0' },	/* end of data */
};
/* *INDENT-ON* */

#define TMODE(ind,var) if (ttymodelist[ind].set) var = ttymodelist[ind].value

static int parse_tty_modes(char *s, struct _xttymodes *modelist);

#ifdef USE_SYSV_UTMP
#if (defined(AIXV3) && (OSMAJORVERSION < 4)) && !(defined(getutid))
extern struct utmp *getutid();
#endif /* AIXV3 */

#else /* not USE_SYSV_UTMP */
static char etc_utmp[] = UTMP_FILENAME;
#endif /* USE_SYSV_UTMP */

#ifdef USE_LASTLOG
static char etc_lastlog[] = LASTLOG_FILENAME;
#endif

#ifdef WTMP
static char etc_wtmp[] = WTMP_FILENAME;
#endif

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

static int inhibit;
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

/* used by VT (charproc.c) */

#define offset(field)	XtOffsetOf(XTERM_RESOURCE, field)

static XtResource application_resources[] =
{
    {"name", "Name", XtRString, sizeof(char *),
     offset(xterm_name), XtRString, DFT_TERMTYPE},
    {"iconGeometry", "IconGeometry", XtRString, sizeof(char *),
     offset(icon_geometry), XtRString, (caddr_t) NULL},
    {XtNtitle, XtCTitle, XtRString, sizeof(char *),
     offset(title), XtRString, (caddr_t) NULL},
    {XtNiconName, XtCIconName, XtRString, sizeof(char *),
     offset(icon_name), XtRString, (caddr_t) NULL},
    {"termName", "TermName", XtRString, sizeof(char *),
     offset(term_name), XtRString, (caddr_t) NULL},
    {"ttyModes", "TtyModes", XtRString, sizeof(char *),
     offset(tty_modes), XtRString, (caddr_t) NULL},
    {"hold", "Hold", XtRBoolean, sizeof(Boolean),
     offset(hold_screen), XtRString, "false"},
    {"utmpInhibit", "UtmpInhibit", XtRBoolean, sizeof(Boolean),
     offset(utmpInhibit), XtRString, "false"},
    {"messages", "Messages", XtRBoolean, sizeof(Boolean),
     offset(messages), XtRString, "true"},
    {"sunFunctionKeys", "SunFunctionKeys", XtRBoolean, sizeof(Boolean),
     offset(sunFunctionKeys), XtRString, "false"},
#if OPT_SUNPC_KBD
    {"sunKeyboard", "SunKeyboard", XtRBoolean, sizeof(Boolean),
     offset(sunKeyboard), XtRString, "false"},
#endif
#if OPT_HP_FUNC_KEYS
    {"hpFunctionKeys", "HpFunctionKeys", XtRBoolean, sizeof(Boolean),
     offset(hpFunctionKeys), XtRString, "false"},
#endif
#if OPT_INITIAL_ERASE
    {"ptyInitialErase", "PtyInitialErase", XtRBoolean, sizeof(Boolean),
     offset(ptyInitialErase), XtRString, "false"},
    {"backarrowKeyIsErase", "BackarrowKeyIsErase", XtRBoolean, sizeof(Boolean),
     offset(backarrow_is_erase), XtRString, "false"},
#endif
    {"waitForMap", "WaitForMap", XtRBoolean, sizeof(Boolean),
     offset(wait_for_map), XtRString, "false"},
    {"useInsertMode", "UseInsertMode", XtRBoolean, sizeof(Boolean),
     offset(useInsertMode), XtRString, "false"},
#if OPT_ZICONBEEP
    {"zIconBeep", "ZIconBeep", XtRInt, sizeof(int),
     offset(zIconBeep), XtRImmediate, 0},
#endif
#if OPT_SAME_NAME
    {"sameName", "SameName", XtRBoolean, sizeof(Boolean),
     offset(sameName), XtRString, "true"},
#endif
#if OPT_SESSION_MGT
    {"sessionMgt", "SessionMgt", XtRBoolean, sizeof(Boolean),
     offset(sessionMgt), XtRString, "true"},
#endif
};
#undef offset

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
#ifdef XRENDERFONT
{"-fa",		"*faceName",	XrmoptionSepArg,	(caddr_t) NULL},
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
{"-t",		"*tekStartup",	XrmoptionNoArg,		(caddr_t) "on"},
{"+t",		"*tekStartup",	XrmoptionNoArg,		(caddr_t) "off"},
{"-ti",		"*decTerminalID",XrmoptionSepArg,	(caddr_t) NULL},
{"-tm",		"*ttyModes",	XrmoptionSepArg,	(caddr_t) NULL},
{"-tn",		"*termName",	XrmoptionSepArg,	(caddr_t) NULL},
#if OPT_WIDE_CHARS
{"-u8",		"*utf8",	XrmoptionNoArg,		(caddr_t) "2"},
{"+u8",		"*utf8",	XrmoptionNoArg,		(caddr_t) "0"},
#endif
#if OPT_LUIT_PROG
{"-lc",		"*locale",	XrmoptionNoArg,		(caddr_t) "True"},
{"+lc",		"*locale",	XrmoptionNoArg,		(caddr_t) "False"},
{"-lcc",	"*localeFilter",XrmoptionSepArg,	(caddr_t) NULL},
{"-en",		"*locale",	XrmoptionSepArg,	(caddr_t) NULL},
#endif
{"-ulc",	"*colorULMode",	XrmoptionNoArg,		(caddr_t) "off"},
{"+ulc",	"*colorULMode",	XrmoptionNoArg,		(caddr_t) "on"},
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
{"-cjk_width",	"*cjkWidth",	XrmoptionNoArg,		(caddr_t) "on"},
{"+cjk_width",	"*cjkWidth",	XrmoptionNoArg,		(caddr_t) "off"},
#endif
{"-wf",		"*waitForMap",	XrmoptionNoArg,		(caddr_t) "on"},
{"+wf",		"*waitForMap",	XrmoptionNoArg,		(caddr_t) "off"},
#if OPT_ZICONBEEP
{"-ziconbeep",  "*zIconBeep",   XrmoptionSepArg,        (caddr_t) NULL},
#endif
#if OPT_SAME_NAME
{"-samename",	"*sameName",	XrmoptionNoArg,		(caddr_t) "on"},
{"+samename",	"*sameName",	XrmoptionNoArg,		(caddr_t) "off"},
#endif
#if OPT_SESSION_MGT
{"-sm",		"*sessionMgt",	XrmoptionNoArg,		(caddr_t) "on"},
{"+sm",		"*sessionMgt",	XrmoptionNoArg,		(caddr_t) "off"},
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
#ifdef XRENDERFONT
{ "-fa pattern",           "FreeType font-selection pattern" },
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
{ "-/+bc",		   "turn on/off text cursor blinking" },
{ "-bcf milliseconds",     "time text cursor is off when blinking"},
{ "-bcn milliseconds",     "time text cursor is on when blinking"},
{ "-/+bdc",                "turn off/on display of bold as color"},
{ "-/+cb",                 "turn on/off cut-to-beginning-of-line inhibit" },
{ "-cc classrange",        "specify additional character classes" },
{ "-/+cm",                 "turn off/on ANSI color mode" },
{ "-/+cn",                 "turn on/off cut newline inhibit" },
{ "-cr color",             "text cursor color" },
{ "-/+cu",                 "turn on/off curses emulation" },
{ "-/+dc",		   "turn off/on dynamic color selection" },
#if OPT_HIGHLIGHT_COLOR
{ "-hc color",             "selection background color" },
#endif
#if OPT_HP_FUNC_KEYS
{ "-/+hf",                 "turn on/off HP Function Key escape codes" },
#endif
{ "-/+hold",		   "turn on/off logic that retains window after exit" },
#if OPT_INITIAL_ERASE
{ "-/+ie",		   "turn on/off initialization of 'erase' from pty" },
#endif
{ "-/+im",		   "use insert mode for TERMCAP" },
{ "-/+j",                  "turn on/off jump scroll" },
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
{ "-/+mesg",		   "forbid/allow messages" },
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
{ "-/+rvc",		   "turn off/on display of reverse as color" },
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
#ifdef HAVE_UTMP
{ "-/+ut",                 "turn on/off utmp support" },
#else
{ "-/+ut",                 "turn on/off utmp support (not available)" },
#endif
{ "-/+vb",                 "turn on/off visual bell" },
{ "-/+pob",                "turn on/off pop on bell" },
#if OPT_WIDE_CHARS
{ "-/+wc",                 "turn on/off wide-character mode" },
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
	    value = A2E(127);
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

/*
 * If we're linked to terminfo, tgetent() will return an empty buffer.  We
 * cannot use that to adjust the $TERMCAP variable.
 */
static Boolean
get_termcap(char *name, char *buffer, char *resized)
{
    register TScreen *screen = &term->screen;

    *buffer = 0;		/* initialize, in case we're using terminfo's tgetent */

    if (name != 0) {
	if (tgetent(buffer, name) == 1) {
	    TRACE(("get_termcap(%s) succeeded (%s)\n", name,
		   (*buffer
		    ? "ok:termcap, we can update $TERMCAP"
		    : "assuming this is terminfo")));
	    if (*buffer) {
		if (!TEK4014_ACTIVE(screen)) {
		    resize(screen, buffer, resized);
		}
	    }
	    return True;
	} else {
	    *buffer = 0;	/* just in case */
	}
    }
    return False;
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
    printf("%s(%d)\n", XFREE86_VERSION, XTERM_PATCH);
    fflush(stdout);
}

static void
Help(void)
{
    OptionHelp *opt;
    OptionHelp *list = sortedOpts(xtermOptions, optionDescList, XtNumber(optionDescList));
    char **cpp;

    fprintf(stderr,
	    "%s(%d) usage:\n    %s [-options ...] [-e command args]\n\n",
	    XFREE86_VERSION, XTERM_PATCH, ProgramName);
    fprintf(stderr, "where options include:\n");
    for (opt = list; opt->opt; opt++) {
	fprintf(stderr, "    %-28s %s\n", opt->opt, opt->desc);
    }

    putc('\n', stderr);
    for (cpp = message; *cpp; cpp++) {
	fputs(*cpp, stderr);
	putc('\n', stderr);
    }
    putc('\n', stderr);
    fflush(stderr);
}

#if defined(TIOCCONS) || defined(SRIOCSREDIR)
/* ARGSUSED */
static Boolean
ConvertConsoleSelection(Widget w GCC_UNUSED,
			Atom * selection GCC_UNUSED,
			Atom * target GCC_UNUSED,
			Atom * type GCC_UNUSED,
			XtPointer * value GCC_UNUSED,
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
#endif /* OPT_SESSION_MGT */

#if OPT_WIDE_CHARS
int (*my_wcwidth) (wchar_t);
#endif

/*
 * DeleteWindow(): Action proc to implement ICCCM delete_window.
 */
/* ARGSUSED */
static void
DeleteWindow(Widget w,
	     XEvent * event GCC_UNUSED,
	     String * params GCC_UNUSED,
	     Cardinal * num_params GCC_UNUSED)
{
#if OPT_TEK4014
    if (w == toplevel) {
	if (term->screen.Tshow)
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
		Cardinal * num_params GCC_UNUSED)
{
    switch (event->type) {
    case MappingNotify:
	XRefreshKeyboardMapping(&event->xmapping);
	break;
    }
}

XtActionsRec actionProcs[] =
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
    Boolean name = False;

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
static Boolean
ParseSccn(char *option)
{
    char *leaf = x_basename(option);
    Boolean code = False;

    if (leaf != option) {
	if (leaf - option > 1
	    && leaf - option <= PTYCHARLEN
	    && sscanf(leaf, "%d", &am_slave) == 1) {
	    size_t len = leaf - option - 1;
	    /*
	     * If the given length is less than PTYCHARLEN, that is
	     * all right because the calling application may be
	     * giving us a path for /dev/pts, which would be
	     * followed by one or more decimal digits.
	     *
	     * For fixed-width fields, it is up to the calling
	     * application to provide leading 0's, if needed.
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

#ifdef USE_SYSV_UTMP
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
    static char result[PTYCHARLEN + 4];
    char *name = my_pty_name(device);
    char *leaf = x_basename(name);

    if (name == leaf) {		/* no '/' in the name */
	int len = strlen(leaf);
	if (PTYCHARLEN < len)
	    leaf = leaf + (len - PTYCHARLEN);
	strcpy(result, leaf);
    } else {
	sprintf(result, "p%s", leaf);
    }
    TRACE(("my_utmp_id  (%s) -> '%s'\n", device, result));
    return result;
}
#endif

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

int
main(int argc, char *argv[]ENVP_ARG)
{
    Widget form_top, menu_top;
    register TScreen *screen;
    int mode;
    char *my_class = DEFCLASS;
    Window winToEmbedInto = None;

    ProgramName = argv[0];

    /* extra length in case longer tty name like /dev/ttyq255 */
    ttydev = (char *) malloc(sizeof(TTYDEV) + 80);
#ifdef USE_PTY_DEVICE
    ptydev = (char *) malloc(sizeof(PTYDEV) + 80);
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

#ifdef __OpenBSD__
    get_pty(NULL, NULL);
    seteuid(getuid());
    setuid(getuid());
#endif /* __OpenBSD__ */

    /* Do these first, since we may not be able to open the display */
    TRACE_OPTS(xtermOptions, optionDescList, XtNumber(optionDescList));
    TRACE_ARGV("Before XtOpenApplication", argv);
    if (argc > 1) {
	int n;
	int unique = 2;
	Boolean quit = True;

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

#if defined(USE_ANY_SYSV_TERMIO) || defined(USE_POSIX_TERMIOS)	/* { */
    /* Initialization is done here rather than above in order
     * to prevent any assumptions about the order of the contents
     * of the various terminal structures (which may change from
     * implementation to implementation).
     */
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

    d_tio.c_cc[VINTR] = CINTR;
    d_tio.c_cc[VQUIT] = CQUIT;
    d_tio.c_cc[VERASE] = CERASE;
    d_tio.c_cc[VKILL] = CKILL;
    d_tio.c_cc[VEOF] = CEOF;
    d_tio.c_cc[VEOL] = CNUL;
    d_tio.c_cc[VEOL2] = CNUL;
#ifdef VSWTCH
    d_tio.c_cc[VSWTCH] = CNUL;
#endif

#if defined(USE_TERMIOS) || defined(USE_POSIX_TERMIOS)	/* { */
    d_tio.c_cc[VSUSP] = CSUSP;
#ifdef VDSUSP
    d_tio.c_cc[VDSUSP] = CDSUSP;
#endif
    d_tio.c_cc[VREPRINT] = CRPRNT;
    d_tio.c_cc[VDISCARD] = CFLUSH;
    d_tio.c_cc[VWERASE] = CWERASE;
    d_tio.c_cc[VLNEXT] = CLNEXT;
    d_tio.c_cc[VMIN] = 1;
    d_tio.c_cc[VTIME] = 0;
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
    d_tio.c_cc[VINTR] = CONTROL('C');	/* '^C' */
    d_tio.c_cc[VERASE] = 0x7f;	/* DEL  */
    d_tio.c_cc[VKILL] = CONTROL('U');	/* '^U' */
    d_tio.c_cc[VQUIT] = CQUIT;	/* '^\' */
    d_tio.c_cc[VEOF] = CEOF;	/* '^D' */
    d_tio.c_cc[VEOL] = CEOL;	/* '^@' */
    d_tio.c_cc[VMIN] = 1;
    d_tio.c_cc[VTIME] = 0;
#ifdef VSWTCH
    d_tio.c_cc[VSWTCH] = CSWTCH;	/* usually '^Z' */
#endif
#ifdef VLNEXT
    d_tio.c_cc[VLNEXT] = CLNEXT;
#endif
#ifdef VWERASE
    d_tio.c_cc[VWERASE] = CWERASE;
#endif
#ifdef VREPRINT
    d_tio.c_cc[VREPRINT] = CRPRNT;
#endif
#ifdef VRPRNT
    d_tio.c_cc[VRPRNT] = CRPRNT;
#endif
#ifdef VDISCARD
    d_tio.c_cc[VDISCARD] = CFLUSH;
#endif
#ifdef VFLUSHO
    d_tio.c_cc[VFLUSHO] = CFLUSH;
#endif
#ifdef VSTOP
    d_tio.c_cc[VSTOP] = CSTOP;
#endif
#ifdef VSTART
    d_tio.c_cc[VSTART] = CSTART;
#endif
#ifdef VSUSP
    d_tio.c_cc[VSUSP] = CSUSP;
#endif
#ifdef VDSUSP
    d_tio.c_cc[VDSUSP] = CDSUSP;
#endif
#ifdef VSTATUS
    d_tio.c_cc[VSTATUS] = CSTATUS;
#endif
    /* now, try to inherit tty settings */
    {
	int i;

	for (i = 0; i <= 2; i++) {
#ifndef USE_POSIX_TERMIOS
	    struct termio deftio;
	    if (ioctl(i, TCGETA, &deftio) == 0)
#else
	    struct termios deftio;
	    if (tcgetattr(i, &deftio) == 0)
#endif
	    {
		d_tio.c_cc[VINTR] = deftio.c_cc[VINTR];
		d_tio.c_cc[VQUIT] = deftio.c_cc[VQUIT];
		d_tio.c_cc[VERASE] = deftio.c_cc[VERASE];
		d_tio.c_cc[VKILL] = deftio.c_cc[VKILL];
		d_tio.c_cc[VEOF] = deftio.c_cc[VEOF];
		d_tio.c_cc[VEOL] = deftio.c_cc[VEOL];
#ifdef VSWTCH
		d_tio.c_cc[VSWTCH] = deftio.c_cc[VSWTCH];
#endif
#ifdef VEOL2
		d_tio.c_cc[VEOL2] = deftio.c_cc[VEOL2];
#endif
#ifdef VLNEXT
		d_tio.c_cc[VLNEXT] = deftio.c_cc[VLNEXT];
#endif
#ifdef VWERASE
		d_tio.c_cc[VWERASE] = deftio.c_cc[VWERASE];
#endif
#ifdef VREPRINT
		d_tio.c_cc[VREPRINT] = deftio.c_cc[VREPRINT];
#endif
#ifdef VRPRNT
		d_tio.c_cc[VRPRNT] = deftio.c_cc[VRPRNT];
#endif
#ifdef VDISCARD
		d_tio.c_cc[VDISCARD] = deftio.c_cc[VDISCARD];
#endif
#ifdef VFLUSHO
		d_tio.c_cc[VFLUSHO] = deftio.c_cc[VFLUSHO];
#endif
#ifdef VSTOP
		d_tio.c_cc[VSTOP] = deftio.c_cc[VSTOP];
#endif
#ifdef VSTART
		d_tio.c_cc[VSTART] = deftio.c_cc[VSTART];
#endif
#ifdef VSUSP
		d_tio.c_cc[VSUSP] = deftio.c_cc[VSUSP];
#endif
#ifdef VDSUSP
		d_tio.c_cc[VDSUSP] = deftio.c_cc[VDSUSP];
#endif
#ifdef VSTATUS
		d_tio.c_cc[VSTATUS] = deftio.c_cc[VSTATUS];
#endif
		break;
	    }
	}
    }
#ifdef HAS_LTCHARS		/* { */
    d_ltc.t_suspc = '\000';	/* t_suspc */
    d_ltc.t_dsuspc = '\000';	/* t_dsuspc */
    d_ltc.t_rprntc = '\377';	/* reserved... */
    d_ltc.t_flushc = '\377';
    d_ltc.t_werasc = '\377';
    d_ltc.t_lnextc = '\377';
#endif /* } HAS_LTCHARS */
#if defined(USE_TERMIOS) || defined(USE_POSIX_TERMIOS)	/* { */
    d_tio.c_cc[VSUSP] = CSUSP;
#ifdef VDSUSP
    d_tio.c_cc[VDSUSP] = '\000';
#endif
#ifdef VSTATUS
    d_tio.c_cc[VSTATUS] = '\377';
#endif
#ifdef VREPRINT
    d_tio.c_cc[VREPRINT] = '\377';
#endif
#ifdef VDISCARD
    d_tio.c_cc[VDISCARD] = '\377';
#endif
#ifdef VWERASE
    d_tio.c_cc[VWERASE] = '\377';
#endif
#ifdef VLNEXT
    d_tio.c_cc[VLNEXT] = '\377';
#endif
#endif /* } USE_TERMIOS */
#ifdef TIOCLSET			/* { */
    d_lmode = 0;
#endif /* } TIOCLSET */
#endif /* } macII, ATT, CRAY */
#endif /* } USE_ANY_SYSV_TERMIO || USE_POSIX_TERMIOS */

    /* Init the Toolkit. */
    {
#ifdef HAS_SAVED_IDS_AND_SETEUID
	uid_t euid = geteuid();
	gid_t egid = getegid();
	uid_t ruid = getuid();
	gid_t rgid = getgid();

	if (setegid(rgid) == -1) {
#ifdef __MVS__
	    if (!(errno == EMVSERR))	/* could happen if _BPX_SHAREAS=REUSE */
#endif
		(void) fprintf(stderr, "setegid(%d): %s\n",
			       (int) rgid, strerror(errno));
	}

	if (seteuid(ruid) == -1) {
#ifdef __MVS__
	    if (!(errno == EMVSERR))
#endif
		(void) fprintf(stderr, "seteuid(%d): %s\n",
			       (int) ruid, strerror(errno));
	}
#endif

	XtSetErrorHandler(xt_error);
#if OPT_SESSION_MGT
	toplevel = XtOpenApplication(&app_con, my_class,
				     optionDescList,
				     XtNumber(optionDescList),
				     &argc, argv, fallback_resources,
				     sessionShellWidgetClass,
				     NULL, 0);
#else
	toplevel = XtAppInitialize(&app_con, my_class,
				   optionDescList,
				   XtNumber(optionDescList),
				   &argc, argv, fallback_resources,
				   NULL, 0);
#endif /* OPT_SESSION_MGT */
	XtSetErrorHandler((XtErrorHandler) 0);

	XtGetApplicationResources(toplevel, (XtPointer) & resource,
				  application_resources,
				  XtNumber(application_resources), NULL, 0);

#ifdef HAS_SAVED_IDS_AND_SETEUID
	if (seteuid(euid) == -1) {
#ifdef __MVS__
	    if (!(errno == EMVSERR))
#endif
		(void) fprintf(stderr, "seteuid(%d): %s\n",
			       (int) euid, strerror(errno));
	}

	if (setegid(egid) == -1) {
#ifdef __MVS__
	    if (!(errno == EMVSERR))
#endif
		(void) fprintf(stderr, "setegid(%d): %s\n",
			       (int) egid, strerror(errno));
	}
#endif

#ifdef __OpenBSD__
	if (resource.utmpInhibit) {
	    /* Can totally revoke group privs */
	    setegid(getgid());
	    setgid(getgid());
	}
#endif
    }

    waiting_for_initial_map = resource.wait_for_map;

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
	    override_tty_modes = 1;
	}
    }
#if OPT_ZICONBEEP
    zIconBeep = resource.zIconBeep;
    zIconBeep_flagged = False;
    if (zIconBeep > 100 || zIconBeep < -100) {
	zIconBeep = 0;		/* was 100, but I prefer to defaulting off. */
	fprintf(stderr,
		"a number between -100 and 100 is required for zIconBeep.  0 used by default\n");
    }
#endif /* OPT_ZICONBEEP */
#if OPT_SAME_NAME
    sameName = resource.sameName;
#endif
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
	if (**argv != '-')
	    Syntax(*argv);

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
		    (sbuf.st_uid == getuid()) &&
		    !access("/dev/console", R_OK | W_OK)) {
		    Console = TRUE;
		} else
		    Console = FALSE;
	    }
#else /* __sgi */
	    Console = TRUE;
#endif /* __sgi */
#endif /* TIOCCONS */
	    continue;
	case 'S':
	    if (!ParseSccn(*argv + 2))
		Syntax(*argv);
	    continue;
#ifdef DEBUG
	case 'D':
	    debug = TRUE;
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
	    break;

	default:
	    Syntax(*argv);
	}
	break;
    }

    SetupMenus(toplevel, &form_top, &menu_top);

    term = (XtermWidget) XtVaCreateManagedWidget("vt100", xtermWidgetClass,
						 form_top,
#if OPT_TOOLBAR
						 XtNmenuBar, menu_top,
						 XtNresizable, True,
						 XtNfromVert, menu_top,
						 XtNleft, XawChainLeft,
						 XtNright, XawChainRight,
						 XtNbottom, XawChainBottom,
#endif
						 (XtPointer) 0);
    /* this causes the initialize method to be called */

#if OPT_HP_FUNC_KEYS
    init_keyboard_type(keyboardIsHP, resource.hpFunctionKeys);
#endif
    init_keyboard_type(keyboardIsSun, resource.sunFunctionKeys);
#if OPT_SUNPC_KBD
    init_keyboard_type(keyboardIsVT220, resource.sunKeyboard);
#endif

    screen = &term->screen;

    inhibit = 0;
#ifdef ALLOWLOGGING
    if (term->misc.logInhibit)
	inhibit |= I_LOG;
#endif
    if (term->misc.signalInhibit)
	inhibit |= I_SIGNAL;
#if OPT_TEK4014
    if (term->misc.tekInhibit)
	inhibit |= I_TEK;
#endif

#if OPT_WIDE_CHARS
    my_wcwidth = &mk_wcwidth;
    if (term->misc.cjk_width)
	my_wcwidth = &mk_wcwidth_cjk;
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
	    c = malloc((n + 3 + u) * sizeof(char *));
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
#if OPT_TEK4014
    if (inhibit & I_TEK)
	screen->TekEmu = FALSE;

    if (screen->TekEmu && !TekInit())
	SysError(ERROR_INIT);
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
	    if (creat_as(getuid(), getgid(), False, dbglogfile, 0666)) {
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

    /* open a terminal for client */
    get_terminal();

    spawn();

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
	sprintf(buf, "%lx\n", XtWindow(XtParent(CURRENT_EMU(screen))));
	write(screen->respond, buf, strlen(buf));
    }

    screen->inhibit = inhibit;

#ifdef AIXV3
#if (OSMAJORVERSION < 4)
    /* In AIXV3, xterms started from /dev/console have CLOCAL set.
     * This means we need to clear CLOCAL so that SIGHUP gets sent
     * to the slave-pty process when xterm exits.
     */

    {
	struct termio tio;

	if (ioctl(screen->respond, TCGETA, &tio) == -1)
	    SysError(ERROR_TIOCGETP);

	tio.c_cflag &= ~(CLOCAL);

	if (ioctl(screen->respond, TCSETA, &tio) == -1)
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

    for (;;) {
#if OPT_TEK4014
	if (screen->TekEmu)
	    TekRun();
	else
#endif
	    VTRun();
    }
}

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

#ifdef __OpenBSD__
    static int m_tty = -1;
    static int m_pty = -1;
    struct group *ttygrp;

    if (pty == NULL) {
	result = openpty(&m_pty, &m_tty, ttydev, NULL, NULL);

	seteuid(0);
	if ((ttygrp = getgrnam(TTY_GROUP_NAME)) != 0) {
	    set_owner(ttydev, getuid(), ttygrp->gr_gid, 0600);
	} else {
	    set_owner(ttydev, getuid(), getgid(), 0600);
	}
	seteuid(getuid());
    } else if (m_pty != -1) {
	*pty = m_pty;
	result = 0;
    } else {
	result = -1;
    }
#elif defined(PUCC_PTYD)

    result = ((*pty = openrpty(ttydev, ptydev,
			       (resource.utmpInhibit ? OPTY_NOP : OPTY_LOGIN),
			       getuid(), from)) < 0);

#elif defined(__osf__) || (defined(__GLIBC__) && !defined(USE_USG_PTYS)) || defined(__NetBSD__)

    int tty;
    result = openpty(pty, &tty, ttydev, NULL, NULL);

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
       device we're dealing with in routine spawn().  That's the reason
       for the "if (IsPts)" statement in spawn(); we have two different
       device types which need to be handled differently.
     */
    result = pty_search(pty);

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
    result = ((*pty = open("/dev/ptmx", O_RDWR)) < 0);
#endif
#if defined(SVR4) || defined(SCO325) || defined(USE_ISPTS_FLAG)
    if (!result)
	strcpy(ttydev, ptsname(*pty));
#ifdef USE_ISPTS_FLAG
    IsPts = !result;		/* true if we're successful */
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
	strcpy(ttydev, ptsname(*pty));
	result = 0;
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

static void
get_terminal(void)
/*
 * sets up X and initializes the terminal structure except for term.buf.fildes.
 */
{
    register TScreen *screen = &term->screen;

    screen->arrow = make_colored_cursor(XC_left_ptr,
					screen->mousecolor,
					screen->mousecolorback);
}

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

/*
 * declared outside USE_HANDSHAKE so HsSysError() callers can use
 */
static int pc_pipe[2];		/* this pipe is used for parent to child transfer */
static int cp_pipe[2];		/* this pipe is used for child to parent transfer */

#ifdef USE_HANDSHAKE
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

/* HsSysError()
 *
 * This routine does the equivalent of a SysError but it handshakes
 * over the errno and error exit to the master process so that it can
 * display our error message and exit with our exit code so that the
 * user can see it.
 */

static void
HsSysError(int pf, int error)
{
    handshake_t handshake;

    handshake.status = PTY_FATALERROR;
    handshake.error = errno;
    handshake.fatal_error = error;
    strcpy(handshake.buffer, ttydev);
    write(pf, (char *) &handshake, sizeof(handshake));
    exit(error);
}

void
first_map_occurred(void)
{
    handshake_t handshake;
    register TScreen *screen = &term->screen;

    handshake.status = PTY_EXEC;
    handshake.rows = screen->max_row;
    handshake.cols = screen->max_col;
    write(pc_pipe[1], (char *) &handshake, sizeof(handshake));
    close(cp_pipe[0]);
    close(pc_pipe[1]);
    waiting_for_initial_map = False;
}
#else
/*
 * temporary hack to get xterm working on att ptys
 */
static void
HsSysError(int pf GCC_UNUSED, int error)
{
    fprintf(stderr, "%s: fatal pty error %d (errno=%d) on tty %s\n",
	    xterm_name, error, errno, ttydev);
    exit(error);
}

void
first_map_occurred(void)
{
    return;
}
#endif /* USE_HANDSHAKE else !USE_HANDSHAKE */

#ifndef VMS
extern char **environ;

static void
set_owner(char *device, int uid, int gid, int mode)
{
    if (chown(device, uid, gid) < 0) {
	if (errno != ENOENT
	    && getuid() == 0) {
	    fprintf(stderr, "Cannot chown %s to %d,%d: %s\n",
		    device, uid, gid, strerror(errno));
	}
    }
    chmod(device, mode);
}

static int
spawn(void)
/*
 *  Inits pty and tty and forks a login process.
 *  Does not close fd Xsocket.
 *  If slave, the pty named in passedPty is already open for use
 */
{
    register TScreen *screen = &term->screen;
#ifdef USE_HANDSHAKE
    handshake_t handshake;
    int done;
#endif
#if OPT_INITIAL_ERASE
    int initial_erase = VAL_INITIAL_ERASE;
#endif
    int rc;
    int tty = -1;
#ifdef USE_ANY_SYSV_TERMIO
    struct termio tio;
#ifdef TIOCLSET
    unsigned lmode;
#endif /* TIOCLSET */
#ifdef HAS_LTCHARS
    struct ltchars ltc;
#endif /* HAS_LTCHARS */
#elif defined(USE_POSIX_TERMIOS)
    struct termios tio;
#else /* !USE_ANY_SYSV_TERMIO && !USE_POSIX_TERMIOS */
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
#endif /* USE_ANY_SYSV_TERMIO */

    char termcap[TERMCAP_SIZE];
    char newtc[TERMCAP_SIZE];
    char *ptr, *shname, *shname_minus;
    int i, no_dev_tty = FALSE;
    char **envnew;		/* new environment */
    int envsize;		/* elements in new environment */
    char buf[64];
    char *TermName = NULL;
#if defined(TIOCSSIZE) && (defined(sun) && !defined(SVR4))
    struct ttysize ts;
#elif defined(TIOCSWINSZ)
    struct winsize ws;
#endif /* sun vs TIOCSWINSZ */
    struct passwd *pw = NULL;
    char *login_name = NULL;
#ifdef HAVE_UTMP
    struct UTMP_STR utmp;
#ifdef USE_SYSV_UTMP
    struct UTMP_STR *utret;
#endif
#ifdef USE_LASTLOG
    struct lastlog lastlog;
#endif /* USE_LASTLOG */
#endif /* HAVE_UTMP */

    screen->uid = getuid();
    screen->gid = getgid();

    termcap[0] = '\0';
    newtc[0] = '\0';

#ifdef SIGTTOU
    /* so that TIOCSWINSZ || TIOCSIZE doesn't block */
    signal(SIGTTOU, SIG_IGN);
#endif

    if (am_slave >= 0) {
	screen->respond = am_slave;
	set_pty_id(ttydev, passedPty);
#ifdef USE_PTY_DEVICE
	set_pty_id(ptydev, passedPty);
#endif
	setgid(screen->gid);
	setuid(screen->uid);
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
	    tty = open("/dev/tty", O_RDWR);
	    alarm(0);
	    tty_got_hung = False;
	} else {
	    tty_got_hung = True;
	    tty = -1;
	    errno = ENXIO;
	}
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
	no_dev_tty = FALSE;
	if (tty < 0) {
	    if (tty_got_hung || errno == ENXIO || errno == EIO ||
#ifdef ENODEV
		errno == ENODEV ||
#endif
#ifdef __CYGWIN__
		errno == ENOENT ||
#endif
		errno == EINVAL || errno == ENOTTY || errno == EACCES) {
		no_dev_tty = TRUE;
#ifdef HAS_LTCHARS
		ltc = d_ltc;
#endif /* HAS_LTCHARS */
#ifdef TIOCLSET
		lmode = d_lmode;
#endif /* TIOCLSET */
#if defined(USE_ANY_SYSV_TERMIO) || defined(USE_POSIX_TERMIOS)
		tio = d_tio;
#else /* not USE_ANY_SYSV_TERMIO and not USE_POSIX_TERMIOS */
		sg = d_sg;
		tc = d_tc;
		discipline = d_disipline;
#ifdef sony
		jmode = d_jmode;
		jtc = d_jtc;
#endif /* sony */
#endif /* USE_ANY_SYSV_TERMIO or USE_POSIX_TERMIOS */
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
	    if (ioctl(tty, TIOCGLTC, &ltc) == -1)
		ltc = d_ltc;
#endif /* HAS_LTCHARS */
#ifdef TIOCLSET
	    if (ioctl(tty, TIOCLGET, &lmode) == -1)
		lmode = d_lmode;
#endif /* TIOCLSET */
#ifdef USE_ANY_SYSV_TERMIO
	    if ((rc = ioctl(tty, TCGETA, &tio)) == -1)
		tio = d_tio;
#elif defined(USE_POSIX_TERMIOS)
	    if ((rc = tcgetattr(tty, &tio)) == -1)
		tio = d_tio;
#else /* !USE_ANY_SYSV_TERMIO && !USE_POSIX_TERMIOS */
	    if ((rc = ioctl(tty, TIOCGETP, (char *) &sg)) == -1)
		sg = d_sg;
	    if (ioctl(tty, TIOCGETC, (char *) &tc) == -1)
		tc = d_tc;
	    if (ioctl(tty, TIOCGETD, (char *) &discipline) == -1)
		discipline = d_disipline;
#ifdef sony
	    if (ioctl(tty, TIOCKGET, (char *) &jmode) == -1)
		jmode = d_jmode;
	    if (ioctl(tty, TIOCKGETC, (char *) &jtc) == -1)
		jtc = d_jtc;
#endif /* sony */
#endif /* USE_ANY_SYSV_TERMIO */

	    /*
	     * If ptyInitialErase is set, we want to get the pty's
	     * erase value.  Just in case that will fail, first get
	     * the value from /dev/tty, so we will have something
	     * at least.
	     */
#if OPT_INITIAL_ERASE
	    if (resource.ptyInitialErase) {
#ifdef USE_ANY_SYSV_TERMIO
		initial_erase = tio.c_cc[VERASE];
#elif defined(USE_POSIX_TERMIOS)
		initial_erase = tio.c_cc[VERASE];
#else /* !USE_ANY_SYSV_TERMIO && !USE_POSIX_TERMIOS */
		initial_erase = sg.sg_erase;
#endif /* USE_ANY_SYSV_TERMIO */
		TRACE(("%s initial_erase:%d (from /dev/tty)\n",
		       rc == 0 ? "OK" : "FAIL",
		       initial_erase));
	    }
#endif

	    close(tty);
	    /* tty is no longer an open fd! */
	    tty = -1;
	}

	if (get_pty(&screen->respond, XDisplayString(screen->display))) {
	    SysError(ERROR_PTYS);
	}
#if OPT_INITIAL_ERASE
	if (resource.ptyInitialErase) {
#ifdef USE_ANY_SYSV_TERMIO
	    struct termio my_tio;
	    if ((rc = ioctl(screen->respond, TCGETA, &my_tio)) == 0)
		initial_erase = my_tio.c_cc[VERASE];
#elif defined(USE_POSIX_TERMIOS)
	    struct termios my_tio;
	    if ((rc = tcgetattr(screen->respond, &my_tio)) == 0)
		initial_erase = my_tio.c_cc[VERASE];
#else /* !USE_ANY_SYSV_TERMIO && !USE_POSIX_TERMIOS */
	    struct sgttyb my_sg;
	    if ((rc = ioctl(screen->respond, TIOCGETP, (char *) &my_sg)) == 0)
		initial_erase = my_sg.sg_erase;
#endif /* USE_ANY_SYSV_TERMIO */
	    TRACE(("%s initial_erase:%d (from pty)\n",
		   (rc == 0) ? "OK" : "FAIL",
		   initial_erase));
	}
#endif /* OPT_INITIAL_ERASE */
    }

    /* avoid double MapWindow requests */
    XtSetMappedWhenManaged(XtParent(CURRENT_EMU(screen)), False);

    wm_delete_window = XInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW",
				   False);

    if (!TEK4014_ACTIVE(screen))
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
	XtOwnSelection(XtParent(CURRENT_EMU(screen)),
		       mit_console, CurrentTime,
		       ConvertConsoleSelection, NULL, NULL);
    }
#endif
#if OPT_TEK4014
    if (screen->TekEmu) {
	envnew = tekterm;
	ptr = newtc;
    } else
#endif
    {
	envnew = vtterm;
	ptr = termcap;
    }

    /*
     * This used to exit if no termcap entry was found for the specified
     * terminal name.  That's a little unfriendly, so instead we'll allow
     * the program to proceed (but not to set $TERMCAP) if the termcap
     * entry is not found.
     */
    get_termcap(TermName = resource.term_name, ptr, newtc);

    /*
     * This block is invoked only if there was no terminal name specified
     * by the command-line option "-tn".
     */
    if (!TermName) {
	TermName = *envnew;
	while (*envnew != NULL) {
	    if (get_termcap(*envnew, ptr, newtc)) {
		TermName = *envnew;
		break;
	    }
	    envnew++;
	}
    }

    /*
     * Check if ptyInitialErase is not set.  If so, we rely on the termcap
     * (or terminfo) to tell us what the erase mode should be set to.
     */
#if OPT_INITIAL_ERASE
    TRACE(("resource ptyInitialErase is %sset\n",
	   resource.ptyInitialErase ? "" : "not "));
    if (!resource.ptyInitialErase) {
	char temp[1024], *p = temp;
	char *s = tgetstr(TERMCAP_ERASE, &p);
	TRACE(("...extracting initial_erase value from termcap\n"));
	if (s != 0) {
	    initial_erase = decode_keyvalue(&s, True);
	}
    }
    TRACE(("...initial_erase:%d\n", initial_erase));

    TRACE(("resource backarrowKeyIsErase is %sset\n",
	   resource.backarrow_is_erase ? "" : "not "));
    if (resource.backarrow_is_erase) {	/* see input.c */
	if (initial_erase == 127) {
	    term->keyboard.flags &= ~MODE_DECBKM;
	} else {
	    term->keyboard.flags |= MODE_DECBKM;
	    term->keyboard.reset_DECBKM = 1;
	}
	TRACE(("...sets DECBKM %s\n",
	       (term->keyboard.flags & MODE_DECBKM) ? "on" : "off"));
    } else {
	term->keyboard.reset_DECBKM = 2;
    }
#endif /* OPT_INITIAL_ERASE */

#if defined(TIOCSSIZE) && (defined(sun) && !defined(SVR4))
    /* tell tty how big window is */
    if (TEK4014_ACTIVE(screen)) {
	ts.ts_lines = 38;
	ts.ts_cols = 81;
    } else {
	ts.ts_lines = screen->max_row + 1;
	ts.ts_cols = screen->max_col + 1;
    }
    i = ioctl(screen->respond, TIOCSSIZE, &ts);
    TRACE(("spawn TIOCSSIZE %dx%d return %d\n", ts.ts_lines, ts.ts_cols, i));
#elif defined(TIOCSWINSZ)
    /* tell tty how big window is */
#if OPT_TEK4014
    if (TEK4014_ACTIVE(screen)) {
	ws.ws_row = 38;
	ws.ws_col = 81;
	ws.ws_xpixel = TFullWidth(screen);
	ws.ws_ypixel = TFullHeight(screen);
    } else
#endif
    {
	ws.ws_row = screen->max_row + 1;
	ws.ws_col = screen->max_col + 1;
	ws.ws_xpixel = FullWidth(screen);
	ws.ws_ypixel = FullHeight(screen);
    }
    i = ioctl(screen->respond, TIOCSWINSZ, (char *) &ws);
    TRACE(("spawn TIOCSWINSZ %dx%d return %d\n", ws.ws_row, ws.ws_col, i));
#endif /* sun vs TIOCSWINSZ */

#if defined(USE_UTEMPTER)
#undef UTMP
    if (!resource.utmpInhibit) {
	addToUtmp(ttydev, NULL, screen->respond);
	added_utmp_entry = True;
    }
#endif

    if (am_slave < 0) {
#ifdef USE_HANDSHAKE
	if (pipe(pc_pipe) || pipe(cp_pipe))
	    SysError(ERROR_FORK);
#endif
	TRACE(("Forking...\n"));
	if ((screen->pid = fork()) == -1)
	    SysError(ERROR_FORK);

	if (screen->pid == 0) {
	    /*
	     * now in child process
	     */
	    TRACE_CHILD
#if defined(_POSIX_SOURCE) || defined(SVR4) || defined(__convex__) || defined(SCO325) || defined(__QNX__)
		int pgrp = setsid();	/* variable may not be used... */
#else
		int pgrp = getpid();
#endif

#ifdef USE_USG_PTYS
#ifdef USE_ISPTS_FLAG
	    if (IsPts) {	/* SYSV386 supports both, which did we open? */
#endif
		int ptyfd = 0;
		char *pty_name = 0;

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
		if (!getenv("CONSEM") && ioctl(ptyfd, I_PUSH, "consem") < 0) {
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
		tty = ptyfd;
		close(screen->respond);
#ifdef TIOCSWINSZ
		/* tell tty how big window is */
#if OPT_TEK4014
		if (TEK4014_ACTIVE(screen)) {
		    ws.ws_row = 24;
		    ws.ws_col = 80;
		    ws.ws_xpixel = TFullWidth(screen);
		    ws.ws_ypixel = TFullHeight(screen);
		} else
#endif
		{
		    ws.ws_row = screen->max_row + 1;
		    ws.ws_col = screen->max_col + 1;
		    ws.ws_xpixel = FullWidth(screen);
		    ws.ws_ypixel = FullHeight(screen);
		}
#endif
#ifdef USE_ISPTS_FLAG
	    } else {		/* else pty, not pts */
#endif
#endif /* USE_USG_PTYS */

#ifdef USE_HANDSHAKE		/* warning, goes for a long ways */
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
		close(screen->respond);

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
		while (1) {
#if defined(TIOCNOTTY) && (!defined(__GLIBC__) || (__GLIBC__ < 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ < 1)))
		    if (!no_dev_tty && (tty = open("/dev/tty", O_RDWR)) >= 0) {
			ioctl(tty, TIOCNOTTY, (char *) NULL);
			close(tty);
		    }
#endif /* TIOCNOTTY && !glibc >= 2.1 */
#ifdef CSRG_BASED
		    (void) revoke(ttydev);
#endif
		    if ((tty = open(ttydev, O_RDWR)) >= 0) {
#if defined(CRAY) && defined(TCSETCTTY)
			/* make /dev/tty work */
			ioctl(tty, TCSETCTTY, 0);
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
		    ioctl(tty, TIOCSCTTY, 0);
#endif
		    /* let our master know that the open failed */
		    handshake.status = PTY_BAD;
		    handshake.error = errno;
		    strcpy(handshake.buffer, ttydev);
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
		    ttydev = (char *) malloc((unsigned)
					     (strlen(handshake.buffer) + 1));
		    if (ttydev == NULL) {
			SysError(ERROR_SPREALLOC);
		    }
		    strcpy(ttydev, handshake.buffer);
		}

		/* use the same tty name that everyone else will use
		 * (from ttyname)
		 */
		if ((ptr = ttyname(tty)) != 0) {
		    /* it may be bigger */
		    ttydev = (char *) realloc(ttydev,
					      (unsigned) (strlen(ptr) + 1));
		    if (ttydev == NULL) {
			SysError(ERROR_SPREALLOC);
		    }
		    (void) strcpy(ttydev, ptr);
		}
#ifdef USE_ISPTS_FLAG
	    }			/* end of IsPts else clause */
#endif

#endif /* USE_HANDSHAKE -- from near fork */

#ifdef USE_TTY_GROUP
	    {
		struct group *ttygrp;
		if ((ttygrp = getgrnam(TTY_GROUP_NAME)) != 0) {
		    /* change ownership of tty to real uid, "tty" gid */
		    set_owner(ttydev, screen->uid, ttygrp->gr_gid,
			      (resource.messages ? 0620 : 0600));
		} else {
		    /* change ownership of tty to real group and user id */
		    set_owner(ttydev, screen->uid, screen->gid,
			      (resource.messages ? 0622 : 0600));
		}
		endgrent();
	    }
#else /* else !USE_TTY_GROUP */
	    /* change ownership of tty to real group and user id */
	    set_owner(ttydev, screen->uid, screen->gid,
		      (resource.messages ? 0622 : 0600));
#endif /* USE_TTY_GROUP */

	    /*
	     * set up the tty modes
	     */
	    {
#if defined(USE_ANY_SYSV_TERMIO) || defined(USE_POSIX_TERMIOS)
#if defined(umips) || defined(CRAY) || defined(linux)
		/* If the control tty had its modes screwed around with,
		   eg. by lineedit in the shell, or emacs, etc. then tio
		   will have bad values.  Let's just get termio from the
		   new tty and tailor it.  */
		if (ioctl(tty, TCGETA, &tio) == -1)
		    SysError(ERROR_TIOCGETP);
		tio.c_lflag |= ECHOE;
#endif /* umips */
		/* Now is also the time to change the modes of the
		 * child pty.
		 */
		/* input: nl->nl, don't ignore cr, cr->nl */
		tio.c_iflag &= ~(INLCR | IGNCR);
		tio.c_iflag |= ICRNL;
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
		tio.c_cflag &= ~CSIZE;
		if (screen->input_eight_bits)
		    tio.c_cflag |= CS8;
		else
		    tio.c_cflag |= CS7;
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
#ifndef __MVS__
		/* reset EOL to default value */
		tio.c_cc[VEOL] = CEOL;	/* '^@' */
		/* certain shells (ksh & csh) change EOF as well */
		tio.c_cc[VEOF] = CEOF;	/* '^D' */
#else
		if (tio.c_cc[VEOL] == 0)
		    tio.c_cc[VEOL] = CEOL;	/* '^@' */
		if (tio.c_cc[VEOF] == 0)
		    tio.c_cc[VEOF] = CEOF;	/* '^D' */
#endif
#ifdef VLNEXT
		tio.c_cc[VLNEXT] = CLNEXT;
#endif
#ifdef VWERASE
		tio.c_cc[VWERASE] = CWERASE;
#endif
#ifdef VREPRINT
		tio.c_cc[VREPRINT] = CRPRNT;
#endif
#ifdef VRPRNT
		tio.c_cc[VRPRNT] = CRPRNT;
#endif
#ifdef VDISCARD
		tio.c_cc[VDISCARD] = CFLUSH;
#endif
#ifdef VFLUSHO
		tio.c_cc[VFLUSHO] = CFLUSH;
#endif
#ifdef VSTOP
		tio.c_cc[VSTOP] = CSTOP;
#endif
#ifdef VSTART
		tio.c_cc[VSTART] = CSTART;
#endif
#ifdef VSUSP
		tio.c_cc[VSUSP] = CSUSP;
#endif
#ifdef VDSUSP
		tio.c_cc[VDSUSP] = CDSUSP;
#endif
		if (override_tty_modes) {
		    /* sysv-specific */
		    TMODE(XTTYMODE_intr, tio.c_cc[VINTR]);
		    TMODE(XTTYMODE_quit, tio.c_cc[VQUIT]);
		    TMODE(XTTYMODE_erase, tio.c_cc[VERASE]);
		    TMODE(XTTYMODE_kill, tio.c_cc[VKILL]);
		    TMODE(XTTYMODE_eof, tio.c_cc[VEOF]);
		    TMODE(XTTYMODE_eol, tio.c_cc[VEOL]);
#ifdef VSWTCH
		    TMODE(XTTYMODE_swtch, tio.c_cc[VSWTCH]);
#endif
#ifdef VSUSP
		    TMODE(XTTYMODE_susp, tio.c_cc[VSUSP]);
#endif
#ifdef VDSUSP
		    TMODE(XTTYMODE_dsusp, tio.c_cc[VDSUSP]);
#endif
#ifdef VREPRINT
		    TMODE(XTTYMODE_rprnt, tio.c_cc[VREPRINT]);
#endif
#ifdef VRPRNT
		    TMODE(XTTYMODE_rprnt, tio.c_cc[VRPRNT]);
#endif
#ifdef VDISCARD
		    TMODE(XTTYMODE_flush, tio.c_cc[VDISCARD]);
#endif
#ifdef VFLUSHO
		    TMODE(XTTYMODE_flush, tio.c_cc[VFLUSHO]);
#endif
#ifdef VWERASE
		    TMODE(XTTYMODE_weras, tio.c_cc[VWERASE]);
#endif
#ifdef VLNEXT
		    TMODE(XTTYMODE_lnext, tio.c_cc[VLNEXT]);
#endif
#ifdef VSTART
		    TMODE(XTTYMODE_start, tio.c_cc[VSTART]);
#endif
#ifdef VSTOP
		    TMODE(XTTYMODE_stop, tio.c_cc[VSTOP]);
#endif
#ifdef VSTATUS
		    TMODE(XTTYMODE_status, tio.c_cc[VSTATUS]);
#endif
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
		if (ioctl(tty, TIOCSLTC, &ltc) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCSETC);
#endif /* HAS_LTCHARS */
#ifdef TIOCLSET
		if (ioctl(tty, TIOCLSET, (char *) &lmode) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCLSET);
#endif /* TIOCLSET */
#ifndef USE_POSIX_TERMIOS
		if (ioctl(tty, TCSETA, &tio) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCSETP);
#else /* USE_POSIX_TERMIOS */
		if (tcsetattr(tty, TCSANOW, &tio) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCSETP);
#endif /* USE_POSIX_TERMIOS */
#else /* USE_ANY_SYSV_TERMIO or USE_POSIX_TERMIOS */
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

		if (ioctl(tty, TIOCSETP, (char *) &sg) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCSETP);
		if (ioctl(tty, TIOCSETC, (char *) &tc) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCSETC);
		if (ioctl(tty, TIOCSETD, (char *) &discipline) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCSETD);
		if (ioctl(tty, TIOCSLTC, (char *) &ltc) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCSLTC);
		if (ioctl(tty, TIOCLSET, (char *) &lmode) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCLSET);
#ifdef sony
		if (ioctl(tty, TIOCKSET, (char *) &jmode) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCKSET);
		if (ioctl(tty, TIOCKSETC, (char *) &jtc) == -1)
		    HsSysError(cp_pipe[1], ERROR_TIOCKSETC);
#endif /* sony */
#endif /* !USE_ANY_SYSV_TERMIO */
#if defined(TIOCCONS) || defined(SRIOCSREDIR)
		if (Console) {
#ifdef TIOCCONS
		    int on = 1;
		    if (ioctl(tty, TIOCCONS, (char *) &on) == -1)
			fprintf(stderr, "%s: cannot open console: %s\n",
				xterm_name, strerror(errno));
#endif
#ifdef SRIOCSREDIR
		    int fd = open("/dev/console", O_RDWR);
		    if (fd == -1 || ioctl(fd, SRIOCSREDIR, tty) == -1)
			fprintf(stderr, "%s: cannot open console: %s\n",
				xterm_name, strerror(errno));
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
	     * If we're not asked to make the parent process set the
	     * terminal's erase mode, and if we had no ttyModes resource,
	     * then set the terminal's erase mode from our best guess.
	     */
#if OPT_INITIAL_ERASE
	    TRACE(("check if we should set erase to %d:%s\n\tptyInitialErase:%d,\n\toveride_tty_modes:%d,\n\tXTTYMODE_erase:%d\n",
		   initial_erase,
		   (!resource.ptyInitialErase
		    && !override_tty_modes
		    && !ttymodelist[XTTYMODE_erase].set)
		   ? "YES" : "NO",
		   resource.ptyInitialErase,
		   override_tty_modes,
		   ttymodelist[XTTYMODE_erase].set));
	    if (!resource.ptyInitialErase
		&& !override_tty_modes
		&& !ttymodelist[XTTYMODE_erase].set) {
		int old_erase;
#ifdef USE_ANY_SYSV_TERMIO
		if (ioctl(tty, TCGETA, &tio) == -1)
		    tio = d_tio;
		old_erase = tio.c_cc[VERASE];
		tio.c_cc[VERASE] = initial_erase;
		rc = ioctl(tty, TCSETA, &tio);
#elif defined(USE_POSIX_TERMIOS)
		if (tcgetattr(tty, &tio) == -1)
		    tio = d_tio;
		old_erase = tio.c_cc[VERASE];
		tio.c_cc[VERASE] = initial_erase;
		rc = tcsetattr(tty, TCSANOW, &tio);
#else /* !USE_ANY_SYSV_TERMIO && !USE_POSIX_TERMIOS */
		if (ioctl(tty, TIOCGETP, (char *) &sg) == -1)
		    sg = d_sg;
		old_erase = sg.sg_erase;
		sg.sg_erase = initial_erase;
		rc = ioctl(tty, TIOCSETP, (char *) &sg);
#endif /* USE_ANY_SYSV_TERMIO */
		TRACE(("%s setting erase to %d (was %d)\n",
		       rc ? "FAIL" : "OK", initial_erase, old_erase));
	    }
#endif

	    /* copy the environment before Setenving */
	    for (i = 0; environ[i] != NULL; i++) ;
	    /* compute number of xtermSetenv() calls below */
	    envsize = 1;	/* (NULL terminating entry) */
	    envsize += 3;	/* TERM, WINDOWID, DISPLAY */
#ifdef HAVE_UTMP
	    envsize += 1;	/* LOGNAME */
#endif /* HAVE_UTMP */
#ifdef USE_SYSV_ENVVARS
	    envsize += 2;	/* COLUMNS, LINES */
#ifdef HAVE_UTMP
	    envsize += 2;	/* HOME, SHELL */
#endif /* HAVE_UTMP */
#ifdef OWN_TERMINFO_DIR
	    envsize += 1;	/* TERMINFO */
#endif
#else /* USE_SYSV_ENVVARS */
	    envsize += 1;	/* TERMCAP */
#endif /* USE_SYSV_ENVVARS */
	    envnew = (char **) calloc((unsigned) i + envsize, sizeof(char *));
	    memmove((char *) envnew, (char *) environ, i * sizeof(char *));
	    environ = envnew;
	    xtermSetenv("TERM=", TermName);
	    if (!TermName)
		*newtc = 0;

	    sprintf(buf, "%lu",
		    ((unsigned long) XtWindow(XtParent(CURRENT_EMU(screen)))));
	    xtermSetenv("WINDOWID=", buf);

	    /* put the display into the environment of the shell */
	    xtermSetenv("DISPLAY=", XDisplayString(screen->display));

	    signal(SIGTERM, SIG_DFL);

	    /* this is the time to go and set up stdin, out, and err
	     */
	    {
#if defined(CRAY) && (OSMAJORVERSION >= 6)
		(void) close(tty);
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
		    if (i != tty) {
			(void) close(i);
			(void) dup(tty);
		    }
#ifndef ATT
		/* and close the tty */
		if (tty > 2)
		    (void) close(tty);
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
		struct termio t;
		if (ioctl(0, TCGETA, &t) >= 0) {
		    /* this gets lost somewhere on our way... */
		    t.c_oflag |= OPOST;
		    ioctl(0, TCSETA, &t);
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
		     || (login_name = getenv("LOGNAME")) != NULL
		     || (login_name = getenv("USER")) != NULL)
		    && strcmp(login_name, pw->pw_name)) {
		    struct passwd *pw2 = getpwnam(login_name);
		    if (pw2 != 0
			&& pw->pw_uid != pw2->pw_uid) {
			login_name = NULL;
		    }
		}
#endif
		if (login_name == NULL)
		    login_name = pw->pw_name;
		if (login_name != NULL)
		    login_name = x_strdup(login_name);
	    }
	    if (login_name != NULL) {
		xtermSetenv("LOGNAME=", login_name);	/* for POSIX */
	    }
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
	    (void) setutent();
	    /* set up entry to search for */
	    bzero((char *) &utmp, sizeof(utmp));
	    (void) strncpy(utmp.ut_id, my_utmp_id(ttydev), sizeof(utmp.ut_id));

	    utmp.ut_type = DEAD_PROCESS;

	    /* position to entry in utmp file */
	    /* Test return value: beware of entries left behind: PSz 9 Mar 00 */
	    if (!(utret = getutid(&utmp))) {
		(void) setutent();
		utmp.ut_type = USER_PROCESS;
		if (!(utret = getutid(&utmp))) {
		    (void) setutent();
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
	    (void) strncpy(buf, DisplayString(screen->display), sizeof(buf));
#ifndef linux
	    {
		char *disfin = strrchr(buf, ':');
		if (disfin)
		    *disfin = '\0';
	    }
#endif
	    (void) strncpy(utmp.ut_host, buf, sizeof(utmp.ut_host));
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
		pututline(&utmp);
		TRACE(("pututline: %d %d %s\n",
		       resource.utmpInhibit,
		       errno, strerror(errno)));
	    }
#ifdef WTMP
#if defined(SVR4) || defined(SCO325)
	    if (term->misc.login_shell)
		updwtmpx(WTMPX_FILE, &utmp);
#elif defined(linux) && defined(__GLIBC__) && (__GLIBC__ >= 2) && !(defined(__powerpc__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 0))
	    if (term->misc.login_shell)
		updwtmp(etc_wtmp, &utmp);
#else
	    if (term->misc.login_shell &&
		(i = open(etc_wtmp, O_WRONLY | O_APPEND)) >= 0) {
		write(i, (char *) &utmp, sizeof(utmp));
		close(i);
	    }
#endif
#endif
	    /* close the file */
	    (void) endutent();

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
		    (void) strncpy(utmp.ut_host,
				   XDisplayString(screen->display),
				   sizeof(utmp.ut_host));
#endif
		    /* cast needed on Ultrix 4.4 */
		    time((time_t *) & utmp.ut_time);
		    lseek(i, (long) (tslot * sizeof(utmp)), 0);
		    write(i, (char *) &utmp, sizeof(utmp));
		    close(i);
		    added_utmp_entry = True;
#if defined(WTMP)
		    if (term->misc.login_shell &&
			(i = open(etc_wtmp, O_WRONLY | O_APPEND)) >= 0) {
			int status;
			status = write(i, (char *) &utmp, sizeof(utmp));
			status = close(i);
		    }
#elif defined(MNX_LASTLOG)
		    if (term->misc.login_shell &&
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
#ifdef USE_HANDSHAKE
	    handshake.tty_slot = tslot;
#endif /* USE_HANDSHAKE */
#endif /* USE_SYSV_UTMP */

#ifdef USE_LASTLOG
	    if (term->misc.login_shell &&
		(i = open(etc_lastlog, O_WRONLY)) >= 0) {
		bzero((char *) &lastlog, sizeof(struct lastlog));
		(void) strncpy(lastlog.ll_line,
			       my_pty_name(ttydev),
			       sizeof(lastlog.ll_line));
		(void) strncpy(lastlog.ll_host,
			       XDisplayString(screen->display),
			       sizeof(lastlog.ll_host));
		time(&lastlog.ll_time);
		lseek(i, (long) (screen->uid * sizeof(struct lastlog)), 0);
		write(i, (char *) &lastlog, sizeof(struct lastlog));
		close(i);
	    }
#endif /* USE_LASTLOG */

#ifdef __OpenBSD__
	    /* Switch to real gid after writing utmp entry */
	    utmpGid = getegid();
	    if (getgid() != getegid()) {
		utmpGid = getegid();
		setegid(getgid());
	    }
#endif

#ifdef USE_HANDSHAKE
	    /* Let our parent know that we set up our utmp entry
	     * so that it can clean up after us.
	     */
	    handshake.status = UTMP_ADDED;
	    handshake.error = 0;
	    strcpy(handshake.buffer, ttydev);
	    (void) write(cp_pipe[1], (char *) &handshake, sizeof(handshake));
#endif /* USE_HANDSHAKE */
#endif /* HAVE_UTMP */

	    (void) setgid(screen->gid);
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
#ifdef USE_HANDSHAKE
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
	    (void) write(cp_pipe[1], (char *) &handshake, sizeof(handshake));

	    if (waiting_for_initial_map) {
		i = read(pc_pipe[0], (char *) &handshake,
			 sizeof(handshake));
		if (i != sizeof(handshake) ||
		    handshake.status != PTY_EXEC) {
		    /* some very bad problem occurred */
		    exit(ERROR_PTY_EXEC);
		}
		if (handshake.rows > 0 && handshake.cols > 0) {
		    screen->max_row = handshake.rows;
		    screen->max_col = handshake.cols;
#if defined(TIOCSSIZE) && (defined(sun) && !defined(SVR4))
		    ts.ts_lines = screen->max_row + 1;
		    ts.ts_cols = screen->max_col + 1;
#elif defined(TIOCSWINSZ)
		    ws.ws_row = screen->max_row + 1;
		    ws.ws_col = screen->max_col + 1;
		    ws.ws_xpixel = FullWidth(screen);
		    ws.ws_ypixel = FullHeight(screen);
#endif /* sun vs TIOCSWINSZ */
		}
	    }
#endif /* USE_HANDSHAKE */

#ifdef USE_SYSV_ENVVARS
	    {
		char numbuf[12];
		sprintf(numbuf, "%d", screen->max_col + 1);
		xtermSetenv("COLUMNS=", numbuf);
		sprintf(numbuf, "%d", screen->max_row + 1);
		xtermSetenv("LINES=", numbuf);
	    }
#ifdef HAVE_UTMP
	    if (pw) {		/* SVR4 doesn't provide these */
		if (!getenv("HOME"))
		    xtermSetenv("HOME=", pw->pw_dir);
		if (!getenv("SHELL"))
		    xtermSetenv("SHELL=", pw->pw_shell);
	    }
#endif /* HAVE_UTMP */
#ifdef OWN_TERMINFO_DIR
	    xtermSetenv("TERMINFO=", OWN_TERMINFO_DIR);
#endif
#else /* USE_SYSV_ENVVARS */
	    if (!TEK4014_ACTIVE(screen) && *newtc) {
		strcpy(termcap, newtc);
		resize(screen, termcap, newtc);
	    }
	    if (term->misc.titeInhibit && !term->misc.tiXtraScroll) {
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
			initial_erase & 0377);
#endif
		xtermSetenv("TERMCAP=", newtc);
	    }
#endif /* USE_SYSV_ENVVARS */

	    /* need to reset after all the ioctl bashing we did above */
#if defined(TIOCSSIZE) && (defined(sun) && !defined(SVR4))
	    i = ioctl(0, TIOCSSIZE, &ts);
	    TRACE(("spawn TIOCSSIZE %dx%d return %d\n",
		   ts.ts_lines, ts.ts_cols, i));
#elif defined(TIOCSWINSZ)
	    i = ioctl(0, TIOCSWINSZ, (char *) &ws);
	    TRACE(("spawn TIOCSWINSZ %dx%d return %d\n",
		   ws.ws_row, ws.ws_col, i));
#else
	    TRACE(("spawn cannot tell pty its size\n"));
#endif /* sun vs TIOCSWINSZ */
	    signal(SIGHUP, SIG_DFL);

#ifdef HAVE_UTMP
	    if (((ptr = getenv("SHELL")) == NULL || *ptr == 0) &&
		((pw == NULL && (pw = getpwuid(screen->uid)) == NULL) ||
		 *(ptr = pw->pw_shell) == 0))
#else /* HAVE_UTMP */
	    if (((ptr = getenv("SHELL")) == NULL || *ptr == 0) &&
		((pw = getpwuid(screen->uid)) == NULL ||
		 *(ptr = pw->pw_shell) == 0))
#endif /* HAVE_UTMP */
		ptr = "/bin/sh";
	    shname = x_basename(ptr);

#if OPT_LUIT_PROG
	    /*
	     * Use two copies of command_to_exec, in case luit is not actually
	     * there, or refuses to run.  In that case we will fall-through to
	     * to command that the user gave anyway.
	     */
	    if (command_to_exec_with_luit) {
		TRACE(("spawning command \"%s\"\n", *command_to_exec_with_luit));
		execvp(*command_to_exec_with_luit, command_to_exec_with_luit);
		/* print error message on screen */
		fprintf(stderr, "%s: Can't execvp %s: %s\n",
			xterm_name, *command_to_exec_with_luit, strerror(errno));
		fprintf(stderr, "%s: cannot support your locale.\n",
			xterm_name);
	    }
#endif
	    if (command_to_exec) {
		TRACE(("spawning command \"%s\"\n", *command_to_exec));
		execvp(*command_to_exec, command_to_exec);
		if (command_to_exec[1] == 0)
		    execlp(ptr, shname, "-c", command_to_exec[0], 0);
		/* print error message on screen */
		fprintf(stderr, "%s: Can't execvp %s: %s\n",
			xterm_name, *command_to_exec, strerror(errno));
	    }
#ifdef USE_SYSV_SIGHUP
	    /* fix pts sh hanging around */
	    signal(SIGHUP, SIG_DFL);
#endif

	    shname_minus = (char *) malloc(strlen(shname) + 2);
	    (void) strcpy(shname_minus, "-");
	    (void) strcat(shname_minus, shname);
#if !defined(USE_ANY_SYSV_TERMIO) && !defined(USE_POSIX_TERMIOS)
	    ldisc = XStrCmp("csh", shname + strlen(shname) - 3) == 0 ?
		NTTYDISC : 0;
	    ioctl(0, TIOCSETD, (char *) &ldisc);
#endif /* !USE_ANY_SYSV_TERMIO && !USE_POSIX_TERMIOS */

#ifdef USE_LOGIN_DASH_P
	    if (term->misc.login_shell && pw && added_utmp_entry)
		execl(bin_login, "login", "-p", "-f", login_name, (void *) 0);
#endif
	    execlp(ptr,
		   (term->misc.login_shell ? shname_minus : shname),
		   (void *) 0);

	    /* Exec failed. */
	    fprintf(stderr, "%s: Could not exec %s: %s\n", xterm_name,
		    ptr, strerror(errno));
	    (void) sleep(5);
	    exit(ERROR_EXEC);
	}
	/* end if in child after fork */
#ifdef USE_HANDSHAKE
	/* Parent process.  Let's handle handshaked requests to our
	 * child process.
	 */

	/* close childs's sides of the pipes */
	close(cp_pipe[1]);
	close(pc_pipe[0]);

	for (done = 0; !done;) {
	    if (read(cp_pipe[0], (char *) &handshake, sizeof(handshake)) <= 0) {
		/* Our child is done talking to us.  If it terminated
		 * due to an error, we will catch the death of child
		 * and clean up.
		 */
		break;
	    }

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
			    xterm_name, strerror(errno));
		    handshake.status = PTY_NOMORE;
		    write(pc_pipe[1], (char *) &handshake, sizeof(handshake));
		    exit(ERROR_PTYS);
		}
		handshake.status = PTY_NEW;
		(void) strcpy(handshake.buffer, ttydev);
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
	    default:
		fprintf(stderr, "%s: unexpected handshake status %d\n",
			xterm_name, handshake.status);
	    }
	}
	/* close our sides of the pipes */
	if (!waiting_for_initial_map) {
	    close(cp_pipe[0]);
	    close(pc_pipe[1]);
	}
#endif /* USE_HANDSHAKE */
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
}				/* end spawn */

SIGNAL_T
Exit(int n)
{
    register TScreen *screen = &term->screen;

#ifdef USE_UTEMPTER
    if (!resource.utmpInhibit && added_utmp_entry)
	removeFromUtmp();
#elif defined(HAVE_UTMP)
#ifdef USE_SYSV_UTMP
    struct UTMP_STR utmp;
    struct UTMP_STR *utptr;
#if defined(WTMP) && !defined(SVR4) && !(defined(linux) && defined(__GLIBC__) && (__GLIBC__ >= 2) && !(defined(__powerpc__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 0)))
    int fd;			/* for /etc/wtmp */
#endif

    /* don't do this more than once */
    if (xterm_exiting)
	SIGNAL_RETURN;
    xterm_exiting = True;

#ifdef PUCC_PTYD
    closepty(ttydev, ptydev, (resource.utmpInhibit ? OPTY_NOP : OPTY_LOGIN), screen->respond);
#endif /* PUCC_PTYD */

    /* cleanup the utmp entry we forged earlier */
    if (!resource.utmpInhibit
#ifdef USE_HANDSHAKE		/* without handshake, no way to know */
	&& added_utmp_entry
#endif /* USE_HANDSHAKE */
	) {
#ifdef __OpenBSD__
	if (utmpGid != -1) {
	    /* Switch back to group utmp */
	    setegid(utmpGid);
	}
#endif
	utmp.ut_type = USER_PROCESS;
	(void) strncpy(utmp.ut_id, my_utmp_id(ttydev), sizeof(utmp.ut_id));
	(void) setutent();
	utptr = getutid(&utmp);
	/* write it out only if it exists, and the pid's match */
	if (utptr && (utptr->ut_pid == screen->pid)) {
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
	    (void) pututline(utptr);
#ifdef WTMP
#if defined(SVR4) || defined(SCO325)
	    if (term->misc.login_shell)
		updwtmpx(WTMPX_FILE, utptr);
#elif defined(linux) && defined(__GLIBC__) && (__GLIBC__ >= 2) && !(defined(__powerpc__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 0))
	    strncpy(utmp.ut_line, utptr->ut_line, sizeof(utmp.ut_line));
	    if (term->misc.login_shell)
		updwtmp(etc_wtmp, utptr);
#else
	    /* set wtmp entry if wtmp file exists */
	    if (term->misc.login_shell &&
		(fd = open(etc_wtmp, O_WRONLY | O_APPEND)) >= 0) {
		write(fd, utptr, sizeof(*utptr));
		close(fd);
	    }
#endif
#endif

	}
	(void) endutent();
    }
#else /* not USE_SYSV_UTMP */
    register int wfd;
    struct utmp utmp;

    if (!resource.utmpInhibit && added_utmp_entry &&
	(am_slave < 0 && tslot > 0 && (wfd = open(etc_utmp, O_WRONLY)) >= 0)) {
	bzero((char *) &utmp, sizeof(utmp));
	lseek(wfd, (long) (tslot * sizeof(utmp)), 0);
	write(wfd, (char *) &utmp, sizeof(utmp));
	close(wfd);
#ifdef WTMP
	if (term->misc.login_shell &&
	    (wfd = open(etc_wtmp, O_WRONLY | O_APPEND)) >= 0) {
	    (void) strncpy(utmp.ut_line,
			   my_pty_name(ttydev),
			   sizeof(utmp.ut_line));
	    time(&utmp.ut_time);
	    write(wfd, (char *) &utmp, sizeof(utmp));
	    close(wfd);
	}
#endif /* WTMP */
    }
#endif /* USE_SYSV_UTMP */
#endif /* HAVE_UTMP */
    close(screen->respond);	/* close explicitly to avoid race with slave side */
#ifdef ALLOWLOGGING
    if (screen->logging)
	CloseLog(screen);
#endif

    if (am_slave < 0) {
	/* restore ownership of tty and pty */
	set_owner(ttydev, 0, 0, 0666);
#if (defined(USE_PTY_DEVICE) && !defined(__sgi) && !defined(__hpux))
	set_owner(ptydev, 0, 0, 0666);
#endif
    }
    exit(n);
    SIGNAL_RETURN;
}

/* ARGSUSED */
static void
resize(TScreen * screen, register char *oldtc, char *newtc)
{
#ifndef USE_SYSV_ENVVARS
    register char *ptr1, *ptr2;
    register size_t i;
    register int li_first = 0;
    register char *temp;

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
			 ? screen->max_row + 1
			 : screen->max_col + 1));
    temp += strlen(temp);
    ptr1 = strchr(ptr1, ':');
    strncpy(temp, ptr1, i = ptr2 - ptr1);
    temp += i;
    sprintf(temp, "%d", (li_first
			 ? screen->max_col + 1
			 : screen->max_row + 1));
    ptr2 = strchr(ptr2, ':');
    strcat(temp, ptr2);
    TRACE(("   ==> %s\n", newtc));
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
    register int pid;

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
		Cleanup(0);
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
	while (*s && isascii(CharOf(*s)) && isspace(CharOf(*s)))
	    s++;
	if (!*s)
	    return count;

	for (mp = modelist; mp->name; mp++) {
	    if (strncmp(s, mp->name, mp->len) == 0)
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
    long arg;
    ioctl(fd, FIONREAD, (char *) &arg);
    return (int) arg;
#elif defined(__CYGWIN__)
    fd_set set;
    struct timeval timeout =
    {0, 0};

    FD_ZERO(&set);
    FD_SET(fd, &set);
    if (select(fd + 1, &set, NULL, NULL, &timeout) > 0)
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
