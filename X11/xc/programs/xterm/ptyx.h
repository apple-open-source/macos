/*
 *	$Xorg: ptyx.h,v 1.3 2000/08/17 19:55:09 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/ptyx.h,v 3.111 2003/12/31 17:12:28 dickey Exp $ */

/*
 * Copyright 1999-2002,2003 by Thomas E. Dickey
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

#if defined(__osf__) || (defined(linux) && defined(__GLIBC__) && (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)) || defined(__NetBSD__)
#undef USE_PTY_DEVICE
#undef USE_PTY_SEARCH
#define USE_PTS_DEVICE 1
#elif defined(VMS)
#undef USE_PTY_DEVICE
#undef USE_PTY_SEARCH
#elif defined(PUCC_PTYD)
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
#ifdef __FreeBSD__
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

typedef enum {NORMAL, LEFTEXTENSION, RIGHTEXTENSION} EventMode;

/*
 * The origin of a screen is 0, 0.  Therefore, the number of rows
 * on a screen is screen->max_row + 1, and similarly for columns.
 */

typedef unsigned char Char;		/* to support 8 bit chars */
typedef Char **ScrnBuf;

#define CharOf(n) ((unsigned char)(n))

/*
 * ANSI emulation, special character codes
 */
#define INQ	0x05
#define BEL	0x07
#define	FF	0x0C			/* C0, C1 control names		*/
#define	LS1	0x0E
#define	LS0	0x0F
#define	NAK	0x15
#define	CAN	0x18
#define	SUB	0x1A
#define	ESC	0x1B
#define XPOUND	0x1E			/* internal mapping for '#'	*/
#define US	0x1F
#define	DEL	0x7F
#define	RI	0x8D
#define	SS2	0x8E
#define	SS3	0x8F
#define	DCS	0x90
#define	SPA	0x96
#define	EPA	0x97
#define	SOS	0x98
#define	OLDID	0x9A			/* ESC Z			*/
#define	CSI	0x9B
#define	ST	0x9C
#define	OSC	0x9D
#define	PM	0x9E
#define	APC	0x9F
#define	RDEL	0xFF

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

typedef struct {
	unsigned char	a_type;
	unsigned char	a_pintro;
	unsigned char	a_final;
	unsigned char	a_inters;
	char	a_nparam;		/* # of parameters		*/
	short	a_param[NPARAM];	/* Parameters			*/
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

/***====================================================================***/

#define	TEXT_FG		0
#define	TEXT_BG		1
#define	TEXT_CURSOR	2
#define	MOUSE_FG	3
#define	MOUSE_BG	4
#define	TEK_FG		5
#define	TEK_BG		6
#define	HIGHLIGHT_BG	7
#define	NCOLORS		8

#define EXCHANGE(a,b,tmp) tmp = a; a = b; b = tmp;

#define	COLOR_DEFINED(s,w)	((s)->which&(1<<(w)))
#define	COLOR_VALUE(s,w)	((s)->colors[w])
#define	SET_COLOR_VALUE(s,w,v)	(((s)->colors[w]=(v)),((s)->which|=(1<<(w))))

#define	COLOR_NAME(s,w)		((s)->names[w])
#define	SET_COLOR_NAME(s,w,v)	(((s)->names[w]=(v)),((s)->which|=(1<<(w))))

#define	UNDEFINE_COLOR(s,w)	((s)->which&=(~((w)<<1)))
#define	OPPOSITE_COLOR(n)	(((n)==TEXT_FG?TEXT_BG:\
				 ((n)==TEXT_BG?TEXT_FG:\
				 ((n)==MOUSE_FG?MOUSE_BG:\
				 ((n)==MOUSE_BG?MOUSE_FG:\
				 ((n)==TEK_FG?TEK_BG:\
				 ((n)==TEXT_BG?TEK_FG:(n))))))))

#ifndef RES_OFFSET
#define RES_OFFSET(offset) XtOffsetOf(XtermWidgetRec, offset)
#endif

#define RES_NAME(name) name
#define RES_CLASS(name) name

#define Bres(name,class,offset,value) \
	{RES_NAME(name), RES_CLASS(class), XtRBoolean, sizeof(Boolean), \
	 RES_OFFSET(offset), XtRImmediate, (XtPointer) value}

#define Cres(name,class,offset,value) \
	{RES_NAME(name), RES_CLASS(class), XtRPixel, sizeof(Pixel), \
	 RES_OFFSET(offset), XtRString, (XtPointer) value}

#define Fres(name,class,offset,value) \
	{RES_NAME(name), RES_CLASS(class), XtRFontStruct, sizeof(XFontStruct *), \
	 RES_OFFSET(offset), XtRString, (XtPointer) value}

#define Ires(name,class,offset,value) \
	{RES_NAME(name), RES_CLASS(class), XtRInt, sizeof(int), \
	 RES_OFFSET(offset), XtRImmediate, (XtPointer) value}

#define Sres(name,class,offset,value) \
	{RES_NAME(name), RES_CLASS(class), XtRString, sizeof(char *), \
	 RES_OFFSET(offset), XtRString, (XtPointer) value}

/***====================================================================***/

#if (XtSpecificationRelease < 6)
#ifndef NO_ACTIVE_ICON
#define NO_ACTIVE_ICON 1 /* Note: code relies on an X11R6 function */
#endif
#endif

#ifndef OPT_AIX_COLORS
#define OPT_AIX_COLORS  1 /* true if xterm is configured with AIX (16) colors */
#endif

#ifndef OPT_BLINK_CURS
#define OPT_BLINK_CURS  1 /* true if xterm has blinking cursor capability */
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

