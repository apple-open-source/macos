/* $XFree86: xc/programs/xterm/xterm.h,v 3.87 2003/02/25 23:36:55 dickey Exp $ */

/************************************************************

Copyright 1999,2000,2001,2002 by Thomas E. Dickey

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

#ifdef HAVE_CONFIG_H
#include <xtermcfg.h>
#endif

#ifndef GCC_UNUSED
#define GCC_UNUSED		/* nothing */
#endif

#ifndef GCC_NORETURN
#define GCC_NORETURN		/* nothing */
#endif

#include <X11/Xos.h>

#ifndef HAVE_CONFIG_H

#define HAVE_LIB_XAW 1

#ifdef CSRG_BASED
/* Get definition of BSD */
#include <sys/param.h>
#endif

#ifndef HAVE_X11_DECKEYSYM_H
#define HAVE_X11_DECKEYSYM_H 1
#endif

#ifndef HAVE_X11_SUNKEYSYM_H
#define HAVE_X11_SUNKEYSYM_H 1
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
#define DECL_ERRNO 1

#if defined(CSRG_BASED) || defined(__GNU__)
#define USE_POSIX_TERMIOS 1
#endif

#if defined(hpux) && !defined(__hpux)
#define __hpux 1		/* HPUX 11.0 does not define this */
#endif

#ifdef USE_POSIX_TERMIOS
#define HAVE_TERMIOS_H 1
#define HAVE_TCGETATTR 1
#endif

#if defined(__UNIXOS2__) || defined(SCO) || defined(sco)
#define USE_TERMCAP 1
#endif

#if defined(UTMP)
#define HAVE_UTMP 1
#endif

#if (defined(__MVS__) || defined(SVR4) || defined(SCO325)) && !defined(__CYGWIN__)
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

#if defined(SVR4) || defined(SCO325) || (defined(linux) && defined(__GLIBC__) && (__GLIBC__ >= 2) && !(defined(__powerpc__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ == 0)))
#define HAVE_UTMP_UT_XTIME 1
#endif

#if defined(linux) || defined(__CYGWIN__)
#define USE_LASTLOG
#define HAVE_LASTLOG_H
#elif defined(BSD) && (BSD >= 199103)
#define USE_LASTLOG
#endif

#if defined(SCO) || defined(SVR4) || defined(_POSIX_SOURCE) || defined(__QNX__) || defined(__hpux) || (defined(BSD) && (BSD >= 199103)) || defined(__CYGWIN__)
#define USE_POSIX_WAIT
#endif

#if defined(AIXV3) || defined(CRAY) || defined(SCO) || defined(SVR4) || (defined(SYSV) && defined(i386)) || defined(__MVS__) || defined(__hpux) || defined(__osf__) || defined(linux) || defined(macII)
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

#endif /* HAVE_CONFIG_H */

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
#define HIDDEN_HI 0xff
#define HIDDEN_LO 0xff
#define HIDDEN_CHAR 0xffff
#endif

/***====================================================================***/

#include <proto.h>
#include <ptyx.h>

#if (XtSpecificationRelease >= 6) && !defined(NO_XPOLL_H)
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

#ifdef USE_SYS_SELECT_H

#include <sys/types.h>

#if defined(USE_XPOLL_H) && defined(AIXV3) && defined(NFDBITS)
#undef NFDBITS			/* conflict between X11/Xpoll.h and sys/select.h */
#endif

#include <sys/select.h>

#endif

#include <setjmp.h>

/***====================================================================***/

