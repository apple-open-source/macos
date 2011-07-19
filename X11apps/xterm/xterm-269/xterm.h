/* $XTermId: xterm.h,v 1.644 2011/02/20 00:49:16 tom Exp $ */

/************************************************************

Copyright 1999-2010,2011 by Thomas E. Dickey

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

********************************************************/

/*
 * Common/useful definitions for XTERM application.
 *
 * This is also where we put the fallback definitions if we do not build using
 * the configure script.
 */
#ifndef included_xterm_h
#define included_xterm_h
/* *INDENT-OFF* */

#ifdef HAVE_CONFIG_H
#include <xtermcfg.h>
#endif

#ifndef GCC_PRINTFLIKE
#define GCC_PRINTFLIKE(f,n)	/* nothing */
#endif

#ifndef GCC_UNUSED
#define GCC_UNUSED		/* nothing */
#endif

#ifndef GCC_NORETURN
#define GCC_NORETURN		/* nothing */
#endif

#if defined(__GNUC__) && defined(_FORTIFY_SOURCE)
#define USE_IGNORE_RC
#define IGNORE_RC(func) ignore_unused = (int) func
#else
#define IGNORE_RC(func) (void) func
#endif /* gcc workarounds */

#include <X11/Xos.h>

#ifndef HAVE_CONFIG_H

#define HAVE_LIB_XAW 1

#ifdef CSRG_BASED
/* Get definition of BSD */
#include <sys/param.h>
#endif

#ifndef DFT_TERMTYPE
#define DFT_TERMTYPE "xterm"
#endif

#ifndef X_NOT_POSIX
#define HAVE_WAITPID 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_UNISTD_H 1
#endif

#define HAVE_STDLIB_H 1

#if defined(sun)
/* errno is declared in <errno.h> */
#else
#define DECL_ERRNO 1
#endif

#undef DECL_PTSNAME		/* ptsname() is normally in stdlib.h */

#ifndef NOPUTENV
#define HAVE_PUTENV 1
#endif

#if defined(CSRG_BASED) || defined(__GNU__)
#define USE_POSIX_TERMIOS 1
#endif

#ifdef __NetBSD__
#if __NetBSD_Version__ >= 106030000	/* 1.6C */
#define BSD_UTMPX 1
#define ut_xtime ut_tv.tv_sec
#endif
#endif

#if defined(hpux) && !defined(__hpux)
#define __hpux 1		/* HPUX 11.0 does not define this */
#endif

#if !defined(__SCO__) && (defined(SCO) || defined(sco) || defined(SCO325))
#define __SCO__ 1
#endif

#ifdef USE_POSIX_TERMIOS
#define HAVE_TERMIOS_H 1
#define HAVE_TCGETATTR 1
#endif

#if defined(__UNIXOS2__) || defined(__SCO__) || defined(__UNIXWARE__)
#define USE_TERMCAP 1
#endif

#if defined(UTMP)
#define HAVE_UTMP 1
#endif

#if (defined(__MVS__) || defined(SVR4) || defined(__SCO__) || defined(BSD_UTMPX)) && !defined(__CYGWIN__)
#define UTMPX_FOR_UTMP 1
#endif

#if !defined(ISC) && !defined(__QNX__)
#define HAVE_UTMP_UT_HOST 1
#endif

#if defined(UTMPX_FOR_UTMP) && !(defined(__MVS__) || defined(__hpux))
#define HAVE_UTMP_UT_SESSION 1
#endif

#if !(defined(linux) && (!defined(__GLIBC__) || (__GLIBC__ < 2))) && !defined(SVR4)
#define ut_xstatus ut_exit.e_exit
#endif

#if defined(SVR4) || defined(__SCO__) || defined(BSD_UTMPX) || (defined(linux) && defined(__GLIBC__) && (__GLIBC__ >= 2) && !(defined(__powerpc__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 0)))
#define HAVE_UTMP_UT_XTIME 1
#endif

#if defined(linux) || defined(__CYGWIN__)
#define USE_LASTLOG
#define HAVE_LASTLOG_H
#define USE_STRUCT_LASTLOG
#elif defined(BSD) && (BSD >= 199103)
#ifdef BSD_UTMPX
#define USE_LASTLOGX
#else
#define USE_LASTLOG
#define USE_STRUCT_LASTLOG
#endif
#endif

#if defined(__OpenBSD__)
#define DEFDELETE_DEL True
#define DEF_BACKARO_ERASE True
#define DEF_INITIAL_ERASE True
#endif

#if defined(__SCO__) || defined(__UNIXWARE__)
#define DEFDELETE_DEL True
#define OPT_SCO_FUNC_KEYS 1
#endif

#if defined(__SCO__) || defined(SVR4) || defined(_POSIX_SOURCE) || defined(__QNX__) || defined(__hpux) || (defined(BSD) && (BSD >= 199103)) || defined(__CYGWIN__)
#define USE_POSIX_WAIT
#endif

#if defined(AIXV3) || defined(CRAY) || defined(__SCO__) || defined(SVR4) || (defined(SYSV) && defined(i386)) || defined(__MVS__) || defined(__hpux) || defined(__osf__) || defined(linux) || defined(macII) || defined(BSD_UTMPX)
#define USE_SYSV_UTMP
#endif

#if defined(__GNU__) || defined(__MVS__) || defined(__osf__)
#define USE_TTY_GROUP
#endif

#if defined(__CYGWIN__)
#define HAVE_NCURSES_TERM_H 1
#endif

#ifdef __osf__
#define TTY_GROUP_NAME "terminal"
#endif

#if defined(__MVS__)
#undef ut_xstatus
#define ut_name ut_user
#define ut_xstatus ut_exit.ut_e_exit
#define ut_xtime ut_tv.tv_sec
#endif

#if defined(ut_xstatus)
#define HAVE_UTMP_UT_XSTATUS 1
#endif

#if defined(XKB)
#define HAVE_XKB_BELL_EXT 1
#endif

#if (defined(SVR4) && !defined(__CYGWIN__)) || defined(linux) || (defined(BSD) && (BSD >= 199103))
#define HAVE_POSIX_SAVED_IDS
#endif

#endif /* HAVE_CONFIG_H */

#ifndef HAVE_X11_DECKEYSYM_H
#define HAVE_X11_DECKEYSYM_H 1
#endif

#ifndef HAVE_X11_SUNKEYSYM_H
#define HAVE_X11_SUNKEYSYM_H 1
#endif

#ifndef HAVE_X11_XF86KEYSYM_H
#define HAVE_X11_XF86KEYSYM_H 0
#endif

/***====================================================================***/

/* if compiling with gcc -ansi -pedantic, we must fix POSIX definitions */
#if defined(SVR4) && defined(sun)
#ifndef __EXTENSIONS__
#define __EXTENSIONS__ 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 1
#endif
#endif

/***====================================================================***/

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
extern char *calloc();
extern char *getenv();
extern char *malloc();
extern char *realloc();
extern void exit();
extern void free();
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <errno.h>
#if defined(DECL_ERRNO) && !defined(errno)
extern int errno;
#endif

/*
 * FIXME:  Toggling logging from xterm hangs under Linux 2.0.29 with libc5 if
 * we use 'waitpid()', while 'wait()' seems to work properly.
 */
#ifdef linux
#undef HAVE_WAITPID
#endif

#ifndef OPT_WIDE_CHARS
#define OPT_WIDE_CHARS 0
#endif

#if OPT_WIDE_CHARS
#define HIDDEN_CHAR 0xffff
#endif

/***====================================================================***/

#include <proto.h>
#include <ptyx.h>

#if (XtSpecificationRelease >= 6) && !defined(NO_XPOLL_H) && !defined(sun)
#include <X11/Xpoll.h>
#define USE_XPOLL_H 1
#else
#define Select(n,r,w,e,t) select(n,(fd_set*)r,(fd_set*)w,(fd_set*)e,(struct timeval *)t)
#define XFD_COPYSET(src,dst) memcpy((dst)->fds_bits, (src)->fds_bits, sizeof(fd_set))
#if defined(__MVS__) && !defined(TIME_WITH_SYS_TIME)
#define TIME_WITH_SYS_TIME
#endif
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

/* these may be needed for sig_atomic_t */
#include <sys/types.h>
#include <signal.h>

#ifdef USE_SYS_SELECT_H

