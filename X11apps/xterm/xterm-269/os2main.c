/* $XTermId: os2main.c,v 1.268 2011/02/14 10:01:09 tom Exp $ */

/* removed all foreign stuff to get the code more clear (hv)
 * and did some rewrite for the obscure OS/2 environment
 */

/***********************************************************

Copyright (c) 1987, 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

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

/* os2main.c */
#define INCL_DOSFILEMGR
#define INCL_DOSDEVIOCTL
#define INCL_DOSSEMAPHORES
#ifdef __INNOTEK_LIBC__
#define INCL_DOSDEVICES
#endif
#define I_NEED_OS2_H
#include <os2.h>
#define XTERM_MAIN

#define RES_OFFSET(field)	XtOffsetOf(XTERM_RESOURCE, field)

#include <xterm.h>

#include <X11/cursorfont.h>
#ifdef I18N
#include <X11/Xlocale.h>
#endif

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

int
setpgrp(pid_t pid, gid_t pgid)
{
    return 0;
}

int
chown(const char *fn, pid_t pid, gid_t gid)
{
    return 0;
}

char *
ttyname(int fd)
{
    return "/dev/tty";
}

#include <sys/stat.h>
#include <sys/param.h>		/* for NOFILE */
#include <stdio.h>
#include <signal.h>

static SIGNAL_T reapchild(int n);
static int spawnXTerm(XtermWidget /* xw */ );
static void resize_termcap(XtermWidget xw);
static void set_owner(char *device, uid_t uid, gid_t gid, mode_t mode);

static Bool added_utmp_entry = False;

static uid_t save_ruid;
static gid_t save_rgid;

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

/* The following structures are initialized in main() in order
** to eliminate any assumptions about the internal order of their
** contents.
*/
static struct termio d_tio;

/* allow use of system default characters if defined and reasonable */
#ifndef CEOF
#define CEOF CONTROL('D')
#endif
#ifndef CEOL
#define CEOL 0
#endif
#ifndef CFLUSH
#define CFLUSH CONTROL('O')
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

#define TERMIO_STRUCT struct termio

/*
 * SYSV has the termio.c_cc[V] and ltchars; BSD has tchars and ltchars;
 * SVR4 has only termio.c_cc, but it includes everything from ltchars.
 * POSIX termios has termios.c_cc, which is similar to SVR4.
 */
#define TTYMODE(name) { name, sizeof(name)-1, 0, 0 }
static Boolean override_tty_modes = False;
/* *INDENT-OFF* */
static struct _xttymodes {
    const char *name;
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
    { NULL,	0, 0, '\0' },	/* end of data */
};
/* *INDENT-ON* */

#define TMODE(ind,var) if (ttymodelist[ind].set) var = ttymodelist[ind].value

static int parse_tty_modes(char *s, struct _xttymodes *modelist);

static char passedPty[2];	/* name if pty if slave */

static int Console;
#include <X11/Xmu/SysUtil.h>	/* XmuGetHostname */
#define MIT_CONSOLE_LEN	12
#define MIT_CONSOLE "MIT_CONSOLE_"
static char mit_console_name[255 + MIT_CONSOLE_LEN + 1] = MIT_CONSOLE;
static Atom mit_console;

static int tslot;
static jmp_buf env;

/* used by VT (charproc.c) */

static XtResource application_resources[] =
{
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
    Sres("menuLocale", "MenuLocale", menuLocale, DEF_MENU_LOCALE),
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
#if OPT_MAXIMIZE
    Bres(XtNmaximized, XtCMaximized, maximized, False),
#endif
};