#ifndef OPT_DEC_CHRSET
#define OPT_DEC_CHRSET  1 /* true if xterm is configured for DEC charset */
#endif

#ifndef OPT_DEC_LOCATOR
#define	OPT_DEC_LOCATOR 0 /* true if xterm supports VT220-style mouse events */
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

#ifndef OPT_MOD_FKEYS
#define OPT_MOD_FKEYS	1 /* modify cursor- and function-keys in normal mode */
#endif

#ifndef OPT_NUM_LOCK
#define OPT_NUM_LOCK	1 /* use NumLock key only for numeric-keypad */
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

#ifndef OPT_SAME_NAME
#define OPT_SAME_NAME   1 /* suppress redundant updates of title, icon, etc. */
#endif

#ifndef OPT_SCO_FUNC_KEYS
#define OPT_SCO_FUNC_KEYS 0 /* true if xterm supports SCO-style function keys */
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

#ifndef OPT_VT52_MODE
#define OPT_VT52_MODE   1 /* true if xterm supports VT52 emulation */
#endif

#ifndef OPT_WIDE_CHARS
#define OPT_WIDE_CHARS  0 /* true if xterm supports 16-bit characters */
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

#if OPT_ISO_COLORS
#define if_OPT_ISO_COLORS(screen, code) if(screen->colorMode) code
#define TERM_COLOR_FLAGS (term->flags & (FG_COLOR|BG_COLOR))
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

#if OPT_256_COLORS
# define NUM_ANSI_COLORS 256
#elif OPT_88_COLORS
# define NUM_ANSI_COLORS 88
#else
# define NUM_ANSI_COLORS 16
#endif

#if NUM_ANSI_COLORS > 16
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
#define DFT_COLORMODE TRUE	/* default colorMode resource */
#endif

#define ReverseOrHilite(screen,flags,hilite) \
		(( screen->colorRVMode && hilite ) || \
		    ( !screen->colorRVMode && \
		      (( (flags & INVERSE) && !hilite) || \
		       (!(flags & INVERSE) &&  hilite)) ))

/* Define a fake XK code, we need it for the fake color response in
 * xtermcapKeycode(). */
#if OPT_TCAP_QUERY
# define XK_COLORS 0x0003
#endif

#else	/* !OPT_ISO_COLORS */

#define if_OPT_ISO_COLORS(screen, code) /* nothing */
#define TERM_COLOR_FLAGS 0

#define ReverseOrHilite(screen,flags,hilite) \
		      (( (flags & INVERSE) && !hilite) || \
		       (!(flags & INVERSE) &&  hilite))

#endif	/* OPT_ISO_COLORS */

#if OPT_AIX_COLORS
#define if_OPT_AIX_COLORS(screen, code) if(screen->colorMode) code
#else
#define if_OPT_AIX_COLORS(screen, code) /* nothing */
#endif

#if OPT_256_COLORS || OPT_88_COLORS
# define if_OPT_EXT_COLORS(screen, code) if(screen->colorMode) code
# define if_OPT_ISO_TRADITIONAL_COLORS(screen, code) /* nothing */
#elif OPT_ISO_COLORS
# define if_OPT_EXT_COLORS(screen, code) /* nothing */
# define if_OPT_ISO_TRADITIONAL_COLORS(screen, code) if(screen->colorMode) code
#else
# define if_OPT_EXT_COLORS(screen, code) /* nothing */
# define if_OPT_ISO_TRADITIONAL_COLORS(screen, code) /*nothing*/
#endif

#define COLOR_RES_NAME(root) "color" root

#if OPT_COLOR_CLASS
#define COLOR_RES_CLASS(root) "Color" root
#else
#define COLOR_RES_CLASS(root) XtCForeground
#endif

#if OPT_COLOR_RES
#define COLOR_RES(root,offset,value) Sres(COLOR_RES_NAME(root), COLOR_RES_CLASS(root), offset.resource, value)
#else
#define COLOR_RES(root,offset,value) Cres(COLOR_RES_NAME(root), COLOR_RES_CLASS(root), offset, value)
#endif

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
#define CSET_EXTEND(code)  ((code) > CSET_DWL)
	/* for doublesize characters, the first cell in a row holds the info */
#define SCRN_ROW_CSET(screen,row) (SCRN_BUF_CSETS((screen), row)[0])
#define CurMaxCol(screen, row) \
	(CSET_DOUBLE(SCRN_ROW_CSET(screen, row)) \
	? (screen->max_col / 2) \
	: (screen->max_col))
#define CurCursorX(screen, row, col) \
	(CSET_DOUBLE(SCRN_ROW_CSET(screen, row)) \
	? CursorX(screen, 2*(col)) \
	: CursorX(screen, (col)))
#define CurFontWidth(screen, row) \
	(CSET_DOUBLE(SCRN_ROW_CSET(screen, row)) \
	? 2*FontWidth(screen) \
	: FontWidth(screen))
#else
#define if_OPT_DEC_CHRSET(code) /*nothing*/
#define CurMaxCol(screen, row) screen->max_col
#define CurCursorX(screen, row, col) CursorX(screen, col)
#define CurFontWidth(screen, row) FontWidth(screen)
#endif

#if OPT_LUIT_PROG && !OPT_WIDE_CHARS
#error Luit requires the wide-chars configuration
#endif

	/* the number of pointers per row in 'ScrnBuf' */
#if OPT_ISO_COLORS || OPT_DEC_CHRSET || OPT_WIDE_CHARS
#define MAX_PTRS term->num_ptrs
#else
#define MAX_PTRS (OFF_ATTRS+1)
#endif

