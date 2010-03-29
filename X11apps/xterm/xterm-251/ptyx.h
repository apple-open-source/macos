/* $XTermId: ptyx.h,v 1.637 2009/11/10 00:06:24 tom Exp $ */

/*
 * Copyright 1999-2008,2009 by Thomas E. Dickey
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

#ifndef included_ptyx_h
#define included_ptyx_h 1

#ifdef HAVE_CONFIG_H
#include <xtermcfg.h>
#endif

/* ptyx.h */
/* @(#)ptyx.h	X10/6.6	11/10/86 */

#include <X11/IntrinsicP.h>
#include <X11/Shell.h>		/* for XtNdieCallback, etc. */
#include <X11/StringDefs.h>	/* for standard resource names */
#include <X11/Xmu/Misc.h>	/* For Max() and Min(). */
#include <X11/Xfuncs.h>
#include <X11/Xosdefs.h>
#include <X11/Xmu/Converters.h>
#ifdef XRENDERFONT
#include <X11/Xft/Xft.h>
#endif

/* adapted from IntrinsicI.h */
#define MyStackAlloc(size, stack_cache_array)     \
    ((size) <= sizeof(stack_cache_array)	  \
    ?  (XtPointer)(stack_cache_array)		  \
    :  (XtPointer)malloc((unsigned)(size)))

#define MyStackFree(pointer, stack_cache_array) \
    if ((pointer) != ((char *)(stack_cache_array))) free(pointer)

/* adapted from vile (vi-like-emacs) */
#define TypeCallocN(type,n)	(type *)calloc((n), sizeof(type))
#define TypeCalloc(type)	TypeCallocN(type,1)

#define TypeMallocN(type,n)	(type *)malloc(sizeof(type) * (n))
#define TypeMalloc(type)	TypeMallocN(type,1)

#define TypeRealloc(type,n,p)	(type *)realloc(p, (n) * sizeof(type))

/* use these to allocate partly-structured data */
#define CastMallocN(type,n)	(type *)malloc(sizeof(type) + (n))
#define CastMalloc(type)	CastMallocN(type,0)

#define BumpBuffer(type, buffer, size, want) \
	if (want >= size) { \
	    size = 1 + (want * 2); \
	    buffer = TypeRealloc(type, size, buffer); \
	}

#define BfBuf(type) screen->bf_buf_##type
#define BfLen(type) screen->bf_len_##type

#define TypedBuffer(type) \
	type		*bf_buf_##type; \
	Cardinal	bf_len_##type

#define BumpTypedBuffer(type, want) \
	BumpBuffer(type, BfBuf(type), BfLen(type), want)

#define FreeTypedBuffer(type) \
	if (BfBuf(type) != 0) { \
	    free(BfBuf(type)); \
	    BfBuf(type) = 0; \
	} \
	BfLen(type) = 0

/*
** System V definitions
*/

#ifdef att
#define ATT
#endif

#ifdef SVR4
#undef  SYSV			/* predefined on Solaris 2.4 */
#define SYSV			/* SVR4 is (approx) superset of SVR3 */
#define ATT
#endif

#ifdef SYSV
#ifdef X_NOT_POSIX
#if !defined(CRAY) && !defined(SVR4)
#define	dup2(fd1,fd2)	((fd1 == fd2) ? fd1 : \
				(close(fd2), fcntl(fd1, F_DUPFD, fd2)))
#endif
#endif
#endif /* SYSV */

/*
 * Newer versions of <X11/Xft/Xft.h> have a version number.  We use certain
 * features from that.
 */
#if defined(XRENDERFONT) && defined(XFT_VERSION) && XFT_VERSION >= 20100
#define HAVE_TYPE_FCCHAR32	1	/* compatible: XftChar16 */
#define HAVE_TYPE_XFTCHARSPEC	1	/* new type XftCharSpec */
#endif

/*
** Definitions to simplify ifdef's for pty's.
*/
#define USE_PTY_DEVICE 1
#define USE_PTY_SEARCH 1

#if defined(__osf__) || (defined(linux) && defined(__GLIBC__) && (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
#undef USE_PTY_DEVICE
#undef USE_PTY_SEARCH
#define USE_PTS_DEVICE 1
#elif defined(VMS)
#undef USE_PTY_DEVICE
#undef USE_PTY_SEARCH
#elif defined(PUCC_PTYD)
#undef USE_PTY_SEARCH
#elif (defined(sun) && defined(SVR4)) || defined(_ALL_SOURCE) || defined(__CYGWIN__)
#undef USE_PTY_SEARCH
#endif

#if defined(SYSV) && defined(i386) && !defined(SVR4)
#define ATT
#define USE_HANDSHAKE 1
#define USE_ISPTS_FLAG 1
#endif

#if (defined (__GLIBC__) && ((__GLIBC__ > 2) || (__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 1)))
#define USE_USG_PTYS
#define USE_HANDSHAKE 0	/* "recent" Linux systems do not require handshaking */
#elif (defined(ATT) && !defined(__sgi)) || defined(__MVS__) || (defined(SYSV) && defined(i386))
#define USE_USG_PTYS
#else
#define USE_HANDSHAKE 1
#endif

/*
 * More systems than not require pty-handshaking.
 */
#ifndef USE_HANDSHAKE
#define USE_HANDSHAKE 1
#endif

/*
** allow for mobility of the pty master/slave directories
*/
#ifndef PTYDEV
#if defined(__hpux)
#define	PTYDEV		"/dev/ptym/ptyxx"
#elif defined(__MVS__)
#define	PTYDEV		"/dev/ptypxxxx"
#else
#define	PTYDEV		"/dev/ptyxx"
#endif
#endif	/* !PTYDEV */

#ifndef TTYDEV
#if defined(__hpux)
#define TTYDEV		"/dev/pty/ttyxx"
#elif defined(__MVS__)
#define TTYDEV		"/dev/ptypxxxx"
#elif defined(USE_PTS_DEVICE)
#define TTYDEV		"/dev/pts/0"
#else
#define	TTYDEV		"/dev/ttyxx"
#endif
#endif	/* !TTYDEV */

#ifndef PTYCHAR1
#ifdef __hpux
#define PTYCHAR1	"zyxwvutsrqp"
#else	/* !__hpux */
#ifdef __UNIXOS2__
#define PTYCHAR1	"pq"
#else
#define	PTYCHAR1	"pqrstuvwxyzPQRSTUVWXYZ"
#endif  /* !__UNIXOS2__ */
#endif	/* !__hpux */
#endif	/* !PTYCHAR1 */

#ifndef PTYCHAR2
#ifdef __hpux
#define	PTYCHAR2	"fedcba9876543210"
#else	/* !__hpux */
#if defined(__DragonFly__) || defined(__FreeBSD__)
#define	PTYCHAR2	"0123456789abcdefghijklmnopqrstuv"
#else /* !__FreeBSD__ */
#define	PTYCHAR2	"0123456789abcdef"
#endif /* !__FreeBSD__ */
#endif	/* !__hpux */
#endif	/* !PTYCHAR2 */

#ifndef TTYFORMAT
#if defined(CRAY)
#define TTYFORMAT "/dev/ttyp%03d"
#elif defined(__MVS__)
#define TTYFORMAT "/dev/ttyp%04d"
#else
#define TTYFORMAT "/dev/ttyp%d"
#endif
#endif /* TTYFORMAT */

#ifndef PTYFORMAT
#ifdef CRAY
#define PTYFORMAT "/dev/pty/%03d"
#elif defined(__MVS__)
#define PTYFORMAT "/dev/ptyp%04d"
#else
#define PTYFORMAT "/dev/ptyp%d"
#endif
#endif /* PTYFORMAT */

#ifndef PTYCHARLEN
#ifdef CRAY
#define PTYCHARLEN 3
#elif defined(__MVS__)
#define PTYCHARLEN 8     /* OS/390 stores, e.g. ut_id="ttyp1234"  */
#else
#define PTYCHARLEN 2
#endif
#endif

#ifndef MAXPTTYS
#ifdef CRAY
#define MAXPTTYS 256
#else
#define MAXPTTYS 2048
#endif
#endif

/* Until the translation manager comes along, I have to do my own translation of
 * mouse events into the proper routines. */

typedef enum {
    NORMAL = 0
    , LEFTEXTENSION
    , RIGHTEXTENSION
} EventMode;

/*
 * The origin of a screen is 0, 0.  Therefore, the number of rows
 * on a screen is screen->max_row + 1, and similarly for columns.
 */
#define MaxCols(screen)		((screen)->max_col + 1)
#define MaxRows(screen)		((screen)->max_row + 1)

typedef unsigned char Char;		/* to support 8 bit chars */
typedef Char *ScrnPtr;
typedef ScrnPtr *ScrnBuf;

#define CharOf(n) ((unsigned char)(n))

typedef struct {
    int row;
    int col;
} CELL;

#define isSameRow(a,b)		((a)->row == (b)->row)
#define isSameCol(a,b)		((a)->col == (b)->col)
#define isSameCELL(a,b)		(isSameRow(a,b) && isSameCol(a,b))

#define xBIT(n)         (1 << (n))

/*
 * ANSI emulation, special character codes
 */
#define ANSI_BEL	0x07
#define	ANSI_FF		0x0C		/* C0, C1 control names		*/
#define	ANSI_NAK	0x15
#define	ANSI_CAN	0x18
#define	ANSI_ESC	0x1B
#define	ANSI_SPA	0x20
#define XTERM_POUND	0x1E		/* internal mapping for '#'	*/
#define	ANSI_DEL	0x7F
#define	ANSI_SS2	0x8E
#define	ANSI_SS3	0x8F
#define	ANSI_DCS	0x90
#define	ANSI_SOS	0x98
#define	ANSI_CSI	0x9B
#define	ANSI_ST		0x9C
#define	ANSI_OSC	0x9D
#define	ANSI_PM		0x9E
#define	ANSI_APC	0x9F

#define MIN_DECID  52			/* can emulate VT52 */
#define MAX_DECID 420			/* ...through VT420 */

#ifndef DFT_DECID
#define DFT_DECID "vt100"		/* default VT100 */
#endif

#ifndef DFT_KBD_DIALECT
#define DFT_KBD_DIALECT "B"		/* default USASCII */
#endif

/* constants used for utf8 mode */
#define UCS_REPL	0xfffd
#define UCS_LIMIT	0x80000000U	/* both limit and flag for non-UCS */

#define TERMCAP_SIZE 1500		/* 1023 is standard; 'screen' exceeds */

#define NMENUFONTS 9			/* font entries in fontMenu */

#define	NBOX	5			/* Number of Points in box	*/
#define	NPARAM	30			/* Max. parameters		*/

typedef struct {
	char *opt;
	char *desc;
} OptionHelp;

typedef short ParmType;

typedef struct {
	Char		a_type;		/* CSI, etc., see unparseq()	*/
	Char		a_pintro;	/* private-mode char, if any	*/
	Char		a_inters;	/* special (before final-char)	*/
	Char		a_final;	/* final-char			*/
	ParmType	a_nparam;	/* # of parameters		*/
	ParmType	a_param[NPARAM]; /* Parameters			*/
} ANSI;

#define TEK_FONT_LARGE 0
#define TEK_FONT_2 1
#define TEK_FONT_3 2
#define TEK_FONT_SMALL 3
#define	TEKNUMFONTS 4

/* Actually there are 5 types of lines, but four are non-solid lines */
#define	TEKNUMLINES	4