static String fallback_resources[] =
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
{"-geometry",	"*vt100.geometry",XrmoptionSepArg,	(XPointer) NULL},
{"-132",	"*c132",	XrmoptionNoArg,		(XPointer) "on"},
{"+132",	"*c132",	XrmoptionNoArg,		(XPointer) "off"},
{"-ah",		"*alwaysHighlight", XrmoptionNoArg,	(XPointer) "on"},
{"+ah",		"*alwaysHighlight", XrmoptionNoArg,	(XPointer) "off"},
{"-aw",		"*autoWrap",	XrmoptionNoArg,		(XPointer) "on"},
{"+aw",		"*autoWrap",	XrmoptionNoArg,		(XPointer) "off"},
#ifndef NO_ACTIVE_ICON
{"-ai",		"*activeIcon",	XrmoptionNoArg,		(XPointer) "off"},
{"+ai",		"*activeIcon",	XrmoptionNoArg,		(XPointer) "on"},
#endif /* NO_ACTIVE_ICON */
{"-b",		"*internalBorder",XrmoptionSepArg,	(XPointer) NULL},
{"-bc",		"*cursorBlink",	XrmoptionNoArg,		(XPointer) "on"},
{"+bc",		"*cursorBlink",	XrmoptionNoArg,		(XPointer) "off"},
{"-bcf",	"*cursorOffTime",XrmoptionSepArg,	(XPointer) NULL},
{"-bcn",	"*cursorOnTime",XrmoptionSepArg,	(XPointer) NULL},
{"-bdc",	"*colorBDMode",	XrmoptionNoArg,		(XPointer) "off"},
{"+bdc",	"*colorBDMode",	XrmoptionNoArg,		(XPointer) "on"},
{"-cb",		"*cutToBeginningOfLine", XrmoptionNoArg, (XPointer) "off"},
{"+cb",		"*cutToBeginningOfLine", XrmoptionNoArg, (XPointer) "on"},
{"-cc",		"*charClass",	XrmoptionSepArg,	(XPointer) NULL},
{"-cm",		"*colorMode",	XrmoptionNoArg,		(XPointer) "off"},
{"+cm",		"*colorMode",	XrmoptionNoArg,		(XPointer) "on"},
{"-cn",		"*cutNewline",	XrmoptionNoArg,		(XPointer) "off"},
{"+cn",		"*cutNewline",	XrmoptionNoArg,		(XPointer) "on"},
{"-cr",		"*cursorColor",	XrmoptionSepArg,	(XPointer) NULL},
{"-cu",		"*curses",	XrmoptionNoArg,		(XPointer) "on"},
{"+cu",		"*curses",	XrmoptionNoArg,		(XPointer) "off"},
{"-dc",		"*dynamicColors",XrmoptionNoArg,	(XPointer) "off"},
{"+dc",		"*dynamicColors",XrmoptionNoArg,	(XPointer) "on"},
{"-fb",		"*boldFont",	XrmoptionSepArg,	(XPointer) NULL},
{"-fbb",	"*freeBoldBox", XrmoptionNoArg,		(XPointer)"off"},
{"+fbb",	"*freeBoldBox", XrmoptionNoArg,		(XPointer)"on"},
{"-fbx",	"*forceBoxChars", XrmoptionNoArg,	(XPointer)"off"},
{"+fbx",	"*forceBoxChars", XrmoptionNoArg,	(XPointer)"on"},
#ifndef NO_ACTIVE_ICON
{"-fi",		"*iconFont",	XrmoptionSepArg,	(XPointer) NULL},
#endif /* NO_ACTIVE_ICON */
#if OPT_RENDERFONT
{"-fa",		"*faceName",	XrmoptionSepArg,	(XPointer) NULL},
{"-fd",		"*faceNameDoublesize", XrmoptionSepArg,	(XPointer) NULL},
{"-fs",		"*faceSize",	XrmoptionSepArg,	(XPointer) NULL},
#endif
#if OPT_WIDE_CHARS
{"-fw",		"*wideFont",	XrmoptionSepArg,	(XPointer) NULL},
{"-fwb",	"*wideBoldFont", XrmoptionSepArg,	(XPointer) NULL},
#endif
#if OPT_INPUT_METHOD
{"-fx",		"*ximFont",	XrmoptionSepArg,	(XPointer) NULL},
#endif
#if OPT_HIGHLIGHT_COLOR
{"-hc",		"*highlightColor", XrmoptionSepArg,	(XPointer) NULL},
{"-hm",		"*highlightColorMode", XrmoptionNoArg,	(XPointer) "on"},
{"+hm",		"*highlightColorMode", XrmoptionNoArg,	(XPointer) "off"},
{"-selfg",	"*highlightTextColor", XrmoptionSepArg,	(XPointer) NULL},
{"-selbg",	"*highlightColor", XrmoptionSepArg,	(XPointer) NULL},
#endif
#if OPT_HP_FUNC_KEYS
{"-hf",		"*hpFunctionKeys",XrmoptionNoArg,	(XPointer) "on"},
{"+hf",		"*hpFunctionKeys",XrmoptionNoArg,	(XPointer) "off"},
#endif
{"-hold",	"*hold",	XrmoptionNoArg,		(XPointer) "on"},
{"+hold",	"*hold",	XrmoptionNoArg,		(XPointer) "off"},
#if OPT_INITIAL_ERASE
{"-ie",		"*ptyInitialErase", XrmoptionNoArg,	(XPointer) "on"},
{"+ie",		"*ptyInitialErase", XrmoptionNoArg,	(XPointer) "off"},
#endif
{"-j",		"*jumpScroll",	XrmoptionNoArg,		(XPointer) "on"},
{"+j",		"*jumpScroll",	XrmoptionNoArg,		(XPointer) "off"},
#if OPT_C1_PRINT
{"-k8",		"*allowC1Printable", XrmoptionNoArg,	(XPointer) "on"},
{"+k8",		"*allowC1Printable", XrmoptionNoArg,	(XPointer) "off"},
#endif
{"-kt",		"*keyboardType", XrmoptionSepArg,	(XPointer) NULL},
{"+kt",		"*keyboardType", XrmoptionSepArg,	(XPointer) NULL},
/* parse logging options anyway for compatibility */
{"-l",		"*logging",	XrmoptionNoArg,		(XPointer) "on"},
{"+l",		"*logging",	XrmoptionNoArg,		(XPointer) "off"},
{"-lf",		"*logFile",	XrmoptionSepArg,	(XPointer) NULL},
{"-ls",		"*loginShell",	XrmoptionNoArg,		(XPointer) "on"},
{"+ls",		"*loginShell",	XrmoptionNoArg,		(XPointer) "off"},
{"-mb",		"*marginBell",	XrmoptionNoArg,		(XPointer) "on"},
{"+mb",		"*marginBell",	XrmoptionNoArg,		(XPointer) "off"},
{"-mc",		"*multiClickTime", XrmoptionSepArg,	(XPointer) NULL},
{"-mesg",	"*messages",	XrmoptionNoArg,		(XPointer) "off"},
{"+mesg",	"*messages",	XrmoptionNoArg,		(XPointer) "on"},
{"-ms",		"*pointerColor",XrmoptionSepArg,	(XPointer) NULL},
{"-nb",		"*nMarginBell",	XrmoptionSepArg,	(XPointer) NULL},
{"-nul",	"*underLine",	XrmoptionNoArg,		(XPointer) "off"},
{"+nul",	"*underLine",	XrmoptionNoArg,		(XPointer) "on"},
{"-pc",		"*boldColors",	XrmoptionNoArg,		(XPointer) "on"},
{"+pc",		"*boldColors",	XrmoptionNoArg,		(XPointer) "off"},
{"-rw",		"*reverseWrap",	XrmoptionNoArg,		(XPointer) "on"},
{"+rw",		"*reverseWrap",	XrmoptionNoArg,		(XPointer) "off"},
{"-s",		"*multiScroll",	XrmoptionNoArg,		(XPointer) "on"},
{"+s",		"*multiScroll",	XrmoptionNoArg,		(XPointer) "off"},
{"-sb",		"*scrollBar",	XrmoptionNoArg,		(XPointer) "on"},
{"+sb",		"*scrollBar",	XrmoptionNoArg,		(XPointer) "off"},
#ifdef SCROLLBAR_RIGHT
{"-leftbar",	"*rightScrollBar", XrmoptionNoArg,	(XPointer) "off"},
{"-rightbar",	"*rightScrollBar", XrmoptionNoArg,	(XPointer) "on"},
#endif
{"-rvc",	"*colorRVMode",	XrmoptionNoArg,		(XPointer) "off"},
{"+rvc",	"*colorRVMode",	XrmoptionNoArg,		(XPointer) "on"},
{"-sf",		"*sunFunctionKeys", XrmoptionNoArg,	(XPointer) "on"},
{"+sf",		"*sunFunctionKeys", XrmoptionNoArg,	(XPointer) "off"},
{"-si",		"*scrollTtyOutput", XrmoptionNoArg,	(XPointer) "off"},
{"+si",		"*scrollTtyOutput", XrmoptionNoArg,	(XPointer) "on"},
{"-sk",		"*scrollKey",	XrmoptionNoArg,		(XPointer) "on"},
{"+sk",		"*scrollKey",	XrmoptionNoArg,		(XPointer) "off"},
{"-sl",		"*saveLines",	XrmoptionSepArg,	(XPointer) NULL},
#if OPT_SUNPC_KBD
{"-sp",		"*sunKeyboard", XrmoptionNoArg,		(XPointer) "on"},
{"+sp",		"*sunKeyboard", XrmoptionNoArg,		(XPointer) "off"},
#endif
#if OPT_TEK4014
{"-t",		"*tekStartup",	XrmoptionNoArg,		(XPointer) "on"},
{"+t",		"*tekStartup",	XrmoptionNoArg,		(XPointer) "off"},
#endif
{"-ti",		"*decTerminalID",XrmoptionSepArg,	(XPointer) NULL},
{"-tm",		"*ttyModes",	XrmoptionSepArg,	(XPointer) NULL},
{"-tn",		"*termName",	XrmoptionSepArg,	(XPointer) NULL},
#if OPT_WIDE_CHARS
{"-u8",		"*utf8",	XrmoptionNoArg,		(XPointer) "2"},
{"+u8",		"*utf8",	XrmoptionNoArg,		(XPointer) "0"},
#endif
#if OPT_LUIT_PROG
{"-lc",		"*locale",	XrmoptionNoArg,		(XPointer) "on"},
{"+lc",		"*locale",	XrmoptionNoArg,		(XPointer) "off"},
{"-lcc",	"*localeFilter",XrmoptionSepArg,	(XPointer) NULL},
{"-en",		"*locale",	XrmoptionSepArg,	(XPointer) NULL},
#endif
{"-uc",		"*cursorUnderLine", XrmoptionNoArg,	(XPointer) "on"},
{"+uc",		"*cursorUnderLine", XrmoptionNoArg,	(XPointer) "off"},
{"-ulc",	"*colorULMode",	XrmoptionNoArg,		(XPointer) "off"},
{"+ulc",	"*colorULMode",	XrmoptionNoArg,		(XPointer) "on"},
{"-ulit",       "*italicULMode", XrmoptionNoArg,        (XPointer) "off"},
{"+ulit",       "*italicULMode", XrmoptionNoArg,        (XPointer) "on"},
{"-ut",		"*utmpInhibit",	XrmoptionNoArg,		(XPointer) "on"},
{"+ut",		"*utmpInhibit",	XrmoptionNoArg,		(XPointer) "off"},
{"-im",		"*useInsertMode", XrmoptionNoArg,	(XPointer) "on"},
{"+im",		"*useInsertMode", XrmoptionNoArg,	(XPointer) "off"},
{"-vb",		"*visualBell",	XrmoptionNoArg,		(XPointer) "on"},
{"+vb",		"*visualBell",	XrmoptionNoArg,		(XPointer) "off"},
{"-pob",	"*popOnBell",	XrmoptionNoArg,		(XPointer) "on"},
{"+pob",	"*popOnBell",	XrmoptionNoArg,		(XPointer) "off"},
#if OPT_WIDE_CHARS
{"-wc",		"*wideChars",	XrmoptionNoArg,		(XPointer) "on"},
{"+wc",		"*wideChars",	XrmoptionNoArg,		(XPointer) "off"},
{"-mk_width",	"*mkWidth",	XrmoptionNoArg,		(XPointer) "on"},
{"+mk_width",	"*mkWidth",	XrmoptionNoArg,		(XPointer) "off"},
{"-cjk_width",	"*cjkWidth",	XrmoptionNoArg,		(XPointer) "on"},
{"+cjk_width",	"*cjkWidth",	XrmoptionNoArg,		(XPointer) "off"},
#endif
{"-wf",		"*waitForMap",	XrmoptionNoArg,		(XPointer) "on"},
{"+wf",		"*waitForMap",	XrmoptionNoArg,		(XPointer) "off"},
#if OPT_ZICONBEEP
{"-ziconbeep",	"*zIconBeep",	XrmoptionSepArg,	(XPointer) NULL},
#endif
#if OPT_SAME_NAME
{"-samename",	"*sameName",	XrmoptionNoArg,		(XPointer) "on"},
{"+samename",	"*sameName",	XrmoptionNoArg,		(XPointer) "off"},
#endif
#if OPT_SESSION_MGT
{"-sm",		"*sessionMgt",	XrmoptionNoArg,		(XPointer) "on"},
{"+sm",		"*sessionMgt",	XrmoptionNoArg,		(XPointer) "off"},
#endif
#if OPT_TOOLBAR
{"-tb",		"*"XtNtoolBar,	XrmoptionNoArg,		(XPointer) "on"},
{"+tb",		"*"XtNtoolBar,	XrmoptionNoArg,		(XPointer) "off"},
#endif
#if OPT_MAXIMIZE
{"-maximized",	"*maximized",	XrmoptionNoArg,		(XPointer) "on"},
{"+maximized",	"*maximized",	XrmoptionNoArg,		(XPointer) "off"},
#endif
/* options that we process ourselves */
{"-help",	NULL,		XrmoptionSkipNArgs,	(XPointer) NULL},
{"-version",	NULL,		XrmoptionSkipNArgs,	(XPointer) NULL},
{"-class",	NULL,		XrmoptionSkipArg,	(XPointer) NULL},
{"-e",		NULL,		XrmoptionSkipLine,	(XPointer) NULL},
{"-into",	NULL,		XrmoptionSkipArg,	(XPointer) NULL},
/* bogus old compatibility stuff for which there are
   standard XtOpenApplication options now */
{"%",		"*tekGeometry",	XrmoptionStickyArg,	(XPointer) NULL},
{"#",		".iconGeometry",XrmoptionStickyArg,	(XPointer) NULL},
{"-T",		".title",	XrmoptionSepArg,	(XPointer) NULL},
{"-n",		"*iconName",	XrmoptionSepArg,	(XPointer) NULL},
{"-r",		"*reverseVideo",XrmoptionNoArg,		(XPointer) "on"},
{"+r",		"*reverseVideo",XrmoptionNoArg,		(XPointer) "off"},
{"-rv",		"*reverseVideo",XrmoptionNoArg,		(XPointer) "on"},
{"+rv",		"*reverseVideo",XrmoptionNoArg,		(XPointer) "off"},
{"-w",		".borderWidth", XrmoptionSepArg,	(XPointer) NULL},
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
{ "-/+uc",                 "turn on/off underline cursor" },
{ "-/+ulc",                "turn off/on display of underline as color" },
{ "-/+ut",                 "turn on/off utmp inhibit (not supported)" },
{ "-/+ulit",               "turn off/on display of underline as italics" },
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
{ "-C",                    "intercept console messages" },
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
#if OPT_MAXIMIZE
{"-/+maximized",           "turn on/off maxmize on startup" },
#endif
{ NULL, NULL }};
/* *INDENT-ON* */