#define BUF_HEAD 1
	/* the number that point to Char data */
#define BUF_PTRS (MAX_PTRS - BUF_HEAD)

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
#define TEK4014_ACTIVE(screen) ((screen)->TekEmu)
#define CURRENT_EMU_VAL(screen,tek,vt) (TEK4014_ACTIVE(screen) ? tek : vt)
#define CURRENT_EMU(screen) CURRENT_EMU_VAL(screen, (Widget)tekWidget, (Widget)term)
#else
#define TEK4014_ACTIVE(screen) 0
#define CURRENT_EMU_VAL(screen,tek,vt) (vt)
#define CURRENT_EMU(screen) ((Widget)term)
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
#define XMC_FLAGS (INVERSE|UNDERLINE|BOLD)
#else
#define if_OPT_XMC_GLITCH(screen, code) /* nothing */
#endif

/***====================================================================***/

#if OPT_WIDE_CHARS
#define if_OPT_WIDE_CHARS(screen, code) if(screen->wide_chars) code
#define PAIRED_CHARS(a,b) a,b
typedef unsigned IChar;		/* for 8 or 16-bit characters, plus flag */
#else
#define if_OPT_WIDE_CHARS(screen, code) /* nothing */
#define PAIRED_CHARS(a,b) a
typedef unsigned char IChar;	/* for 8-bit characters */
#endif

/***====================================================================***/

#define BUF_SIZE 4096

typedef struct {
	int	cnt;		/* number of chars left to process */
	IChar *	ptr;		/* pointer into decoded data */
	Char	buf[BUF_SIZE];	/* we read directly into this */
#if OPT_WIDE_CHARS
	IChar	buf2[BUF_SIZE];	/* ...and may decode into this */
#define DecodedData(data) (data)->buf2
#else
#define DecodedData(data) (data)->buf
#endif
	} PtyData;

/***====================================================================***/

#if OPT_TRACE
#include <trace.h>
#else
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
#ifndef TRACE_OPTS
#define TRACE_OPTS(opts,ress,lens) /*nothing*/
#endif
#ifndef TRACE_TRANS
#define TRACE_TRANS(name,w) /*nothing*/
#endif
#endif

/***====================================================================***/

/* The order of ifdef's matches the logic for num_ptrs in VTInitialize */
typedef enum {
	OFF_FLAGS = 0		/* BUF_HEAD */
	, OFF_CHARS = 1
	, OFF_ATTRS = 2
#if OPT_ISO_COLORS
#if OPT_256_COLORS || OPT_88_COLORS
	, OFF_FGRND
	, OFF_BGRND
#else
	, OFF_COLOR
#endif
#endif
#if OPT_DEC_CHRSET
	, OFF_CSETS
#endif
#if OPT_WIDE_CHARS
	, OFF_WIDEC
	, OFF_COM1L
	, OFF_COM1H
	, OFF_COM2L
	, OFF_COM2H
#endif
} BufOffsets;

	/* ScrnBuf-level macros */
#define BUF_FLAGS(buf, row) (buf[MAX_PTRS * (row) + OFF_FLAGS])
#define BUF_CHARS(buf, row) (buf[MAX_PTRS * (row) + OFF_CHARS])
#define BUF_ATTRS(buf, row) (buf[MAX_PTRS * (row) + OFF_ATTRS])
#define BUF_COLOR(buf, row) (buf[MAX_PTRS * (row) + OFF_COLOR])
#define BUF_FGRND(buf, row) (buf[MAX_PTRS * (row) + OFF_FGRND])
#define BUF_BGRND(buf, row) (buf[MAX_PTRS * (row) + OFF_BGRND])
#define BUF_CSETS(buf, row) (buf[MAX_PTRS * (row) + OFF_CSETS])
#define BUF_WIDEC(buf, row) (buf[MAX_PTRS * (row) + OFF_WIDEC])
#define BUF_COM1L(buf, row) (buf[MAX_PTRS * (row) + OFF_COM1L])
#define BUF_COM1H(buf, row) (buf[MAX_PTRS * (row) + OFF_COM1H])
#define BUF_COM2L(buf, row) (buf[MAX_PTRS * (row) + OFF_COM2L])
#define BUF_COM2H(buf, row) (buf[MAX_PTRS * (row) + OFF_COM2H])

	/* TScreen-level macros */
#define SCRN_BUF_FLAGS(screen, row) BUF_FLAGS(screen->visbuf, row)
#define SCRN_BUF_CHARS(screen, row) BUF_CHARS(screen->visbuf, row)
#define SCRN_BUF_ATTRS(screen, row) BUF_ATTRS(screen->visbuf, row)
#define SCRN_BUF_COLOR(screen, row) BUF_COLOR(screen->visbuf, row)
#define SCRN_BUF_FGRND(screen, row) BUF_FGRND(screen->visbuf, row)
#define SCRN_BUF_BGRND(screen, row) BUF_BGRND(screen->visbuf, row)
#define SCRN_BUF_CSETS(screen, row) BUF_CSETS(screen->visbuf, row)
#define SCRN_BUF_WIDEC(screen, row) BUF_WIDEC(screen->visbuf, row)
#define SCRN_BUF_COM1L(screen, row) BUF_COM1L(screen->visbuf, row)
#define SCRN_BUF_COM2L(screen, row) BUF_COM2L(screen->visbuf, row)
#define SCRN_BUF_COM1H(screen, row) BUF_COM1H(screen->visbuf, row)
#define SCRN_BUF_COM2H(screen, row) BUF_COM2H(screen->visbuf, row)