#define XtNallowSendEvents	"allowSendEvents"
#define XtNallowWindowOps	"allowWindowOps"
#define XtNalwaysHighlight	"alwaysHighlight"
#define XtNalwaysUseMods	"alwaysUseMods"
#define XtNanswerbackString	"answerbackString"
#define XtNappcursorDefault	"appcursorDefault"
#define XtNappkeypadDefault	"appkeypadDefault"
#define XtNautoWrap		"autoWrap"
#define XtNawaitInput		"awaitInput"
#define XtNbackarrowKey		"backarrowKey"
#define XtNbellSuppressTime	"bellSuppressTime"
#define XtNboldColors		"boldColors"
#define XtNboldFont		"boldFont"
#define XtNboldMode		"boldMode"
#define XtNbrokenSelections	"brokenSelections"
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
#define XtNctrlFKeys		"ctrlFKeys"
#define XtNcurses		"curses"
#define XtNcursorBlink		"cursorBlink"
#define XtNcursorColor		"cursorColor"
#define XtNcursorOffTime	"cursorOffTime"
#define XtNcursorOnTime		"cursorOnTime"
#define XtNcutNewline		"cutNewline"
#define XtNcutToBeginningOfLine	"cutToBeginningOfLine"
#define XtNdecTerminalID	"decTerminalID"
#define XtNdeleteIsDEL		"deleteIsDEL"
#define XtNdynamicColors	"dynamicColors"
#define XtNeightBitControl	"eightBitControl"
#define XtNeightBitInput	"eightBitInput"
#define XtNeightBitOutput	"eightBitOutput"
#define XtNfaceName		"faceName"
#define XtNfaceSize		"faceSize"
#define XtNfontDoublesize	"fontDoublesize"
#define XtNfontStyle		"fontStyle"
#define XtNforceBoxChars	"forceBoxChars"
#define XtNfreeBoldBox		"freeBoldBox"
#define XtNhighlightColor	"highlightColor"
#define XtNhighlightSelection	"highlightSelection"
#define XtNhpLowerleftBugCompat	"hpLowerleftBugCompat"
#define XtNi18nSelections	"i18nSelections"
#define XtNinternalBorder	"internalBorder"
#define XtNjumpScroll		"jumpScroll"
#define XtNkeyboardDialect	"keyboardDialect"
#define XtNlimitResize		"limitResize"
#define XtNlocale		"locale"
#define XtNlocaleFilter		"localeFilter"
#define XtNlogFile		"logFile"
#define XtNlogInhibit		"logInhibit"
#define XtNlogging		"logging"
#define XtNloginShell		"loginShell"
#define XtNmarginBell		"marginBell"
#define XtNmenuBar		"menuBar"
#define XtNmenuHeight		"menuHeight"
#define XtNmetaSendsEscape	"metaSendsEscape"
#define XtNmodifyCursorKeys	"modifyCursorKeys"
#define XtNmultiClickTime	"multiClickTime"
#define XtNmultiScroll		"multiScroll"
#define XtNnMarginBell		"nMarginBell"
#define XtNnumLock		"numLock"
#define XtNoldXtermFKeys	"oldXtermFKeys"
#define XtNpointerColor		"pointerColor"
#define XtNpointerColorBackground "pointerColorBackground"
#define XtNpointerShape		"pointerShape"
#define XtNpopOnBell		"popOnBell"
#define XtNprintAttributes	"printAttributes"
#define XtNprinterAutoClose	"printerAutoClose"
#define XtNprinterCommand	"printerCommand"
#define XtNprinterControlMode	"printerControlMode"
#define XtNprinterExtent	"printerExtent"
#define XtNprinterFormFeed	"printerFormFeed"
#define XtNresizeGravity	"resizeGravity"
#define XtNreverseWrap		"reverseWrap"
#define XtNrightScrollBar	"rightScrollBar"
#define XtNsaveLines		"saveLines"
#define XtNscrollBar		"scrollBar"
#define XtNscrollKey		"scrollKey"
#define XtNscrollLines		"scrollLines"
#define XtNscrollPos		"scrollPos"
#define XtNscrollTtyOutput	"scrollTtyOutput"
#define XtNshiftFonts		"shiftFonts"
#define XtNsignalInhibit	"signalInhibit"
#define XtNtekGeometry		"tekGeometry"
#define XtNtekInhibit		"tekInhibit"
#define XtNtekSmall		"tekSmall"
#define XtNtekStartup		"tekStartup"
#define XtNtiXtraScroll		"tiXtraScroll"
#define XtNtiteInhibit		"titeInhibit"
#define XtNtrimSelection	"trimSelection"
#define XtNunderLine		"underLine"
#define XtNutf8			"utf8"
#define XtNveryBoldColors	"veryBoldColors"
#define XtNvisualBell		"visualBell"
#define XtNvt100Graphics	"vt100Graphics"
#define XtNwideBoldFont		"wideBoldFont"
#define XtNwideChars		"wideChars"
#define XtNwideFont		"wideFont"
#define XtNximFont		"ximFont"
#define XtNxmcAttributes	"xmcAttributes"
#define XtNxmcGlitch		"xmcGlitch"
#define XtNxmcInline		"xmcInline"
#define XtNxmcMoveSGR		"xmcMoveSGR"