#if defined(USE_XPOLL_H) && defined(AIXV3) && defined(NFDBITS)
#undef NFDBITS			/* conflict between X11/Xpoll.h and sys/select.h */
#endif

#include <sys/select.h>

#endif /* USE_SYS_SELECT_H */

#include <setjmp.h>

#if defined(__UNIXOS2__) && !defined(XTERM_MAIN)
#define environ gblenvp		/* circumvent a bug */
#endif

#if !defined(VMS) && !(defined(linux) && defined(__USE_GNU)) && !defined(__hpux) && !defined(_ALL_SOURCE) && !defined(__osf__)
extern char **environ;
#endif

#define XK_Fn(n)	(XK_F1 + (n) - 1)

#define Maybe		2

/***====================================================================***/

#define XtNallowC1Printable	"allowC1Printable"
#define XtNallowColorOps	"allowColorOps"
#define XtNallowFontOps		"allowFontOps"
#define XtNallowScrollLock	"allowScrollLock"
#define XtNallowSendEvents	"allowSendEvents"
#define XtNallowTcapOps		"allowTcapOps"
#define XtNallowTitleOps	"allowTitleOps"
#define XtNallowWindowOps	"allowWindowOps"
#define XtNaltIsNotMeta		"altIsNotMeta"
#define XtNaltSendsEscape	"altSendsEscape"
#define XtNalwaysBoldMode	"alwaysBoldMode"
#define XtNalwaysHighlight	"alwaysHighlight"
#define XtNalwaysUseMods	"alwaysUseMods"
#define XtNanswerbackString	"answerbackString"
#define XtNappcursorDefault	"appcursorDefault"
#define XtNappkeypadDefault	"appkeypadDefault"
#define XtNautoWrap		"autoWrap"
#define XtNawaitInput		"awaitInput"
#define XtNbackarrowKey		"backarrowKey"
#define XtNbellIsUrgent		"bellIsUrgent"
#define XtNbellOnReset		"bellOnReset"
#define XtNbellSuppressTime	"bellSuppressTime"
#define XtNboldColors		"boldColors"
#define XtNboldFont		"boldFont"
#define XtNboldMode		"boldMode"
#define XtNbrokenLinuxOSC	"brokenLinuxOSC"
#define XtNbrokenSelections	"brokenSelections"
#define XtNbrokenStringTerm	"brokenStringTerm"
#define XtNc132			"c132"
#define XtNcacheDoublesize	"cacheDoublesize"
#define XtNcharClass		"charClass"
#define XtNcjkWidth		"cjkWidth"
#define XtNcolorAttrMode	"colorAttrMode"
#define XtNcolorBDMode		"colorBDMode"
#define XtNcolorBLMode		"colorBLMode"
#define XtNcolorMode		"colorMode"
#define XtNcolorRVMode		"colorRVMode"
#define XtNcolorULMode		"colorULMode"
#define XtNcombiningChars	"combiningChars"
#define XtNctrlFKeys		"ctrlFKeys"
#define XtNcurses		"curses"
#define XtNcursorBlink		"cursorBlink"
#define XtNcursorColor		"cursorColor"
#define XtNcursorOffTime	"cursorOffTime"
#define XtNcursorOnTime		"cursorOnTime"
#define XtNcursorUnderline	"cursorUnderLine"
#define XtNcutNewline		"cutNewline"
#define XtNcutToBeginningOfLine	"cutToBeginningOfLine"
#define XtNdecTerminalID	"decTerminalID"
#define XtNdefaultString	"defaultString"
#define XtNdeleteIsDEL		"deleteIsDEL"
#define XtNdisallowedColorOps	"disallowedColorOps"
#define XtNdisallowedFontOps	"disallowedFontOps"
#define XtNdisallowedTcapOps	"disallowedTcapOps"
#define XtNdisallowedWindowOps	"disallowedWindowOps"
#define XtNdynamicColors	"dynamicColors"
#define XtNeightBitControl	"eightBitControl"
#define XtNeightBitInput	"eightBitInput"
#define XtNeightBitOutput	"eightBitOutput"
#define XtNeightBitSelectTypes	"eightBitSelectTypes"
#define XtNfaceName		"faceName"
#define XtNfaceNameDoublesize	"faceNameDoublesize"
#define XtNfaceSize		"faceSize"
#define XtNfastScroll		"fastScroll"
#define XtNfont1		"font1"
#define XtNfont2		"font2"
#define XtNfont3		"font3"
#define XtNfont4		"font4"
#define XtNfont5		"font5"
#define XtNfont6		"font6"
#define XtNfontDoublesize	"fontDoublesize"
#define XtNfontStyle		"fontStyle"
#define XtNfontWarnings		"fontWarnings"
#define XtNforceBoxChars	"forceBoxChars"
#define XtNforcePackedFont	"forcePackedFont"
#define XtNformatOtherKeys	"formatOtherKeys"
#define XtNfreeBoldBox		"freeBoldBox"
#define XtNfullscreen		"fullscreen"
#define XtNhighlightColor	"highlightColor"
#define XtNhighlightColorMode	"highlightColorMode"
#define XtNhighlightReverse	"highlightReverse"
#define XtNhighlightSelection	"highlightSelection"
#define XtNhighlightTextColor	"highlightTextColor"
#define XtNhpLowerleftBugCompat	"hpLowerleftBugCompat"
#define XtNi18nSelections	"i18nSelections"
#define XtNinitialFont		"initialFont"
#define XtNinternalBorder	"internalBorder"
#define XtNitalicULMode		"italicULMode"
#define XtNjumpScroll		"jumpScroll"
#define XtNkeepSelection	"keepSelection"
#define XtNkeyboardDialect	"keyboardDialect"
#define XtNlimitResize		"limitResize"
#define XtNlocale		"locale"
#define XtNlocaleFilter		"localeFilter"
#define XtNlogFile		"logFile"
#define XtNlogInhibit		"logInhibit"
#define XtNlogging		"logging"
#define XtNloginShell		"loginShell"
#define XtNmarginBell		"marginBell"
#define XtNmaximized		"maximized"
#define XtNmenuBar		"menuBar"
#define XtNmenuHeight		"menuHeight"
#define XtNmetaSendsEscape	"metaSendsEscape"
#define XtNmkSamplePass		"mkSamplePass"
#define XtNmkSampleSize		"mkSampleSize"
#define XtNmkWidth		"mkWidth"
#define XtNmodifyCursorKeys	"modifyCursorKeys"
#define XtNmodifyFunctionKeys	"modifyFunctionKeys"
#define XtNmodifyKeypadKeys	"modifyKeypadKeys"
#define XtNmodifyOtherKeys	"modifyOtherKeys"
#define XtNmodifyStringKeys	"modifyStringKeys"
#define XtNmultiClickTime	"multiClickTime"
#define XtNmultiScroll		"multiScroll"
#define XtNnMarginBell		"nMarginBell"
#define XtNnumLock		"numLock"
#define XtNoldXtermFKeys	"oldXtermFKeys"
#define XtNpointerColor		"pointerColor"
#define XtNpointerColorBackground "pointerColorBackground"
#define XtNpointerMode		"pointerMode"
#define XtNpointerShape		"pointerShape"
#define XtNpopOnBell		"popOnBell"
#define XtNprintAttributes	"printAttributes"
#define XtNprinterAutoClose	"printerAutoClose"
#define XtNprinterCommand	"printerCommand"
#define XtNprinterControlMode	"printerControlMode"
#define XtNprinterExtent	"printerExtent"
#define XtNprinterFormFeed	"printerFormFeed"
#define XtNprinterNewLine	"printerNewLine"
#define XtNquietGrab		"quietGrab"
#define XtNrenderFont		"renderFont"
#define XtNresizeGravity	"resizeGravity"
#define XtNretryInputMethod     "retryInputMethod"
#define XtNreverseWrap		"reverseWrap"
#define XtNrightScrollBar	"rightScrollBar"
#define XtNsaveLines		"saveLines"
#define XtNscrollBar		"scrollBar"
#define XtNscrollBarBorder	"scrollBarBorder"
#define XtNscrollKey		"scrollKey"
#define XtNscrollLines		"scrollLines"
#define XtNscrollPos		"scrollPos"
#define XtNscrollTtyOutput	"scrollTtyOutput"
#define XtNselectToClipboard	"selectToClipboard"
#define XtNshiftFonts		"shiftFonts"
#define XtNshowBlinkAsBold	"showBlinkAsBold"
#define XtNshowMissingGlyphs	"showMissingGlyphs"
#define XtNshowWrapMarks	"showWrapMarks"
#define XtNsignalInhibit	"signalInhibit"
#define XtNtekGeometry		"tekGeometry"
#define XtNtekInhibit		"tekInhibit"
#define XtNtekSmall		"tekSmall"
#define XtNtekStartup		"tekStartup"
#define XtNtiXtraScroll		"tiXtraScroll"
#define XtNtiteInhibit		"titeInhibit"
#define XtNtitleModes		"titleModes"
#define XtNtoolBar		"toolBar"
#define XtNtrimSelection	"trimSelection"
#define XtNunderLine		"underLine"
#define XtNuseClipping		"useClipping"
#define XtNutf8			"utf8"
#define XtNutf8Latin1		"utf8Latin1"
#define XtNutf8SelectTypes	"utf8SelectTypes"
#define XtNutf8Title		"utf8Title"
#define XtNveryBoldColors	"veryBoldColors"
#define XtNvisualBell		"visualBell"
#define XtNvisualBellDelay	"visualBellDelay"
#define XtNvt100Graphics	"vt100Graphics"
#define XtNwideBoldFont		"wideBoldFont"
#define XtNwideChars		"wideChars"
#define XtNwideFont		"wideFont"
#define XtNximFont		"ximFont"
#define XtNxmcAttributes	"xmcAttributes"
#define XtNxmcGlitch		"xmcGlitch"
#define XtNxmcInline		"xmcInline"
#define XtNxmcMoveSGR		"xmcMoveSGR"