typedef struct {
	unsigned	chrset;
	unsigned	flags;
	XFontStruct *	fs;
	GC		gc;
	char *		fn;
} XTermFonts;

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
	DP_LAST
	} SaveModes;

#define DoSM(code,value) screen->save_modes[code] = value
#define DoRM(code,value) value = screen->save_modes[code]

	/* index into vt_shell[] or tek_shell[] */
typedef enum {
	noMenu = -1,
	mainMenu,
	vtMenu,
	fontMenu,
	tekMenu
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
	unsigned	which;	/* must have NCOLORS bits */
	Pixel		colors[NCOLORS];
	char		*names[NCOLORS];
} ScrnColors;

typedef struct {
	Boolean		saved;
	int		row;
	int		col;
	unsigned	flags;		/* VTxxx saves graphics rendition */
	char		curgl;
	char		curgr;
	char		gsets[4];
#if OPT_ISO_COLORS
	int		cur_foreground; /* current foreground color	*/
	int		cur_background; /* current background color	*/
	int		sgr_foreground; /* current SGR foreground color */
	int		sgr_background; /* current SGR background color */
	Boolean		sgr_extended;	/* SGR set with extended codes? */
#endif
} SavedCursor;

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

struct _vtwin {
	Window		window;		/* X window id			*/
	int		width;		/* width of columns		*/
	int		height;		/* height of rows		*/
	int		fullwidth;	/* full width of window		*/
	int		fullheight;	/* full height of window	*/
	int		f_width;	/* width of fonts in pixels	*/
	int		f_height;	/* height of fonts in pixels	*/
	int		f_ascent;	/* ascent of font in pixels	*/
	int		f_descent;	/* descent of font in pixels	*/
	SbInfo		sb_info;
	GC		normalGC;	/* normal painting		*/
	GC		reverseGC;	/* reverse painting		*/
	GC		normalboldGC;	/* normal painting, bold font	*/
	GC		reverseboldGC;	/* reverse painting, bold font	*/
#if OPT_TOOLBAR
	Widget		menu_bar;	/* toolbar, if initialized	*/
	Dimension	menu_height;	/* ...and its height		*/
#endif
};

struct _tekwin {
	Window		window;		/* X window id			*/
	int		width;		/* width of columns		*/
	int		height;		/* height of rows		*/
	int		fullwidth;	/* full width of window		*/
	int		fullheight;	/* full height of window	*/
	double		tekscale;	/* scale factor Tek -> vs100	*/
};