typedef struct {
	int	x;
	int	y;
	int	fontsize;
	unsigned linetype;
} Tmodes;

typedef struct {
	int Twidth;
	int Theight;
} T_fontsize;

typedef struct {
	short *bits;
	int x;
	int y;
	int width;
	int height;
} BitmapBits;

#define	SAVELINES		64      /* default # lines to save      */
#define SCROLLLINES 1			/* default # lines to scroll    */

#define EXCHANGE(a,b,tmp) tmp = a; a = b; b = tmp

/***====================================================================***/

#if (XtSpecificationRelease < 6)
#ifndef NO_ACTIVE_ICON
#define NO_ACTIVE_ICON 1 /* Note: code relies on an X11R6 function */
#endif
#endif

#ifndef OPT_AIX_COLORS
#define OPT_AIX_COLORS  1 /* true if xterm is configured with AIX (16) colors */
#endif

#ifndef OPT_ALLOW_XXX_OPS
#define OPT_ALLOW_XXX_OPS 1 /* true if xterm adds "Allow XXX Ops" submenu */
#endif

#ifndef OPT_BLINK_CURS
#define OPT_BLINK_CURS  1 /* true if xterm has blinking cursor capability */
#endif

#ifndef OPT_BLINK_TEXT
#define OPT_BLINK_TEXT  OPT_BLINK_CURS /* true if xterm has blinking text capability */
#endif

#ifndef OPT_BOX_CHARS
#define OPT_BOX_CHARS	1 /* true if xterm can simulate box-characters */
#endif

#ifndef OPT_BROKEN_OSC
#ifdef linux
#define OPT_BROKEN_OSC	1 /* man console_codes, 1st paragraph - cf: ECMA-48 */
#else
#define OPT_BROKEN_OSC	0 /* true if xterm allows Linux's broken OSC parsing */
#endif
#endif

#ifndef OPT_BROKEN_ST
#define OPT_BROKEN_ST	1 /* true if xterm allows old/broken OSC parsing */
#endif

#ifndef OPT_C1_PRINT
#define OPT_C1_PRINT	1 /* true if xterm allows C1 controls to be printable */
#endif

#ifndef OPT_CLIP_BOLD
#define OPT_CLIP_BOLD	1 /* true if xterm uses clipping to avoid bold-trash */
#endif

#ifndef OPT_COLOR_CLASS
#define OPT_COLOR_CLASS 1 /* true if xterm uses separate color-resource classes */
#endif

#ifndef OPT_COLOR_RES
#define OPT_COLOR_RES   1 /* true if xterm delays color-resource evaluation */
#endif

#ifndef OPT_DABBREV
#define OPT_DABBREV 0	/* dynamic abbreviations */
#endif

#ifndef OPT_DEC_CHRSET
#define OPT_DEC_CHRSET  1 /* true if xterm is configured for DEC charset */
#endif

#ifndef OPT_DEC_LOCATOR
#define	OPT_DEC_LOCATOR 0 /* true if xterm supports VT220-style mouse events */
#endif

#ifndef OPT_DEC_RECTOPS
#define OPT_DEC_RECTOPS 0 /* true if xterm is configured for VT420 rectangles */
#endif

#ifndef OPT_DEC_SOFTFONT
#define OPT_DEC_SOFTFONT 0 /* true if xterm is configured for VT220 softfonts */
#endif

#ifndef OPT_EBCDIC
#ifdef __MVS__
#define OPT_EBCDIC 1
#else
#define OPT_EBCDIC 0
#endif
#endif

#ifndef OPT_EXEC_XTERM
#define OPT_EXEC_XTERM 0 /* true if xterm can fork/exec copies of itself */
#endif

#ifndef OPT_EXTRA_PASTE
#define OPT_EXTRA_PASTE 1
#endif

#ifndef OPT_FIFO_LINES
#define OPT_FIFO_LINES 0 /* optimize save-lines feature using FIFO */
#endif

#ifndef OPT_FOCUS_EVENT
#define OPT_FOCUS_EVENT	1 /* focus in/out events */
#endif

#ifndef OPT_HP_FUNC_KEYS
#define OPT_HP_FUNC_KEYS 0 /* true if xterm supports HP-style function keys */
#endif

#ifndef OPT_I18N_SUPPORT
#if (XtSpecificationRelease >= 5)
#define OPT_I18N_SUPPORT 1 /* true if xterm uses internationalization support */
#else
#define OPT_I18N_SUPPORT 0
#endif
#endif

#ifndef OPT_INITIAL_ERASE
#define OPT_INITIAL_ERASE 1 /* use pty's erase character if it's not 128 */
#endif

#ifndef OPT_INPUT_METHOD
#if (XtSpecificationRelease >= 6)
#define OPT_INPUT_METHOD 1 /* true if xterm uses input-method support */
#else
#define OPT_INPUT_METHOD 0
#endif
#endif

#ifndef OPT_ISO_COLORS
#define OPT_ISO_COLORS  1 /* true if xterm is configured with ISO colors */
#endif

#ifndef OPT_256_COLORS
#define OPT_256_COLORS  0 /* true if xterm is configured with 256 colors */
#endif

#ifndef OPT_88_COLORS
#define OPT_88_COLORS	0 /* true if xterm is configured with 88 colors */
#endif

#ifndef OPT_HIGHLIGHT_COLOR
#define OPT_HIGHLIGHT_COLOR 1 /* true if xterm supports color highlighting */
#endif

#ifndef OPT_LOAD_VTFONTS
#define OPT_LOAD_VTFONTS 0 /* true if xterm has load-vt-fonts() action */
#endif

#ifndef OPT_LUIT_PROG
#define OPT_LUIT_PROG   0 /* true if xterm supports luit */
#endif

#ifndef OPT_MAXIMIZE
#define OPT_MAXIMIZE	1 /* add actions for iconify ... maximize */
#endif

#ifndef OPT_MINI_LUIT
#define OPT_MINI_LUIT   0 /* true if xterm supports built-in mini-luit */
#endif

#ifndef OPT_MOD_FKEYS
#define OPT_MOD_FKEYS	1 /* modify cursor- and function-keys in normal mode */
#endif

#ifndef OPT_NUM_LOCK
#define OPT_NUM_LOCK	1 /* use NumLock key only for numeric-keypad */
#endif

#ifndef OPT_PASTE64
#define OPT_PASTE64	0 /* program control of select/paste via base64 */
#endif

#ifndef OPT_PC_COLORS
#define OPT_PC_COLORS   1 /* true if xterm supports PC-style (bold) colors */
#endif

#ifndef OPT_PTY_HANDSHAKE
#define OPT_PTY_HANDSHAKE USE_HANDSHAKE	/* avoid pty races on older systems */
#endif

#ifndef OPT_PRINT_COLORS
#define OPT_PRINT_COLORS 1 /* true if we print color information */
#endif

#ifndef OPT_READLINE
#define OPT_READLINE	0 /* mouse-click/paste support for readline */
#endif

#ifndef OPT_RENDERFONT
#ifdef XRENDERFONT
#define OPT_RENDERFONT 1
#else
#define OPT_RENDERFONT 0
#endif
#endif

#ifndef OPT_RENDERWIDE
#if OPT_RENDERFONT && OPT_WIDE_CHARS && defined(HAVE_TYPE_XFTCHARSPEC)
#define OPT_RENDERWIDE 1
#else
#define OPT_RENDERWIDE 0
#endif
#endif

#ifndef OPT_SAME_NAME
#define OPT_SAME_NAME   1 /* suppress redundant updates of title, icon, etc. */
#endif

#ifndef OPT_SAVE_LINES
#define OPT_SAVE_LINES OPT_FIFO_LINES /* optimize save-lines feature */
#endif

#ifndef OPT_SCO_FUNC_KEYS
#define OPT_SCO_FUNC_KEYS 0 /* true if xterm supports SCO-style function keys */
#endif

#ifndef OPT_SUN_FUNC_KEYS
#define OPT_SUN_FUNC_KEYS 1 /* true if xterm supports Sun-style function keys */
#endif

#ifndef OPT_SELECT_REGEX
#define OPT_SELECT_REGEX 0 /* true if xterm supports regular-expression selects */
#endif

#ifndef OPT_SESSION_MGT
#if defined(XtNdieCallback) && defined(XtNsaveCallback)
#define OPT_SESSION_MGT 1
#else
#define OPT_SESSION_MGT 0
#endif
#endif

#ifndef OPT_SHIFT_FONTS
#define OPT_SHIFT_FONTS 1 /* true if xterm interprets fontsize-shifting */
#endif

#ifndef OPT_SUNPC_KBD
#define OPT_SUNPC_KBD	1 /* true if xterm supports Sun/PC keyboard map */
#endif

#ifndef OPT_TCAP_FKEYS
#define OPT_TCAP_FKEYS	0 /* true for experimental termcap function-keys */
#endif

#ifndef OPT_TCAP_QUERY
#define OPT_TCAP_QUERY	0 /* true for experimental termcap query */
#endif

#ifndef OPT_TEK4014
#define OPT_TEK4014     1 /* true if we're using tek4014 emulation */
#endif

#ifndef OPT_TOOLBAR
#define OPT_TOOLBAR	0 /* true if xterm supports toolbar menus */
#endif

#ifndef OPT_TRACE
#define OPT_TRACE       0 /* true if we're using debugging traces */
#endif

#ifndef OPT_TRACE_FLAGS
#define OPT_TRACE_FLAGS 0 /* additional tracing used for SCRN_BUF_FLAGS */
#endif

#ifndef OPT_VT52_MODE
#define OPT_VT52_MODE   1 /* true if xterm supports VT52 emulation */
#endif

#ifndef OPT_WIDE_CHARS
#define OPT_WIDE_CHARS  0 /* true if xterm supports 16-bit characters */
#endif

#ifndef OPT_WIDER_ICHAR
#define OPT_WIDER_ICHAR 1 /* true if xterm uses 32-bits for wide-chars */
#endif

#ifndef OPT_XMC_GLITCH
#define OPT_XMC_GLITCH	0 /* true if xterm supports xmc (magic cookie glitch) */
#endif

#ifndef OPT_ZICONBEEP
#define OPT_ZICONBEEP   1 /* true if xterm supports "-ziconbeep" option */
#endif

/***====================================================================***/

#if OPT_AIX_COLORS && !OPT_ISO_COLORS
/* You must have ANSI/ISO colors to support AIX colors */
#undef  OPT_AIX_COLORS
#define OPT_AIX_COLORS 0
#endif

#if OPT_COLOR_RES && !OPT_ISO_COLORS
/* You must have ANSI/ISO colors to support ColorRes logic */
#undef  OPT_COLOR_RES
#define OPT_COLOR_RES 0
#endif

#if OPT_256_COLORS && (OPT_WIDE_CHARS || OPT_RENDERFONT || OPT_XMC_GLITCH)
/* It's actually more complicated than that - but by trimming options you can
 * have 256 color resources though.
 */
#define OPT_COLOR_RES2 1
#else
#define OPT_COLOR_RES2 0
#endif

#if OPT_PASTE64 && !OPT_READLINE
/* OPT_PASTE64 uses logic from OPT_READLINE */
#undef  OPT_READLINE
#define OPT_READLINE 1
#endif

#if OPT_PC_COLORS && !OPT_ISO_COLORS
/* You must have ANSI/ISO colors to support PC colors */
#undef  OPT_PC_COLORS
#define OPT_PC_COLORS 0
#endif