#define XtCAllowC1Printable	"AllowC1Printable"
#define XtCAllowColorOps	"AllowColorOps"
#define XtCAllowFontOps		"AllowFontOps"
#define XtCAllowScrollLock	"AllowScrollLock"
#define XtCAllowSendEvents	"AllowSendEvents"
#define XtCAllowTcapOps		"AllowTcapOps"
#define XtCAllowTitleOps	"AllowTitleOps"
#define XtCAllowWindowOps	"AllowWindowOps"
#define XtCAltIsNotMeta		"AltIsNotMeta"
#define XtCAltSendsEscape	"AltSendsEscape"
#define XtCAlwaysBoldMode	"AlwaysBoldMode"
#define XtCAlwaysHighlight	"AlwaysHighlight"
#define XtCAlwaysUseMods	"AlwaysUseMods"
#define XtCAnswerbackString	"AnswerbackString"
#define XtCAppcursorDefault	"AppcursorDefault"
#define XtCAppkeypadDefault	"AppkeypadDefault"
#define XtCAutoWrap		"AutoWrap"
#define XtCAwaitInput		"AwaitInput"
#define XtCBackarrowKey		"BackarrowKey"
#define XtCBellIsUrgent		"BellIsUrgent"
#define XtCBellOnReset		"BellOnReset"
#define XtCBellSuppressTime	"BellSuppressTime"
#define XtCBoldFont		"BoldFont"
#define XtCBoldMode		"BoldMode"
#define XtCBrokenLinuxOSC	"BrokenLinuxOSC"
#define XtCBrokenSelections	"BrokenSelections"
#define XtCBrokenStringTerm	"BrokenStringTerm"
#define XtCC132			"C132"
#define XtCCacheDoublesize	"CacheDoublesize"
#define XtCCharClass		"CharClass"
#define XtCCjkWidth		"CjkWidth"
#define XtCColorAttrMode	"ColorAttrMode"
#define XtCColorMode		"ColorMode"
#define XtCColumn		"Column"
#define XtCCombiningChars	"CombiningChars"
#define XtCCtrlFKeys		"CtrlFKeys"
#define XtCCurses		"Curses"
#define XtCCursorBlink		"CursorBlink"
#define XtCCursorOffTime	"CursorOffTime"
#define XtCCursorOnTime		"CursorOnTime"
#define XtCCursorUnderline	"CursorUnderLine"
#define XtCCutNewline		"CutNewline"
#define XtCCutToBeginningOfLine	"CutToBeginningOfLine"
#define XtCDecTerminalID	"DecTerminalID"
#define XtCDefaultString	"DefaultString"
#define XtCDeleteIsDEL		"DeleteIsDEL"
#define XtCDisallowedColorOps	"DisallowedColorOps"
#define XtCDisallowedFontOps	"DisallowedFontOps"
#define XtCDisallowedTcapOps	"DisallowedTcapOps"
#define XtCDisallowedWindowOps	"DisallowedWindowOps"
#define XtCDynamicColors	"DynamicColors"
#define XtCEightBitControl	"EightBitControl"
#define XtCEightBitInput	"EightBitInput"
#define XtCEightBitOutput	"EightBitOutput"
#define XtCEightBitSelectTypes	"EightBitSelectTypes"
#define XtCFaceName		"FaceName"
#define XtCFaceNameDoublesize	"FaceNameDoublesize"
#define XtCFaceSize		"FaceSize"
#define XtCFastScroll		"FastScroll"
#define XtCFont1		"Font1"
#define XtCFont2		"Font2"
#define XtCFont3		"Font3"
#define XtCFont4		"Font4"
#define XtCFont5		"Font5"
#define XtCFont6		"Font6"
#define XtCFontDoublesize	"FontDoublesize"
#define XtCFontStyle		"FontStyle"
#define XtCFontWarnings		"FontWarnings"
#define XtCForceBoxChars	"ForceBoxChars"
#define XtCForcePackedFont	"ForcePackedFont"
#define XtCFormatOtherKeys	"FormatOtherKeys"
#define XtCFreeBoldBox		"FreeBoldBox"
#define XtCFullscreen		"Fullscreen"
#define XtCHighlightColorMode	"HighlightColorMode"
#define XtCHighlightReverse	"HighlightReverse"
#define XtCHighlightSelection	"HighlightSelection"
#define XtCHpLowerleftBugCompat	"HpLowerleftBugCompat"
#define XtCI18nSelections	"I18nSelections"
#define XtCInitialFont		"InitialFont"
#define XtCJumpScroll		"JumpScroll"
#define XtCKeepSelection	"KeepSelection"
#define XtCKeyboardDialect	"KeyboardDialect"
#define XtCLimitResize		"LimitResize"
#define XtCLocale		"Locale"
#define XtCLocaleFilter		"LocaleFilter"
#define XtCLogInhibit		"LogInhibit"
#define XtCLogfile		"Logfile"
#define XtCLogging		"Logging"
#define XtCLoginShell		"LoginShell"
#define XtCMarginBell		"MarginBell"
#define XtCMaximized		"Maximized"
#define XtCMenuBar		"MenuBar"
#define XtCMenuHeight		"MenuHeight"
#define XtCMetaSendsEscape	"MetaSendsEscape"
#define XtCMkSamplePass 	"MkSamplePass"
#define XtCMkSampleSize 	"MkSampleSize"
#define XtCMkWidth		"MkWidth"
#define XtCModifyCursorKeys	"ModifyCursorKeys"
#define XtCModifyFunctionKeys	"ModifyFunctionKeys"
#define XtCModifyKeypadKeys	"ModifyKeypadKeys"
#define XtCModifyOtherKeys	"ModifyOtherKeys"
#define XtCModifyStringKeys	"ModifyStringKeys"
#define XtCMultiClickTime	"MultiClickTime"
#define XtCMultiScroll		"MultiScroll"
#define XtCNumLock		"NumLock"
#define XtCOldXtermFKeys	"OldXtermFKeys"
#define XtCPointerMode		"PointerMode"
#define XtCPopOnBell		"PopOnBell"
#define XtCPrintAttributes	"PrintAttributes"
#define XtCPrinterAutoClose	"PrinterAutoClose"
#define XtCPrinterCommand	"PrinterCommand"
#define XtCPrinterControlMode	"PrinterControlMode"
#define XtCPrinterExtent	"PrinterExtent"
#define XtCPrinterFormFeed	"PrinterFormFeed"
#define XtCPrinterNewLine	"PrinterNewLine"
#define XtCQuietGrab		"QuietGrab"
#define XtCRenderFont		"RenderFont"
#define XtCResizeGravity	"ResizeGravity"
#define XtCRetryInputMethod     "RetryInputMethod"
#define XtCReverseWrap		"ReverseWrap"
#define XtCRightScrollBar	"RightScrollBar"
#define XtCSaveLines		"SaveLines"
#define XtCScrollBar		"ScrollBar"
#define XtCScrollBarBorder	"ScrollBarBorder"
#define XtCScrollCond		"ScrollCond"
#define XtCScrollLines		"ScrollLines"
#define XtCScrollPos		"ScrollPos"
#define XtCSelectToClipboard	"SelectToClipboard"
#define XtCShiftFonts		"ShiftFonts"
#define XtCShowBlinkAsBold	"ShowBlinkAsBold"
#define XtCShowMissingGlyphs	"ShowMissingGlyphs"
#define XtCShowWrapMarks	"ShowWrapMarks"
#define XtCSignalInhibit	"SignalInhibit"
#define XtCTekInhibit		"TekInhibit"
#define XtCTekSmall		"TekSmall"
#define XtCTekStartup		"TekStartup"
#define XtCTiXtraScroll		"TiXtraScroll"
#define XtCTiteInhibit		"TiteInhibit"
#define XtCTitleModes		"TitleModes"
#define XtCToolBar		"ToolBar"
#define XtCTrimSelection	"TrimSelection"
#define XtCUnderLine		"UnderLine"
#define XtCUseClipping		"UseClipping"
#define XtCUtf8			"Utf8"
#define XtCUtf8Latin1		"Utf8Latin1"
#define XtCUtf8SelectTypes	"Utf8SelectTypes"
#define XtCUtf8Title		"Utf8Title"
#define XtCVT100Graphics	"VT100Graphics"
#define XtCVeryBoldColors	"VeryBoldColors"
#define XtCVisualBell		"VisualBell"
#define XtCVisualBellDelay	"VisualBellDelay"
#define XtCWideBoldFont		"WideBoldFont"
#define XtCWideChars		"WideChars"
#define XtCWideFont		"WideFont"
#define XtCXimFont		"XimFont"
#define XtCXmcAttributes	"XmcAttributes"
#define XtCXmcGlitch		"XmcGlitch"
#define XtCXmcInline		"XmcInline"
#define XtCXmcMoveSGR		"XmcMoveSGR"