#ifdef DBG_CONSOLE
FILE *confd;

static void
closecons(void)
{
    if (confs != 0) {
	fclose(confd);
	confd = 0;
    }
}
static void
opencons(void)
{
    closecons();
    if ((confd = fopen("/dev/console$", "w")) < 0) {
	fputs("!!! Cannot open console device.\n",
	      stderr);
	exit(1);
    }
}
#else
#define opencons()		/* nothing */
#define closecons()		/* nothing */
#endif

static const char *message[] =
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

    TRACE(("decode_keyvalue '%s'\n", string));
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
		    value = (int) fpathconf(0, _PC_VDISABLE);
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
	int temp = (int) strtol(string + 1, &d, 8);
	if (temp > 0 && d != string) {
	    value = temp;
	    string = d;
	}
    } else {
	value = CharOf(*string);
	++string;
    }
    *ptr = string;
    TRACE(("...decode_keyvalue %#x\n", value));
    return value;
}

static int
abbrev(const char *tst, const char *cmp, size_t need)
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
    col = 8 + (int) strlen(ProgramName);
    for (opt = list; opt->opt; opt++) {
	int len = 3 + (int) strlen(opt->opt);	/* space [ string ] */
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
    const char **cpp;

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
    } else if (TScreenOf(term)->Vshow)
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