typedef struct {
/* These parameters apply to both windows */
	Display		*display;	/* X display for screen		*/
	int		respond;	/* socket for responses
					   (position report, etc.)	*/
#if OPT_TCAP_QUERY
	int		tc_query;
#endif
	long		pid;		/* pid of process on far side   */
	int		uid;		/* user id of actual person	*/
	int		gid;		/* group id of actual person	*/
	GC		cursorGC;	/* normal cursor painting	*/
	GC		fillCursorGC;	/* special cursor painting	*/
	GC		reversecursorGC;/* reverse cursor painting	*/
	GC		cursoroutlineGC;/* for painting lines around    */
	Pixel		foreground;	/* foreground color		*/
	Pixel		cursorcolor;	/* Cursor color			*/
	Pixel		mousecolor;	/* Mouse color			*/
	Pixel		mousecolorback;	/* Mouse color background	*/
#if OPT_ISO_COLORS
	ColorRes	Acolors[MAXCOLORS]; /* ANSI color emulation	*/
	int		veryBoldColors;	/* modifier for boldColors	*/
	Boolean		boldColors;	/* can we make bold colors?	*/
	Boolean		colorMode;	/* are we using color mode?	*/
	Boolean		colorULMode;	/* use color for underline?	*/
	Boolean		colorBDMode;	/* use color for bold?		*/
	Boolean		colorBLMode;	/* use color for blink?		*/
	Boolean		colorRVMode;	/* use color for reverse?	*/
	Boolean		colorAttrMode;	/* prefer colorUL/BD to SGR	*/
#endif
#if OPT_HIGHLIGHT_COLOR
	Pixel		highlightcolor;	/* Highlight background color	*/
#endif
#if OPT_DEC_CHRSET
	Boolean		font_doublesize;/* enable font-scaling		*/
	int		cache_doublesize;/* limit of our cache		*/
	Char		cur_chrset;	/* character-set index & code	*/
	int		fonts_used;	/* count items in double_fonts	*/
	XTermFonts	double_fonts[NUM_CHRSET];
#endif
#if OPT_WIDE_CHARS
	Boolean		wide_chars;	/* true when 16-bit chars	*/
	Boolean		vt100_graphics;	/* true to allow vt100-graphics	*/
	int		utf8_mode;	/* use UTF-8 decode/encode: 0-2	*/
	int		utf_count;	/* state of utf_char */
	IChar		utf_char;	/* in-progress character */
#endif
#if OPT_BROKEN_OSC
	Boolean		brokenLinuxOSC; /* true to ignore Linux palette ctls */
#endif
#if OPT_BROKEN_ST
	Boolean		brokenStringTerm; /* true to match old OSC parse */
#endif
#if OPT_C1_PRINT
	Boolean		c1_printable;	/* true if we treat C1 as print	*/
#endif
	int		border;		/* inner border			*/
	Cursor		arrow;		/* arrow cursor			*/
	unsigned long	event_mask;
	unsigned short	send_mouse_pos;	/* user wants mouse transition  */
					/* and position information	*/
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
	unsigned short	locator_events;	/* what events to report	*/
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
	Boolean		visualbell;	/* visual bell mode		*/
	Boolean		poponbell;	/* pop on bell mode		*/
	Boolean		allowSendEvents;/* SendEvent mode		*/
	Boolean		allowWindowOps;	/* WindowOps mode		*/
	Boolean		awaitInput;	/* select-timeout mode		*/
	Boolean		grabbedKbd;	/* keyboard is grabbed		*/
#ifdef ALLOWLOGGING
	int		logging;	/* logging mode			*/
	int		logfd;		/* file descriptor of log	*/
	char		*logfile;	/* log file name		*/
	IChar		*logstart;	/* current start of log buffer	*/
#endif
	int		inhibit;	/* flags for inhibiting changes	*/

/* VT window parameters */
	Boolean		Vshow;		/* VT window showing		*/
	struct _vtwin	fullVwin;
#ifndef NO_ACTIVE_ICON
	struct _vtwin	iconVwin;
	struct _vtwin *	whichVwin;
#endif /* NO_ACTIVE_ICON */

	Cursor	pointer_cursor;		/* pointer cursor in window	*/

	String	answer_back;		/* response to ENQ		*/
	String	printer_command;	/* pipe/shell command string	*/
	Boolean printer_autoclose;	/* close printer when offline	*/
	Boolean printer_extent;		/* print complete page		*/
	Boolean printer_formfeed;	/* print formfeed per function	*/
	int	printer_controlmode;	/* 0=off, 1=auto, 2=controller	*/
	int	print_attributes;	/* 0=off, 1=normal, 2=color	*/

	Boolean		fnt_prop;	/* true if proportional fonts	*/
	Boolean		fnt_boxes;	/* true if font has box-chars	*/
#if OPT_BOX_CHARS
	Boolean		force_box_chars;/* true if we assume that	*/
#endif
	Dimension	fnt_wide;
	Dimension	fnt_high;
	XFontStruct	*fnt_norm;	/* normal font of terminal	*/
	XFontStruct	*fnt_bold;	/* bold font of terminal	*/
	Boolean		free_bold_box;	/* same_font_size's austerity	*/
#if OPT_WIDE_CHARS
	XFontStruct	*fnt_dwd;	/* wide font of terminal	*/
	XFontStruct	*fnt_dwdb;	/* wide bold font of terminal	*/
#endif
#ifndef NO_ACTIVE_ICON
	XFontStruct	*fnt_icon;	/* icon font */
#endif /* NO_ACTIVE_ICON */
	int		enbolden;	/* overstrike for bold font	*/
	XPoint		*box;		/* draw unselected cursor	*/

	int		cursor_state;	/* ON, OFF, or BLINKED_OFF	*/
#if OPT_BLINK_CURS
	Boolean		cursor_blink;	/* cursor blink enable		*/
	int		cursor_on;	/* cursor on time (msecs)	*/
	int		cursor_off;	/* cursor off time (msecs)	*/
	XtIntervalId	cursor_timer;	/* timer-id for cursor-proc	*/
#endif
	int		cursor_set;	/* requested state		*/
	int		cursor_col;	/* previous cursor column	*/
	int		cursor_row;	/* previous cursor row		*/
	int		cur_col;	/* current cursor column	*/
	int		cur_row;	/* current cursor row		*/
	int		max_col;	/* rightmost column		*/
	int		max_row;	/* bottom row			*/
	int		top_marg;	/* top line of scrolling region */
	int		bot_marg;	/* bottom line of  "	    "	*/
	Widget		scrollWidget;	/* pointer to scrollbar struct	*/
	int		topline;	/* line number of top, <= 0	*/
	int		savedlines;     /* number of lines that've been saved */
	int		savelines;	/* number of lines off top to save */
	int		scrolllines;	/* number of lines to button scroll */
	Boolean		scrollttyoutput; /* scroll to bottom on tty output */
	Boolean		scrollkey;	/* scroll to bottom on key	*/

	ScrnBuf		visbuf;		/* ptr to visible screen buf (main) */
	ScrnBuf		allbuf;		/* screen buffer (may include
					   lines scrolled off top)	*/
	Char		*sbuf_address;	/* main screen memory address   */
	ScrnBuf		altbuf;		/* alternate screen buffer	*/
	Char		*abuf_address;	/* alternate screen memory address */
	Char		**save_ptr;	/* workspace for save-pointers  */
	size_t		save_len;	/* ...and its length		*/
	Boolean		alternate;	/* true if using alternate buf	*/
	unsigned short	do_wrap;	/* true if cursor in last column
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
	Boolean 	multiscroll;	/* true if multi-scroll		*/
	int		scrolls;	/* outstanding scroll count,
					    used only with multiscroll	*/
	SavedCursor	sc[2];		/* data for restore cursor	*/
	int		save_modes[DP_LAST]; /* save dec/xterm private modes */

	/* Improved VT100 emulation stuff.				*/
	String		keyboard_dialect; /* default keyboard dialect	*/
	char		gsets[4];	/* G0 through G3.		*/
	char		curgl;		/* Current GL setting.		*/
	char		curgr;		/* Current GR setting.		*/
	char		curss;		/* Current single shift.	*/
	String		term_id;	/* resource for terminal_id	*/
	int		terminal_id;	/* 100=vt100, 220=vt220, etc.	*/
	int		vtXX_level;	/* 0=vt52, 1,2,3 = vt100 ... vt320 */
	int		ansi_level;	/* levels 1,2,3			*/
	int		scroll_amt;	/* amount to scroll		*/
	int		refresh_amt;	/* amount to refresh		*/
	int		protected_mode;	/* 0=off, 1=DEC, 2=ISO		*/
	Boolean		old_fkeys;	/* true for compatible fkeys	*/
	Boolean		delete_is_del;	/* true for compatible Delete key */
	Boolean		jumpscroll;	/* whether we should jumpscroll */
	Boolean         always_highlight; /* whether to highlight cursor */
	Boolean		underline;	/* whether to underline text	*/
	Boolean		bold_mode;	/* whether to use bold font	*/

#if OPT_MAXIMIZE
	Boolean		restore_data;
	unsigned	restore_x;
	unsigned	restore_y;
	unsigned	restore_width;
	unsigned	restore_height;
#endif

#if OPT_VT52_MODE
	int		vt52_save_level; /* save-area for DECANM	*/
	char		vt52_save_curgl;
	char		vt52_save_curgr;
	char		vt52_save_curss;
	char		vt52_save_gsets[4];
#endif
	/* Testing */
#if OPT_XMC_GLITCH
	int		xmc_glitch;	/* # of spaces to pad on SGR's	*/
	int		xmc_attributes;	/* attrs that make a glitch	*/
	Boolean		xmc_inline;	/* SGR's propagate only to eol	*/
	Boolean		move_sgr_ok;	/* SGR is reset on move		*/
#endif

#if OPT_TEK4014
/* Tektronix window parameters */
	GC		TnormalGC;	/* normal painting		*/
	GC		TcursorGC;	/* normal cursor painting	*/
	Pixel		Tforeground;	/* foreground color		*/
	Pixel		Tbackground;	/* Background color		*/
	Pixel		Tcursorcolor;	/* Cursor color			*/

	int		Tcolor;		/* colors used			*/
	Boolean		Tshow;		/* Tek window showing		*/
	Boolean		waitrefresh;	/* postpone refresh		*/
	struct _tekwin	fullTwin;
#ifndef NO_ACTIVE_ICON
	struct _tekwin	iconTwin;
	struct _tekwin *whichTwin;
#endif /* NO_ACTIVE_ICON */

	int		xorplane;	/* z plane for inverts		*/
	GC		linepat[TEKNUMLINES]; /* line patterns		*/
	Boolean		TekEmu;		/* true if Tektronix emulation	*/
	int		cur_X;		/* current x			*/
	int		cur_Y;		/* current y			*/
	Tmodes		cur;		/* current tek modes		*/
	Tmodes		page;		/* starting tek modes on page	*/
	int		margin;		/* 0 -> margin 1, 1 -> margin 2	*/
	int		pen;		/* current Tektronix pen 0=up, 1=dn */
	char		*TekGIN;	/* nonzero if Tektronix GIN mode*/
	int		gin_terminator; /* Tek strap option */
#endif /* OPT_TEK4014 */

	int		multiClickTime;	 /* time between multiclick selects */
	int		visualBellDelay; /* msecs to delay for visibleBell */
	int		bellSuppressTime; /* msecs after Bell before another allowed */
	Boolean		bellInProgress; /* still ringing/flashing prev bell? */
	char		*charClass;	/* for overriding word selection */
	Boolean		cutNewline;	/* whether or not line cut has \n */
	Boolean		cutToBeginningOfLine;  /* line cuts to BOL? */
	Boolean		highlight_selection; /* controls appearance of selection */
	Boolean		trim_selection; /* controls trimming of selection */
	Boolean		i18nSelections;
	Boolean		brokenSelections;
	Char		*selection_data; /* the current selection */
	int		selection_size; /* size of allocated buffer */
	int		selection_length; /* number of significant bytes */
	Time		selection_time;	/* latest event timestamp */
	int		startHRow, startHCol, /* highlighted text */
			endHRow, endHCol,
			startHCoord, endHCoord;
	Atom*		selection_atoms; /* which selections we own */
	Cardinal	sel_atoms_size;	/*  how many atoms allocated */
	Cardinal	selection_count; /* how many atoms in use */
	Boolean		input_eight_bits;/* use 8th bit instead of ESC prefix */
	Boolean		output_eight_bits; /* honor all bits or strip */
	Boolean		control_eight_bits; /* send CSI as 8-bits */
	Boolean		backarrow_key;		/* backspace/delete */
	Boolean		meta_sends_esc;		/* Meta-key sends ESC prefix */
	Pixmap		menu_item_bitmap;	/* mask for checking items */
	String		menu_font_names[NMENUFONTS];
	long		menu_font_sizes[NMENUFONTS];
	int		menu_font_number;
	XIC		xic;
#ifdef XRENDERFONT
	XftFont		*renderFont;
	XftFont		*renderFontBold;
	XftDraw		*renderDraw;
#endif
#if OPT_INPUT_METHOD
	XFontSet	fs;		/* fontset for XIM preedit */
	int		fs_ascent;	/* ascent of fs */
#endif
} TScreen;