#if defined(NO_ACTIVE_ICON) && !defined(XtNgeometry)
#define XtNgeometry		"geometry"
#define XtCGeometry		"Geometry"
#endif

#if OPT_COLOR_CLASS
#define XtCCursorColor		"CursorColor"
#define XtCPointerColor		"PointerColor"
#define XtCHighlightColor	"HighlightColor"
#define XtCHighlightTextColor	"HighlightTextColor"
#else
#define XtCCursorColor		XtCForeground
#define XtCPointerColor		XtCForeground
#define XtCHighlightColor	XtCForeground
#define XtCHighlightTextColor	XtCBackground
#endif

/***====================================================================***/

#ifdef __cplusplus
extern "C" {
#endif

struct XTERM_RESOURCE;

/* Tekproc.c */
#if OPT_TEK4014
extern TekWidget getTekWidget(Widget /* w */);
extern int TekGetFontSize (const char * /* param */);
extern int TekInit (void);
extern int TekPtyData(void);
extern void ChangeTekColors (TekWidget /* tw */, TScreen * /* screen */, ScrnColors * /* pNew */);
extern void HandleGINInput             PROTO_XT_ACTIONS_ARGS;
extern void TCursorToggle (TekWidget /* tw */, int /* toggle */);
extern void TekCopy (TekWidget /* tw */);
extern void TekEnqMouse (TekWidget /* tw */, int /* c */);
extern void TekExpose (Widget  /* w */, XEvent * /* event */, Region  /* region */);
extern void TekGINoff (TekWidget /* tw */);
extern void TekRefresh (TekWidget /* tw */);
extern void TekRepaint (TekWidget /* xw */);
extern void TekReverseVideo (TekWidget /* tw */);
extern void TekRun (void);
extern void TekSetFontSize (TekWidget /* tw */, Bool /* fromMenu */, int  /* newitem */);
extern void TekSimulatePageButton (TekWidget /* tw */, Bool /* reset */);
#endif

/* button.c */
#define	MotionOff( s, t ) if (!(screen->hide_pointer)) {		\
	    (s)->event_mask |= ButtonMotionMask;			\
	    (s)->event_mask &= ~PointerMotionMask;			\
	    XSelectInput(XtDisplay((t)), XtWindow((t)), (long) (s)->event_mask); }

#define	MotionOn( s, t ) {						\
	    (s)->event_mask &= ~ButtonMotionMask;			\
	    (s)->event_mask |= PointerMotionMask;			\
	    XSelectInput(XtDisplay((t)), XtWindow((t)), (long) (s)->event_mask); }

extern Bool SendMousePosition (XtermWidget  /* w */, XEvent*  /* event */);
extern void DiredButton                PROTO_XT_ACTIONS_ARGS;
extern void DisownSelection (XtermWidget  /* termw */);
extern void UnhiliteSelection (XtermWidget  /* termw */);
extern void HandleCopySelection        PROTO_XT_ACTIONS_ARGS;
extern void HandleInsertSelection      PROTO_XT_ACTIONS_ARGS;
extern void HandleKeyboardSelectEnd    PROTO_XT_ACTIONS_ARGS;
extern void HandleKeyboardSelectExtend PROTO_XT_ACTIONS_ARGS;
extern void HandleKeyboardSelectStart  PROTO_XT_ACTIONS_ARGS;
extern void HandleKeyboardStartExtend  PROTO_XT_ACTIONS_ARGS;
extern void HandleSelectEnd            PROTO_XT_ACTIONS_ARGS;
extern void HandleSelectExtend         PROTO_XT_ACTIONS_ARGS;
extern void HandleSelectSet            PROTO_XT_ACTIONS_ARGS;
extern void HandleSelectStart          PROTO_XT_ACTIONS_ARGS;
extern void HandleStartExtend          PROTO_XT_ACTIONS_ARGS;
extern void ResizeSelection (TScreen * /* screen */, int  /* rows */, int  /* cols */);
extern void ScrollSelection (TScreen * /* screen */, int  /* amount */,  Bool /* always */);
extern void TrackMouse (XtermWidget /* xw */, int /* func */, CELL * /* start */, int /* firstrow */, int /* lastrow */);
extern void ViButton                   PROTO_XT_ACTIONS_ARGS;

extern int xtermUtf8ToTextList(XtermWidget /* xw */, XTextProperty * /* text_prop */, char *** /* text_list */, int * /* text_list_count */);

#if OPT_DEC_LOCATOR
extern void GetLocatorPosition (XtermWidget  /* w */);
extern void InitLocatorFilter (XtermWidget  /* w */);
#endif	/* OPT_DEC_LOCATOR */

#if OPT_FOCUS_EVENT
extern void SendFocusButton(XtermWidget /* xw */, XFocusChangeEvent * /* event */);
#else
#define SendFocusBotton(xw, event) /* nothing */
#endif

#if OPT_PASTE64
extern void AppendToSelectionBuffer (TScreen * /* screen */, unsigned  /* c */);
extern void ClearSelectionBuffer (TScreen * /* screen */);
extern void CompleteSelection (XtermWidget  /* xw */, String * /* args */, Cardinal  /* len */);
extern void xtermGetSelection (Widget  /* w */, Time  /* ev_time */, String * /* params */, Cardinal  /* num_params */, Atom * /* targets */);
#endif

#if OPT_READLINE
extern void ReadLineButton             PROTO_XT_ACTIONS_ARGS;
#endif

#if OPT_WIDE_CHARS
extern Bool iswide(int  /* i */);
#define WideCells(n) (((IChar)(n) >= first_widechar) ? my_wcwidth((wchar_t) (n)) : 1)
#define isWide(n)    (((IChar)(n) >= first_widechar) && iswide(n))
#else
#define WideCells(n) 1
#endif