#if OPT_PRINT_COLORS && !OPT_ISO_COLORS
/* You must have ANSI/ISO colors to be able to print them */
#undef  OPT_PRINT_COLORS
#define OPT_PRINT_COLORS 0
#endif

#if OPT_256_COLORS && !OPT_ISO_COLORS
/* You must have ANSI/ISO colors to support 256 colors */
#undef  OPT_256_COLORS
#define OPT_256_COLORS 0
#endif

#if OPT_88_COLORS && !OPT_ISO_COLORS
/* You must have ANSI/ISO colors to support 88 colors */
#undef  OPT_88_COLORS
#define OPT_88_COLORS 0
#endif

#if OPT_88_COLORS && OPT_256_COLORS
/* 256 colors supersedes 88 colors */
#undef  OPT_88_COLORS
#define OPT_88_COLORS 0
#endif

/***====================================================================***/

/*
 * Indices for menu_font_names[][]
 */
typedef enum {
    fNorm = 0			/* normal font */
    , fBold			/* bold font */
#if OPT_WIDE_CHARS
    , fWide			/* double-width font */
    , fWBold			/* double-width bold font */
#endif
    , fMAX
} VTFontEnum;

/*
 * Indices for cachedGCs.c (unrelated to VTFontEnum).
 */
typedef enum {
    gcNorm = 0
    , gcBold
    , gcNormReverse
    , gcBoldReverse
#if OPT_BOX_CHARS
    , gcLine
    , gcDots
#endif
#if OPT_DEC_CHRSET
    , gcCNorm
    , gcCBold
#endif
#if OPT_WIDE_CHARS
    , gcWide
    , gcWBold
    , gcWideReverse
    , gcWBoldReverse
#endif
    , gcVTcursNormal
    , gcVTcursFilled
    , gcVTcursReverse
    , gcVTcursOutline
#if OPT_TEK4014
    , gcTKcurs
#endif
    , gcMAX
} CgsEnum;

#define for_each_text_gc(n) for (n = gcNorm; n < gcVTcursNormal; ++n)
#define for_each_curs_gc(n) for (n = gcVTcursNormal; n <= gcVTcursOutline; ++n)
#define for_each_gc(n)      for (n = gcNorm; n < gcMAX; ++n)

/* indices for the normal terminal colors in screen.Tcolors[] */
typedef enum {
    TEXT_FG = 0			/* text foreground */
    , TEXT_BG			/* text background */
    , TEXT_CURSOR		/* text cursor */
    , MOUSE_FG			/* mouse foreground */
    , MOUSE_BG			/* mouse background */
#if OPT_TEK4014
    , TEK_FG			/* tektronix foreground */
    , TEK_BG			/* tektronix background */
    , TEK_CURSOR		/* tektronix cursor */
#endif
#if OPT_HIGHLIGHT_COLOR
    , HIGHLIGHT_BG		/* highlight background */
    , HIGHLIGHT_FG		/* highlight foreground */
#endif
    , NCOLORS			/* total number of colors */
} TermColors;

/* indices for mapping multiple clicks to selection types */
typedef enum {
    Select_CHAR=0
    ,Select_WORD
    ,Select_LINE
    ,Select_GROUP
    ,Select_PAGE
    ,Select_ALL
#if OPT_SELECT_REGEX
    ,Select_REGEX
#endif
    ,NSELECTUNITS
} SelectUnit;

typedef enum {
    /* 1-21 are chosen to be the same as the control-sequence coding */
    ewRestoreWin = 1
    , ewMinimizeWin = 2
    , ewSetWinPosition = 3
    , ewSetWinSizePixels = 4
    , ewRaiseWin = 5
    , ewLowerWin = 6
    , ewRefreshWin = 7
    , ewSetWinSizeChars = 8
#if OPT_MAXIMIZE
    , ewMaximizeWin = 9
#endif
    , ewGetWinState = 11
    , ewGetWinPosition = 13
    , ewGetWinSizePixels = 14
    , ewGetWinSizeChars = 18
#if OPT_MAXIMIZE
    , ewGetScreenSizeChars = 19
#endif
    , ewGetIconTitle = 20
    , ewGetWinTitle = 21
    , ewPushTitle = 22
    , ewPopTitle = 23
    /* these do not fit into that scheme, which is why we use an array */
    , ewSetWinLines
    , ewSetXprop
    , ewGetSelection
    , ewSetSelection
    /* get the size of the array... */
    , ewLAST
} WindowOps;

#define	COLOR_DEFINED(s,w)	((s)->which & (1<<(w)))
#define	COLOR_VALUE(s,w)	((s)->colors[w])
#define	SET_COLOR_VALUE(s,w,v)	(((s)->colors[w] = (v)), ((s)->which |= (1<<(w))))

#define	COLOR_NAME(s,w)		((s)->names[w])
#define	SET_COLOR_NAME(s,w,v)	(((s)->names[w] = (v)), ((s)->which |= (1<<(w))))

#define	UNDEFINE_COLOR(s,w)	((s)->which &= (~((w)<<1)))

/***====================================================================***/

#if OPT_ISO_COLORS
#define TERM_COLOR_FLAGS(xw)	((xw)->flags & (FG_COLOR|BG_COLOR))
#define COLOR_0		0
#define COLOR_1		1
#define COLOR_2		2
#define COLOR_3		3
#define COLOR_4		4
#define COLOR_5		5
#define COLOR_6		6
#define COLOR_7		7
#define COLOR_8		8
#define COLOR_9		9
#define COLOR_10	10
#define COLOR_11	11
#define COLOR_12	12
#define COLOR_13	13
#define COLOR_14	14
#define COLOR_15	15
#define MIN_ANSI_COLORS 16

#if OPT_256_COLORS
# define NUM_ANSI_COLORS 256
#elif OPT_88_COLORS
# define NUM_ANSI_COLORS 88
#else
# define NUM_ANSI_COLORS MIN_ANSI_COLORS
#endif

#if NUM_ANSI_COLORS > MIN_ANSI_COLORS
# define OPT_EXT_COLORS  1
#else
# define OPT_EXT_COLORS  0
#endif

#define COLOR_BD	(NUM_ANSI_COLORS)	/* BOLD */
#define COLOR_UL	(NUM_ANSI_COLORS+1)	/* UNDERLINE */
#define COLOR_BL	(NUM_ANSI_COLORS+2)	/* BLINK */
#define COLOR_RV	(NUM_ANSI_COLORS+3)	/* REVERSE */
#define MAXCOLORS	(NUM_ANSI_COLORS+4)
#ifndef DFT_COLORMODE
#define DFT_COLORMODE True	/* default colorMode resource */
#endif

#define ReverseOrHilite(screen,flags,hilite) \
		(( screen->colorRVMode && hilite ) || \
		    ( !screen->colorRVMode && \
		      (( (flags & INVERSE) && !hilite) || \
		       (!(flags & INVERSE) &&  hilite)) ))

/* Define a fake XK code, we need it for the fake color response in
 * xtermcapKeycode(). */
#if OPT_TCAP_QUERY && OPT_ISO_COLORS
# define XK_COLORS 0x0003
#endif

#else	/* !OPT_ISO_COLORS */

#define TERM_COLOR_FLAGS(xw) 0

#define ReverseOrHilite(screen,flags,hilite) \
		      (( (flags & INVERSE) && !hilite) || \
		       (!(flags & INVERSE) &&  hilite))

#endif	/* OPT_ISO_COLORS */

#if OPT_AIX_COLORS
#define if_OPT_AIX_COLORS(screen, code) if(screen->colorMode) code
#else
#define if_OPT_AIX_COLORS(screen, code) /* nothing */
#endif

#if OPT_256_COLORS || OPT_88_COLORS || OPT_ISO_COLORS
# define if_OPT_ISO_COLORS(screen, code) if (screen->colorMode) code
#else
# define if_OPT_ISO_COLORS(screen, code) /* nothing */
#endif

#define COLOR_RES_NAME(root) "color" root

#if OPT_COLOR_CLASS
#define COLOR_RES_CLASS(root) "Color" root
#else
#define COLOR_RES_CLASS(root) XtCForeground
#endif

#if OPT_COLOR_RES
#define COLOR_RES(root,offset,value) Sres(COLOR_RES_NAME(root), COLOR_RES_CLASS(root), offset.resource, value)
#define COLOR_RES2(name,class,offset,value) Sres(name, class, offset.resource, value)
#else
#define COLOR_RES(root,offset,value) Cres(COLOR_RES_NAME(root), COLOR_RES_CLASS(root), offset, value)
#define COLOR_RES2(name,class,offset,value) Cres(name, class, offset, value)
#endif

#define CLICK_RES_NAME(count)  "on" count "Clicks"
#define CLICK_RES_CLASS(count) "On" count "Clicks"
#define CLICK_RES(count,offset,value) Sres(CLICK_RES_NAME(count), CLICK_RES_CLASS(count), offset, value)

/***====================================================================***/

#if OPT_DEC_CHRSET
#define if_OPT_DEC_CHRSET(code) code
	/* Use 2 bits for encoding the double high/wide sense of characters */
#define CSET_SWL        0
#define CSET_DHL_TOP    1
#define CSET_DHL_BOT    2
#define CSET_DWL        3
#define NUM_CHRSET      8	/* normal/bold and 4 CSET_xxx values */

	/* Use remaining bits for encoding the other character-sets */
#define CSET_NORMAL(code)  ((code) == CSET_SWL)
#define CSET_DOUBLE(code)  (!CSET_NORMAL(code) && !CSET_EXTEND(code))
#define CSET_EXTEND(code)  ((int)(code) > CSET_DWL)

#define DBLCS_BITS            4
#define DBLCS_MASK            BITS2MASK(DBLCS_BITS)

#define GetLineDblCS(ld)      (((ld)->bufHead >> LINEFLAG_BITS) & DBLCS_MASK)
#define SetLineDblCS(ld,cs)   (ld)->bufHead = (RowData) ((ld->bufHead & LINEFLAG_MASK) | (cs << LINEFLAG_BITS))

#define LineCharSet(screen, ld) \
	((CSET_DOUBLE(GetLineDblCS(ld))) \
		? GetLineDblCS(ld) \
		: (screen)->cur_chrset)
#define LineMaxCol(screen, ld) \
	(CSET_DOUBLE(GetLineDblCS(ld)) \
	 ? (screen->max_col / 2) \
	 : (screen->max_col))
#define LineCursorX(screen, ld, col) \
	(CSET_DOUBLE(GetLineDblCS(ld)) \
	 ? CursorX(screen, 2*(col)) \
	 : CursorX(screen, (col)))
#define LineFontWidth(screen, ld) \
	(CSET_DOUBLE(GetLineDblCS(ld)) \
	 ? 2*FontWidth(screen) \
	 : FontWidth(screen))
#else

#define if_OPT_DEC_CHRSET(code) /*nothing*/

#define GetLineDblCS(ld)       0

#define LineCharSet(screen, ld)         0
#define LineMaxCol(screen, ld)          screen->max_col
#define LineCursorX(screen, ld, col)    CursorX(screen, col)
#define LineFontWidth(screen, ld)       FontWidth(screen)

#endif

#if OPT_LUIT_PROG && !OPT_WIDE_CHARS
#error Luit requires the wide-chars configuration
#endif

/***====================================================================***/