typedef struct _TekPart {
	XFontStruct *	Tfont[TEKNUMFONTS];
	int		tobaseline[TEKNUMFONTS]; /* top-baseline, each font */
	char *		initial_font;		/* large, 2, 3, small */
	char *		gin_terminator_str;	/* ginTerminator resource */
#if OPT_TOOLBAR
	Widget		menu_bar;	/* toolbar, if initialized	*/
	Dimension	menu_height;	/* ...and its height		*/
#endif
} TekPart;

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
    keyboardIsLegacy,		/* bogus vt220 codes for F1-F4, etc. */
    keyboardIsDefault,
    keyboardIsHP,
    keyboardIsSCO,
    keyboardIsSun,
    keyboardIsVT220
} xtermKeyboardType;

typedef struct
{
    xtermKeyboardType type;
    unsigned	flags;
#if OPT_INITIAL_ERASE
    int	reset_DECBKM;		/* reset should set DECBKM */
#endif
    int modify_cursor_keys;	/* how to handle modifiers */
} TKeyboard;

typedef struct {
    char *f_n;			/* the normal font */
    char *f_b;			/* the bold font */
#if OPT_WIDE_CHARS
    char *f_w;			/* the normal wide font */
    char *f_wb;			/* the bold wide font */
#endif
} VTFontNames;