/* cachedCgs.c */
extern CgsEnum getCgsId(XtermWidget /*xw*/, VTwin * /*cgsWin*/, GC /*gc*/);
extern GC freeCgs(XtermWidget /*xw*/, VTwin * /*cgsWin*/, CgsEnum /*cgsId*/);
extern GC getCgsGC(XtermWidget /*xw*/, VTwin * /*cgsWin*/, CgsEnum /*cgsId*/);
extern Pixel getCgsBack(XtermWidget /*xw*/, VTwin * /*cgsWin*/, GC /*gc*/);
extern Pixel getCgsFore(XtermWidget /*xw*/, VTwin * /*cgsWin*/, GC /*gc*/);
extern XTermFonts * getCgsFont(XtermWidget /*xw*/, VTwin * /*cgsWin*/, GC /*gc*/);
extern void clrCgsFonts(XtermWidget /*xw*/, VTwin * /*cgsWin*/, XTermFonts * /*font*/);
extern void copyCgs(XtermWidget /*xw*/, VTwin * /*cgsWin*/, CgsEnum /*dstCgsId*/, CgsEnum /*srcCgsId*/);
extern void redoCgs(XtermWidget /*xw*/, Pixel /*fg*/, Pixel /*bg*/, CgsEnum /*cgsId*/);
extern void setCgsBack(XtermWidget /*xw*/, VTwin * /*cgsWin*/, CgsEnum /*cgsId*/, Pixel /*bg*/);
extern void setCgsCSet(XtermWidget /*xw*/, VTwin * /*cgsWin*/, CgsEnum /*cgsId*/, unsigned /*cset*/);
extern void setCgsFont(XtermWidget /*xw*/, VTwin * /*cgsWin*/, CgsEnum /*cgsId*/, XTermFonts * /*font*/);
extern void setCgsFore(XtermWidget /*xw*/, VTwin * /*cgsWin*/, CgsEnum /*cgsId*/, Pixel /*fg*/);
extern void swapCgs(XtermWidget /*xw*/, VTwin * /*cgsWin*/, CgsEnum /*dstCgsId*/, CgsEnum /*srcCgsId*/);

#ifdef NO_LEAKS
extern void noleaks_cachedCgs (XtermWidget /* xw */);
#endif

/* charproc.c */
extern Bool CheckBufPtrs (TScreen * /* screen */);
extern Bool set_cursor_gcs (XtermWidget /* xw */);
extern int VTInit (XtermWidget /* xw */);
extern void FindFontSelection (XtermWidget /* xw */, const char * /* atom_name */, Bool  /* justprobe */);
extern void HideCursor (void);
extern void RestartBlinking(TScreen * /* screen */);
extern void ShowCursor (void);
extern void SwitchBufPtrs (TScreen * /* screen */, int /* toBuf */);
extern void ToggleAlternate (XtermWidget /* xw */);
extern void VTInitTranslations (void);
extern void VTReset (XtermWidget /* xw */, int /* full */, int /* saved */);
extern void VTRun (XtermWidget /* xw */);
extern void dotext (XtermWidget /* xw */, int  /* charset */, IChar * /* buf */, Cardinal  /* len */);
extern void releaseCursorGCs(XtermWidget /*xw*/);
extern void releaseWindowGCs(XtermWidget /*xw*/, VTwin * /*win*/);
extern void resetCharsets (TScreen * /* screen */);
extern void set_max_col(TScreen *  /* screen */, int  /* cols */);
extern void set_max_row(TScreen *  /* screen */, int  /* rows */);
extern void set_tb_margins (TScreen * /* screen */, int  /* top */, int  /* bottom */);
extern void unparse_end (XtermWidget /* xw */);
extern void unparseputc (XtermWidget /* xw */, int  /* c */);
extern void unparseputc1 (XtermWidget /* xw */, int  /* c */);
extern void unparseputn (XtermWidget /* xw */, unsigned /* n */);
extern void unparseputs (XtermWidget /* xw */, const char * /* s */);
extern void unparseseq (XtermWidget /* xw */, ANSI * /* ap */);
extern void v_write (int  /* f */, const Char * /* d */, unsigned  /* len */);
extern void xtermAddInput(Widget  /* w */);

#if OPT_BLINK_CURS
extern void ToggleCursorBlink(TScreen * /* screen */);
#endif

#if OPT_ISO_COLORS
extern void SGR_Background (XtermWidget /* xw */, int  /* color */);
extern void SGR_Foreground (XtermWidget /* xw */, int  /* color */);
#endif

#ifdef NO_LEAKS
extern void noleaks_charproc (void);
#endif

/* charsets.c */
extern unsigned xtermCharSetIn (unsigned  /* code */, int  /* charset */);
extern int xtermCharSetOut (XtermWidget /* xw */, IChar * /* buf */, IChar * /* ptr */, int  /* charset */);

/* cursor.c */
extern void AdjustSavedCursor (XtermWidget /* xw */, int /* adjust */);
extern void CarriageReturn (TScreen * /* screen */);
extern void CursorBack (XtermWidget /* xw */, int   /* n */);
extern void CursorDown (TScreen * /* screen */, int   /* n */);
extern void CursorForward (TScreen * /* screen */, int   /* n */);
extern void CursorNextLine (TScreen * /* screen */, int  /* count */);
extern void CursorPrevLine (TScreen * /* screen */, int  /* count */);
extern void CursorRestore (XtermWidget  /* xw */);
extern void CursorSave (XtermWidget  /* xw */);
extern void CursorSet (TScreen * /* screen */, int  /* row */, int  /* col */, unsigned  /* flags */);
extern void CursorUp (TScreen * /* screen */, int   /* n */);
extern void RevIndex (XtermWidget /* xw */, int  /* amount */);
extern void xtermIndex (XtermWidget /* xw */, int  /* amount */);

#if OPT_TRACE
extern int set_cur_col(TScreen * /* screen */, int  /* value */);
extern int set_cur_row(TScreen * /* screen */, int  /* value */);
#else
#define set_cur_col(screen, value) screen->cur_col = value
#define set_cur_row(screen, value) screen->cur_row = value
#endif

/* doublechr.c */
extern void xterm_DECDHL (XtermWidget /* xw */, Bool  /* top */);
extern void xterm_DECSWL (XtermWidget /* xw */);
extern void xterm_DECDWL (XtermWidget /* xw */);
#if OPT_DEC_CHRSET
extern int xterm_Double_index(XtermWidget /* xw */, unsigned  /* chrset */, unsigned  /* flags */);
extern GC xterm_DoubleGC(XtermWidget /* xw */, unsigned  /* chrset */, unsigned  /* flags */, GC  /* old_gc */, int * /* inxp */);
#endif

/* input.c */
extern unsigned xtermParamToState (XtermWidget /* xw */, unsigned /* param */);
extern unsigned xtermStateToParam (XtermWidget /* xw */, unsigned /* state */);
extern Bool xtermDeleteIsDEL (XtermWidget /* xw */);
extern void Input (XtermWidget /* xw */, XKeyEvent */* event */, Bool /* eightbit */);
extern void StringInput (XtermWidget /* xw */, const Char * /* string */, size_t  /* nbytes */);

#if OPT_NUM_LOCK
extern void VTInitModifiers(XtermWidget /* xw */);
#endif

/* linedata.c */
extern LineData *getLineData(TScreen * /* screen */, int /* row */);
extern void copyLineData(LineData * /* dst */, LineData * /* src */);
extern void initLineData(XtermWidget /* xw */);

extern CellData *newCellData(XtermWidget /* xw */, Cardinal /* count */);
extern void saveCellData(TScreen * /* screen */, CellData * /* data */, Cardinal /* cell */, LineData * /* ld */, int /* column */);
extern void restoreCellData(TScreen * /* screen */, CellData * /* data */, Cardinal /* cell */, LineData * /* ld */, int /* column */);

/* main.c */
#ifndef __UNIXOS2__
#define ENVP_ARG /**/
#else
#define ENVP_ARG , char ** /* envp */
#endif

extern int main (int  /* argc */, char ** /* argv */ ENVP_ARG);
extern int GetBytesAvailable (int  /* fd */);
extern int kill_process_group (int  /* pid */, int  /* sig */);
extern int nonblocking_wait (void);

#if OPT_PTY_HANDSHAKE
extern void first_map_occurred (void);
#else
#define first_map_occurred() /* nothing */
#endif

#ifdef SIGNAL_T
extern SIGNAL_T Exit (int  /* n */);
#endif

#ifndef SIG_ATOMIC_T
#define SIG_ATOMIC_T int
#endif

#if OPT_WIDE_CHARS
extern unsigned first_widechar;
extern int (*my_wcwidth)(wchar_t);
#endif

/* menu.c */
extern void do_hangup          PROTO_XT_CALLBACK_ARGS;
extern void repairSizeHints    (void);
extern void show_8bit_control  (Bool  /* value */);