#if OPT_EBCDIC
extern int E2A(int);
extern int A2E(int);
#else
#define E2A(a) (a)
#define A2E(a) (a)
#endif

#define CONTROL(a) (A2E(E2A(a)&037))

/***====================================================================***/

#if OPT_TEK4014
#define TEK4014_ACTIVE(xw)      ((xw)->misc.TekEmu)
#define TEK4014_SHOWN(xw)       ((xw)->misc.Tshow)
#define CURRENT_EMU_VAL(tek,vt) (TEK4014_ACTIVE(term) ? tek : vt)
#define CURRENT_EMU()           CURRENT_EMU_VAL((Widget)tekWidget, (Widget)term)
#else
#define TEK4014_ACTIVE(screen)  0
#define TEK4014_SHOWN(xw)       0
#define CURRENT_EMU_VAL(tek,vt) (vt)
#define CURRENT_EMU()           ((Widget)term)
#endif

/***====================================================================***/

#if OPT_TOOLBAR
#define SHELL_OF(widget) XtParent(XtParent(widget))
#else
#define SHELL_OF(widget) XtParent(widget)
#endif

/***====================================================================***/

#if OPT_VT52_MODE
#define if_OPT_VT52_MODE(screen, code) if(screen->vtXX_level == 0) code
#else
#define if_OPT_VT52_MODE(screen, code) /* nothing */
#endif

/***====================================================================***/

#if OPT_XMC_GLITCH
#define if_OPT_XMC_GLITCH(screen, code) if(screen->xmc_glitch) code
#define XMC_GLITCH 1	/* the character we'll show */
#define XMC_FLAGS (INVERSE|UNDERLINE|BOLD|BLINK)
#else
#define if_OPT_XMC_GLITCH(screen, code) /* nothing */
#endif

/***====================================================================***/

#define LO_BYTE(ch) CharOf((ch) & 0xff)
#define HI_BYTE(ch) CharOf((ch) >> 8)

#if OPT_WIDE_CHARS
#define if_OPT_WIDE_CHARS(screen, code) if(screen->wide_chars) code
#define if_WIDE_OR_NARROW(screen, wide, narrow) if(screen->wide_chars) wide else narrow
#if OPT_WIDER_ICHAR
typedef unsigned IChar;		/* for 8-21 bit characters */
#else
typedef unsigned short IChar;	/* for 8-16 bit characters */
#endif
#else
#define if_OPT_WIDE_CHARS(screen, code) /* nothing */
#define if_WIDE_OR_NARROW(screen, wide, narrow) narrow
typedef unsigned char IChar;	/* for 8-bit characters */
#endif

/***====================================================================***/

#ifndef RES_OFFSET
#define RES_OFFSET(offset) XtOffsetOf(XtermWidgetRec, offset)
#endif

#define RES_NAME(name) name
#define RES_CLASS(name) name

#define Bres(name, class, offset, dftvalue) \
	{RES_NAME(name), RES_CLASS(class), XtRBoolean, sizeof(Boolean), \
	 RES_OFFSET(offset), XtRImmediate, (XtPointer) dftvalue}

#define Cres(name, class, offset, dftvalue) \
	{RES_NAME(name), RES_CLASS(class), XtRPixel, sizeof(Pixel), \
	 RES_OFFSET(offset), XtRString, (XtPointer) dftvalue}

#define Tres(name, class, offset, dftvalue) \
	COLOR_RES2(name, class, screen.Tcolors[offset], dftvalue) \

#define Fres(name, class, offset, dftvalue) \
	{RES_NAME(name), RES_CLASS(class), XtRFontStruct, sizeof(XFontStruct *), \
	 RES_OFFSET(offset), XtRString, (XtPointer) dftvalue}

#define Ires(name, class, offset, dftvalue) \
	{RES_NAME(name), RES_CLASS(class), XtRInt, sizeof(int), \
	 RES_OFFSET(offset), XtRImmediate, (XtPointer) dftvalue}

#define Dres(name, class, offset, dftvalue) \
	{RES_NAME(name), RES_CLASS(class), XtRFloat, sizeof(float), \
	 RES_OFFSET(offset), XtRString, (XtPointer) dftvalue}

#define Sres(name, class, offset, dftvalue) \
	{RES_NAME(name), RES_CLASS(class), XtRString, sizeof(char *), \
	 RES_OFFSET(offset), XtRString, (XtPointer) dftvalue}

#define Wres(name, class, offset, dftvalue) \
	{RES_NAME(name), RES_CLASS(class), XtRWidget, sizeof(Widget), \
	 RES_OFFSET(offset), XtRWidget, (XtPointer) dftvalue}

/***====================================================================***/

#define FRG_SIZE resource.minBufSize
#define BUF_SIZE resource.maxBufSize

typedef struct {
	Char *	next;
	Char *	last;
	int	update;		/* HandleInterpret */
#if OPT_WIDE_CHARS
	IChar	utf_data;	/* resulting character */
	int	utf_size;	/* ...number of bytes decoded */
	Char	*write_buf;
	unsigned write_len;
#endif
	Char	buffer[1];
} PtyData;

/***====================================================================***/

#if OPT_ISO_COLORS
#if OPT_256_COLORS || OPT_88_COLORS
#define COLOR_BITS 8
typedef unsigned short CellColor;
#else
#define COLOR_BITS 4
typedef Char CellColor;
#endif
#else
typedef int CellColor;
#endif

#define BITS2MASK(b)          ((1 << b) - 1)

#define COLOR_MASK            BITS2MASK(COLOR_BITS)

#define GetCellColorFG(src)   ((src) & COLOR_MASK)
#define GetCellColorBG(src)   (((src) >> COLOR_BITS) & COLOR_MASK)

typedef Char RowData;		/* wrap/blink, and DEC single-double chars */

#define LINEFLAG_BITS         4
#define LINEFLAG_MASK         BITS2MASK(LINEFLAG_BITS)

#define GetLineFlags(ld)      ((ld)->bufHead & LINEFLAG_MASK)

#if OPT_DEC_CHRSET
#define SetLineFlags(ld,xx)   (ld)->bufHead = (RowData) ((ld->bufHead & (DBLCS_MASK << LINEFLAG_BITS)) | (xx & LINEFLAG_MASK))
#else
#define SetLineFlags(ld,xx)   (ld)->bufHead = (RowData) (xx & LINEFLAG_MASK)
#endif

typedef IChar CharData;

/*
 * This is the xterm line-data/scrollback structure.
 */
typedef struct {
	Dimension lineSize;	/* number of columns in this row */
	RowData bufHead;	/* flag for wrapped lines */
#if OPT_WIDE_CHARS
	Char combSize;		/* number of items in combData[] */
#endif
	Char *attribs;		/* video attributes */
#if OPT_ISO_COLORS
	CellColor *color;	/* foreground+background color numbers */
#endif
	CharData *charData;	/* cell's base character */
	CharData *combData[1];	/* first enum past fixed-offsets */
} LineData;

/*
 * We use CellData in a few places, when copying a cell's data to a temporary
 * variable.
 */
typedef struct {
	Char attribs;
#if OPT_WIDE_CHARS
	Char combSize;		/* number of items in combData[] */
#endif
#if OPT_ISO_COLORS
	CellColor color;	/* color-array */
#endif
	CharData charData;	/* cell's base character */
	CharData combData[1];	/* array of combining chars */
} CellData;

#define for_each_combData(off, ld) for (off = 0; off < ld->combSize; ++off)

/*
 * Accommodate older compilers by not using variable-length arrays.
 */
#define SizeOfLineData  offsetof(LineData, combData)
#define SizeOfCellData  offsetof(CellData, combData)

	/*
	 * A "row" is the index within the visible part of the screen, and an
	 * "inx" is the index within the whole set of scrollable lines.
	 */
#define ROW2INX(screen, row)	((row) + (screen)->topline)
#define INX2ROW(screen, inx)	((inx) - (screen)->topline)

#define ROW2ABS(screen, row)	((row) + (screen)->savedlines)
#define INX2ABS(screen, inx)	ROW2ABS(screen, INX2ROW(screen, inx))

#define okScrnRow(screen, row) \
	((row) <= ((screen)->max_row - (screen)->topline) \
      && (row) >= -((screen)->savedlines))

	/*
	 * Cache data for "proportional" and other fonts containing a mixture
	 * of widths.
	 */
typedef struct {
    	Bool		mixed;
	Dimension	min_width;	/* nominal cell width for 0..255 */
	Dimension	max_width;	/* maximum cell width */
} FontMap;

typedef struct {
	unsigned	chrset;
	unsigned	flags;
	XFontStruct *	fs;
	char *		fn;
	FontMap		map;
	Char		known_missing[256];
} XTermFonts;

#if OPT_RENDERFONT
typedef struct {
	XftFont *	font;
	FontMap		map;
} XTermXftFonts;
#endif

typedef struct {
	int		top;
	int		left;
	int		bottom;
	int		right;
} XTermRect;

	/* indices into save_modes[] */
typedef enum {
	DP_CRS_VISIBLE,
	DP_DECANM,
	DP_DECARM,
	DP_DECAWM,
	DP_DECBKM,
	DP_DECCKM,
	DP_DECCOLM,	/* IN132COLUMNS */
	DP_DECOM,
	DP_DECPEX,
	DP_DECPFF,
	DP_DECSCLM,
	DP_DECSCNM,
	DP_DECTCEM,
	DP_DECTEK,
	DP_PRN_EXTENT,
	DP_PRN_FORMFEED,
	DP_X_ALTSCRN,
	DP_X_DECCOLM,
	DP_X_LOGGING,
	DP_X_MARGIN,
	DP_X_MORE,
	DP_X_MOUSE,
	DP_X_REVWRAP,
	DP_X_X10MSE,
#if OPT_BLINK_CURS
	DP_CRS_BLINK,
#endif
#if OPT_FOCUS_EVENT
	DP_X_FOCUS,
#endif
#if OPT_TOOLBAR
	DP_TOOLBAR,
#endif
	DP_LAST
} SaveModes;

#define DoSM(code,value)  screen->save_modes[code] = (unsigned) (value)
#define DoRM(code,value)  value = (Boolean) screen->save_modes[code]
#define DoRM0(code,value) value = screen->save_modes[code]

	/* index into vt_shell[] or tek_shell[] */
typedef enum {
	noMenu = -1,
	mainMenu,
	vtMenu,
	fontMenu,
#if OPT_TEK4014
	tekMenu
#endif
} MenuIndex;

#define NUM_POPUP_MENUS 4

#if OPT_COLOR_RES
typedef struct {
	String		resource;
	Pixel		value;
	int		mode;
} ColorRes;
#else
#define ColorRes Pixel
#endif

typedef struct {
	unsigned	which;		/* must have NCOLORS bits */
	Pixel		colors[NCOLORS];
	char		*names[NCOLORS];
} ScrnColors;

typedef struct {
	Boolean		saved;
	int		row;
	int		col;
	unsigned	flags;		/* VTxxx saves graphics rendition */
	Char		curgl;
	Char		curgr;
	Char		gsets[4];
#if OPT_ISO_COLORS
	int		cur_foreground; /* current foreground color	*/
	int		cur_background; /* current background color	*/
	int		sgr_foreground; /* current SGR foreground color */
	int		sgr_background; /* current SGR background color */
	Boolean		sgr_extended;	/* SGR set with extended codes? */
#endif
} SavedCursor;

typedef struct _SaveTitle {
    	struct _SaveTitle *next;
	char		*iconName;
	char		*windowName;
} SaveTitle;