#define XtCAllowSendEvents	"AllowSendEvents"
#define XtCAllowWindowOps	"AllowWindowOps"
#define XtCAlwaysHighlight	"AlwaysHighlight"
#define XtCAlwaysUseMods	"AlwaysUseMods"
#define XtCAnswerbackString	"AnswerbackString"
#define XtCAppcursorDefault	"AppcursorDefault"
#define XtCAppkeypadDefault	"AppkeypadDefault"
#define XtCAutoWrap		"AutoWrap"
#define XtCAwaitInput		"AwaitInput"
#define XtCBackarrowKey		"BackarrowKey"
#define XtCBellSuppressTime	"BellSuppressTime"
#define XtCBoldFont		"BoldFont"
#define XtCBoldMode		"BoldMode"
#define XtCBrokenSelections	"BrokenSelections"
#define XtCC132			"C132"
#define XtCCacheDoublesize	"CacheDoublesize"
#define XtCCharClass		"CharClass"
#define XtCCjkWidth 		"CjkWidth"
#define XtCColorMode		"ColorMode"
#define XtCColumn		"Column"
#define XtCCtrlFKeys		"CtrlFKeys"
#define XtCCurses		"Curses"
#define XtCCursorBlink		"CursorBlink"
#define XtCCursorOffTime	"CursorOffTime"
#define XtCCursorOnTime		"CursorOnTime"
#define XtCCutNewline		"CutNewline"
#define XtCCutToBeginningOfLine	"CutToBeginningOfLine"
#define XtCDecTerminalID	"DecTerminalID"
#define XtCDeleteIsDEL		"DeleteIsDEL"
#define XtCDynamicColors	"DynamicColors"
#define XtCEightBitControl	"EightBitControl"
#define XtCEightBitInput	"EightBitInput"
#define XtCEightBitOutput	"EightBitOutput"
#define XtCFaceName		"FaceName"
#define XtCFaceSize		"FaceSize"
#define XtCFontDoublesize	"FontDoublesize"
#define XtCFontStyle		"FontStyle"
#define XtCHighlightSelection	"HighlightSelection"
#define XtCHpLowerleftBugCompat	"HpLowerleftBugCompat"
#define XtCI18nSelections	"I18nSelections"
#define XtCJumpScroll		"JumpScroll"
#define XtCKeyboardDialect	"KeyboardDialect"
#define XtCLimitResize		"LimitResize"
#define XtCLocale		"Locale"
#define XtCLocaleFilter		"LocaleFilter"
#define XtCLogInhibit		"LogInhibit"
#define XtCLogfile		"Logfile"
#define XtCLogging		"Logging"
#define XtCLoginShell		"LoginShell"
#define XtCMarginBell		"MarginBell"
#define XtCMenuBar		"MenuBar"
#define XtCMenuHeight		"MenuHeight"
#define XtCMetaSendsEscape	"MetaSendsEscape"
#define XtCModifyCursorKeys	"ModifyCursorKeys"
#define XtCMultiClickTime	"MultiClickTime"
#define XtCMultiScroll		"MultiScroll"
#define XtCNumLock		"NumLock"
#define XtCOldXtermFKeys	"OldXtermFKeys"
#define XtCPopOnBell		"PopOnBell"
#define XtCPrintAttributes	"PrintAttributes"
#define XtCPrinterAutoClose	"PrinterAutoClose"
#define XtCPrinterCommand	"PrinterCommand"
#define XtCPrinterControlMode	"PrinterControlMode"
#define XtCPrinterExtent	"PrinterExtent"
#define XtCPrinterFormFeed	"PrinterFormFeed"
#define XtCResizeGravity	"ResizeGravity"
#define XtCReverseWrap		"ReverseWrap"
#define XtCRightScrollBar	"RightScrollBar"
#define XtCSaveLines		"SaveLines"
#define XtCScrollBar		"ScrollBar"
#define XtCScrollCond		"ScrollCond"
#define XtCScrollLines		"ScrollLines"
#define XtCScrollPos		"ScrollPos"
#define XtCShiftFonts		"ShiftFonts"
#define XtCSignalInhibit	"SignalInhibit"
#define XtCTekInhibit		"TekInhibit"
#define XtCTekSmall		"TekSmall"
#define XtCTekStartup		"TekStartup"
#define XtCTiXtraScroll		"TiXtraScroll"
#define XtCTiteInhibit		"TiteInhibit"
#define XtCTrimSelection	"TrimSelection"
#define XtCUnderLine		"UnderLine"
#define XtCUtf8			"Utf8"
#define XtCVT100Graphics	"VT100Graphics"
#define XtCVisualBell		"VisualBell"
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
#else
#define XtCCursorColor		XtCForeground
#define XtCPointerColor		XtCForeground
#define XtCHighlightColor	XtCForeground
#endif