char **gblenvp;

int
main(int argc, char **argv ENVP_ARG)
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

    save_ruid = getuid();
    save_rgid = getgid();

    /* Do these first, since we may not be able to open the display */
    TRACE_OPTS(xtermOptions, optionDescList, XtNumber(optionDescList));
    TRACE_ARGV("Before XtOpenApplication", argv);
    if (argc > 1) {
	int n;
	size_t unique = 2;
	Bool quit = True;

	for (n = 1; n < argc; n++) {
	    TRACE(("parsing %s\n", argv[n]));
	    if (abbrev(argv[n], "-version", unique)) {
		Version();
	    } else if (abbrev(argv[n], "-help", unique)) {
		Help();
	    } else if (abbrev(argv[n], "-class", (size_t) 3)) {
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

    /* XXX: for some obscure reason EMX seems to lose the value of
     * the environ variable, don't understand why, so save it recently
     */
    gblenvp = envp;

#ifdef I18N
    setlocale(LC_ALL, NULL);
#endif

    opencons();

    ttydev = TypeMallocN(char, PTMS_BUFSZ);
    ptydev = TypeMallocN(char, PTMS_BUFSZ);
    if (!ttydev || !ptydev) {
	fprintf(stderr,
		"%s:  unable to allocate memory for ttydev or ptydev\n",
		ProgramName);
	exit(1);
    }
    strcpy(ttydev, TTYDEV);
    strcpy(ptydev, PTYDEV);

    /* Initialization is done here rather than above in order
     * to prevent any assumptions about the order of the contents
     * of the various terminal structures (which may change from
     * implementation to implementation).
     */
    d_tio.c_iflag = ICRNL | IXON;
    d_tio.c_oflag = OPOST | ONLCR | TAB3;
    d_tio.c_cflag = B38400 | CS8 | CREAD | PARENB | HUPCL;
    d_tio.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
    d_tio.c_line = 0;
    d_tio.c_cc[VINTR] = CONTROL('C');	/* '^C' */
    d_tio.c_cc[VERASE] = ANSI_DEL;	/* DEL  */
    d_tio.c_cc[VKILL] = CONTROL('U');	/* '^U' */
    d_tio.c_cc[VQUIT] = CQUIT;	/* '^\' */
    d_tio.c_cc[VEOF] = CEOF;	/* '^D' */
    d_tio.c_cc[VEOL] = CEOL;	/* '^@' */

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
	    continue;
	case 'S':
	    if (sscanf(*argv + 2, "%c%c%d", passedPty, passedPty + 1,
		       &am_slave) != 3)
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
	    c = TypeMallocN(char *, (unsigned) (n + 3 + u));
	    if (c == NULL)
		SysError(ERROR_LUMALLOC);
	    memcpy(c + 2 + u, command_to_exec, (unsigned) (n + 1) * sizeof(char *));
	    c[0] = term->misc.localefilter;
	    if (u) {
		c[1] = "-encoding";
		c[2] = term->misc.locale_str;
	    }
	    c[1 + u] = "--";
	    command_to_exec_with_luit = c;
	} else {
	    static char *luit[6];
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

    /* Child process is out there, let's catch its termination */
    (void) signal(SIGCHLD, reapchild);

    /* Realize procs have now been executed */

    if (am_slave >= 0) {	/* Write window id so master end can read and use */
	char buf[80];

	buf[0] = '\0';
	sprintf(buf, "%lx\n", XtWindow(SHELL_OF(CURRENT_EMU())));
	IGNORE_RC(write(screen->respond, buf, strlen(buf)));
    }

    if (0 > (mode = fcntl(screen->respond, F_GETFL, 0)))
	SysError(ERROR_F_GETFL);
    mode |= O_NDELAY;

    if (fcntl(screen->respond, F_SETFL, mode))
	SysError(ERROR_F_SETFL);

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

#ifdef DEBUG
    if (debug)
	printf("debugging on\n");
#endif /* DEBUG */
    XSetErrorHandler(xerror);
    XSetIOErrorHandler(xioerror);

    initPtyData(&VTbuffer);
#ifdef ALLOWLOGGING
    if (term->misc.log_on) {
	StartLog(term);
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
    TRACE(("checking reverseVideo before rv %s fg %s, bg %s\n",
	   term->misc.re_verse0 ? "reverse" : "normal",
	   NonNull(TScreenOf(term)->Tcolors[TEXT_FG].resource),
	   NonNull(TScreenOf(term)->Tcolors[TEXT_BG].resource)));

    if (term->misc.re_verse0) {
	if (isDefaultForeground(TScreenOf(term)->Tcolors[TEXT_FG].resource)
	    && isDefaultBackground(TScreenOf(term)->Tcolors[TEXT_BG].resource)) {
	    TScreenOf(term)->Tcolors[TEXT_FG].resource = x_strdup(XtDefaultBackground);
	    TScreenOf(term)->Tcolors[TEXT_BG].resource = x_strdup(XtDefaultForeground);
	} else {
	    ReverseVideo(term);
	}
	term->misc.re_verse = True;
	update_reversevideo();
	TRACE(("updated  reverseVideo after  rv %s fg %s, bg %s\n",
	       term->misc.re_verse ? "reverse" : "normal",
	       NonNull(TScreenOf(term)->Tcolors[TEXT_FG].resource),
	       NonNull(TScreenOf(term)->Tcolors[TEXT_BG].resource)));
    }
#endif /* OPT_COLOR_RES */

#if OPT_MAXIMIZE
    if (resource.maximized)
	RequestMaximize(term, True);
#endif
    for (;;) {
#if OPT_TEK4014
	if (TEK4014_ACTIVE(term))
	    TekRun();
	else
#endif
	    VTRun(term);
    }
    return 0;
}

/*
 * Called from get_pty to iterate over likely pseudo terminals
 * we might allocate.  Used on those systems that do not have
 * a functional interface for allocating a pty.
 * Returns 0 if found a pty, 1 if fails.
 */
static int
pty_search(int *pty)
{
    char namebuf[PTMS_BUFSZ];

    /* ask the PTY manager */
    int fd = open("/dev/ptms$", 0);
    if (fd && ptioctl(fd, PTMS_GETPTY, namebuf) == 0) {
	strcpy(ttydev, namebuf);
	strcpy(ptydev, namebuf);
	*x_basename(ttydev) = 't';
	close(fd);
	if ((*pty = open(ptydev, O_RDWR)) >= 0) {
#ifdef PTYDEBUG
	    ptioctl(*pty, XTY_TRACE, 0);
#endif
	    return 0;
	} else {
	    fprintf(stderr, "Unable to open %s, errno=%d\n", ptydev, errno);
	}
    }
    return 1;
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
get_pty(int *pty)
{
    return pty_search(pty);
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
    longjmp(env, 1);
    SIGNAL_RETURN;
}

#if OPT_PTY_HANDSHAKE
struct {
    int rows;
    int cols;
} handshake = {

    -1, -1
};

void
first_map_occurred(void)
{
    if (resource.wait_for_map) {
	TScreen *screen = TScreenOf(term);
	handshake.rows = screen->max_row;
	handshake.cols = screen->max_col;
	resource.wait_for_map = False;
    }
}
#endif /* OPT_PTY_HANDSHAKE else !OPT_PTY_HANDSHAKE */

static void
set_owner(char *device, uid_t uid, gid_t gid, mode_t mode)
{
    int why;

    if (chown(device, uid, gid) < 0) {
	why = errno;
	if (why != ENOENT
	    && save_ruid == 0) {
	    fprintf(stderr, "Cannot chown %s to %ld,%ld: %s\n",
		    device, (long) uid, (long) gid,
		    strerror(why));
	}
    }
}

#define THE_PARENT 1
#define THE_CHILD  2
int whoami = -1;

SIGNAL_T
killit(int sig)
{
    switch (whoami) {
    case -1:
	signal(sig, killit);
	kill(-getpid(), sig);
	break;
    case THE_PARENT:
	wait(NULL);
	signal(SIGTERM, SIG_DFL);
	kill(-getpid(), SIGTERM);
	Exit(0);
	break;
    case THE_CHILD:
	signal(SIGTERM, SIG_DFL);
	kill(-getppid(), SIGTERM);
	Exit(0);
	break;
    }

    SIGNAL_RETURN;
}

#define close_fd(fd) close(fd), fd = -1

static int
spawnXTerm(XtermWidget xw)
/*
 *  Inits pty and tty and forks a login process.
 *  Does not close fd Xsocket.
 *  If slave, the pty named in passedPty is already open for use
 */
{
    TScreen *screen = TScreenOf(xw);
    int Xsocket = ConnectionNumber(screen->display);

    int ttyfd = -1;
    TERMIO_STRUCT tio;
    int status;
    Bool ok_termcap;

    char *TermName = NULL;
    char *ptr, *shname, buf[64];
    int i, no_dev_tty = False;
    char *dev_tty_name = (char *) 0;
    TTYSIZE_STRUCT ts;
    int pgrp = getpid();
    char numbuf[12], **envnew;

    screen->uid = save_ruid;
    screen->gid = save_rgid;

    if (am_slave >= 0) {
	screen->respond = am_slave;
	ptydev[strlen(ptydev) - 2] =
	    ttydev[strlen(ttydev) - 2] = passedPty[0];
	ptydev[strlen(ptydev) - 1] =
	    ttydev[strlen(ttydev) - 1] = passedPty[1];

	(void) xtermResetIds(screen);
    } else {
	Bool tty_got_hung;

	/*
	 * Sometimes /dev/tty hangs on open (as in the case of a pty
	 * that has gone away).  Simply make up some reasonable
	 * defaults.
	 */

	signal(SIGALRM, hungtty);
	alarm(2);		/* alarm(1) might return too soon */
	if (!setjmp(env)) {
	    ttyfd = open("/dev/tty", O_RDWR);
	    alarm(0);
	    tty_got_hung = False;
	} else {
	    tty_got_hung = True;
	    ttyfd = -1;
	    errno = ENXIO;
	}
	signal(SIGALRM, SIG_DFL);

	/*
	 * Check results and ignore current control terminal if
	 * necessary.  ENXIO is what is normally returned if there is
	 * no controlling terminal, but some systems (e.g. SunOS 4.0)
	 * seem to return EIO.  Solaris 2.3 is said to return EINVAL.
	 */
	if (ttyfd < 0) {
	    if (tty_got_hung || errno == ENXIO || errno == EIO ||
		errno == EINVAL || errno == ENOTTY) {
		no_dev_tty = True;
		tio = d_tio;
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
	    if (ioctl(ttyfd, TCGETA, &tio) == -1)
		tio = d_tio;

	    close_fd(ttyfd);
	}

	if (get_pty(&screen->respond)) {
	    /*  no ptys! */
	    exit(ERROR_PTYS);
	}
    }

    /* avoid double MapWindow requests */
    XtSetMappedWhenManaged(SHELL_OF(CURRENT_EMU()), False);

    wm_delete_window = XInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW",
				   False);

    if (!TEK4014_ACTIVE(xw))
	VTInit(xw);		/* realize now so know window size for tty driver */

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
#if OPT_TEK4014
    if (TEK4014_ACTIVE(xw)) {
	envnew = tekterm;
    } else
#endif
    {
	envnew = vtterm;
    }

    /*
     * This used to exit if no termcap entry was found for the specified
     * terminal name.  That's a little unfriendly, so instead we'll allow
     * the program to proceed (but not to set $TERMCAP) if the termcap
     * entry is not found.
     */
    ok_termcap = True;
    if (!get_termcap(xw, TermName = resource.term_name)) {
	const char *last = NULL;
	char *next;

	TermName = x_strdup(*envnew);
	ok_termcap = False;
	while (*envnew != NULL) {
	    if (last == NULL || strcmp(last, *envnew)) {
		next = x_strdup(*envnew);
		if (get_termcap(xw, next)) {
		    free(TermName);
		    TermName = next;
		    ok_termcap = True;
		    break;
		} else {
		    free(next);
		}
	    }
	    last = *envnew;
	    envnew++;
	}
    }
    if (ok_termcap) {
	resource.term_name = TermName;
	resize_termcap(xw);
    }

    /* tell tty how big window is */
#if OPT_TEK4014
    if (TEK4014_ACTIVE(xw)) {
	TTYSIZE_ROWS(ts) = 38;
	TTYSIZE_COLS(ts) = 81;
	ts.ws_xpixel = TFullWidth(TekScreenOf(tekWidget));
	ts.ws_ypixel = TFullHeight(TekScreenOf(tekWidget));
    } else
#endif
    {
	TTYSIZE_ROWS(ts) = MaxRows(screen);
	TTYSIZE_COLS(ts) = MaxCols(screen);
	ts.ws_xpixel = FullWidth(screen);
	ts.ws_ypixel = FullHeight(screen);
    }

    if (am_slave < 0) {

	char sema[40];
	HEV sev;
	/* start a child process
	 * use an event sema for sync
	 */
	sprintf(sema, "\\SEM32\\xterm%s", &ptydev[8]);
	if (DosCreateEventSem(sema, &sev, DC_SEM_SHARED, False))
	    SysError(ERROR_FORK);

	switch ((screen->pid = fork())) {
	case -1:		/* error */
	    SysError(ERROR_FORK);
	default:		/* parent */
	    whoami = THE_PARENT;
	    DosWaitEventSem(sev, 1000L);
	    DosCloseEventSem(sev);
	    break;
	case 0:		/* child */
	    whoami = THE_CHILD;

	    opencons();
	    /* we don't need the socket, or the pty master anymore */
	    close(ConnectionNumber(screen->display));
	    close(screen->respond);

	    /* Now is the time to set up our process group and
	     * open up the pty slave.
	     */
	    if ((ttyfd = open(ttydev, O_RDWR)) < 0) {
		/* dumm gelaufen */
		fprintf(stderr, "Cannot open slave side of PTY\n");
		exit(1);
	    }

	    /* use the same tty name that everyone else will use
	     * (from ttyname)
	     */
#ifdef EMXNOTBOGUS
	    if ((ptr = ttyname(ttyfd)) != 0) {
		ttydev = x_strdup(ptr);
	    }
#else
	    ptr = ttydev;
#endif
	    /* for safety: enable DUPs */
	    ptioctl(ttyfd, XTY_ENADUP, 0);

	    /* change ownership of tty to real group and user id */
	    set_owner(ttydev, screen->uid, screen->gid,
		      (resource.messages ? 0622U : 0600U));

	    /* for the xf86sup-pty, we set the pty to bypass: OS/2 does
	     * not have a line discipline structure
	     */
	    {
		TERMIO_STRUCT t, t1;
		if (ptioctl(ttyfd, TCGETA, (char *) &t) < 0)
		    t = d_tio;

		t.c_iflag = ICRNL;
		t.c_oflag = OPOST | ONLCR;
		t.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;

		/* ignore error code, user will see it :-) */
		ptioctl(ttyfd, TCSETA, (char *) &t);

		/* set the console mode */
		if (Console) {
		    int on = 1;
		    if (ioctl(ttyfd, TIOCCONS, (char *) &on) == -1)
			fprintf(stderr, "%s: cannot open console: %s\n",
				ProgramName, strerror(errno));
		}
	    }

	    signal(SIGCHLD, SIG_DFL);
	    signal(SIGHUP, SIG_IGN);

	    /* restore various signals to their defaults */
	    signal(SIGINT, SIG_DFL);
	    signal(SIGQUIT, SIG_DFL);
	    signal(SIGTERM, SIG_DFL);

	    xtermCopyEnv(gblenvp);

	    xtermSetenv("TERM", resource.term_name);
	    if (!resource.term_name)
		*get_tcap_buffer(xw) = 0;

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
	    /* dup the tty */
	    for (i = 0; i <= 2; i++)
		if (i != ttyfd) {
		    IGNORE_RC(close(i));
		    IGNORE_RC(dup(ttyfd));
		}

	    /* and close the tty */
	    if (ttyfd > 2)
		close_fd(ttyfd);

	    setpgrp(0, pgrp);
	    (void) xtermResetIds(screen);

	    if (handshake.rows > 0 && handshake.cols > 0) {
		TRACE(("handshake ttysize: %dx%d\n",
		       handshake.rows, handshake.cols));
		set_max_row(screen, handshake.rows);
		set_max_col(screen, handshake.cols);
		TTYSIZE_ROWS(ts) = (ttySize_t) MaxRows(screen);
		TTYSIZE_COLS(ts) = (ttySize_t) MaxCols(screen);
		ts.ws_xpixel = (ttySize_t) FullWidth(screen);
		ts.ws_ypixel = (ttySize_t) FullHeight(screen);
	    }

	    sprintf(numbuf, "%d", MaxCols(screen));
	    xtermSetenv("COLUMNS", numbuf);
	    sprintf(numbuf, "%d", MaxRows(screen));
	    xtermSetenv("LINES", numbuf);

	    gblenvp = environ;

	    /* need to reset after all the ioctl bashing we did above */
	    ptioctl(0, TIOCSWINSZ, (char *) &ts);

	    signal(SIGHUP, SIG_DFL);

	    /* okay everything seems right, so tell the parent, we are going */
	    {
		char sema[40];
		HEV sev;
		sprintf(sema, "\\SEM32\\xterm%s", &ttydev[8]);
		DosOpenEventSem(sema, &sev);
		DosPostEventSem(sev);
		DosCloseEventSem(sev);
	    }

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
		execvpe(*command_to_exec, command_to_exec, gblenvp);

		/* print error message on screen */
		fprintf(stderr, "%s: Can't execvp %s\n",
			ProgramName, *command_to_exec);
	    }

	    /* use a layered mechanism to find a shell */
	    ptr = x_getenv("X11SHELL");
	    if (!ptr)
		ptr = x_getenv("SHELL");
	    if (!ptr)
		ptr = x_getenv("OS2_SHELL");
	    if (!ptr)
		ptr = "SORRY_NO_SHELL_FOUND";
	    xtermSetenv("XTERM_SHELL", ptr);

	    shname = x_basename(ptr);
	    if (command_to_exec) {
		char *exargv[10];	/*XXX */

		exargv[0] = ptr;
		exargv[1] = "/C";
		exargv[2] = command_to_exec[0];
		exargv[3] = command_to_exec[1];
		exargv[4] = command_to_exec[2];
		exargv[5] = command_to_exec[3];
		exargv[6] = command_to_exec[4];
		exargv[7] = command_to_exec[5];
		exargv[8] = command_to_exec[6];
		exargv[9] = 0;
		execvpe(exargv[0], exargv, gblenvp);

		/* print error message on screen */
		fprintf(stderr, "%s: Can't execvp %s\n",
			ProgramName, *command_to_exec);
	    } else {
		execlpe(ptr, shname, 0, gblenvp);

		/* Exec failed. */
		fprintf(stderr, "%s: Could not exec %s!\n",
			ProgramName, ptr);
	    }
	    sleep(5);

	    /* preventively shoot the parent */
	    kill(-getppid(), SIGTERM);

	    exit(ERROR_EXEC);
	}			/* endcase */
    }
    /* !am_slave */
    signal(SIGHUP, SIG_IGN);
/*
 * Unfortunately, System V seems to have trouble divorcing the child process
 * from the process group of xterm.  This is a problem because hitting the
 * INTR or QUIT characters on the keyboard will cause xterm to go away if we
 * don't ignore the signals.  This is annoying.
 */

/*  signal (SIGINT, SIG_IGN);*/
    signal(SIGINT, killit);
    signal(SIGTERM, killit);

    /* hung shell problem */
    signal(SIGQUIT, SIG_IGN);
/*  signal (SIGTERM, SIG_IGN);*/
    return 0;
}				/* end spawnXTerm */

SIGNAL_T
Exit(int n)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);
    int pty = TScreenOf(xw)->respond;
    close(pty);			/* close explicitly to avoid race with slave side */
#ifdef ALLOWLOGGING
    if (screen->logging)
	CloseLog(xw);
#endif
    if (am_slave < 0) {
	/* restore ownership of tty and pty */
	set_owner(ttydev, 0, 0, 0666U);
	set_owner(ptydev, 0, 0, 0666U);
    }

    /*
     * Close after releasing ownership to avoid race condition: other programs 
     * grabbing it, and *then* having us release ownership....
     */
    close(screen->respond);	/* close explicitly to avoid race with slave side */
#ifdef ALLOWLOGGING
    if (screen->logging)
	CloseLog(xw);
#endif

#ifdef NO_LEAKS
    if (n == 0) {
	TRACE(("Freeing memory leaks\n"));
	if (xw != 0) {
	    Display *dpy = TScreenOf(xw)->display;

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
	TRACE_CLOSE();
    }
#endif

    exit(n);
    SIGNAL_RETURN;
}

/* ARGSUSED */
static void
resize_termcap(XtermWidget xw)
{
}

/*
 * Does a non-blocking wait for a child process.  If the system
 * doesn't support non-blocking wait, do nothing.
 * Returns the pid of the child, or 0 or -1 if none or error.
 */
int
nonblocking_wait(void)
{
    pid_t pid;

    pid = waitpid(-1, NULL, WNOHANG);
    return pid;
}

/* ARGSUSED */
static SIGNAL_T
reapchild(int n GCC_UNUSED)
{
    int olderrno = errno;
    int pid;

    pid = wait(NULL);

    /* cannot re-enable signal before waiting for child
     * because then SVR4 loops.  Sigh.  HP-UX 9.01 too.
     */
    (void) signal(SIGCHLD, reapchild);

    do {
	if (pid == TScreenOf(term)->pid) {
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
    for (;;) {
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

int
GetBytesAvailable(int fd)
{
    long arg;
    ioctl(fd, FIONREAD, (char *) &arg);
    return (int) arg;
}

/* Utility function to try to hide system differences from
   everybody who used to call killpg() */

int
kill_process_group(int pid, int sig)
{
    return kill(-pid, sig);
}

int
ptioctl(int fd, int func, void *data)
{
    APIRET rc;
    ULONG len;
    struct pt_termios pt;
    TERMIO_STRUCT *t;
    int i;

    switch (func) {
    case TCGETA:
	rc = DosDevIOCtl(fd, XFREE86_PTY, XTY_TIOCGETA,
			 NULL, 0, NULL,
			 (ULONG *) & pt, sizeof(struct pt_termios), &len);
	if (rc)
	    return -1;
	t = (TERMIO_STRUCT *) data;
	t->c_iflag = pt.c_iflag;
	t->c_oflag = pt.c_oflag;
	t->c_cflag = pt.c_cflag;
	t->c_lflag = pt.c_lflag;
	for (i = 0; i < NCC; i++)
	    t->c_cc[i] = pt.c_cc[i];
	return 0;
    case TCSETA:
    case TCSETAW:
    case TCSETAF:
	t = (TERMIO_STRUCT *) data;
	pt.c_iflag = t->c_iflag;
	pt.c_oflag = t->c_oflag;
	pt.c_cflag = t->c_cflag;
	pt.c_lflag = t->c_lflag;

	for (i = 0; i < NCC; i++)
	    pt.c_cc[i] = t->c_cc[i];
	if (func == TCSETA)
	    i = XTY_TIOCSETA;
	else if (func == TCSETAW)
	    i = XTY_TIOCSETAW;
	else
	    i = XTY_TIOCSETAF;
	rc = DosDevIOCtl(fd, XFREE86_PTY, i,
			 (ULONG *) & pt, sizeof(struct pt_termios), &len,
			 NULL, 0, NULL);
	return (rc) ? -1 : 0;
    case TIOCCONS:
	return DosDevIOCtl(fd, XFREE86_PTY, XTY_TIOCCONS,
			   (ULONG *) data, sizeof(ULONG), &len,
			   NULL, 0, NULL);
    case TIOCSWINSZ:
	return DosDevIOCtl(fd, XFREE86_PTY, XTY_TIOCSWINSZ,
			   (ULONG *) data, sizeof(TTYSIZE_STRUCT), &len,
			   NULL, 0, NULL);
    case TIOCGWINSZ:
	return DosDevIOCtl(fd, XFREE86_PTY, XTY_TIOCGWINSZ,
			   NULL, 0, NULL,
			   (ULONG *) data, sizeof(TTYSIZE_STRUCT), &len);
    case XTY_ENADUP:
	i = 1;
	return DosDevIOCtl(fd, XFREE86_PTY, XTY_ENADUP,
			   (ULONG *) & i, sizeof(ULONG), &len,
			   NULL, 0, NULL);
    case XTY_TRACE:
	i = 2;
	return DosDevIOCtl(fd, XFREE86_PTY, XTY_TRACE,
			   (ULONG *) & i, sizeof(ULONG), &len,
			   NULL, 0, NULL);
    case PTMS_GETPTY:
	i = 1;
	return DosDevIOCtl(fd, XFREE86_PTY, PTMS_GETPTY,
			   (ULONG *) & i, sizeof(ULONG), &len,
			   (UCHAR *) data, 14, &len);
    default:
	return -1;
    }
}