#define SAVED_CURSORS 2

typedef struct {
	int		width;		/* if > 0, width of scrollbar,	*/
					/* and scrollbar is showing	*/
	Boolean		rv_cached;	/* see ScrollBarReverseVideo	*/
	int		rv_active;	/* ...current reverse-video	*/
	Pixel		bg;		/* ...cached background color	*/
	Pixel		fg;		/* ...cached foreground color	*/
	Pixel		bdr;		/* ...cached border color	*/
	Pixmap		bdpix;		/* ...cached border pixmap	*/
} SbInfo;

#if OPT_TOOLBAR
typedef struct {
	Widget		menu_bar;	/* toolbar, if initialized	*/
	Dimension	menu_height;	/* ...and its height		*/
	Dimension	menu_border;	/* ...and its border		*/
} TbInfo;
#define VT100_TB_INFO(name) screen.fullVwin.tb_info.name
#endif

typedef struct {
	Window		window;		/* X window id			*/
	int		width;		/* width of columns		*/
	int		height;		/* height of rows		*/
	Dimension	fullwidth;	/* full width of window		*/
	Dimension	fullheight;	/* full height of window	*/
	int		f_width;	/* width of fonts in pixels	*/
	int		f_height;	/* height of fonts in pixels	*/
	int		f_ascent;	/* ascent of font in pixels	*/
	int		f_descent;	/* descent of font in pixels	*/
	SbInfo		sb_info;
#if OPT_TOOLBAR
	Boolean		active;		/* true if toolbars are used	*/
	TbInfo		tb_info;	/* toolbar information		*/
#endif
} VTwin;

typedef struct {
	Window		window;		/* X window id			*/
	int		width;		/* width of columns		*/
	int		height;		/* height of rows		*/
	Dimension	fullwidth;	/* full width of window		*/
	Dimension	fullheight;	/* full height of window	*/
	double		tekscale;	/* scale factor Tek -> vs100	*/
} TKwin;

