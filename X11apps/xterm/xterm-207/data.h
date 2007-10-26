/* $XTermId: data.h,v 1.92 2005/09/18 23:48:12 tom Exp $ */

/*
 *	$Xorg: data.h,v 1.3 2000/08/17 19:55:08 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/data.h,v 3.37 2005/09/18 23:48:12 dickey Exp $ */

/*
 * Copyright 2002-2004,2005 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 *
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef included_data_h
#define included_data_h 1

#include <xterm.h>

extern Widget toplevel;

extern XtAppContext app_con;

#ifdef VMS
/* actually in vms.c */
extern int tt_width;
extern int tt_length;
extern int tt_changed;
extern int tt_pasting;
extern int tt_new_output;
#define VMS_TERM_BUFFER_SIZE	500
struct q_head {
    int flink;
    int blink;
};
extern struct q_head read_queue;
#endif

#if OPT_TEK4014
extern Char *Tpushb;
extern Char *Tpushback;
extern TekLink *TekRefresh;
extern TekWidget tekWidget;
extern Widget tekshellwidget;
extern int T_lastx;
extern int T_lasty;
extern int Ttoggled;
extern jmp_buf Tekend;
#endif

extern char *ptydev;
extern char *ttydev;
extern char *xterm_name;
extern int hold_screen;

#if OPT_ZICONBEEP
extern int zIconBeep;
extern Boolean zIconBeep_flagged;
#endif

#if OPT_SAME_NAME
extern Boolean sameName;
#endif

extern PtyData *VTbuffer;
extern int am_slave;
extern int max_plus1;
extern jmp_buf VTend;

#ifdef DEBUG
extern int debug;
#endif /* DEBUG */

extern PtySelect Select_mask;
extern PtySelect X_mask;
extern PtySelect pty_mask;

extern Boolean waitingForTrackInfo;

extern EventMode eventMode;

extern XtermWidget term;

extern SIG_ATOMIC_T need_cleanup;

#if defined(HAVE_XKB_BELL_EXT)
#include <X11/XKBlib.h>		/* has the prototype */
#include <X11/extensions/XKBbells.h>	/* has the XkbBI_xxx definitions */
#endif

#ifndef XkbBI_Info
#define	XkbBI_Info			0
#define	XkbBI_MinorError		1
#define	XkbBI_MajorError		2
#define	XkbBI_TerminalBell		9
#define	XkbBI_MarginBell		10
#endif

extern char *ProgramName;
extern Arg ourTopLevelShellArgs[];
extern Cardinal number_ourTopLevelShellArgs;
extern Bool waiting_for_initial_map;
extern Atom wm_delete_window;

typedef struct XTERM_RESOURCE {
    char *xterm_name;
    char *icon_geometry;
    char *title;
    char *icon_name;
    char *term_name;
    char *tty_modes;

    int minBufSize;
    int maxBufSize;

    Boolean hold_screen;	/* true if we keep window open  */
    Boolean utmpInhibit;
    Boolean utmpDisplayId;
    Boolean messages;

    String keyboardType;
    Boolean sunFunctionKeys;	/* %%% should be widget resource? */
#if OPT_SUNPC_KBD
    Boolean sunKeyboard;
#endif
#if OPT_HP_FUNC_KEYS
    Boolean hpFunctionKeys;
#endif
#if OPT_SCO_FUNC_KEYS
    Boolean scoFunctionKeys;
#endif

#if OPT_INITIAL_ERASE
    Boolean ptyInitialErase;	/* if true, use pty's sense of erase char */
    Boolean backarrow_is_erase;	/* override backspace/delete */
#endif
    Boolean wait_for_map;
    Boolean useInsertMode;
#if OPT_ZICONBEEP
    int zIconBeep;		/* beep level when output while iconified */
#endif
#if OPT_PTY_HANDSHAKE
    Boolean ptyHandshake;	/* use pty-handshaking */
#endif
#if OPT_SAME_NAME
    Boolean sameName;		/* Don't change the title or icon name if it is
				 * the same.  This prevents flicker on the
				 * screen at the cost of an extra request to
				 * the server.
				 */
#endif
#if OPT_SESSION_MGT
    Boolean sessionMgt;
#endif
#if OPT_TOOLBAR
    Boolean toolBar;
#endif
} XTERM_RESOURCE;

extern XTERM_RESOURCE resource;

#endif /* included_data_h */