/* misc.c */
extern Bool AllocateTermColor(XtermWidget, ScrnColors *, int, const char *, Bool);
extern Cursor make_colored_cursor (unsigned /* cursorindex */, unsigned long  /* fg */, unsigned long  /* bg */);
extern OptionHelp * sortedOpts(OptionHelp *, XrmOptionDescRec *, Cardinal);
extern Window WMFrameWindow(XtermWidget /* termw */);
extern XrmOptionDescRec * sortedOptDescs(XrmOptionDescRec *, Cardinal);
extern XtermWidget getXtermWidget(Widget /* w */);
extern char *udk_lookup (int /* keycode */, int * /* len */);
extern char *xtermEnvEncoding (void);
extern char *xtermEnvLocale (void);
extern char *xtermFindShell(char * /* leaf */, Bool  /* warning */);
extern char *xtermVersion(void);
extern const char *SysErrorMsg (int /* n */);
extern const char *SysReasonMsg (int /* n */);
extern int ResetAnsiColorRequest(XtermWidget, char *, int);
extern int XStrCmp (char * /* s1 */, char * /* s2 */);
extern int creat_as (uid_t  /* uid */, gid_t  /* gid */, Bool  /* append */, char * /* pathname */, int  /* mode */);
extern int open_userfile (uid_t  /* uid */, gid_t  /* gid */, char * /* path */, Bool  /* append */);
extern int xerror (Display * /* d */, XErrorEvent * /* ev */);
extern int xioerror (Display * /* dpy */);
extern int xtermResetIds(TScreen *  /* screen */);
extern void Bell (XtermWidget /* xw */, int  /* which */, int  /* percent */);
extern void ChangeIconName (XtermWidget /* xw */, char * /* name */);
extern void ChangeTitle (XtermWidget /* xw */, char * /* name */);
extern void ChangeXprop (char * /* name */);
extern void Cleanup (int  /* code */);
extern void HandleBellPropertyChange   PROTO_XT_EV_HANDLER_ARGS;
extern void HandleEightBitKeyPressed   PROTO_XT_ACTIONS_ARGS;
extern void HandleEnterWindow          PROTO_XT_EV_HANDLER_ARGS;
extern void HandleFocusChange          PROTO_XT_EV_HANDLER_ARGS;
extern void HandleInterpret            PROTO_XT_ACTIONS_ARGS;
extern void HandleKeyPressed           PROTO_XT_ACTIONS_ARGS;
extern void HandleLeaveWindow          PROTO_XT_EV_HANDLER_ARGS;
extern void HandleSpawnTerminal        PROTO_XT_ACTIONS_ARGS;
extern void HandleStringEvent          PROTO_XT_ACTIONS_ARGS;
extern void Panic (const char * /* s */, int  /* a */);
extern void Redraw (void);
extern void ReverseOldColors (void);
extern void SysError (int /* i */) GCC_NORETURN;
extern void VisualBell (void);
extern void do_dcs (XtermWidget /* xw */, Char * /* buf */, size_t  /* len */);
extern void do_decrpm (XtermWidget /* xw */, int /* nparam */, int *  /* params */);
extern void do_osc (XtermWidget /* xw */, Char * /* buf */, size_t  /* len */, int  /* final */);
extern void do_rpm (XtermWidget /* xw */, int /* nparam */, int *  /* params */);
extern void do_xevents (void);
extern void end_tek_mode (void);
extern void end_vt_mode (void);
extern void hide_tek_window (void);
extern void hide_vt_window (void);
extern void reset_decudk (void);
extern void set_tek_visibility (Bool  /* on */);
extern void set_vt_visibility (Bool  /* on */);
extern void switch_modes (Bool  /* tovt */);
extern void timestamp_filename(char * /* dst */, const char * /* src */);
extern void xevents (void);
extern void xt_error (String  /* message */);
extern void xtermBell(XtermWidget /* xw */, int /* which */, int /* percent */);
extern void xtermCopyEnv (char ** /* oldenv */);
extern void xtermDisplayCursor (XtermWidget /* xw */);
extern void xtermSetenv (const char * /* var */, const char * /* value */);
extern void xtermShowPointer (XtermWidget /* xw */, Bool /* enable */);

#if OPT_DABBREV
extern void HandleDabbrevExpand        PROTO_XT_ACTIONS_ARGS;
#endif

#if OPT_MAXIMIZE
extern int QueryMaximize (XtermWidget  /* termw */, unsigned * /* width */, unsigned * /* height */);
extern void HandleDeIconify            PROTO_XT_ACTIONS_ARGS;
extern void HandleIconify              PROTO_XT_ACTIONS_ARGS;
extern void HandleMaximize             PROTO_XT_ACTIONS_ARGS;
extern void HandleRestoreSize          PROTO_XT_ACTIONS_ARGS;
extern void RequestMaximize (XtermWidget  /* termw */, int  /* maximize */);
#endif

#if OPT_SCROLL_LOCK
extern void GetScrollLock (TScreen * /* screen */);
extern void HandleScrollLock           PROTO_XT_ACTIONS_ARGS;
extern void ShowScrollLock (TScreen * /* screen */, Bool /* enable */);
extern void SetScrollLock (TScreen * /* screen */, Bool /* enable */);
extern void xtermShowLED (TScreen * /* screen */, Cardinal /* led_number */, Bool /* enable */);
extern void xtermClearLEDs (TScreen * /* screen */);
#else
#define ShowScrollLock(screen, enable) /* nothing */
#define SetScrollLock(screen, enable) /* nothing */
#define GetScrollLock(screen) /* nothing */
#endif

#if OPT_WIDE_CHARS
extern Bool xtermEnvUTF8(void);
#else
#define xtermEnvUTF8() False
#endif

#ifdef ALLOWLOGGING
extern void StartLog (XtermWidget /* xw */);
extern void CloseLog (XtermWidget /* xw */);
extern void FlushLog (XtermWidget /* xw */);
#else
#define FlushLog(xw) /*nothing*/
#endif

/* print.c */
extern Bool xtermHasPrinter (XtermWidget /* xw */);
extern PrinterFlags *getPrinterFlags(XtermWidget /* xw */, String * /* params */, Cardinal * /* param_count */);
extern int xtermPrinterControl (XtermWidget /* xw */, int /* chr */);
extern void setPrinterControlMode (XtermWidget /* xw */, int /* mode */);
extern void xtermAutoPrint (XtermWidget /* xw */, unsigned /* chr */);
extern void xtermMediaControl (XtermWidget /* xw */, int  /* param */, int  /* private_seq */);
extern void xtermPrintScreen (XtermWidget /* xw */, Bool  /* use_DECPEX */, PrinterFlags * /* p */);
extern void xtermPrintEverything (XtermWidget /* xw */, PrinterFlags * /* p */);

/* ptydata.c */
#ifdef VMS
#define PtySelect int
#else
#define PtySelect fd_set
#endif

extern Bool decodeUtf8 (PtyData * /* data */);
extern int readPtyData (XtermWidget /* xw */, PtySelect * /* select_mask */, PtyData * /* data */);
extern void fillPtyData (XtermWidget /* xw */, PtyData * /* data */, const char * /* value */, int  /* length */);
extern void initPtyData (PtyData ** /* data */);
extern void trimPtyData (XtermWidget /* xw */, PtyData * /* data */);

#ifdef NO_LEAKS
extern void noleaks_ptydata ( void );
#endif

#if OPT_WIDE_CHARS
extern Char *convertToUTF8 (Char * /* lp */, unsigned  /* c */);
extern IChar nextPtyData (TScreen * /* screen */, PtyData * /* data */);
extern IChar skipPtyData (PtyData * /* data */);
extern PtyData * fakePtyData(PtyData * /* result */, Char * /* next */, Char * /* last */);
extern void switchPtyData (TScreen * /* screen */, int  /* f */);
extern void writePtyData (int  /* f */, IChar * /* d */, unsigned  /* len */);

#define morePtyData(screen,data) \
	(((data)->last > (data)->next) \
	 ? (((screen)->utf8_inparse && !(data)->utf_size) \
	    ? decodeUtf8(data) \
	    : True) \
	 : False)
#else
#define morePtyData(screen, data) ((data)->last > (data)->next)
#define nextPtyData(screen, data) (IChar) (*((data)->next++) & \
					   (screen->output_eight_bits \
					    ? 0xff \
					    : 0x7f))
#define writePtyData(f,d,len) v_write(f,d,len)
#endif