typedef struct {
/* These parameters apply to both windows */
	Display		*display;	/* X display for screen		*/
	int		respond;	/* socket for responses
					   (position report, etc.)	*/
/* These parameters apply to VT100 window */
	IChar		unparse_bfr[256];
	unsigned	unparse_len;

#if OPT_TCAP_QUERY
	int		tc_query_code;
	Bool		tc_query_fkey;
#endif
	pid_t		pid;		/* pid of process on far side   */
	uid_t		uid;		/* user id of actual person	*/
	gid_t		gid;		/* group id of actual person	*/
	ColorRes	Tcolors[NCOLORS]; /* terminal colors		*/
#if OPT_HIGHLIGHT_COLOR
	Boolean		hilite_color;	/* hilite colors override	*/
	Boolean		hilite_reverse;	/* hilite overrides reverse	*/
#endif
#if OPT_ISO_COLORS
	ColorRes	Acolors[MAXCOLORS]; /* ANSI color emulation	*/
	int		veryBoldColors;	/* modifier for boldColors	*/
	Boolean		boldColors;	/* can we make bold colors?	*/
	Boolean		colorMode;	/* are we using color mode?	*/
	Boolean		colorULMode;	/* use color for underline?	*/
	Boolean		italicULMode;	/* italic font for underline?	*/
	Boolean		colorBDMode;	/* use color for bold?		*/
	Boolean		colorBLMode;	/* use color for blink?		*/
	Boolean		colorRVMode;	/* use color for reverse?	*/
	Boolean		colorAttrMode;	/* prefer colorUL/BD to SGR	*/
#endif
#if OPT_DEC_CHRSET
	Boolean		font_doublesize;/* enable font-scaling		*/
	int		cache_doublesize;/* limit of our cache		*/
	Char		cur_chrset;	/* character-set index & code	*/
	int		fonts_used;	/* count items in double_fonts	*/
	XTermFonts	double_fonts[NUM_CHRSET];
#endif
#if OPT_DEC_RECTOPS
	int		cur_decsace;	/* parameter for DECSACE	*/
#endif
#if OPT_WIDE_CHARS
	Boolean		wide_chars;	/* true when 16-bit chars	*/
	Boolean		vt100_graphics;	/* true to allow vt100-graphics	*/
	Boolean		utf8_inparse;	/* true to enable UTF-8 parser	*/
	int		utf8_mode;	/* use UTF-8 decode/encode: 0-2	*/
	int		max_combining;	/* maximum # of combining chars	*/
	Boolean		utf8_latin1;	/* use UTF-8 with Latin-1 bias	*/
	Boolean		utf8_title;	/* use UTF-8 titles		*/
	int		latin9_mode;	/* poor man's luit, latin9	*/
	int		unicode_font;	/* font uses unicode encoding	*/
	int		utf_count;	/* state of utf_char		*/
	IChar		utf_char;	/* in-progress character	*/
	int		last_written_col;
	int		last_written_row;
	TypedBuffer(XChar2b);
	TypedBuffer(char);
#endif
#if OPT_BROKEN_OSC
	Boolean		brokenLinuxOSC; /* true to ignore Linux palette ctls */
#endif
#if OPT_BROKEN_ST
	Boolean		brokenStringTerm; /* true to match old OSC parse */
#endif
#if OPT_C1_PRINT || OPT_WIDE_CHARS
	Boolean		c1_printable;	/* true if we treat C1 as print	*/
#endif
	int		border;		/* inner border			*/
	int		scrollBarBorder; /* scrollBar border		*/
	long		event_mask;
	unsigned	send_mouse_pos;	/* user wants mouse transition  */
					/* and position information	*/
	Boolean		send_focus_pos; /* user wants focus in/out info */
	Boolean		quiet_grab;	/* true if no cursor change on focus */
#if OPT_PASTE64
	Cardinal	base64_paste;	/* set to send paste in base64	*/
	int		base64_final;	/* string-terminator for paste	*/
	/* _qWriteSelectionData expects these to be initialized to zero.
	 * base64_flush() is the last step of the conversion, it clears these
	 * variables.
	 */
	unsigned	base64_accu;
	unsigned	base64_count;
	unsigned	base64_pad;
#endif
#if OPT_READLINE
	unsigned	click1_moves;
	unsigned	paste_moves;
	unsigned	dclick3_deletes;
	unsigned	paste_brackets;
	unsigned	paste_quotes;
	unsigned	paste_literal_nl;
#endif	/* OPT_READLINE */
#if OPT_DEC_LOCATOR
	Boolean		locator_reset;	/* turn mouse off after 1 report? */
	Boolean		locator_pixels;	/* report in pixels?		*/
					/* if false, report in cells	*/
	unsigned	locator_events;	/* what events to report	*/
	Boolean		loc_filter;	/* is filter rectangle active?	*/
	int		loc_filter_top;	/* filter rectangle for DEC Locator */
	int		loc_filter_left;
	int		loc_filter_bottom;
	int		loc_filter_right;
#endif	/* OPT_DEC_LOCATOR */
	int		mouse_button;	/* current button pressed	*/
	int		mouse_row;	/* ...and its row		*/
	int		mouse_col;	/* ...and its column		*/
	int		select;		/* xterm selected		*/
	Boolean		bellOnReset;	/* bellOnReset			*/
	Boolean		visualbell;	/* visual bell mode		*/
	Boolean		poponbell;	/* pop on bell mode		*/

	Boolean		allowFontOps;	/* FontOps mode			*/
	Boolean		allowSendEvents;/* SendEvent mode		*/
	Boolean		allowTcapOps;	/* TcapOps mode			*/
	Boolean		allowTitleOps;	/* TitleOps mode		*/
	Boolean		allowWindowOps;	/* WindowOps mode		*/

	Boolean		allowFontOp0;	/* initial FontOps mode		*/
	Boolean		allowSendEvent0;/* initial SendEvent mode	*/
	Boolean		allowTcapOp0;	/* initial TcapOps mode		*/
	Boolean		allowTitleOp0;	/* initial TitleOps mode	*/
	Boolean		allowWindowOp0;	/* initial WindowOps mode	*/

	String		disallowedWinOps;
	char		disallow_win_ops[ewLAST];

	Boolean		awaitInput;	/* select-timeout mode		*/
	Boolean		grabbedKbd;	/* keyboard is grabbed		*/
#ifdef ALLOWLOGGING
	int		logging;	/* logging mode			*/
	int		logfd;		/* file descriptor of log	*/
	char		*logfile;	/* log file name		*/
	Char		*logstart;	/* current start of log buffer	*/
#endif
	int		inhibit;	/* flags for inhibiting changes	*/

/* VT window parameters */
	Boolean		Vshow;		/* VT window showing		*/
	VTwin		fullVwin;
#ifndef NO_ACTIVE_ICON
	VTwin		iconVwin;
	VTwin		*whichVwin;
#endif /* NO_ACTIVE_ICON */

	int		pointer_mode;	/* when to use hidden_cursor	*/
	Boolean 	hide_pointer;	/* true to use "hidden_cursor"  */
	Cursor		pointer_cursor;	/* pointer cursor in window	*/
	Cursor		hidden_cursor;	/* hidden cursor in window	*/

	String	answer_back;		/* response to ENQ		*/
	String	printer_command;	/* pipe/shell command string	*/
	Boolean printer_autoclose;	/* close printer when offline	*/
	Boolean printer_extent;		/* print complete page		*/
	Boolean printer_formfeed;	/* print formfeed per function	*/
	int	printer_controlmode;	/* 0=off, 1=auto, 2=controller	*/
	int	print_attributes;	/* 0=off, 1=normal, 2=color	*/

	Boolean		fnt_prop;	/* true if proportional fonts	*/
	Boolean		fnt_boxes;	/* true if font has box-chars	*/
	Boolean		force_packed;	/* true to override proportional */
#if OPT_BOX_CHARS
	Boolean		force_box_chars;/* true if we assume that	*/
	Boolean		force_all_chars;/* true to outline missing chars */
	Boolean		allow_packing;	/* true to allow packed-fonts	*/
#endif
	Dimension	fnt_wide;
	Dimension	fnt_high;
	XTermFonts	fnts[fMAX];	/* normal/bold/etc for terminal	*/
	Boolean		free_bold_box;	/* same_font_size's austerity	*/
#ifndef NO_ACTIVE_ICON
	XTermFonts	fnt_icon;	/* icon font */
#endif /* NO_ACTIVE_ICON */
	int		enbolden;	/* overstrike for bold font	*/
	XPoint		*box;		/* draw unselected cursor	*/

	int		cursor_state;	/* ON, OFF, or BLINKED_OFF	*/
	int		cursor_busy;	/* do not redraw...		*/
	Boolean		cursor_underline; /* true if cursor is in underline mode */
#if OPT_BLINK_CURS
	Boolean		cursor_blink;	/* cursor blink enable		*/
	Boolean		cursor_blink_res; /* initial cursor blink value	*/
	Boolean		cursor_blink_esc; /* cursor blink escape-state	*/
#endif
#if OPT_BLINK_TEXT
	Boolean		blink_as_bold;	/* text blink disable		*/
#endif
#if OPT_BLINK_CURS || OPT_BLINK_TEXT
	int		blink_state;	/* ON, OFF, or BLINKED_OFF	*/
	int		blink_on;	/* cursor on time (msecs)	*/
	int		blink_off;	/* cursor off time (msecs)	*/
	XtIntervalId	blink_timer;	/* timer-id for cursor-proc	*/
#endif
#if OPT_ZICONBEEP
	Boolean		zIconBeep_flagged; /* True if icon name was changed */
#endif /* OPT_ZICONBEEP */
	int		cursor_GC;	/* see ShowCursor()		*/
	int		cursor_set;	/* requested state		*/
	CELL		cursorp;	/* previous cursor row/column	*/
	int		cur_col;	/* current cursor column	*/
	int		cur_row;	/* current cursor row		*/
	int		max_col;	/* rightmost column		*/
	int		max_row;	/* bottom row			*/
	int		top_marg;	/* top line of scrolling region */
	int		bot_marg;	/* bottom line of  "	    "	*/
	Widget		scrollWidget;	/* pointer to scrollbar struct	*/
	/*
	 * Indices used to keep track of the top of the vt100 window and
	 * the saved lines, taking scrolling into account.
	 */
	int		topline;	/* line number of top, <= 0	*/
	long		saved_fifo;     /* number of lines that've been saved */
	int		savedlines;     /* number of lines that've been saved */
	int		savelines;	/* number of lines off top to save */
	int		scroll_amt;	/* amount to scroll		*/
	int		refresh_amt;	/* amount to refresh		*/
	/*
	 * Working variables for getLineData().
	 */
	size_t		lineExtra;	/* extra space for combining chars */
	/*
	 * Pointer to the current visible buffer.
	 */
	ScrnBuf		visbuf;		/* ptr to visible screen buf (main) */
	/*
	 * Data for the normal buffer, which may have saved lines to which
	 * the user can scroll.
	 */
	ScrnBuf		saveBuf_index;
	Char		*saveBuf_data;
	/*
	 * Data for visible and alternate buffer.
	 */
	ScrnBuf		editBuf_index[2];
	Char		*editBuf_data[2];
	int		whichBuf;	/* 0/1 for normal/alternate buf */
	Boolean		is_running;	/* true when buffers are legal	*/
	/*
	 * Workspace used for screen operations.
	 */
	Char		**save_ptr;	/* workspace for save-pointers  */
	size_t		save_len;	/* ...and its length		*/

	int		scrolllines;	/* number of lines to button scroll */
	Boolean		scrollttyoutput; /* scroll to bottom on tty output */
	Boolean		scrollkey;	/* scroll to bottom on key	*/
	Boolean		cursor_moved;	/* scrolling makes cursor move	*/

	Boolean		do_wrap;	/* true if cursor in last column
					    and character just output    */

	int		incopy;		/* 0 idle; 1 XCopyArea issued;
					    -1 first GraphicsExpose seen,
					    but last not seen		*/
	int		copy_src_x;	/* params from last XCopyArea ... */
	int		copy_src_y;
	unsigned int	copy_width;
	unsigned int	copy_height;
	int		copy_dest_x;
	int		copy_dest_y;

	Boolean		c132;		/* allow change to 132 columns	*/
	Boolean		curses;		/* kludge line wrap for more	*/
	Boolean		hp_ll_bc;	/* kludge HP-style ll for xdb	*/
	Boolean		marginbell;	/* true if margin bell on	*/
	int		nmarginbell;	/* columns from right margin	*/
	int		bellarmed;	/* cursor below bell margin	*/
	Boolean		multiscroll;	/* true if multi-scroll		*/
	int		scrolls;	/* outstanding scroll count,
					    used only with multiscroll	*/
	SavedCursor	sc[SAVED_CURSORS]; /* data for restore cursor	*/
	unsigned 	save_modes[DP_LAST]; /* save dec/xterm private modes */

	SaveTitle	*save_title;

	/* Improved VT100 emulation stuff.				*/
	String		keyboard_dialect; /* default keyboard dialect	*/
	Char		gsets[4];	/* G0 through G3.		*/
	Char		curgl;		/* Current GL setting.		*/
	Char		curgr;		/* Current GR setting.		*/
	Char		curss;		/* Current single shift.	*/
	String		term_id;	/* resource for terminal_id	*/
	int		terminal_id;	/* 100=vt100, 220=vt220, etc.	*/
	int		vtXX_level;	/* 0=vt52, 1,2,3 = vt100 ... vt320 */
	int		ansi_level;	/* levels 1,2,3			*/
	int		protected_mode;	/* 0=off, 1=DEC, 2=ISO		*/
	Boolean		always_bold_mode; /* compare normal/bold font	*/
	Boolean		always_highlight; /* whether to highlight cursor */
	Boolean		bold_mode;	/* use bold font or overstrike	*/
	Boolean		delete_is_del;	/* true for compatible Delete key */
	Boolean		jumpscroll;	/* whether we should jumpscroll */
	Boolean		fastscroll;	/* whether we should fastscroll */
	Boolean		old_fkeys;	/* true for compatible fkeys	*/
	Boolean		underline;	/* whether to underline text	*/

#if OPT_MAXIMIZE
	Boolean		restore_data;
	int		restore_x;
	int		restore_y;
	unsigned	restore_width;
	unsigned	restore_height;
#endif

#if OPT_VT52_MODE
	int		vt52_save_level; /* save-area for DECANM	*/
	Char		vt52_save_curgl;
	Char		vt52_save_curgr;
	Char		vt52_save_curss;
	Char		vt52_save_gsets[4];
#endif
	/* Testing */
#if OPT_XMC_GLITCH
	unsigned	xmc_glitch;	/* # of spaces to pad on SGR's	*/
	int		xmc_attributes;	/* attrs that make a glitch	*/
	Boolean		xmc_inline;	/* SGR's propagate only to eol	*/
	Boolean		move_sgr_ok;	/* SGR is reset on move		*/
#endif

	/*
	 * Bell
	 */
	int		visualBellDelay; /* msecs to delay for visibleBell */
	int		bellSuppressTime; /* msecs after Bell before another allowed */
	Boolean		bellInProgress; /* still ringing/flashing prev bell? */
	Boolean		bellIsUrgent;	/* set XUrgency WM hint on bell */
	/*
	 * Select/paste state.
	 */
	Boolean		selectToClipboard; /* primary vs clipboard */
	String		*mappedSelect;	/* mapping for "SELECT" to "PRIMARY" */

	Boolean		waitingForTrackInfo;
	int		numberOfClicks;
	int		maxClicks;
	int		multiClickTime;	/* time between multiclick selects */
	SelectUnit	selectUnit;
	SelectUnit	selectMap[NSELECTUNITS];
	String		onClick[NSELECTUNITS + 1];

	char		*charClass;	/* for overriding word selection */
	Boolean		cutNewline;	/* whether or not line cut has \n */
	Boolean		cutToBeginningOfLine;  /* line cuts to BOL? */
	Boolean		highlight_selection; /* controls appearance of selection */
	Boolean		trim_selection; /* controls trimming of selection */
	Boolean		i18nSelections;
	Boolean		brokenSelections;
	Boolean		keepSelection;	/* do not lose selection on output */
	Boolean		replyToEmacs;	/* Send emacs escape code when done selecting or extending? */
	Char		*selection_data; /* the current selection */
	int		selection_size; /* size of allocated buffer */
	unsigned long	selection_length; /* number of significant bytes */
	EventMode	eventMode;
	Time		selection_time;	/* latest event timestamp */
	Time		lastButtonUpTime;
	unsigned	lastButton;

	CELL		rawPos;		/* raw position for selection start */
	CELL		startRaw;	/* area before selectUnit processing */
	CELL		endRaw;		/* " " */
	CELL		startSel;	/* area after selectUnit processing */
	CELL		endSel;		/* " " */
	CELL		startH;		/* start highlighted text */
	CELL		endH;		/* end highlighted text */
	CELL		saveStartW;	/* saved WORD state, for LINE */
	CELL		startExt;	/* Start, end of extension */
	CELL		endExt;		/* " " */
	CELL		saveStartR;	/* Saved values of raw selection for extend to restore to */
	CELL		saveEndR;	/* " " */
	int		startHCoord, endHCoord;
	int		firstValidRow;	/* Valid rows for selection clipping */
	int		lastValidRow;	/* " " */

	String		default_string;
	String		eightbit_select_types;
	Atom*		selection_targets_8bit;
#if OPT_WIDE_CHARS
	String		utf8_select_types;
	Atom*		selection_targets_utf8;
#endif
	Atom*		selection_atoms; /* which selections we own */
	Cardinal	sel_atoms_size;	/*  how many atoms allocated */
	Cardinal	selection_count; /* how many atoms in use */
#if OPT_SELECT_REGEX
	char *		selectExpr[NSELECTUNITS];
#endif
	/*
	 * Input/output state.
	 */
	Boolean		input_eight_bits;/* use 8th bit instead of ESC prefix */
	Boolean		output_eight_bits; /* honor all bits or strip */
	Boolean		control_eight_bits; /* send CSI as 8-bits */
	Boolean		backarrow_key;		/* backspace/delete */
	Boolean		alt_is_not_meta;	/* use both Alt- and Meta-key */
	Boolean		alt_sends_esc;		/* Alt-key sends ESC prefix */
	Boolean		meta_sends_esc;		/* Meta-key sends ESC prefix */
	/*
	 * Fonts
	 */
	Pixmap		menu_item_bitmap;	/* mask for checking items */
	String		initial_font;
	String		menu_font_names[NMENUFONTS][fMAX];
#define MenuFontName(n) menu_font_names[n][fNorm]
	long		menu_font_sizes[NMENUFONTS];
	int		menu_font_number;
#if OPT_CLIP_BOLD
	Boolean		use_clipping;
#endif
	void *		main_cgs_cache;
#ifndef NO_ACTIVE_ICON
	void *		icon_cgs_cache;
#endif
#if OPT_RENDERFONT
	XTermXftFonts	renderFontNorm[NMENUFONTS];
	XTermXftFonts	renderFontBold[NMENUFONTS];
	XTermXftFonts	renderFontItal[NMENUFONTS];
#if OPT_RENDERWIDE
	XTermXftFonts	renderWideNorm[NMENUFONTS];
	XTermXftFonts	renderWideBold[NMENUFONTS];
	XTermXftFonts	renderWideItal[NMENUFONTS];
	TypedBuffer(XftCharSpec);
#else
	TypedBuffer(XftChar8);
#endif
	XftDraw *	renderDraw;
#endif
#if OPT_INPUT_METHOD
	XIM		xim;
	XFontSet	fs;		/* fontset for XIM preedit */
	int		fs_ascent;	/* ascent of fs */
#endif
	XIC		xic;		/* this is used even without XIM */
#if OPT_DABBREV
	Boolean		dabbrev_working;	/* nonzero during dabbrev process */
	unsigned char	dabbrev_erase_char;	/* used for deleting inserted completion */
#endif
	char		tcapbuf[TERMCAP_SIZE];
#if OPT_TCAP_FKEYS
	char **		tcap_fkeys;
	char		tcap_area[TERMCAP_SIZE];
#endif
} TScreen;

typedef struct _TekPart {
	XFontStruct *	Tfont[TEKNUMFONTS];
	int		tobaseline[TEKNUMFONTS]; /* top-baseline, each font */
	char *		initial_font;		/* large, 2, 3, small */
	char *		gin_terminator_str;	/* ginTerminator resource */
#if OPT_TOOLBAR
	TbInfo		tb_info;	/* toolbar information		*/
#endif
} TekPart;