typedef struct _Misc {
    VTFontNames default_font;
    char *geo_metry;
    char *T_geometry;
#if OPT_WIDE_CHARS
    Boolean	cjk_width;	/* true when CJK width convention is turned on */
#endif
#if OPT_LUIT_PROG
    Boolean callfilter;		/* true to invoke luit */
    Boolean use_encoding;	/* true to use -encoding option for luit */
    char *locale_str;		/* "locale" resource */
    char *localefilter;		/* path for luit */
#endif
#if OPT_INPUT_METHOD
    char *f_x;			/* font for XIM */
#endif
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
    char* input_method;
    char* preedit_type;
    Boolean open_im;
#endif
    Boolean dynamicColors;
    Boolean shared_ic;
#ifndef NO_ACTIVE_ICON
    Boolean active_icon;	/* use application icon window  */
    int icon_border_width;
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
    unsigned long alt_left;	/* modifier for Alt_L */
    unsigned long alt_right;	/* modifier for Alt_R */
    Boolean meta_trans;		/* true if Meta is used in translations */
    unsigned long meta_left;	/* modifier for Meta_L */
    unsigned long meta_right;	/* modifier for Meta_R */
#endif
#ifdef XRENDERFONT
    char *face_name;
    int face_size;
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
#endif

/* define masks for keyboard.flags */
#define MODE_KAM	0x01	/* keyboard action mode */
#define MODE_DECKPAM	0x02	/* keypad application mode */
#define MODE_DECCKM	0x04	/* cursor keys */
#define MODE_SRM	0x08	/* send-receive mode */
#define MODE_DECBKM	0x10	/* backarrow */


#define N_MARGINBELL	10

#define TAB_BITS_SHIFT	5	/* 2**5 == 32 */
#define TAB_BITS_WIDTH	(1 << TAB_BITS_SHIFT)
#define TAB_ARRAY_SIZE	10	/* number of ints to provide MAX_TABS bits */
#define MAX_TABS	(TAB_BITS_WIDTH * TAB_ARRAY_SIZE)

typedef unsigned Tabs [TAB_ARRAY_SIZE];

typedef struct _XtermWidgetRec {
    CorePart	core;
    TKeyboard	keyboard;	/* terminal keyboard		*/
    TScreen	screen;		/* terminal screen		*/
    unsigned	flags;		/* mode flags			*/
    int         cur_foreground;	/* current foreground color	*/
    int         cur_background;	/* current background color	*/
    Pixel       dft_foreground;	/* default foreground color	*/
    Pixel       dft_background;	/* default background color	*/
#if OPT_ISO_COLORS
    int         sgr_foreground;	/* current SGR foreground color	*/
    int         sgr_background;	/* current SGR background color	*/
    Boolean     sgr_extended;	/* SGR set with extended codes? */
#endif
#if OPT_ISO_COLORS || OPT_DEC_CHRSET || OPT_WIDE_CHARS
    int         num_ptrs;	/* number of pointers per row in 'ScrnBuf' */
#endif
    unsigned	initflags;	/* initial mode flags		*/
    Tabs	tabs;		/* tabstops of the terminal	*/
    Misc	misc;		/* miscellaneous parameters	*/
} XtermWidgetRec, *XtermWidget;

#if OPT_TEK4014
typedef struct _TekWidgetRec {
    CorePart core;
    TekPart tek;
} TekWidgetRec, *TekWidget;
#endif /* OPT_TEK4014 */

/*
 * terminal flags
 * There are actually two namespaces mixed together here.
 * One is the set of flags that can go in screen->visbuf attributes
 * and which must fit in a char.
 * The other is the global setting stored in
 * term->flags and screen->save_modes.  This need only fit in an unsigned.
 */

/* global flags and character flags (visible character attributes) */
#define INVERSE		0x01	/* invert the characters to be output */
#define UNDERLINE	0x02	/* true if underlining */
#define BOLD		0x04
#define BLINK		0x08
/* global flags (also character attributes) */
#define BG_COLOR	0x10	/* true if background set */
#define FG_COLOR	0x20	/* true if foreground set */

/* character flags (internal attributes) */
#define PROTECTED	0x40	/* a character is drawn that cannot be erased */
#define CHARDRAWN	0x80    /* a character has been drawn here on the
				   screen.  Used to distinguish blanks from
				   empty parts of the screen when selecting */

/* The following attributes make sense in the argument of drawXtermText()  */
#define NOBACKGROUND	0x100	/* Used for overstrike */
#define NOTRANSLATION	0x200	/* No scan for chars missing in font */
#define NATIVEENCODING	0x400	/* strings are in the font encoding */
#define DOUBLEWFONT	0x800	/* The actual X-font is double-width */
#define DOUBLEHFONT	0x1000	/* The actual X-font is double-height */
#define CHARBYCHAR	0x2000	/* Draw chars one-by-one */

/* The toplevel-call to drawXtermText() should have text-attributes guarded: */
#define DRAWX_MASK	0xff	/* text flags should be bitand'ed */

/* The following attribute makes sense in the argument of xtermSpecialFont etc */
#define NORESOLUTION	0x800000	/* find the font without resolution */

			/* mask: user-visible attributes */
#define	ATTRIBUTES	(INVERSE|UNDERLINE|BOLD|BLINK|BG_COLOR|FG_COLOR|INVISIBLE|PROTECTED)

#define WRAPAROUND	0x400	/* true if auto wraparound mode */
#define	REVERSEWRAP	0x800	/* true if reverse wraparound mode */
#define REVERSE_VIDEO	0x1000	/* true if screen white on black */
#define LINEFEED	0x2000	/* true if in auto linefeed mode */
#define ORIGIN		0x4000	/* true if in origin mode */
#define INSERT		0x8000	/* true if in insert mode */
#define SMOOTHSCROLL	0x10000	/* true if in smooth scroll mode */
#define IN132COLUMNS	0x20000	/* true if in 132 column mode */
#define INVISIBLE	0x40000	/* true if writing invisible text */
#define NATIONAL       0x100000	/* true if writing national charset */

/*
 * Per-line flags
 */
#define LINEWRAPPED	0x01	/* used once per line to indicate that it wraps
				 * onto the next line so we can tell the
				 * difference between lines that have wrapped
				 * around and lines that have ended naturally
				 * with a CR at column max_col.
				 */
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

#ifdef SCROLLBAR_RIGHT
#define OriginX(screen) (((term->misc.useRight)?0:ScrollbarWidth(screen)) + screen->border)
#else
#define OriginX(screen) (ScrollbarWidth(screen) + screen->border)
#endif

#define CursorX(screen,col) ((col) * FontWidth(screen) + OriginX(screen))
#define CursorY(screen,row) ((((row) - screen->topline) * FontHeight(screen)) \
			+ screen->border)