/* screen.c */
extern Bool non_blank_line (TScreen */* screen */, int  /* row */, int  /* col */, int  /* len */);
extern Char * allocScrnData(TScreen * /* screen */, unsigned /* nrow */, unsigned /* ncol */);
extern ScrnBuf allocScrnBuf (XtermWidget /* xw */, unsigned  /* nrow */, unsigned  /* ncol */, ScrnPtr * /* addr */);
extern ScrnBuf scrnHeadAddr (TScreen * /* screen */, ScrnBuf /* base */, unsigned /* offset */);
extern int ScreenResize (XtermWidget /* xw */, int  /* width */, int  /* height */, unsigned * /* flags */);
extern size_t ScrnPointers (TScreen * /* screen */, size_t  /* len */);
extern void ClearBufRows (XtermWidget /* xw */, int  /* first */, int  /* last */);
extern void ClearCells (XtermWidget /* xw */, int /* flags */, unsigned /* len */, int /* row */, int /* col */);
extern void FullScreen(XtermWidget /* xw */, Bool /* enabled */);
extern void ScrnAllocBuf (XtermWidget /* xw */);
extern void ScrnClearCells (XtermWidget /* xw */, int /* row */, int /* col */, unsigned /* len */);
extern void ScrnDeleteChar (XtermWidget /* xw */, unsigned  /* n */);
extern void ScrnDeleteLine (XtermWidget /* xw */, ScrnBuf  /* sb */, int  /* n */, int  /* last */, unsigned /* where */);
extern void ScrnDisownSelection (XtermWidget /* xw */);
extern void ScrnFillRectangle (XtermWidget /* xw */, XTermRect *,  int ,  unsigned /* flags */, Bool /* keepColors */);
extern void ScrnInsertChar (XtermWidget /* xw */, unsigned  /* n */);
extern void ScrnInsertLine (XtermWidget /* xw */, ScrnBuf /* sb */, int  /* last */, int  /* where */, unsigned  /* n */);
extern void ScrnRefresh (XtermWidget /* xw */, int  /* toprow */, int  /* leftcol */, int  /* nrows */, int  /* ncols */, Bool  /* force */);
extern void ScrnUpdate (XtermWidget /* xw */, int  /* toprow */, int  /* leftcol */, int  /* nrows */, int  /* ncols */, Bool  /* force */);
extern void ScrnWriteText (XtermWidget /* xw */, IChar * /* str */, unsigned  /* flags */, unsigned /* cur_fg_bg */, unsigned  /* length */);
extern void ShowWrapMarks (XtermWidget /* xw */, int /* row */, LineData * /* ld */);
extern void setupLineData (TScreen * /* screen */, ScrnBuf /* base */, Char * /* data */, unsigned /* nrow */, unsigned /* ncol */);
extern void xtermParseRect (XtermWidget /* xw */, int, int *, XTermRect *);

#if OPT_TRACE && OPT_TRACE_FLAGS
extern int  LineTstFlag(LineData /* ld */, int /* flag */);
extern void LineClrFlag(LineData /* ld */, int /* flag */);
extern void LineSetFlag(LineData /* ld */, int /* flag */);
#else

#define LineFlags(ld)         GetLineFlags(ld)

#define LineClrFlag(ld, flag) SetLineFlags(ld, (GetLineFlags(ld) & ~ (flag)))
#define LineSetFlag(ld, flag) SetLineFlags(ld, (GetLineFlags(ld) | (flag)))
#define LineTstFlag(ld, flag) ((GetLineFlags(ld) & flag) != 0)

#endif /* OPT_TRACE && OPT_TRACE_FLAGS */

#define LineClrBlinked(ld) LineClrFlag(ld, LINEBLINKED)
#define LineSetBlinked(ld) LineSetFlag(ld, LINEBLINKED)
#define LineTstBlinked(ld) LineTstFlag(ld, LINEBLINKED)

#define LineClrWrapped(ld) LineClrFlag(ld, LINEWRAPPED)
#define LineSetWrapped(ld) LineSetFlag(ld, LINEWRAPPED)
#define LineTstWrapped(ld) LineTstFlag(ld, LINEWRAPPED)

#define ScrnHaveSelection(screen) \
			((screen)->startH.row != (screen)->endH.row \
			|| (screen)->startH.col != (screen)->endH.col)

#define ScrnAreLinesInSelection(screen, first, last) \
	((last) >= (screen)->startH.row && (first) <= (screen)->endH.row)

#define ScrnIsLineInSelection(screen, line) \
	((line) >= (screen)->startH.row && (line) <= (screen)->endH.row)

#define ScrnHaveLineMargins(screen) \
			((screen)->top_marg != 0 \
			|| ((screen)->bot_marg != screen->max_row))

#define ScrnIsLineInMargins(screen, line) \
	((line) >= (screen)->top_marg && (line) <= (screen)->bot_marg)

#if OPT_DEC_RECTOPS
extern void ScrnCopyRectangle (XtermWidget /* xw */, XTermRect *, int, int *);
extern void ScrnMarkRectangle (XtermWidget /* xw */, XTermRect *, Bool, int, int *);
extern void ScrnWipeRectangle (XtermWidget /* xw */, XTermRect *);
#endif

#if OPT_WIDE_CHARS
extern void ChangeToWide(XtermWidget /* xw */);
#endif

/* scrollback.c */
extern LineData *getScrollback (TScreen * /* screen */, int /* row */);
extern LineData *addScrollback (TScreen * /* screen */);
extern void deleteScrollback (TScreen * /* screen */, int /* row */);

/* scrollbar.c */
extern void DoResizeScreen (XtermWidget /* xw */);
extern void HandleScrollBack           PROTO_XT_ACTIONS_ARGS;
extern void HandleScrollForward        PROTO_XT_ACTIONS_ARGS;
extern void ResizeScrollBar (XtermWidget  /* xw */);
extern void ScrollBarDrawThumb (Widget  /* scrollWidget */);
extern void ScrollBarOff (XtermWidget  /* xw */);
extern void ScrollBarOn (XtermWidget  /* xw */, Bool /* init */);
extern void ScrollBarReverseVideo (Widget  /* scrollWidget */);
extern void ToggleScrollBar (XtermWidget  /* xw */);
extern void WindowScroll (XtermWidget /* xw */, int  /* top */, Bool /* always */);

#ifdef SCROLLBAR_RIGHT
extern void updateRightScrollbar(XtermWidget  /* xw */);
#else
#define updateRightScrollbar(xw) /* nothing */
#endif

/* tabs.c */
extern Bool TabToNextStop (XtermWidget /* xw */);
extern Bool TabToPrevStop (XtermWidget /* xw */);
extern void TabClear (Tabs  /* tabs */, int  /* col */);
extern void TabReset (Tabs  /* tabs */);
extern void TabSet (Tabs  /* tabs */, int  /* col */);
extern void TabZonk (Tabs  /* tabs */);