/* Tektronix window parameters */
typedef struct _TekScreen {
	GC		TnormalGC;	/* normal painting		*/
	GC		TcursorGC;	/* normal cursor painting	*/

	Boolean		waitrefresh;	/* postpone refresh		*/
	TKwin		fullTwin;
#ifndef NO_ACTIVE_ICON
	TKwin		iconTwin;
	TKwin		*whichTwin;
#endif /* NO_ACTIVE_ICON */

	Cursor		arrow;		/* arrow cursor			*/
	GC		linepat[TEKNUMLINES]; /* line patterns		*/
	int		cur_X;		/* current x			*/
	int		cur_Y;		/* current y			*/
	Tmodes		cur;		/* current tek modes		*/
	Tmodes		page;		/* starting tek modes on page	*/
	int		margin;		/* 0 -> margin 1, 1 -> margin 2	*/
	int		pen;		/* current Tektronix pen 0=up, 1=dn */
	char		*TekGIN;	/* nonzero if Tektronix GIN mode*/
	int		gin_terminator; /* Tek strap option */
	char		tcapbuf[TERMCAP_SIZE];
} TekScreen;

#if OPT_READLINE
#define SCREEN_FLAG(screenp,f)		(1&(screenp)->f)
#define SCREEN_FLAG_set(screenp,f)	((screenp)->f |= 1)
#define SCREEN_FLAG_unset(screenp,f)	((screenp)->f &= ~1L)
#define SCREEN_FLAG_save(screenp,f)	\
	((screenp)->f = (((screenp)->f)<<1) | SCREEN_FLAG(screenp,f))
#define SCREEN_FLAG_restore(screenp,f)	((screenp)->f = (((screenp)->f)>>1))
#else
#define SCREEN_FLAG(screenp,f)		(0)
#endif

/* meaning of bits in screen.select flag */
#define	INWINDOW	01	/* the mouse is in one of the windows */
#define	FOCUS		02	/* one of the windows is the focus window */

#define MULTICLICKTIME 250	/* milliseconds */

typedef enum {
    fwNever = 0,
    fwResource,
    fwAlways
} fontWarningTypes;

typedef enum {
    keyboardIsLegacy,		/* bogus vt220 codes for F1-F4, etc. */
    keyboardIsDefault,
    keyboardIsHP,
    keyboardIsSCO,
    keyboardIsSun,
    keyboardIsTermcap,
    keyboardIsVT220
} xtermKeyboardType;

typedef enum {			/* legal values for screen.pointer_mode */
    pNever = 0,
    pNoMouse = 1,
    pAlways = 2
} pointerModeTypes;

typedef enum {			/* legal values for screen.utf8_mode */
    uFalse = 0,
    uTrue = 1,
    uAlways = 2,
    uDefault = 3
} utf8ModeTypes;

#if OPT_HP_FUNC_KEYS
#define NAME_HP_KT " hp"
#else
#define NAME_HP_KT /*nothing*/
#endif

#if OPT_SCO_FUNC_KEYS
#define NAME_SCO_KT " sco"
#else
#define NAME_SCO_KT /*nothing*/
#endif

#if OPT_SUN_FUNC_KEYS
#define NAME_SUN_KT " sun"
#else
#define NAME_SUN_KT /*nothing*/
#endif

#if OPT_SUNPC_KBD
#define NAME_VT220_KT " vt220"
#else
#define NAME_VT220_KT /*nothing*/
#endif

#if OPT_TCAP_FKEYS
#define NAME_TCAP_KT " tcap"
#else
#define NAME_TCAP_KT /*nothing*/
#endif

#define KEYBOARD_TYPES NAME_TCAP_KT NAME_HP_KT NAME_SCO_KT NAME_SUN_KT NAME_VT220_KT

#if OPT_TRACE
extern	const char * visibleKeyboardType(xtermKeyboardType);
#endif

typedef struct
{
    int cursor_keys;		/* how to handle cursor-key modifiers */
    int function_keys;		/* how to handle function-key modifiers */
    int keypad_keys;		/* how to handle keypad key-modifiers */
    int other_keys;		/* how to handle other key-modifiers */
    int string_keys;		/* how to handle string() modifiers */
} TModify;

typedef struct
{
    xtermKeyboardType type;
    unsigned flags;
    char *shell_translations;
    char *xterm_translations;
    char *extra_translations;
#if OPT_INITIAL_ERASE
    int	reset_DECBKM;		/* reset should set DECBKM */
#endif
#if OPT_MOD_FKEYS
    TModify modify_now;		/* current modifier value */
    TModify modify_1st;		/* original modifier value, for resets */
    int format_keys;		/* format of modifyOtherKeys */
#endif
} TKeyboard;

typedef struct {
    char *f_n;			/* the normal font */
    char *f_b;			/* the bold font */
#if OPT_WIDE_CHARS
    char *f_w;			/* the normal wide font */
    char *f_wb;			/* the bold wide font */
#endif
} VTFontNames;

#define GravityIsNorthWest(w) ((w)->misc.resizeGravity == NorthWestGravity)
#define GravityIsSouthWest(w) ((w)->misc.resizeGravity == SouthWestGravity)

typedef struct _Misc {
    VTFontNames default_font;
    char *geo_metry;
    char *T_geometry;
#if OPT_WIDE_CHARS
    Boolean cjk_width;		/* true for built-in CJK wcwidth() */
    Boolean mk_width;		/* true for simpler built-in wcwidth() */
    int mk_samplesize;
    int mk_samplepass;
#endif
#if OPT_LUIT_PROG
    Boolean callfilter;		/* true to invoke luit */
    Boolean use_encoding;	/* true to use -encoding option for luit */
    char *locale_str;		/* "locale" resource */
    char *localefilter;		/* path for luit */
#endif
    fontWarningTypes fontWarnings;
    int limit_resize;
#ifdef ALLOWLOGGING
    Boolean log_on;
#endif
    Boolean login_shell;
    Boolean re_verse;
    Boolean re_verse0;		/* initial value of "-rv" */
    XtGravity resizeGravity;
    Boolean reverseWrap;
    Boolean autoWrap;
    Boolean logInhibit;
    Boolean signalInhibit;
#if OPT_TEK4014
    Boolean tekInhibit;
    Boolean tekSmall;		/* start tek window in small size */
    Boolean TekEmu;		/* true if Tektronix emulation	*/
    Boolean Tshow;		/* Tek window showing		*/
#endif
    Boolean scrollbar;
#ifdef SCROLLBAR_RIGHT
    Boolean useRight;
#endif
    Boolean titeInhibit;
    Boolean tiXtraScroll;
    Boolean appcursorDefault;
    Boolean appkeypadDefault;
#if OPT_INPUT_METHOD
    char* f_x;			/* font for XIM */
    char* input_method;
    char* preedit_type;
    Boolean open_im;		/* true if input-method is opened */
    Boolean cannot_im;		/* true if we cannot use input-method */
    int retry_im;
#endif
    Boolean dynamicColors;
    Boolean shared_ic;
#ifndef NO_ACTIVE_ICON
    Boolean active_icon;	/* use application icon window  */
    unsigned icon_border_width;
    Pixel icon_border_pixel;
#endif /* NO_ACTIVE_ICON */
#if OPT_DEC_SOFTFONT
    Boolean font_loadable;
#endif
#if OPT_SHIFT_FONTS
    Boolean shift_fonts;	/* true if we interpret fontsize-shifting */
#endif
#if OPT_SUNPC_KBD
    int ctrl_fkeys;		/* amount to add to XK_F1 for ctrl modifier */
#endif
#if OPT_NUM_LOCK
    Boolean real_NumLock;	/* true if we treat NumLock key specially */
    Boolean alwaysUseMods;	/* true if we always want f-key modifiers */
    unsigned long num_lock;	/* modifier for Num_Lock */
    unsigned long alt_mods;	/* modifier for Alt_L or Alt_R */
    unsigned long meta_mods;	/* modifier for Meta_L or Meta_R */
    unsigned long other_mods;	/* conflicting modifiers, e.g., Mode_Switch */
#endif
#if OPT_RENDERFONT
    char *face_name;
    char *face_wide_name;
    float face_size[NMENUFONTS];
    Boolean render_font;
#endif
} Misc;

typedef struct {int foo;} XtermClassPart, TekClassPart;

typedef struct _XtermClassRec {
    CoreClassPart  core_class;
    XtermClassPart xterm_class;
} XtermClassRec;

extern WidgetClass xtermWidgetClass;

#define IsXtermWidget(w) (XtClass(w) == xtermWidgetClass)

#if OPT_TEK4014
typedef struct _TekClassRec {
    CoreClassPart core_class;
    TekClassPart tek_class;
} TekClassRec;

extern WidgetClass tekWidgetClass;

#define IsTekWidget(w) (XtClass(w) == tekWidgetClass)

#endif

/* define masks for keyboard.flags */
#define MODE_KAM	xBIT(0)	/* keyboard action mode */
#define MODE_DECKPAM	xBIT(1)	/* keypad application mode */
#define MODE_DECCKM	xBIT(2)	/* cursor keys */
#define MODE_SRM	xBIT(3)	/* send-receive mode */
#define MODE_DECBKM	xBIT(4)	/* backarrow */


#define N_MARGINBELL	10

#define TAB_BITS_SHIFT	5	/* FIXME: 2**5 == 32 (should derive) */
#define TAB_BITS_WIDTH	(1 << TAB_BITS_SHIFT)
#define TAB_ARRAY_SIZE	(1024 / TAB_BITS_WIDTH)
#define MAX_TABS	(TAB_BITS_WIDTH * TAB_ARRAY_SIZE)

typedef unsigned Tabs [TAB_ARRAY_SIZE];

typedef struct _XtermWidgetRec {
    CorePart	core;
    XSizeHints	hints;
    Bool	init_menu;
    TKeyboard	keyboard;	/* terminal keyboard		*/
    TScreen	screen;		/* terminal screen		*/
    unsigned	flags;		/* mode flags			*/
    int		cur_foreground; /* current foreground color	*/
    int		cur_background; /* current background color	*/
    Pixel	dft_foreground; /* default foreground color	*/
    Pixel	dft_background; /* default background color	*/
#if OPT_ISO_COLORS
    int		sgr_foreground; /* current SGR foreground color */
    int		sgr_background; /* current SGR background color */
    Boolean	sgr_extended;	/* SGR set with extended codes? */
#endif
    unsigned	initflags;	/* initial mode flags		*/
    Tabs	tabs;		/* tabstops of the terminal	*/
    Misc	misc;		/* miscellaneous parameters	*/
} XtermWidgetRec, *XtermWidget;

#if OPT_TEK4014
typedef struct _TekWidgetRec {
    CorePart	core;
    TekPart	tek;		/* contains resources */
    TekScreen	screen;		/* contains working data (no resources) */
    Bool	init_menu;
    XSizeHints	hints;
} TekWidgetRec, *TekWidget;
#endif /* OPT_TEK4014 */

/*
 * terminal flags
 * There are actually two namespaces mixed together here.
 * One is the set of flags that can go in screen->visbuf attributes
 * and which must fit in a char (see OFF_ATTRS).
 * The other is the global setting stored in
 * term->flags and screen->save_modes.  This need only fit in an unsigned.
 */

#define AttrBIT(n)	xBIT(n)		/* text-attributes */
#define DrawBIT(n)	xBIT(n + 8)	/* drawXtermText flags */
#define MiscBIT(n)	xBIT(n + 16)	/* miscellaneous state flags */