#ifndef NO_ACTIVE_ICON
#define IsIcon(screen)		((screen)->whichVwin == &(screen)->iconVwin)
#define VWindow(screen)		((screen)->whichVwin->window)
#define VShellWindow		XtWindow(SHELL_OF(term))
#define TWindow(screen)		((screen)->whichTwin->window)
#define TShellWindow		XtWindow(SHELL_OF(tekWidget))
#define Width(screen)		((screen)->whichVwin->width)
#define Height(screen)		((screen)->whichVwin->height)
#define FullWidth(screen)	((screen)->whichVwin->fullwidth)
#define FullHeight(screen)	((screen)->whichVwin->fullheight)
#define FontWidth(screen)	((screen)->whichVwin->f_width)
#define FontHeight(screen)	((screen)->whichVwin->f_height)
#define FontAscent(screen)	(IsIcon(screen) ? (screen)->fnt_icon->ascent \
						: (screen)->whichVwin->f_ascent)
#define FontDescent(screen)	(IsIcon(screen) ? (screen)->fnt_icon->descent \
						: (screen)->whichVwin->f_descent)
#define ScrollbarWidth(screen)	((screen)->whichVwin->sb_info.width)
#define NormalGC(screen)	((screen)->whichVwin->normalGC)
#define ReverseGC(screen)	((screen)->whichVwin->reverseGC)
#define NormalBoldGC(screen)	((screen)->whichVwin->normalboldGC)
#define ReverseBoldGC(screen)	((screen)->whichVwin->reverseboldGC)
#define TWidth(screen)		((screen)->whichTwin->width)
#define THeight(screen)		((screen)->whichTwin->height)
#define TFullWidth(screen)	((screen)->whichTwin->fullwidth)
#define TFullHeight(screen)	((screen)->whichTwin->fullheight)
#define TekScale(screen)	((screen)->whichTwin->tekscale)

#else /* NO_ACTIVE_ICON */

#define IsIcon(screen)		(False)
#define VWindow(screen)		((screen)->fullVwin.window)
#define VShellWindow		XtWindow(SHELL_OF(term))
#define TWindow(screen)		((screen)->fullTwin.window)
#define TShellWindow		XtWindow(SHELL_OF(tekWidget))
#define Width(screen)		((screen)->fullVwin.width)
#define Height(screen)		((screen)->fullVwin.height)
#define FullWidth(screen)	((screen)->fullVwin.fullwidth)
#define FullHeight(screen)	((screen)->fullVwin.fullheight)
#define FontWidth(screen)	((screen)->fullVwin.f_width)
#define FontHeight(screen)	((screen)->fullVwin.f_height)
#define FontAscent(screen)	((screen)->fullVwin.f_ascent)
#define FontDescent(screen)	((screen)->fullVwin.f_descent)
#define ScrollbarWidth(screen)	((screen)->fullVwin.sb_info.width)
#define NormalGC(screen)	((screen)->fullVwin.normalGC)
#define ReverseGC(screen)	((screen)->fullVwin.reverseGC)
#define NormalBoldGC(screen)	((screen)->fullVwin.normalboldGC)
#define ReverseBoldGC(screen)	((screen)->fullVwin.reverseboldGC)
#define TWidth(screen)		((screen)->fullTwin.width)
#define THeight(screen)		((screen)->fullTwin.height)
#define TFullWidth(screen)	((screen)->fullTwin.fullwidth)
#define TFullHeight(screen)	((screen)->fullTwin.fullheight)
#define TekScale(screen)	((screen)->fullTwin.tekscale)

#endif /* NO_ACTIVE_ICON */

#if OPT_TEK4014
#define TEK_LINK_BLOCK_SIZE 1024

typedef struct Tek_Link
{
	struct Tek_Link	*next;	/* pointer to next TekLink in list
				   NULL <=> this is last TekLink */
	short fontsize;		/* character size, 0-3 */
	short count;		/* number of chars in data */
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

#endif /* included_ptyx_h */