/***====================================================================***/

#ifdef __cplusplus
extern "C" {
#endif

/* Tekproc.c */
extern int TekInit (void);
extern int TekPtyData (void);
extern void ChangeTekColors (TScreen *screen, ScrnColors *pNew);
extern void TCursorToggle (int toggle);
extern void TekCopy (void);
extern void TekEnqMouse (int c);
extern void TekExpose (Widget w, XEvent *event, Region region);
extern void TekGINoff (void);
extern void TekReverseVideo (TScreen *screen);
extern void TekRun (void);
extern void TekSetFontSize (int newitem);
extern void TekSimulatePageButton (Bool reset);
extern void dorefresh (void);

/* button.c */
extern Boolean SendMousePosition (Widget w, XEvent* event);
extern void DiredButton               PROTO_XT_ACTIONS_ARGS;
extern void DisownSelection (XtermWidget termw);
extern void HandleGINInput            PROTO_XT_ACTIONS_ARGS;
extern void HandleInsertSelection     PROTO_XT_ACTIONS_ARGS;
extern void HandleKeyboardSelectEnd   PROTO_XT_ACTIONS_ARGS;
extern void HandleKeyboardSelectStart PROTO_XT_ACTIONS_ARGS;
extern void HandleKeyboardStartExtend PROTO_XT_ACTIONS_ARGS;
extern void HandleSecure              PROTO_XT_ACTIONS_ARGS;
extern void HandleSelectEnd           PROTO_XT_ACTIONS_ARGS;
extern void HandleSelectExtend        PROTO_XT_ACTIONS_ARGS;
extern void HandleSelectSet           PROTO_XT_ACTIONS_ARGS;
extern void HandleSelectStart         PROTO_XT_ACTIONS_ARGS;
extern void HandleStartExtend         PROTO_XT_ACTIONS_ARGS;
extern void ReadLineButton            PROTO_XT_ACTIONS_ARGS;
extern void ResizeSelection (TScreen *screen, int rows, int cols);
extern void ScrollSelection (TScreen* screen, int amount);
extern void TrackMouse (int func, int startrow, int startcol, int firstrow, int lastrow);
extern void TrackText (int frow, int fcol, int trow, int tcol);
extern void ViButton                  PROTO_XT_ACTIONS_ARGS;

#if OPT_DEC_LOCATOR
extern Boolean SendLocatorPosition (Widget w, XEvent* event);
extern void CheckLocatorPosition (Widget w, XEvent *event);
extern void GetLocatorPosition (XtermWidget w);
extern void InitLocatorFilter (XtermWidget w);
#endif	/* OPT_DEC_LOCATOR */

#if OPT_WIDE_CHARS
extern int iswide(int i);
#endif

/* charproc.c */
extern int VTInit (void);
extern int v_write (int f, Char *d, int len);
extern void FindFontSelection (char *atom_name, Bool justprobe);
extern void HideCursor (void);
extern void ShowCursor (void);
extern void SwitchBufPtrs (TScreen *screen);
extern void ToggleAlternate (TScreen *screen);
extern void VTReset (int full, int saved);
extern void VTRun (void);
extern void dotext (TScreen *screen, int charset, IChar *buf, Cardinal len);
extern void resetCharsets (TScreen *screen);
extern void set_cursor_gcs (TScreen *screen);
extern void unparseputc (int c, int fd);
extern void unparseputc1 (int c, int fd);
extern void unparseputs (char *s, int fd);
extern void unparseseq (ANSI *ap, int fd);

#if OPT_BLINK_CURS
extern void ToggleCursorBlink(TScreen *screen);
#endif

#if OPT_ISO_COLORS
extern void SGR_Background (int color);
extern void SGR_Foreground (int color);
#endif

/* charsets.c */
extern unsigned xtermCharSetIn (unsigned code, int charset);
extern int xtermCharSetOut (IChar *buf, IChar *ptr, char charset);

/* cursor.c */
extern void CarriageReturn (TScreen *screen);
extern void CursorBack (TScreen *screen, int  n);
extern void CursorDown (TScreen *screen, int  n);
extern void CursorForward (TScreen *screen, int  n);
extern void CursorNextLine (TScreen *screen, int count);
extern void CursorPrevLine (TScreen *screen, int count);
extern void CursorRestore (XtermWidget tw);
extern void CursorSave (XtermWidget tw);
extern void CursorSet (TScreen *screen, int row, int col, unsigned flags);
extern void CursorUp (TScreen *screen, int  n);
extern void RevIndex (TScreen *screen, int amount);
extern void xtermIndex (TScreen *screen, int amount);

/* doublechr.c */
extern void xterm_DECDHL (Bool top);
extern void xterm_DECSWL (void);
extern void xterm_DECDWL (void);
#if OPT_DEC_CHRSET
extern int xterm_Double_index(unsigned chrset, unsigned flags);
extern GC xterm_DoubleGC(unsigned chrset, unsigned flags, GC old_gc);
#endif

/* input.c */
extern Boolean xtermDeleteIsDEL (void);
extern void Input (TKeyboard *keyboard, TScreen *screen, XKeyEvent *event, Bool eightbit);
extern void StringInput (TScreen *screen, Char *string, size_t nbytes);
extern void xtermAddInput(Widget w);

#if OPT_NUM_LOCK
extern void VTInitModifiers(void);
#endif

#if OPT_TCAP_QUERY
extern int xtermcapKeycode(char *params, unsigned *state);
#endif

#if OPT_WIDE_CHARS
extern int convertFromUTF8(unsigned long c, Char *strbuf);
#endif

/* main.c */
#ifndef __UNIXOS2__
#define ENVP_ARG /**/
#else
#define ENVP_ARG , char **envp
#endif

extern int main (int argc, char **argv ENVP_ARG);
extern int GetBytesAvailable (int fd);
extern int kill_process_group (int pid, int sig);
extern int nonblocking_wait (void);
extern void first_map_occurred (void);

#ifdef SIGNAL_T
extern SIGNAL_T Exit (int n);
#endif

#if OPT_WIDE_CHARS
extern int (*my_wcwidth)(wchar_t);
#endif

/* menu.c */
extern void do_hangup          PROTO_XT_CALLBACK_ARGS;
extern void show_8bit_control  (Bool value);

/* misc.c */
extern Cursor make_colored_cursor (unsigned cursorindex, unsigned long fg, unsigned long bg);
extern OptionHelp * sortedOpts(OptionHelp *, XrmOptionDescRec *, Cardinal);
extern Window WMFrameWindow(XtermWidget termw);
extern XrmOptionDescRec * sortedOptDescs(XrmOptionDescRec *, Cardinal);
extern char *SysErrorMsg (int n);
extern char *udk_lookup (int keycode, int *len);
extern int XStrCmp (char *s1, char *s2);
extern int creat_as (int uid, int gid, Boolean append, char *pathname, int mode);
extern int open_userfile (int uid, int gid, char *path, Boolean append);
extern int xerror (Display *d, XErrorEvent *ev);
extern int xioerror (Display *dpy);
extern void Bell (int which, int percent);
extern void ChangeXprop (char *name);
extern void Changename (char *name);
extern void Changetitle (char *name);
extern void Cleanup (int code);
extern void HandleBellPropertyChange PROTO_XT_EV_HANDLER_ARGS;
extern void HandleEightBitKeyPressed PROTO_XT_ACTIONS_ARGS;
extern void HandleEnterWindow        PROTO_XT_EV_HANDLER_ARGS;
extern void HandleFocusChange        PROTO_XT_EV_HANDLER_ARGS;
extern void HandleInterpret          PROTO_XT_ACTIONS_ARGS;
extern void HandleKeyPressed         PROTO_XT_ACTIONS_ARGS;
extern void HandleLeaveWindow        PROTO_XT_EV_HANDLER_ARGS;
extern void HandleStringEvent        PROTO_XT_ACTIONS_ARGS;
extern void Panic (char *s, int a);
extern void Redraw (void);
extern void ReverseOldColors (void);
extern void SysError (int i) GCC_NORETURN;
extern void VisualBell (void);
extern void do_dcs (Char *buf, size_t len);
extern void do_osc (Char *buf, int len, int final);
extern void do_xevents (void);
extern void end_tek_mode (void);
extern void end_vt_mode (void);
extern void hide_tek_window (void);
extern void hide_vt_window (void);
extern void reset_decudk (void);
extern void set_tek_visibility (Boolean on);
extern void set_vt_visibility (Boolean on);
extern void switch_modes (Bool tovt);
extern void timestamp_filename(char *dst, const char *src);
extern void xevents (void);
extern void xt_error (String message);
extern void xtermSetenv (char *var, char *value);

#if OPT_MAXIMIZE
extern int QueryMaximize (TScreen *screen, unsigned *width, unsigned *height);
extern void HandleDeIconify          PROTO_XT_ACTIONS_ARGS;
extern void HandleIconify            PROTO_XT_ACTIONS_ARGS;
extern void HandleMaximize           PROTO_XT_ACTIONS_ARGS;
extern void HandleRestoreSize        PROTO_XT_ACTIONS_ARGS;
extern void RequestMaximize (XtermWidget termw, int maximize);
#endif

#ifdef ALLOWLOGGING
extern void StartLog (TScreen *screen);
extern void CloseLog (TScreen *screen);
extern void FlushLog (TScreen *screen);
#else
#define FlushLog(screen) /*nothing*/
#endif

/* print.c */
extern Boolean xtermHasPrinter (void);
extern int xtermPrinterControl (int chr);
extern void setPrinterControlMode (int mode);
extern void xtermAutoPrint (int chr);
extern void xtermMediaControl (int param, int private_seq);
extern void xtermPrintScreen (Boolean use_DECPEX);

/* ptydata.c */
extern int getPtyData (TScreen *screen, fd_set *select_mask, PtyData *data);
extern unsigned usedPtyData(PtyData *data);
extern void initPtyData (PtyData *data);

#define nextPtyData(data) ((data)->cnt)--, (*((data)->ptr)++)
#define morePtyData(data) ((data)->cnt > 0)

#if OPT_WIDE_CHARS
extern Char * convertToUTF8(Char *lp, unsigned c);
extern void writePtyData(int f, IChar *d, unsigned len);
#else
#define writePtyData(f,d,len) v_write(f,d,len)
#endif

/* screen.c */
extern Bool non_blank_line (ScrnBuf sb, int row, int col, int len);
extern ScrnBuf Allocate (int nrow, int ncol, Char **addr);
extern int ScreenResize (TScreen *screen, int width, int height, unsigned *flags);
extern size_t ScrnPointers (TScreen *screen, size_t len);
extern void ClearBufRows (TScreen *screen, int first, int last);
extern void ScreenWrite (TScreen *screen, PAIRED_CHARS(Char *str, Char *str2), unsigned flags, unsigned cur_fg_bg, int length);
extern void ScrnDeleteChar (TScreen *screen, int n);
extern void ScrnDeleteLine (TScreen *screen, ScrnBuf sb, int n, int last, int size, int where);
extern void ScrnInsertChar (TScreen *screen, int n);
extern void ScrnInsertLine (TScreen *screen, ScrnBuf sb, int last, int where, int n, int size);
extern void ScrnRefresh (TScreen *screen, int toprow, int leftcol, int nrows, int ncols, Bool force);

#define ScrnClrWrapped(screen, row) \
	SCRN_BUF_FLAGS(screen, row + screen->topline) = \
		(Char *)((long)SCRN_BUF_FLAGS(screen, row + screen->topline) & ~ LINEWRAPPED)

#define ScrnSetWrapped(screen, row) \
	SCRN_BUF_FLAGS(screen, row + screen->topline) = \
		(Char *)(((long)SCRN_BUF_FLAGS(screen, row + screen->topline) | LINEWRAPPED))

#define ScrnTstWrapped(screen, row) \
	(((long)SCRN_BUF_FLAGS(screen, row + screen->topline) & LINEWRAPPED) != 0)

/* scrollbar.c */
extern void DoResizeScreen (XtermWidget xw);
extern void HandleScrollBack PROTO_XT_ACTIONS_ARGS;
extern void HandleScrollForward PROTO_XT_ACTIONS_ARGS;
extern void ResizeScrollBar (TScreen *screen);
extern void ScrollBarDrawThumb (Widget scrollWidget);
extern void ScrollBarOff (TScreen *screen);
extern void ScrollBarOn (XtermWidget xw, int init, int doalloc);
extern void ScrollBarReverseVideo (Widget scrollWidget);
extern void ToggleScrollBar (XtermWidget w);
extern void WindowScroll (TScreen *screen, int top);

/* tabs.c */
extern Boolean TabToNextStop (void);
extern Boolean TabToPrevStop (void);
extern int TabNext (Tabs tabs, int col);
extern int TabPrev (Tabs tabs, int col);
extern void TabClear (Tabs tabs, int col);
extern void TabReset (Tabs tabs);
extern void TabSet (Tabs tabs, int col);
extern void TabZonk (Tabs tabs);

/* util.c */
extern GC updatedXtermGC (TScreen *screen, int flags, int fg_bg, Bool hilite);
extern int AddToRefresh (TScreen *screen);
extern int HandleExposure (TScreen *screen, XEvent *event);
extern int char2lower(int ch);
extern int drawXtermText (TScreen *screen, unsigned flags, GC gc, int x, int y, int chrset, PAIRED_CHARS(Char *text, Char *text2), Cardinal len, int on_wide);
extern void ChangeAnsiColors (XtermWidget tw);
extern void ChangeColors (XtermWidget tw, ScrnColors *pNew);
extern void ClearRight (TScreen *screen, int n);
extern void ClearScreen (TScreen *screen);
extern void DeleteChar (TScreen *screen, int n);
extern void DeleteLine (TScreen *screen, int n);
extern void FlushScroll (TScreen *screen);
extern void GetColors (XtermWidget tw, ScrnColors *pColors);
extern void InsertChar (TScreen *screen, int n);
extern void InsertLine (TScreen *screen, int n);
extern void RevScroll (TScreen *screen, int amount);
extern void ReverseVideo (XtermWidget termw);
extern void do_erase_display (TScreen *screen, int param, int mode);
extern void do_erase_line (TScreen *screen, int param, int mode);
extern void init_keyboard_type (xtermKeyboardType, Bool set);
extern void recolor_cursor (Cursor cursor, unsigned long fg, unsigned long bg);
extern void resetXtermGC (TScreen *screen, int flags, Bool hilite);
extern void scrolling_copy_area (TScreen *screen, int firstline, int nlines, int amount);
extern void set_keyboard_type (xtermKeyboardType type, Bool set);
extern void toggle_keyboard_type (xtermKeyboardType type);
extern void update_keyboard_type (void);
extern void xtermScroll (TScreen *screen, int amount);

#if OPT_ISO_COLORS

extern Pixel getXtermBackground (int flags, int color);
extern Pixel getXtermForeground (int flags, int color);
extern int extract_fg (unsigned color, unsigned flags);
extern int extract_bg (unsigned color, unsigned flags);
extern unsigned makeColorPair (int fg, int bg);
extern void ClearCurBackground (TScreen *screen, int top, int left, unsigned height, unsigned width);

#define xtermColorPair() makeColorPair(term->sgr_foreground, term->sgr_background)

#define getXtermForeground(flags, color) \
	(((flags) & FG_COLOR) && ((color) >= 0 && (color) < MAXCOLORS) \
			? GET_COLOR_RES(term->screen.Acolors[color]) \
			: term->screen.foreground)

#define getXtermBackground(flags, color) \
	(((flags) & BG_COLOR) && ((color) >= 0 && (color) < MAXCOLORS) \
			? GET_COLOR_RES(term->screen.Acolors[color]) \
			: term->core.background_pixel)

#if OPT_COLOR_RES
#define GET_COLOR_RES(res) xtermGetColorRes(&res)
#define SET_COLOR_RES(res,color) res->value = color
extern Pixel xtermGetColorRes(ColorRes *res);
#else
#define GET_COLOR_RES(res) res
#define SET_COLOR_RES(res,color) *res = color
#endif

#if OPT_EXT_COLORS
#define ExtractForeground(color) ((color >> 8) & 0xff)
#define ExtractBackground(color) (color & 0xff)
#else
#define ExtractForeground(color) ((color >> 4) & 0xf)
#define ExtractBackground(color) (color & 0xf)
#endif

#define checkVeryBoldAttr(flags, fg, code, attr) \
	if ((flags & FG_COLOR) != 0 \
	 && (screen->veryBoldColors & attr) == 0 \
	 && (flags & attr) != 0 \
	 && (fg == code)) \
		 flags &= ~(attr)

#define checkVeryBoldColors(flags, fg) \
	checkVeryBoldAttr(flags, fg, COLOR_RV, INVERSE); \
	checkVeryBoldAttr(flags, fg, COLOR_UL, UNDERLINE); \
	checkVeryBoldAttr(flags, fg, COLOR_BD, BOLD); \
	checkVeryBoldAttr(flags, fg, COLOR_BL, BLINK)

#else /* !OPT_ISO_COLORS */

#define ClearCurBackground(screen, top, left, height, width) \
	XClearArea (screen->display, VWindow(screen), \
		left, top, width, height, FALSE)

#define extract_fg(color, flags) term->cur_foreground
#define extract_bg(color, flags) term->cur_background

		/* FIXME: Reverse-Video? */
#define getXtermBackground(flags, color) term->core.background_pixel
#define getXtermForeground(flags, color) term->screen.foreground
#define makeColorPair(fg, bg) 0
#define xtermColorPair() 0

#define checkVeryBoldColors(flags, fg) /* nothing */

#endif	/* OPT_ISO_COLORS */

#if OPT_DEC_CHRSET
#define curXtermChrSet(row) \
	((CSET_DOUBLE(SCRN_ROW_CSET((&term->screen), row))) \
		? SCRN_ROW_CSET((&term->screen), row) \
		: (term->screen).cur_chrset)
#else
#define curXtermChrSet(row) 0
#endif

#if OPT_WIDE_CHARS
extern unsigned getXtermCell (TScreen *screen, int row, int col);
extern unsigned getXtermCellComb1 (TScreen *screen, int row, int col);
extern unsigned getXtermCellComb2 (TScreen *screen, int row, int col);
extern void addXtermCombining (TScreen *screen, int row, int col, unsigned ch);
extern void putXtermCell (TScreen *screen, int row, int col, int ch);
#else
#define getXtermCell(screen,row,col) SCRN_BUF_CHARS(screen, row)[col]
#define putXtermCell(screen,row,col,ch) SCRN_BUF_CHARS(screen, row)[col] = ch
#endif

#if OPT_XMC_GLITCH
extern void Mark_XMC (TScreen *screen, int param);
extern void Jump_XMC (TScreen *screen);
extern void Resolve_XMC (TScreen *screen);
#endif

#if OPT_WIDE_CHARS
int visual_width(PAIRED_CHARS(Char *str, Char *str2), Cardinal len);
#else
#define visual_width(a, b) (b)
#endif

#ifdef __cplusplus
	}
#endif

#endif	/* included_xterm_h */