/* global flags and character flags (visible character attributes) */
#define INVERSE		AttrBIT(0)	/* invert the characters to be output */
#define UNDERLINE	AttrBIT(1)	/* true if underlining */
#define BOLD		AttrBIT(2)
#define BLINK		AttrBIT(3)
/* global flags (also character attributes) */
#define BG_COLOR	AttrBIT(4)	/* true if background set */
#define FG_COLOR	AttrBIT(5)	/* true if foreground set */

/* character flags (internal attributes) */
#define PROTECTED	AttrBIT(6)	/* a character that cannot be erased */
#define CHARDRAWN	AttrBIT(7)	/* a character has been drawn here on
					   the screen.  Used to distinguish
					   blanks from empty parts of the
					   screen when selecting */

/* The following attributes are used in the argument of drawXtermText()  */
#define NOBACKGROUND	DrawBIT(0)	/* Used for overstrike */
#define NOTRANSLATION	DrawBIT(1)	/* No scan for chars missing in font */
#define DOUBLEWFONT	DrawBIT(2)	/* The actual X-font is double-width */
#define DOUBLEHFONT	DrawBIT(3)	/* The actual X-font is double-height */
#define CHARBYCHAR	DrawBIT(4)	/* Draw chars one-by-one */

/* The following attribute is used in the argument of xtermSpecialFont etc */
#define NORESOLUTION	DrawBIT(5)	/* find the font without resolution */

/*
 * Other flags
 */
#define WRAPAROUND	MiscBIT(0)	/* true if auto wraparound mode */
#define	REVERSEWRAP	MiscBIT(1)	/* true if reverse wraparound mode */
#define REVERSE_VIDEO	MiscBIT(2)	/* true if screen white on black */
#define LINEFEED	MiscBIT(3)	/* true if in auto linefeed mode */
#define ORIGIN		MiscBIT(4)	/* true if in origin mode */
#define INSERT		MiscBIT(5)	/* true if in insert mode */
#define SMOOTHSCROLL	MiscBIT(6)	/* true if in smooth scroll mode */
#define IN132COLUMNS	MiscBIT(7)	/* true if in 132 column mode */
#define INVISIBLE	MiscBIT(8)	/* true if writing invisible text */
#define NATIONAL        MiscBIT(9)	/* true if writing national charset */

/*
 * Groups of attributes
 */
			/* mask for video-attributes only */
#define SGR_MASK	(BOLD | BLINK | UNDERLINE | INVERSE)

			/* mask: user-visible attributes */
#define	ATTRIBUTES	(SGR_MASK | BG_COLOR | FG_COLOR | INVISIBLE | PROTECTED)

/* The toplevel-call to drawXtermText() should have text-attributes guarded: */
#define DRAWX_MASK	(ATTRIBUTES | CHARDRAWN)

#if OPT_BLINK_TEXT
#define BOLDATTR(screen) (BOLD | ((screen)->blink_as_bold ? BLINK : 0))
#else
#define BOLDATTR(screen) (BOLD | BLINK)
#endif

/*
 * Per-line flags
 */
#define LINEWRAPPED	AttrBIT(0)
/* used once per line to indicate that it wraps onto the next line so we can
 * tell the difference between lines that have wrapped around and lines that
 * have ended naturally with a CR at column max_col.
 */
#define LINEBLINKED	AttrBIT(1)
/* set when the line contains blinking text.
 */

#if OPT_ZICONBEEP || OPT_TOOLBAR
#define HANDLE_STRUCT_NOTIFY 1
#else
#define HANDLE_STRUCT_NOTIFY 0
#endif

/*
 * If we've set protected attributes with the DEC-style DECSCA, then we'll have
 * to use DECSED or DECSEL to erase preserving protected text.  (The normal ED,
 * EL won't preserve protected-text).  If we've used SPA, then normal ED and EL
 * will preserve protected-text.  To keep things simple, just remember the last
 * control that was used to begin protected-text, and use that to determine how
 * erases are performed (otherwise we'd need 2 bits per protected character).
 */
#define OFF_PROTECT 0
#define DEC_PROTECT 1
#define ISO_PROTECT 2

#define TScreenOf(xw)	(&(xw)->screen)
#define TekScreenOf(tw) (&(tw)->screen)

#ifdef SCROLLBAR_RIGHT
#define OriginX(screen) (((term->misc.useRight)?0:ScrollbarWidth(screen)) + screen->border)
#else
#define OriginX(screen) (ScrollbarWidth(screen) + screen->border)
#endif

#define OriginY(screen) (screen->border)

#define CursorMoved(screen) \
		((screen)->cursor_moved || \
		    ((screen)->cursorp.col != (screen)->cur_col || \
		     (screen)->cursorp.row != (screen)->cur_row))

#define CursorX(screen,col) ((col) * FontWidth(screen) + OriginX(screen))
#define CursorY(screen,row) ((INX2ROW(screen, row) * FontHeight(screen)) \
			+ screen->border)

/*
 * These definitions depend on whether xterm supports active-icon.
 */
#ifndef NO_ACTIVE_ICON
#define IsIconWin(screen,win)	((win) == &(screen)->iconVwin)
#define IsIcon(screen)		(WhichVWin(screen) == &(screen)->iconVwin)
#define WhichVWin(screen)	((screen)->whichVwin)
#define WhichTWin(screen)	((screen)->whichTwin)

#define WhichVFont(screen,name)	(IsIcon(screen) ? (screen)->fnt_icon.fs \
						: (screen)->name)
#define FontAscent(screen)	(IsIcon(screen) ? (screen)->fnt_icon.fs->ascent \
						: WhichVWin(screen)->f_ascent)
#define FontDescent(screen)	(IsIcon(screen) ? (screen)->fnt_icon.fs->descent \
						: WhichVWin(screen)->f_descent)
#else /* NO_ACTIVE_ICON */

#define IsIconWin(screen,win)	(False)
#define IsIcon(screen)		(False)
#define WhichVWin(screen)	(&((screen)->fullVwin))
#define WhichTWin(screen)	(&((screen)->fullTwin))

#define WhichVFont(screen,name)	((screen)->name)
#define FontAscent(screen)	WhichVWin(screen)->f_ascent
#define FontDescent(screen)	WhichVWin(screen)->f_descent

#endif /* NO_ACTIVE_ICON */

#define okFont(font) ((font) != 0 && (font)->fid != 0)

/*
 * Macro to check if we are iconified; do not use render for that case.
 */
#define UsingRenderFont(xw)	((xw)->misc.render_font && !IsIcon(&((xw)->screen)))

/*
 * These definitions do not depend on whether xterm supports active-icon.
 */
#define VWindow(screen)		WhichVWin(screen)->window
#define VShellWindow		XtWindow(SHELL_OF(term))
#define TWindow(screen)		WhichTWin(screen)->window
#define TShellWindow		XtWindow(SHELL_OF(tekWidget))

#define Width(screen)		WhichVWin(screen)->width
#define Height(screen)		WhichVWin(screen)->height
#define FullWidth(screen)	WhichVWin(screen)->fullwidth
#define FullHeight(screen)	WhichVWin(screen)->fullheight
#define FontWidth(screen)	WhichVWin(screen)->f_width
#define FontHeight(screen)	WhichVWin(screen)->f_height

#define NormalFont(screen)	WhichVFont(screen, fnts[fNorm].fs)
#define BoldFont(screen)	WhichVFont(screen, fnts[fBold].fs)

#if OPT_WIDE_CHARS
#define NormalWFont(screen)	WhichVFont(screen, fnts[fWide].fs)
#define BoldWFont(screen)	WhichVFont(screen, fnts[fWBold].fs)
#endif

#define ScrollbarWidth(screen)	WhichVWin(screen)->sb_info.width

#define NormalGC(w,sp)		getCgsGC(w, WhichVWin(sp), gcNorm)
#define ReverseGC(w,sp)		getCgsGC(w, WhichVWin(sp), gcNormReverse)
#define NormalBoldGC(w,sp)	getCgsGC(w, WhichVWin(sp), gcBold)
#define ReverseBoldGC(w,sp)	getCgsGC(w, WhichVWin(sp), gcBoldReverse)

#define TWidth(screen)		WhichTWin(screen)->width
#define THeight(screen)		WhichTWin(screen)->height
#define TFullWidth(screen)	WhichTWin(screen)->fullwidth
#define TFullHeight(screen)	WhichTWin(screen)->fullheight
#define TekScale(screen)	WhichTWin(screen)->tekscale

#define BorderWidth(w)		((w)->core.border_width)
#define BorderPixel(w)		((w)->core.border_pixel)

#define AllowXtermOps(w,name)	((w)->screen.name && !(w)->screen.allowSendEvents)

#define AllowFontOps(w)		AllowXtermOps(w, allowFontOps)
#define AllowTcapOps(w)		AllowXtermOps(w, allowTcapOps)
#define AllowTitleOps(w)	AllowXtermOps(w, allowTitleOps)

#define AllowWindowOps(w,name)	(AllowXtermOps(w, allowWindowOps) || \
				 !(w)->screen.disallow_win_ops[name])

#if OPT_TOOLBAR
#define ToolbarHeight(w)	((resource.toolBar) \
				 ? (term->VT100_TB_INFO(menu_height) \
				  + term->VT100_TB_INFO(menu_border) * 2) \
				 : 0)
#else
#define ToolbarHeight(w) 0
#endif

#if OPT_TEK4014
#define TEK_LINK_BLOCK_SIZE 1024

typedef struct Tek_Link
{
	struct Tek_Link	*next;	/* pointer to next TekLink in list
				   NULL <=> this is last TekLink */
	unsigned short fontsize;/* character size, 0-3 */
	unsigned short count;	/* number of chars in data */
	char *ptr;		/* current pointer into data */
	char data [TEK_LINK_BLOCK_SIZE];
} TekLink;
#endif /* OPT_TEK4014 */

/* flags for cursors */
#define	OFF		0
#define	ON		1
#define	BLINKED_OFF	2
#define	CLEAR		0
#define	TOGGLE		1

/* flags for inhibit */
#ifdef ALLOWLOGGING
#define	I_LOG		0x01
#endif
#define	I_SIGNAL	0x02
#define	I_TEK		0x04

/***====================================================================***/

#if OPT_TRACE
#undef NDEBUG			/* turn on assert's */
#else
#ifndef NDEBUG
#define NDEBUG			/* not debugging, don't do assert's */
#endif
#endif

#include <trace.h>

#ifndef TRACE
#define TRACE(p) /*nothing*/
#endif

#ifndef TRACE_ARGV
#define TRACE_ARGV(tag,argv) /*nothing*/
#endif

#ifndef TRACE_CHILD
#define TRACE_CHILD /*nothing*/
#endif

#ifndef TRACE_HINTS
#define TRACE_HINTS(hints) /*nothing*/
#endif

#ifndef TRACE_IDS
#define TRACE_IDS /*nothing*/
#endif

#ifndef TRACE_OPTS
#define TRACE_OPTS(opts,ress,lens) /*nothing*/
#endif

#ifndef TRACE_TRANS
#define TRACE_TRANS(name,w) /*nothing*/
#endif

#ifndef TRACE_WM_HINTS
#define TRACE_WM_HINTS(w) /*nothing*/
#endif

#ifndef TRACE_XRES
#define TRACE_XRES() /*nothing*/
#endif

#ifndef TRACE2
#define TRACE2(p) /*nothing*/
#endif

#endif /* included_ptyx_h */