/* util.c */
extern Boolean isDefaultBackground(const char * /* name */);
extern Boolean isDefaultForeground(const char * /* name */);
extern GC updatedXtermGC (XtermWidget /* xw */, unsigned  /* flags */, unsigned /* fg_bg */, Bool  /* hilite */);
extern int AddToRefresh (XtermWidget /* xw */);
extern int ClearInLine (XtermWidget /* xw */, int /* row */, int /* col */, unsigned /* len */);
extern int HandleExposure (XtermWidget /* xw */, XEvent * /* event */);
extern int drawXtermText (XtermWidget /* xw */, unsigned /* flags */, GC /* gc */, int /* x */, int /* y */, int /* chrset */, IChar * /* text */, Cardinal /* len */, int /* on_wide */);
extern int extendedBoolean(const char * /* value */, FlagList * /* table */, Cardinal /* limit */);
extern void ChangeColors (XtermWidget  /* xw */, ScrnColors * /* pNew */);
extern void ClearRight (XtermWidget /* xw */, int /* n */);
extern void ClearScreen (XtermWidget /* xw */);
extern void DeleteChar (XtermWidget /* xw */, unsigned /* n */);
extern void DeleteLine (XtermWidget /* xw */, int /* n */);
extern void FlushScroll (XtermWidget /* xw */);
extern void GetColors (XtermWidget  /* xw */, ScrnColors * /* pColors */);
extern void InsertChar (XtermWidget /* xw */, unsigned /* n */);
extern void InsertLine (XtermWidget /* xw */, int  /* n */);
extern void RevScroll (XtermWidget /* xw */, int  /* amount */);
extern void ReverseVideo (XtermWidget  /* termw */);
extern void WriteText (XtermWidget /* xw */, IChar * /* str */, Cardinal /* len */);
extern void decode_keyboard_type (XtermWidget /* xw */, struct XTERM_RESOURCE * /* rp */);
extern void decode_wcwidth (XtermWidget  /* xw */);
extern void do_erase_display (XtermWidget /* xw */, int  /* param */, int  /* mode */);
extern void do_erase_line (XtermWidget /* xw */, int  /* param */, int  /* mode */);
extern void getXtermSizeHints (XtermWidget /* xw */);
extern void init_keyboard_type (XtermWidget /* xw */, xtermKeyboardType /* type */, Bool  /* set */);
extern void recolor_cursor (TScreen * /* screen */, Cursor  /* cursor */, unsigned long  /* fg */, unsigned long  /* bg */);
extern void resetXtermGC (XtermWidget /* xw */, unsigned  /* flags */, Bool  /* hilite */);
extern void scrolling_copy_area (XtermWidget /* xw */, int  /* firstline */, int  /* nlines */, int  /* amount */);
extern void set_keyboard_type (XtermWidget /* xw */, xtermKeyboardType  /* type */, Bool  /* set */);
extern void toggle_keyboard_type (XtermWidget /* xw */, xtermKeyboardType  /* type */);
extern void update_keyboard_type (void);
extern void xtermClear (XtermWidget /* xw */);
extern void xtermRepaint (XtermWidget /* xw */);
extern void xtermScroll (XtermWidget /* xw */, int /* amount */);
extern void xtermSizeHints (XtermWidget  /* xw */, int /* scrollbarWidth */);

#if OPT_ISO_COLORS

extern unsigned extract_fg (XtermWidget /* xw */, unsigned  /* color */, unsigned  /* flags */);
extern unsigned extract_bg (XtermWidget /* xw */, unsigned  /* color */, unsigned  /* flags */);
extern CellColor makeColorPair (int  /* fg */, int  /* bg */);
extern void ClearCurBackground (XtermWidget /* xw */, int  /* top */, int  /* left */, unsigned  /* height */, unsigned  /* width */);

#define xtermColorPair(xw) makeColorPair(xw->sgr_foreground, xw->sgr_background)

#define getXtermForeground(xw, flags, color) \
	(((flags) & FG_COLOR) && ((int)(color) >= 0 && (color) < MAXCOLORS) \
			? GET_COLOR_RES(xw, TScreenOf(xw)->Acolors[color]) \
			: T_COLOR(TScreenOf(xw), TEXT_FG))

#define getXtermBackground(xw, flags, color) \
	(((flags) & BG_COLOR) && ((int)(color) >= 0 && (color) < MAXCOLORS) \
			? GET_COLOR_RES(xw, TScreenOf(xw)->Acolors[color]) \
			: T_COLOR(TScreenOf(xw), TEXT_BG))

#if OPT_COLOR_RES
#define GET_COLOR_RES(xw, res) xtermGetColorRes(xw, &(res))
#define SET_COLOR_RES(res,color) (res)->value = color
#define EQL_COLOR_RES(res,color) (res)->value == color
#define T_COLOR(v,n) (v)->Tcolors[n].value
extern Pixel xtermGetColorRes(XtermWidget /* xw */, ColorRes * /* res */);
#else
#define GET_COLOR_RES(xw, res) res
#define SET_COLOR_RES(res,color) *res = color
#define EQL_COLOR_RES(res,color) *res == color
#define T_COLOR(v,n) (v)->Tcolors[n]
#endif

#define ExtractForeground(color) (unsigned) GetCellColorFG(color)
#define ExtractBackground(color) (unsigned) GetCellColorBG(color)

#define MapToColorMode(fg, screen, flags) \
	(((screen)->colorBLMode && ((flags) & BLINK)) \
	 ? COLOR_BL \
	 : (((screen)->colorBDMode && ((flags) & BOLD)) \
	    ? COLOR_BD \
	    : (((screen)->colorULMode && ((flags) & UNDERLINE)) \
	       ? COLOR_UL \
	       : fg)))

#define checkVeryBoldAttr(flags, fg, code, attr) \
	if ((flags & FG_COLOR) != 0 \
	 && (screen->veryBoldColors & attr) == 0 \
	 && (flags & attr) != 0 \
	 && (fg == code)) \
		 UIntClr(flags, attr)

#define checkVeryBoldColors(flags, fg) \
	checkVeryBoldAttr(flags, fg, COLOR_RV, INVERSE); \
	checkVeryBoldAttr(flags, fg, COLOR_UL, UNDERLINE); \
	checkVeryBoldAttr(flags, fg, COLOR_BD, BOLD); \
	checkVeryBoldAttr(flags, fg, COLOR_BL, BLINK)

#else /* !OPT_ISO_COLORS */

#define MapToColorMode(fg, screen, flags) fg

#define ClearDFtBackground(xw, top, left, height, width) \
	ClearCurBackground(xw, top, left, height, width)

#define ClearCurBackground(xw, top, left, height, width) \
	XClearArea (TScreenOf(xw)->display, VWindow(TScreenOf(xw)), \
		left, top, width, height, False)

#define extract_fg(xw, color, flags) (xw)->cur_foreground
#define extract_bg(xw, color, flags) (xw)->cur_background

		/* FIXME: Reverse-Video? */
#define T_COLOR(v,n) (v)->Tcolors[n]
#define getXtermBackground(xw, flags, color) T_COLOR(TScreenOf(xw), TEXT_BG)
#define getXtermForeground(xw, flags, color) T_COLOR(TScreenOf(xw), TEXT_FG)
#define makeColorPair(fg, bg) 0
#define xtermColorPair(xw) 0

#define checkVeryBoldColors(flags, fg) /* nothing */

#endif	/* OPT_ISO_COLORS */

#define XTERM_CELL(row,col)    getXtermCell(screen,     ROW2INX(screen, row), col)

extern unsigned getXtermCell (TScreen * /* screen */, int  /* row */, int  /* col */);
extern void putXtermCell (TScreen * /* screen */, int  /* row */, int  /* col */, int  /* ch */);

#if OPT_HIGHLIGHT_COLOR
#define isNotForeground(xw, fg, bg, sel) \
		(Boolean) ((sel) != T_COLOR(TScreenOf(xw), TEXT_FG) \
			   && (sel) != (fg) \
			   && (sel) != (bg) \
			   && (sel) != (xw)->dft_foreground)
#define isNotBackground(xw, fg, bg, sel) \
		(Boolean) ((sel) != T_COLOR(TScreenOf(xw), TEXT_BG) \
			   && (sel) != (fg) \
			   && (sel) != (bg) \
			   && (sel) != (xw)->dft_background)
#endif

#if OPT_WIDE_CHARS
extern Boolean isWideControl(unsigned /* ch */);
extern int DamagedCells(TScreen * /* screen */, unsigned /* n */, int * /* klp */, int * /* krp */, int /* row */, int /* col */);
extern int DamagedCurCells(TScreen * /* screen */, unsigned /* n */, int * /* klp */, int * /* krp */);
extern unsigned AsciiEquivs(unsigned /* ch */);
extern void addXtermCombining (TScreen * /* screen */, int  /* row */, int  /* col */, unsigned  /* ch */);
extern void allocXtermChars(ScrnPtr * /* buffer */, Cardinal /* length */);
#endif

#if OPT_XMC_GLITCH
extern void Mark_XMC (XtermWidget /* xw */, int  /* param */);
extern void Jump_XMC (XtermWidget /* xw */);
extern void Resolve_XMC (XtermWidget /* xw */);
#endif

#if OPT_WIDE_CHARS
unsigned visual_width(IChar * /* str */, Cardinal  /* len */);
#else
#define visual_width(a, b) (b)
#endif

#define BtoS(b)    (((b) == Maybe) ? "maybe" : ((b) ? "on" : "off"))
#define NonNull(s) ((s) ? (s) : "<null>")

#define UIntSet(dst,bits) dst = dst | (unsigned) (bits)
#define UIntClr(dst,bits) dst = dst & (unsigned) ~(bits)

#ifdef __cplusplus
	}
#endif
/* *INDENT-ON* */

#endif /* included_xterm_h */
