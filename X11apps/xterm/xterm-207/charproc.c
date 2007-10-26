/* $XTermId: charproc.c,v 1.627 2005/11/13 23:10:35 tom Exp $ */

/*
 * $Xorg: charproc.c,v 1.6 2001/02/09 02:06:02 xorgcvs Exp $
 */

/* $XFree86: xc/programs/xterm/charproc.c,v 3.177 2005/11/13 23:10:35 dickey Exp $ */

/*

Copyright 1999-2004,2005 by Thomas E. Dickey

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

Copyright 1988  The Open Group

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

*/
/*
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

/* charproc.c */

#include <version.h>
#include <xterm.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/CharSet.h>
#include <X11/Xmu/Converters.h>

#if OPT_INPUT_METHOD

#if defined(HAVE_LIB_XAW)
#include <X11/Xaw/XawImP.h>
#elif defined(HAVE_LIB_XAW3D)
#include <X11/Xaw3d/XawImP.h>
#elif defined(HAVE_LIB_NEXTAW)
#include <X11/neXtaw/XawImP.h>
#elif defined(HAVE_LIB_XAWPLUS)
#include <X11/XawPlus/XawImP.h>
#endif

#endif

#if OPT_WIDE_CHARS
#include <wcwidth.h>
#include <precompose.h>
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif
#endif

#if OPT_INPUT_METHOD
#include <X11/Xlocale.h>
#endif

#include <stdio.h>
#include <ctype.h>

#if defined(HAVE_SCHED_YIELD)
#include <sched.h>
#endif

#include <VTparse.h>
#include <data.h>
#include <error.h>
#include <menu.h>
#include <main.h>
#include <fontutils.h>
#include <xcharmouse.h>
#include <charclass.h>
#include <xstrings.h>

#if OPT_ZICONBEEP || OPT_TOOLBAR
#define HANDLE_STRUCT_NOTIFY 1
#else
#define HANDLE_STRUCT_NOTIFY 0
#endif

static IChar doinput(void);
static int set_character_class(char *s);
static void FromAlternate(TScreen * screen);
static void RequestResize(XtermWidget termw, int rows, int cols, int text);
static void SwitchBufs(TScreen * screen);
static void ToAlternate(TScreen * screen);
static void VTallocbuf(void);
static void WriteText(TScreen * screen,
		      PAIRED_CHARS(Char * str, Char * str2),
		      Cardinal len);
static void ansi_modes(XtermWidget termw,
		       void (*func) (unsigned *p, unsigned mask));
static void bitclr(unsigned *p, unsigned mask);
static void bitcpy(unsigned *p, unsigned q, unsigned mask);
static void bitset(unsigned *p, unsigned mask);
static void dpmodes(XtermWidget termw, void (*func) (unsigned *p, unsigned mask));
static void restoremodes(XtermWidget termw);
static void savemodes(XtermWidget termw);
static void unparseputn(unsigned int n, int fd);
static void window_ops(XtermWidget termw);

#define DoStartBlinking(s) ((s)->cursor_blink ^ (s)->cursor_blink_esc)

#if OPT_BLINK_CURS || OPT_BLINK_TEXT
static void HandleBlinking(XtPointer closure, XtIntervalId * id);
static void StartBlinking(TScreen * screen);
static void StopBlinking(TScreen * screen);
#else
#define StartBlinking(screen)	/* nothing */
#define StopBlinking(screen)	/* nothing */
#endif

#if OPT_INPUT_METHOD
static void PreeditPosition(TScreen * screen);
#endif

#define	DEFAULT		-1
#define BELLSUPPRESSMSEC 200

static int nparam;
static ANSI reply;
static int param[NPARAM];

static jmp_buf vtjmpbuf;

typedef long fd_mask;

/* event handlers */
static void HandleBell PROTO_XT_ACTIONS_ARGS;
static void HandleIgnore PROTO_XT_ACTIONS_ARGS;
static void HandleKeymapChange PROTO_XT_ACTIONS_ARGS;
static void HandleVisualBell PROTO_XT_ACTIONS_ARGS;
#if HANDLE_STRUCT_NOTIFY
static void HandleStructNotify PROTO_XT_EV_HANDLER_ARGS;
#endif

/*
 * NOTE: VTInitialize zeros out the entire ".screen" component of the
 * XtermWidget, so make sure to add an assignment statement in VTInitialize()
 * for each new ".screen" field added to this resource list.
 */

/* Defaults */
#if OPT_ISO_COLORS

/*
 * If we default to colorMode enabled, compile-in defaults for the ANSI colors.
 */
#if DFT_COLORMODE
#define DFT_COLOR(name) name
#else
#define DFT_COLOR(name) XtDefaultForeground
#endif
#endif

static char *_Font_Selected_ = "yes";	/* string is arbitrary */

static char defaultTranslations[] =
"\
          Shift <KeyPress> Prior:scroll-back(1,halfpage) \n\
           Shift <KeyPress> Next:scroll-forw(1,halfpage) \n\
         Shift <KeyPress> Select:select-cursor-start() select-cursor-end(PRIMARY, CUT_BUFFER0) \n\
         Shift <KeyPress> Insert:insert-selection(PRIMARY, CUT_BUFFER0) \n\
"
#if OPT_SHIFT_FONTS
"\
    Shift~Ctrl <KeyPress> KP_Add:larger-vt-font() \n\
    Shift Ctrl <KeyPress> KP_Add:smaller-vt-font() \n\
    Shift <KeyPress> KP_Subtract:smaller-vt-font() \n\
"
#endif
"\
                ~Meta <KeyPress>:insert-seven-bit() \n\
                 Meta <KeyPress>:insert-eight-bit() \n\
                !Ctrl <Btn1Down>:popup-menu(mainMenu) \n\
           !Lock Ctrl <Btn1Down>:popup-menu(mainMenu) \n\
 !Lock Ctrl @Num_Lock <Btn1Down>:popup-menu(mainMenu) \n\
     ! @Num_Lock Ctrl <Btn1Down>:popup-menu(mainMenu) \n\
                ~Meta <Btn1Down>:select-start() \n\
              ~Meta <Btn1Motion>:select-extend() \n\
                !Ctrl <Btn2Down>:popup-menu(vtMenu) \n\
           !Lock Ctrl <Btn2Down>:popup-menu(vtMenu) \n\
 !Lock Ctrl @Num_Lock <Btn2Down>:popup-menu(vtMenu) \n\
     ! @Num_Lock Ctrl <Btn2Down>:popup-menu(vtMenu) \n\
          ~Ctrl ~Meta <Btn2Down>:ignore() \n\
                 Meta <Btn2Down>:clear-saved-lines() \n\
            ~Ctrl ~Meta <Btn2Up>:insert-selection(PRIMARY, CUT_BUFFER0) \n\
                !Ctrl <Btn3Down>:popup-menu(fontMenu) \n\
           !Lock Ctrl <Btn3Down>:popup-menu(fontMenu) \n\
 !Lock Ctrl @Num_Lock <Btn3Down>:popup-menu(fontMenu) \n\
     ! @Num_Lock Ctrl <Btn3Down>:popup-menu(fontMenu) \n\
          ~Ctrl ~Meta <Btn3Down>:start-extend() \n\
              ~Meta <Btn3Motion>:select-extend() \n\
                 Ctrl <Btn4Down>:scroll-back(1,halfpage,m) \n\
            Lock Ctrl <Btn4Down>:scroll-back(1,halfpage,m) \n\
  Lock @Num_Lock Ctrl <Btn4Down>:scroll-back(1,halfpage,m) \n\
       @Num_Lock Ctrl <Btn4Down>:scroll-back(1,halfpage,m) \n\
                      <Btn4Down>:scroll-back(5,line,m)     \n\
                 Ctrl <Btn5Down>:scroll-forw(1,halfpage,m) \n\
            Lock Ctrl <Btn5Down>:scroll-forw(1,halfpage,m) \n\
  Lock @Num_Lock Ctrl <Btn5Down>:scroll-forw(1,halfpage,m) \n\
       @Num_Lock Ctrl <Btn5Down>:scroll-forw(1,halfpage,m) \n\
                      <Btn5Down>:scroll-forw(5,line,m)     \n\
                         <BtnUp>:select-end(PRIMARY, CUT_BUFFER0) \n\
                       <BtnDown>:ignore() \
";				/* PROCURA added "Meta <Btn2Down>:clear-saved-lines()" */
/* *INDENT-OFF* */
static XtActionsRec actionsList[] = {
    { "allow-send-events",	HandleAllowSends },
    { "bell",			HandleBell },
    { "clear-saved-lines",	HandleClearSavedLines },
    { "create-menu",		HandleCreateMenu },
    { "dired-button",		DiredButton },
    { "hard-reset",		HandleHardReset },
    { "ignore",			HandleIgnore },
    { "insert",			HandleKeyPressed },  /* alias for insert-seven-bit */
    { "insert-eight-bit",	HandleEightBitKeyPressed },
    { "insert-selection",	HandleInsertSelection },
    { "insert-seven-bit",	HandleKeyPressed },
    { "interpret",		HandleInterpret },
    { "keymap",			HandleKeymapChange },
    { "popup-menu",		HandlePopupMenu },
    { "print",			HandlePrintScreen },
    { "print-redir",		HandlePrintControlMode },
    { "quit",			HandleQuit },
    { "redraw",			HandleRedraw },
    { "delete-is-del",		HandleDeleteIsDEL },
    { "scroll-back",		HandleScrollBack },
    { "scroll-forw",		HandleScrollForward },
    { "secure",			HandleSecure },
    { "select-cursor-end",	HandleKeyboardSelectEnd },
    { "select-cursor-extend",   HandleKeyboardSelectExtend },
    { "select-cursor-start",	HandleKeyboardSelectStart },
    { "select-end",		HandleSelectEnd },
    { "select-extend",		HandleSelectExtend },
    { "select-set",		HandleSelectSet },
    { "select-start",		HandleSelectStart },
    { "send-signal",		HandleSendSignal },
    { "set-8-bit-control",	Handle8BitControl },
    { "set-allow132",		HandleAllow132 },
    { "set-altscreen",		HandleAltScreen },
    { "set-appcursor",		HandleAppCursor },
    { "set-appkeypad",		HandleAppKeypad },
    { "set-autolinefeed",	HandleAutoLineFeed },
    { "set-autowrap",		HandleAutoWrap },
    { "set-backarrow",		HandleBackarrow },
    { "set-cursesemul",		HandleCursesEmul },
    { "set-jumpscroll",		HandleJumpscroll },
    { "set-old-function-keys",	HandleOldFunctionKeys },
    { "set-marginbell",		HandleMarginBell },
    { "set-reverse-video",	HandleReverseVideo },
    { "set-reversewrap",	HandleReverseWrap },
    { "set-scroll-on-key",	HandleScrollKey },
    { "set-scroll-on-tty-output", HandleScrollTtyOutput },
    { "set-scrollbar",		HandleScrollbar },
    { "set-sun-function-keys",	HandleSunFunctionKeys },
    { "set-sun-keyboard",	HandleSunKeyboard },
    { "set-titeInhibit",	HandleTiteInhibit },
    { "set-visual-bell",	HandleSetVisualBell },
    { "set-pop-on-bell",	HandleSetPopOnBell },
    { "set-vt-font",		HandleSetFont },
    { "soft-reset",		HandleSoftReset },
    { "start-cursor-extend",	HandleKeyboardStartExtend },
    { "start-extend",		HandleStartExtend },
    { "string",			HandleStringEvent },
    { "vi-button",		ViButton },
    { "visual-bell",		HandleVisualBell },
#ifdef ALLOWLOGGING
    { "set-logging",		HandleLogging },
#endif
#if OPT_BLINK_CURS
    { "set-cursorblink",	HandleCursorBlink },
#endif
#if OPT_BOX_CHARS
    { "set-font-linedrawing",	HandleFontBoxChars },
#endif
#if OPT_DABBREV
    { "dabbrev-expand",		HandleDabbrevExpand },
#endif
#if OPT_DEC_CHRSET
    { "set-font-doublesize",	HandleFontDoublesize },
#endif
#if OPT_DEC_SOFTFONT
    { "set-font-loading",	HandleFontLoading },
#endif
#if OPT_HP_FUNC_KEYS
    { "set-hp-function-keys",	HandleHpFunctionKeys },
#endif
#if OPT_LOAD_VTFONTS
    { "load-vt-fonts",		HandleLoadVTFonts },
#endif
#if OPT_MAXIMIZE
    { "deiconify",		HandleDeIconify },
    { "iconify",		HandleIconify },
    { "maximize",		HandleMaximize },
    { "restore",		HandleRestoreSize },
#endif
#if OPT_NUM_LOCK
    { "alt-sends-escape",	HandleAltEsc },
    { "meta-sends-escape",	HandleMetaEsc },
    { "set-num-lock",		HandleNumLock },
#endif
#if OPT_READLINE
    { "readline-button",	ReadLineButton },
#endif
#if OPT_RENDERFONT
    { "set-render-font",	HandleRenderFont },
#endif
#if OPT_SCO_FUNC_KEYS
    { "set-sco-function-keys",	HandleScoFunctionKeys },
#endif
#if OPT_SHIFT_FONTS
    { "larger-vt-font",		HandleLargerFont },
    { "smaller-vt-font",	HandleSmallerFont },
#endif
#if OPT_TEK4014
    { "set-terminal-type",	HandleSetTerminalType },
    { "set-visibility",		HandleVisibility },
    { "set-tek-text",		HandleSetTekText },
    { "tek-page",		HandleTekPage },
    { "tek-reset",		HandleTekReset },
    { "tek-copy",		HandleTekCopy },
#endif
#if OPT_TOOLBAR
    { "set-toolbar",		HandleToolbar },
#endif
#if OPT_WIDE_CHARS
    { "set-utf8-mode",		HandleUTF8Mode },
#endif
};
/* *INDENT-ON* */

static XtResource resources[] =
{
    Bres(XtNallowSendEvents, XtCAllowSendEvents, screen.allowSendEvent0, False),
    Bres(XtNallowWindowOps, XtCAllowWindowOps, screen.allowWindowOp0, True),
    Bres(XtNalwaysHighlight, XtCAlwaysHighlight, screen.always_highlight, False),
    Bres(XtNappcursorDefault, XtCAppcursorDefault, misc.appcursorDefault, False),
    Bres(XtNappkeypadDefault, XtCAppkeypadDefault, misc.appkeypadDefault, False),
    Bres(XtNautoWrap, XtCAutoWrap, misc.autoWrap, True),
    Bres(XtNawaitInput, XtCAwaitInput, screen.awaitInput, False),
    Bres(XtNfreeBoldBox, XtCFreeBoldBox, screen.free_bold_box, False),
    Bres(XtNbackarrowKey, XtCBackarrowKey, screen.backarrow_key, True),
    Bres(XtNbellOnReset, XtCBellOnReset, screen.bellOnReset, True),
    Bres(XtNboldMode, XtCBoldMode, screen.bold_mode, True),
    Bres(XtNbrokenSelections, XtCBrokenSelections, screen.brokenSelections, False),
    Bres(XtNc132, XtCC132, screen.c132, False),
    Bres(XtNcurses, XtCCurses, screen.curses, False),
    Bres(XtNcutNewline, XtCCutNewline, screen.cutNewline, True),
    Bres(XtNcutToBeginningOfLine, XtCCutToBeginningOfLine,
	 screen.cutToBeginningOfLine, True),
    Bres(XtNdeleteIsDEL, XtCDeleteIsDEL, screen.delete_is_del, DEFDELETE_DEL),
    Bres(XtNdynamicColors, XtCDynamicColors, misc.dynamicColors, True),
    Bres(XtNeightBitControl, XtCEightBitControl, screen.control_eight_bits, False),
    Bres(XtNeightBitInput, XtCEightBitInput, screen.input_eight_bits, True),
    Bres(XtNeightBitOutput, XtCEightBitOutput, screen.output_eight_bits, True),
    Bres(XtNhighlightSelection, XtCHighlightSelection,
	 screen.highlight_selection, False),
    Bres(XtNhpLowerleftBugCompat, XtCHpLowerleftBugCompat, screen.hp_ll_bc, False),
    Bres(XtNi18nSelections, XtCI18nSelections, screen.i18nSelections, True),
    Bres(XtNjumpScroll, XtCJumpScroll, screen.jumpscroll, True),
    Bres(XtNloginShell, XtCLoginShell, misc.login_shell, False),
    Bres(XtNmarginBell, XtCMarginBell, screen.marginbell, False),
    Bres(XtNmetaSendsEscape, XtCMetaSendsEscape, screen.meta_sends_esc, False),
    Bres(XtNmultiScroll, XtCMultiScroll, screen.multiscroll, False),
    Bres(XtNoldXtermFKeys, XtCOldXtermFKeys, screen.old_fkeys, False),
    Bres(XtNpopOnBell, XtCPopOnBell, screen.poponbell, False),
    Bres(XtNprinterAutoClose, XtCPrinterAutoClose, screen.printer_autoclose, False),
    Bres(XtNprinterExtent, XtCPrinterExtent, screen.printer_extent, False),
    Bres(XtNprinterFormFeed, XtCPrinterFormFeed, screen.printer_formfeed, False),
    Bres(XtNreverseVideo, XtCReverseVideo, misc.re_verse, False),
    Bres(XtNreverseWrap, XtCReverseWrap, misc.reverseWrap, False),
    Bres(XtNscrollBar, XtCScrollBar, misc.scrollbar, False),
    Bres(XtNscrollKey, XtCScrollCond, screen.scrollkey, False),
    Bres(XtNscrollTtyOutput, XtCScrollCond, screen.scrollttyoutput, True),
    Bres(XtNsignalInhibit, XtCSignalInhibit, misc.signalInhibit, False),
    Bres(XtNtiteInhibit, XtCTiteInhibit, misc.titeInhibit, False),
    Bres(XtNtiXtraScroll, XtCTiXtraScroll, misc.tiXtraScroll, False),
    Bres(XtNtrimSelection, XtCTrimSelection, screen.trim_selection, False),
    Bres(XtNunderLine, XtCUnderLine, screen.underline, True),
    Bres(XtNvisualBell, XtCVisualBell, screen.visualbell, False),

    Ires(XtNbellSuppressTime, XtCBellSuppressTime, screen.bellSuppressTime, BELLSUPPRESSMSEC),
    Ires(XtNinternalBorder, XtCBorderWidth, screen.border, DEFBORDER),
    Ires(XtNlimitResize, XtCLimitResize, misc.limit_resize, 1),
    Ires(XtNmultiClickTime, XtCMultiClickTime, screen.multiClickTime, MULTICLICKTIME),
    Ires(XtNnMarginBell, XtCColumn, screen.nmarginbell, N_MARGINBELL),
    Ires(XtNprinterControlMode, XtCPrinterControlMode,
	 screen.printer_controlmode, 0),
    Ires(XtNvisualBellDelay, XtCVisualBellDelay, screen.visualBellDelay, 100),
    Ires(XtNsaveLines, XtCSaveLines, screen.savelines, SAVELINES),
    Ires(XtNscrollBarBorder, XtCScrollBarBorder, screen.scrollBarBorder, 1),
    Ires(XtNscrollLines, XtCScrollLines, screen.scrolllines, SCROLLLINES),

    Sres(XtNfont1, XtCFont1, screen.MenuFontName(fontMenu_font1), NULL),
    Sres(XtNfont2, XtCFont2, screen.MenuFontName(fontMenu_font2), NULL),
    Sres(XtNfont3, XtCFont3, screen.MenuFontName(fontMenu_font3), NULL),
    Sres(XtNfont4, XtCFont4, screen.MenuFontName(fontMenu_font4), NULL),
    Sres(XtNfont5, XtCFont5, screen.MenuFontName(fontMenu_font5), NULL),
    Sres(XtNfont6, XtCFont6, screen.MenuFontName(fontMenu_font6), NULL),
    Sres(XtNanswerbackString, XtCAnswerbackString, screen.answer_back, ""),
    Sres(XtNboldFont, XtCBoldFont, misc.default_font.f_b, DEFBOLDFONT),
    Sres(XtNcharClass, XtCCharClass, screen.charClass, NULL),
    Sres(XtNdecTerminalID, XtCDecTerminalID, screen.term_id, DFT_DECID),
    Sres(XtNfont, XtCFont, misc.default_font.f_n, DEFFONT),
    Sres(XtNgeometry, XtCGeometry, misc.geo_metry, NULL),
    Sres(XtNkeyboardDialect, XtCKeyboardDialect, screen.keyboard_dialect, DFT_KBD_DIALECT),
    Sres(XtNprinterCommand, XtCPrinterCommand, screen.printer_command, ""),
    Sres(XtNtekGeometry, XtCGeometry, misc.T_geometry, NULL),

    Tres(XtNcursorColor, XtCCursorColor, TEXT_CURSOR, XtDefaultForeground),
    Tres(XtNforeground, XtCForeground, TEXT_FG, XtDefaultForeground),
    Tres(XtNpointerColor, XtCPointerColor, MOUSE_FG, XtDefaultForeground),
    Tres(XtNbackground, XtCBackground, TEXT_BG, XtDefaultBackground),
    Tres(XtNpointerColorBackground, XtCBackground, MOUSE_BG, XtDefaultBackground),

    {XtNresizeGravity, XtCResizeGravity, XtRGravity, sizeof(XtGravity),
     XtOffsetOf(XtermWidgetRec, misc.resizeGravity),
     XtRImmediate, (XtPointer) SouthWestGravity},

    {XtNpointerShape, XtCCursor, XtRCursor, sizeof(Cursor),
     XtOffsetOf(XtermWidgetRec, screen.pointer_cursor),
     XtRString, (XtPointer) "xterm"},

#ifdef ALLOWLOGGING
    Bres(XtNlogInhibit, XtCLogInhibit, misc.logInhibit, False),
    Bres(XtNlogging, XtCLogging, misc.log_on, False),
    Sres(XtNlogFile, XtCLogfile, screen.logfile, NULL),
#endif

#ifndef NO_ACTIVE_ICON
    Bres("activeIcon", "ActiveIcon", misc.active_icon, False),
    Ires("iconBorderWidth", XtCBorderWidth, misc.icon_border_width, 2),
    Fres("iconFont", "IconFont", screen.fnt_icon, XtDefaultFont),
    Cres("iconBorderColor", XtCBorderColor, misc.icon_border_pixel, XtDefaultBackground),
#endif				/* NO_ACTIVE_ICON */

#if OPT_BLINK_CURS
    Bres(XtNcursorBlink, XtCCursorBlink, screen.cursor_blink, False),
#endif

#if OPT_BLINK_TEXT
    Bres(XtNshowBlinkAsBold, XtCCursorBlink, screen.blink_as_bold, DEFBLINKASBOLD),
#endif

#if OPT_BLINK_CURS || OPT_BLINK_TEXT
    Ires(XtNcursorOnTime, XtCCursorOnTime, screen.blink_on, 600),
    Ires(XtNcursorOffTime, XtCCursorOffTime, screen.blink_off, 300),
#endif

#if OPT_BOX_CHARS
    Bres(XtNforceBoxChars, XtCForceBoxChars, screen.force_box_chars, False),
    Bres(XtNshowMissingGlyphs, XtCShowMissingGlyphs, screen.force_all_chars, False),
#endif

#if OPT_BROKEN_OSC
    Bres(XtNbrokenLinuxOSC, XtCBrokenLinuxOSC, screen.brokenLinuxOSC, True),
#endif

#if OPT_BROKEN_ST
    Bres(XtNbrokenStringTerm, XtCBrokenStringTerm, screen.brokenStringTerm, True),
#endif

#if OPT_C1_PRINT
    Bres(XtNallowC1Printable, XtCAllowC1Printable, screen.c1_printable, False),
#endif

#if OPT_DEC_CHRSET
    Bres(XtNfontDoublesize, XtCFontDoublesize, screen.font_doublesize, True),
    Ires(XtNcacheDoublesize, XtCCacheDoublesize, screen.cache_doublesize, NUM_CHRSET),
#endif

#if OPT_HIGHLIGHT_COLOR
    Tres(XtNhighlightColor, XtCHighlightColor, HIGHLIGHT_BG, XtDefaultForeground),
#endif				/* OPT_HIGHLIGHT_COLOR */

#if OPT_INPUT_METHOD
    Bres(XtNopenIm, XtCOpenIm, misc.open_im, True),
    Sres(XtNinputMethod, XtCInputMethod, misc.input_method, NULL),
    Sres(XtNpreeditType, XtCPreeditType, misc.preedit_type,
	 "OverTheSpot,Root"),
#endif

#if OPT_ISO_COLORS
    Bres(XtNboldColors, XtCColorMode, screen.boldColors, True),
    Ires(XtNveryBoldColors, XtCVeryBoldColors, screen.veryBoldColors, 0),
    Bres(XtNcolorMode, XtCColorMode, screen.colorMode, DFT_COLORMODE),

    Bres(XtNcolorAttrMode, XtCColorAttrMode, screen.colorAttrMode, False),
    Bres(XtNcolorBDMode, XtCColorAttrMode, screen.colorBDMode, False),
    Bres(XtNcolorBLMode, XtCColorAttrMode, screen.colorBLMode, False),
    Bres(XtNcolorRVMode, XtCColorAttrMode, screen.colorRVMode, False),
    Bres(XtNcolorULMode, XtCColorAttrMode, screen.colorULMode, False),
    Bres(XtNitalicULMode, XtCColorAttrMode, screen.italicULMode, False),

    COLOR_RES("0", screen.Acolors[COLOR_0], DFT_COLOR("black")),
    COLOR_RES("1", screen.Acolors[COLOR_1], DFT_COLOR("red3")),
    COLOR_RES("2", screen.Acolors[COLOR_2], DFT_COLOR("green3")),
    COLOR_RES("3", screen.Acolors[COLOR_3], DFT_COLOR("yellow3")),
    COLOR_RES("4", screen.Acolors[COLOR_4], DFT_COLOR(DEF_COLOR4)),
    COLOR_RES("5", screen.Acolors[COLOR_5], DFT_COLOR("magenta3")),
    COLOR_RES("6", screen.Acolors[COLOR_6], DFT_COLOR("cyan3")),
    COLOR_RES("7", screen.Acolors[COLOR_7], DFT_COLOR("gray90")),
    COLOR_RES("8", screen.Acolors[COLOR_8], DFT_COLOR("gray50")),
    COLOR_RES("9", screen.Acolors[COLOR_9], DFT_COLOR("red")),
    COLOR_RES("10", screen.Acolors[COLOR_10], DFT_COLOR("green")),
    COLOR_RES("11", screen.Acolors[COLOR_11], DFT_COLOR("yellow")),
    COLOR_RES("12", screen.Acolors[COLOR_12], DFT_COLOR(DEF_COLOR12)),
    COLOR_RES("13", screen.Acolors[COLOR_13], DFT_COLOR("magenta")),
    COLOR_RES("14", screen.Acolors[COLOR_14], DFT_COLOR("cyan")),
    COLOR_RES("15", screen.Acolors[COLOR_15], DFT_COLOR("white")),
    COLOR_RES("BD", screen.Acolors[COLOR_BD], DFT_COLOR(XtDefaultForeground)),
    COLOR_RES("BL", screen.Acolors[COLOR_BL], DFT_COLOR(XtDefaultForeground)),
    COLOR_RES("UL", screen.Acolors[COLOR_UL], DFT_COLOR(XtDefaultForeground)),
    COLOR_RES("RV", screen.Acolors[COLOR_RV], DFT_COLOR(XtDefaultForeground)),

#if !OPT_COLOR_RES2
#if OPT_256_COLORS
# include <256colres.h>
#elif OPT_88_COLORS
# include <88colres.h>
#endif
#endif				/* !OPT_COLOR_RES2 */

#endif				/* OPT_ISO_COLORS */

#if OPT_MOD_FKEYS
    Ires(XtNmodifyCursorKeys, XtCModifyCursorKeys,
	 keyboard.modify_cursor_keys, 2),
#endif

#if OPT_NUM_LOCK
    Bres(XtNalwaysUseMods, XtCAlwaysUseMods, misc.alwaysUseMods, False),
    Bres(XtNnumLock, XtCNumLock, misc.real_NumLock, True),
#endif

#if OPT_PRINT_COLORS
    Ires(XtNprintAttributes, XtCPrintAttributes, screen.print_attributes, 1),
#endif

#if OPT_SHIFT_FONTS
    Bres(XtNshiftFonts, XtCShiftFonts, misc.shift_fonts, True),
#endif

#if OPT_SUNPC_KBD
    Ires(XtNctrlFKeys, XtCCtrlFKeys, misc.ctrl_fkeys, 10),
#endif

#if OPT_TEK4014
    Bres(XtNtekInhibit, XtCTekInhibit, misc.tekInhibit, False),
    Bres(XtNtekSmall, XtCTekSmall, misc.tekSmall, False),
    Bres(XtNtekStartup, XtCTekStartup, screen.TekEmu, False),
#endif

#if OPT_TOOLBAR
    Wres(XtNmenuBar, XtCMenuBar, VT100_TB_INFO(menu_bar), 0),
    Ires(XtNmenuHeight, XtCMenuHeight, VT100_TB_INFO(menu_height), 25),
#endif

#if OPT_WIDE_CHARS
    Ires(XtNutf8, XtCUtf8, screen.utf8_mode, uDefault),
    Bres(XtNwideChars, XtCWideChars, screen.wide_chars, False),
    Bres(XtNmkWidth, XtCMkWidth, misc.mk_width, False),
    Bres(XtNcjkWidth, XtCCjkWidth, misc.cjk_width, False),
    Bres(XtNvt100Graphics, XtCVT100Graphics, screen.vt100_graphics, True),
    Sres(XtNwideBoldFont, XtCWideBoldFont, misc.default_font.f_wb, DEFWIDEBOLDFONT),
    Sres(XtNwideFont, XtCWideFont, misc.default_font.f_w, DEFWIDEFONT),
#endif

#if OPT_LUIT_PROG
    Sres(XtNlocale, XtCLocale, misc.locale_str, "medium"),
    Sres(XtNlocaleFilter, XtCLocaleFilter, misc.localefilter, DEFLOCALEFILTER),
#endif

#if OPT_INPUT_METHOD
    Sres(XtNximFont, XtCXimFont, misc.f_x, DEFXIMFONT),
#endif

#if OPT_XMC_GLITCH
    Bres(XtNxmcInline, XtCXmcInline, screen.xmc_inline, False),
    Bres(XtNxmcMoveSGR, XtCXmcMoveSGR, screen.move_sgr_ok, True),
    Ires(XtNxmcAttributes, XtCXmcAttributes, screen.xmc_attributes, 1),
    Ires(XtNxmcGlitch, XtCXmcGlitch, screen.xmc_glitch, 0),
#endif

#ifdef SCROLLBAR_RIGHT
    Bres(XtNrightScrollBar, XtCRightScrollBar, misc.useRight, False),
#endif

#if OPT_RENDERFONT
    Dres(XtNfaceSize, XtCFaceSize, misc.face_size, DEFFACESIZE),
    Sres(XtNfaceName, XtCFaceName, misc.face_name, DEFFACENAME),
    Sres(XtNfaceNameDoublesize, XtCFaceNameDoublesize, misc.face_wide_name, DEFFACENAME),
    Bres(XtNrenderFont, XtCRenderFont, misc.render_font, True),
#endif
};

static Boolean VTSetValues(Widget cur, Widget request, Widget new_arg,
			   ArgList args, Cardinal *num_args);
static void VTClassInit(void);
static void VTDestroy(Widget w);
static void VTExpose(Widget w, XEvent * event, Region region);
static void VTInitialize(Widget wrequest, Widget new_arg, ArgList args,
			 Cardinal *num_args);
static void VTRealize(Widget w, XtValueMask * valuemask,
		      XSetWindowAttributes * values);
static void VTResize(Widget w);

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
static void VTInitI18N(void);
#endif

#ifdef VMS
globaldef {
    "xtermclassrec"
} noshare

#else
static
#endif				/* VMS */
WidgetClassRec xtermClassRec =
{
    {
/* core_class fields */
	(WidgetClass) & widgetClassRec,		/* superclass     */
	"VT100",		/* class_name                   */
	sizeof(XtermWidgetRec),	/* widget_size                  */
	VTClassInit,		/* class_initialize             */
	NULL,			/* class_part_initialize        */
	False,			/* class_inited                 */
	VTInitialize,		/* initialize                   */
	NULL,			/* initialize_hook              */
	VTRealize,		/* realize                      */
	actionsList,		/* actions                      */
	XtNumber(actionsList),	/* num_actions                  */
	resources,		/* resources                    */
	XtNumber(resources),	/* num_resources                */
	NULLQUARK,		/* xrm_class                    */
	True,			/* compress_motion              */
	False,			/* compress_exposure            */
	True,			/* compress_enterleave          */
	False,			/* visible_interest             */
	VTDestroy,		/* destroy                      */
	VTResize,		/* resize                       */
	VTExpose,		/* expose                       */
	VTSetValues,		/* set_values                   */
	NULL,			/* set_values_hook              */
	XtInheritSetValuesAlmost,	/* set_values_almost    */
	NULL,			/* get_values_hook              */
	NULL,			/* accept_focus                 */
	XtVersion,		/* version                      */
	NULL,			/* callback_offsets             */
	defaultTranslations,	/* tm_table                     */
	XtInheritQueryGeometry,	/* query_geometry               */
	XtInheritDisplayAccelerator,	/* display_accelerator  */
	NULL			/* extension                    */
    }
};

#ifdef VMS
globaldef {
    "xtermwidgetclass"
}
noshare
#endif /* VMS */
WidgetClass xtermWidgetClass = (WidgetClass) & xtermClassRec;

/*
 * Add input-actions for widgets that are overlooked (scrollbar and toolbar):
 *
 *	a) Sometimes the scrollbar passes through translations, sometimes it
 *	   doesn't.  We add the KeyPress translations here, just to be sure.
 *	b) In the normal (non-toolbar) configuration, the xterm widget covers
 *	   almost all of the window.  With a toolbar, there's a relatively
 *	   large area that the user would expect to enter keystrokes since the
 *	   program can get the focus.
 */
void
xtermAddInput(Widget w)
{
#if OPT_TOOLBAR
    /* *INDENT-OFF* */
    XtActionsRec input_actions[] = {
	{ "insert",		    HandleKeyPressed }, /* alias */
	{ "insert-eight-bit",	    HandleEightBitKeyPressed },
	{ "insert-seven-bit",	    HandleKeyPressed },
	{ "secure",		    HandleSecure },
	{ "string",		    HandleStringEvent },
	{ "scroll-back",	    HandleScrollBack },
	{ "scroll-forw",	    HandleScrollForward },
	{ "select-cursor-end",	    HandleKeyboardSelectEnd },
	{ "select-cursor-extend",   HandleKeyboardSelectExtend },
	{ "select-cursor-start",    HandleKeyboardSelectStart },
	{ "insert-selection",	    HandleInsertSelection },
	{ "select-start",	    HandleSelectStart },
	{ "select-extend",	    HandleSelectExtend },
	{ "start-extend",	    HandleStartExtend },
	{ "select-end",		    HandleSelectEnd },
	{ "clear-saved-lines",	    HandleClearSavedLines },
	{ "popup-menu",		    HandlePopupMenu },
	{ "bell",		    HandleBell },
	{ "ignore",		    HandleIgnore },
#if OPT_DABBREV
	{ "dabbrev-expand",	    HandleDabbrevExpand },
#endif
#if OPT_SHIFT_FONTS
	{ "larger-vt-font",	    HandleLargerFont },
	{ "smaller-vt-font",	    HandleSmallerFont },
#endif
    };
    /* *INDENT-ON* */

    XtAppAddActions(app_con, input_actions, XtNumber(input_actions));
#endif
    XtAugmentTranslations(w, XtParseTranslationTable(defaultTranslations));
}

#if OPT_ISO_COLORS
/*
 * The terminal's foreground and background colors are set via two mechanisms:
 *	text (cur_foreground, cur_background values that are passed down to
 *		XDrawImageString and XDrawString)
 *	area (X11 graphics context used in XClearArea and XFillRectangle)
 */
void
SGR_Foreground(int color)
{
    TScreen *screen = &term->screen;
    Pixel fg;

    if (color >= 0) {
	term->flags |= FG_COLOR;
    } else {
	term->flags &= ~FG_COLOR;
    }
    fg = getXtermForeground(term->flags, color);
    term->cur_foreground = color;

    XSetForeground(screen->display, NormalGC(screen), fg);
    XSetBackground(screen->display, ReverseGC(screen), fg);

    if (NormalGC(screen) != NormalBoldGC(screen)) {
	XSetForeground(screen->display, NormalBoldGC(screen), fg);
	XSetBackground(screen->display, ReverseBoldGC(screen), fg);
    }
}

void
SGR_Background(int color)
{
    TScreen *screen = &term->screen;
    Pixel bg;

    /*
     * An indexing operation may have set screen->scroll_amt, which would
     * normally result in calling FlushScroll() in WriteText().  However,
     * if we're changing the background color now, then the new value
     * should not apply to the pending blank lines.
     */
    if (screen->scroll_amt && (color != term->cur_background))
	FlushScroll(screen);

    if (color >= 0) {
	term->flags |= BG_COLOR;
    } else {
	term->flags &= ~BG_COLOR;
    }
    bg = getXtermBackground(term->flags, color);
    term->cur_background = color;

    XSetBackground(screen->display, NormalGC(screen), bg);
    XSetForeground(screen->display, ReverseGC(screen), bg);

    if (NormalGC(screen) != NormalBoldGC(screen)) {
	XSetBackground(screen->display, NormalBoldGC(screen), bg);
	XSetForeground(screen->display, ReverseBoldGC(screen), bg);
    }
}

/* Invoked after updating bold/underline flags, computes the extended color
 * index to use for foreground.  (See also 'extract_fg()').
 */
static void
setExtendedFG(void)
{
    int fg = term->sgr_foreground;

    if (term->screen.colorAttrMode
	|| (fg < 0)) {
	if (term->screen.colorULMode && (term->flags & UNDERLINE))
	    fg = COLOR_UL;
	if (term->screen.colorBDMode && (term->flags & BOLD))
	    fg = COLOR_BD;
	if (term->screen.colorBLMode && (term->flags & BLINK))
	    fg = COLOR_BL;
    }

    /* This implements the IBM PC-style convention of 8-colors, with one
     * bit for bold, thus mapping the 0-7 codes to 8-15.  It won't make
     * much sense for 16-color applications, but we keep it to retain
     * compatiblity with ANSI-color applications.
     */
#if OPT_PC_COLORS		/* XXXJTL should be settable at runtime (resource or OSC?) */
    if (term->screen.boldColors
	&& (!term->sgr_extended)
	&& (fg >= 0)
	&& (fg < 8)
	&& (term->flags & BOLD))
	fg |= 8;
#endif

    SGR_Foreground(fg);
}

/* Invoked after updating inverse flag, computes the extended color
 * index to use for background.  (See also 'extract_bg()').
 */
static void
setExtendedBG(void)
{
    int bg = term->sgr_background;

    if (term->screen.colorAttrMode
	|| (bg < 0)) {
	if (term->screen.colorRVMode && (term->flags & INVERSE))
	    bg = COLOR_RV;
    }

    SGR_Background(bg);
}

static void
reset_SGR_Foreground(void)
{
    term->sgr_foreground = -1;
    term->sgr_extended = 0;
    setExtendedFG();
}

static void
reset_SGR_Background(void)
{
    term->sgr_background = -1;
    setExtendedBG();
}

static void
reset_SGR_Colors(void)
{
    reset_SGR_Foreground();
    reset_SGR_Background();
}
#endif /* OPT_ISO_COLORS */

void
resetCharsets(TScreen * screen)
{
    TRACE(("resetCharsets\n"));

    screen->gsets[0] = 'B';	/* ASCII_G              */
    screen->gsets[1] = 'B';	/* ASCII_G              */
    screen->gsets[2] = 'B';	/* ASCII_G              */
    screen->gsets[3] = 'B';	/* ASCII_G              */

    screen->curgl = 0;		/* G0 => GL.            */
    screen->curgr = 2;		/* G2 => GR.            */
    screen->curss = 0;		/* No single shift.     */

#if OPT_VT52_MODE
    if (screen->vtXX_level == 0)
	screen->gsets[1] = '0';	/* Graphics             */
#endif
}

/*
 * VT300 and up support three ANSI conformance levels, defined according to
 * the dpANSI X3.134.1 standard.  DEC's manuals equate levels 1 and 2, and
 * are unclear.  This code is written based on the manuals.
 */
static void
set_ansi_conformance(TScreen * screen, int level)
{
    TRACE(("set_ansi_conformance(%d) terminal_id %d, ansi_level %d\n",
	   level,
	   screen->terminal_id,
	   screen->ansi_level));
    if (screen->vtXX_level >= 3) {
	switch (screen->ansi_level = level) {
	case 1:
	    /* FALLTHRU */
	case 2:
	    screen->gsets[0] = 'B';	/* G0 is ASCII */
	    screen->gsets[1] = 'B';	/* G1 is ISO Latin-1 (FIXME) */
	    screen->curgl = 0;
	    screen->curgr = 1;
	    break;
	case 3:
	    screen->gsets[0] = 'B';	/* G0 is ASCII */
	    screen->curgl = 0;
	    break;
	}
    }
}

/*
 * Set scrolling margins.  VTxxx terminals require that the top/bottom are
 * different, so we have at least two lines in the scrolling region.
 */
void
set_tb_margins(TScreen * screen, int top, int bottom)
{
    TRACE(("set_tb_margins %d..%d, prior %d..%d\n",
	   top, bottom,
	   screen->top_marg,
	   screen->bot_marg));
    if (bottom > top) {
	screen->top_marg = top;
	screen->bot_marg = bottom;
    }
    if (screen->top_marg > screen->max_row)
	screen->top_marg = screen->max_row;
    if (screen->bot_marg > screen->max_row)
	screen->bot_marg = screen->max_row;
}

void
set_max_col(TScreen * screen, int cols)
{
    TRACE(("set_max_col %d, prior %d\n", cols, screen->max_col));
    if (cols < 0)
	cols = 0;
    screen->max_col = cols;
}

void
set_max_row(TScreen * screen, int rows)
{
    TRACE(("set_max_row %d, prior %d\n", rows, screen->max_row));
    if (rows < 0)
	rows = 0;
    screen->max_row = rows;
}

#if OPT_TRACE
#define WHICH_TABLE(name) if (table == name) result = #name
static char *
which_table(Const PARSE_T * table)
{
    char *result = "?";
    /* *INDENT-OFF* */
    WHICH_TABLE (ansi_table);
    else WHICH_TABLE (csi_table);
    else WHICH_TABLE (csi2_table);
    else WHICH_TABLE (csi_ex_table);
    else WHICH_TABLE (csi_quo_table);
#if OPT_DEC_LOCATOR
    else WHICH_TABLE (csi_tick_table);
#endif
#if OPT_DEC_RECTOPS
    else WHICH_TABLE (csi_dollar_table);
    else WHICH_TABLE (csi_star_table);
#endif
    else WHICH_TABLE (dec_table);
    else WHICH_TABLE (dec2_table);
    else WHICH_TABLE (dec3_table);
    else WHICH_TABLE (cigtable);
    else WHICH_TABLE (eigtable);
    else WHICH_TABLE (esc_table);
    else WHICH_TABLE (esc_sp_table);
    else WHICH_TABLE (scrtable);
    else WHICH_TABLE (scstable);
    else WHICH_TABLE (sos_table);
#if OPT_WIDE_CHARS
    else WHICH_TABLE (esc_pct_table);
#endif
#if OPT_VT52_MODE
    else WHICH_TABLE (vt52_table);
    else WHICH_TABLE (vt52_esc_table);
    else WHICH_TABLE (vt52_ignore_table);
#endif
    /* *INDENT-ON* */

    return result;
}
#endif

	/* allocate larger buffer if needed/possible */
#define SafeAlloc(type, area, used, size) \
		type *new_string = area; \
		unsigned new_length = size; \
		if (new_length == 0) { \
		    new_length = 256; \
		    new_string = TypeMallocN(type, new_length); \
		} else if (used+1 >= new_length) { \
		    new_length = size * 2; \
		    new_string = TypeMallocN(type, new_length); \
		    if (new_string != 0 \
		     && area != 0 \
		     && used != 0) \
			memcpy(new_string, area, used * sizeof(type)); \
		}

#define WriteNow() {						\
	    unsigned single = 0;				\
								\
	    if (screen->curss) {				\
		dotext(screen,					\
		       screen->gsets[(int) (screen->curss)],	\
		       print_area, 1);				\
		screen->curss = 0;				\
		single++;					\
	    }							\
	    if (print_used > single) {				\
		dotext(screen,					\
		       screen->gsets[(int) (screen->curgl)],	\
		       print_area + single,			\
		       print_used - single);			\
	    }							\
	    print_used = 0;					\
	}							\

struct ParseState {
#if OPT_VT52_MODE
    Bool vt52_cup;
#endif
    Const PARSE_T *groundtable;
    Const PARSE_T *parsestate;
    int scstype;
    Bool private_function;	/* distinguish private-mode from standard */
    int string_mode;		/* nonzero iff we're processing a string */
    int lastchar;		/* positive iff we had a graphic character */
    int nextstate;
#if OPT_WIDE_CHARS
    int last_was_wide;
#endif
};

static struct ParseState myState;

static Boolean
doparsing(unsigned c, struct ParseState *sp)
{
    /* Buffer for processing printable text */
    static IChar *print_area;
    static size_t print_size, print_used;

    /* Buffer for processing strings (e.g., OSC ... ST) */
    static Char *string_area;
    static size_t string_size, string_used;

    TScreen *screen = &term->screen;
    int row;
    int col;
    int top;
    int bot;
    int count;
    int laststate;
    int thischar = -1;
    XTermRect myRect;

    do {
#if OPT_WIDE_CHARS

	/*
	 * Handle zero-width combining characters.  Make it faster by noting
	 * that according to the Unicode charts, the majority of Western
	 * character sets do not use this feature.  There are some unassigned
	 * codes at 0x242, but no zero-width characters until past 0x300.
	 */
	if (c >= 0x300 && screen->wide_chars
	    && my_wcwidth((int) c) == 0) {
	    int prev, precomposed;

	    WriteNow();

	    prev = getXtermCell(screen,
				screen->last_written_row,
				screen->last_written_col);
	    precomposed = do_precomposition(prev, (int) c);

	    if (precomposed != -1) {
		putXtermCell(screen,
			     screen->last_written_row,
			     screen->last_written_col, precomposed);
	    } else {
		addXtermCombining(screen,
				  screen->last_written_row,
				  screen->last_written_col, c);
	    }
	    if (!screen->scroll_amt)
		ScrnUpdate(screen,
			   screen->last_written_row,
			   screen->last_written_col, 1, 1, 1);
	    continue;
	}
#endif

	/* Intercept characters for printer controller mode */
	if (screen->printer_controlmode == 2) {
	    if ((c = xtermPrinterControl((int) c)) == 0)
		continue;
	}

	/*
	 * VT52 is a little ugly in the one place it has a parameterized
	 * control sequence, since the parameter falls after the character
	 * that denotes the type of sequence.
	 */
#if OPT_VT52_MODE
	if (sp->vt52_cup) {
	    if (nparam < NPARAM)
		param[nparam++] = (c & 0x7f) - 32;
	    if (nparam < 2)
		continue;
	    sp->vt52_cup = False;
	    if ((row = param[0]) < 0)
		row = 0;
	    if ((col = param[1]) < 0)
		col = 0;
	    CursorSet(screen, row, col, term->flags);
	    sp->parsestate = vt52_table;
	    param[0] = 0;
	    param[1] = 0;
	    continue;
	}
#endif

	/*
	 * The parsing tables all have 256 entries.  If we're supporting
	 * wide characters, we handle them by treating them the same as
	 * printing characters.
	 */
	laststate = sp->nextstate;
#if OPT_WIDE_CHARS
	if (c > 255) {
	    if (sp->parsestate == sp->groundtable) {
		sp->nextstate = CASE_PRINT;
	    } else if (sp->parsestate == sos_table) {
		c &= 0xffff;
		if (c > 255) {
		    TRACE(("Found code > 255 while in SOS state: %04X\n", c));
		    c = '?';
		}
	    } else {
		sp->nextstate = CASE_GROUND_STATE;
	    }
	} else
#endif
	    sp->nextstate = sp->parsestate[E2A(c)];

#if OPT_BROKEN_OSC
	/*
	 * Linux console palette escape sequences start with an OSC, but do
	 * not terminate correctly.  Some scripts do not check before writing
	 * them, making xterm appear to hang (it's awaiting a valid string
	 * terminator).  Just ignore these if we see them - there's no point
	 * in emulating bad code.
	 */
	if (screen->brokenLinuxOSC
	    && sp->parsestate == sos_table) {
	    if (string_used) {
		switch (string_area[0]) {
		case 'P':
		    if (string_used <= 7)
			break;
		    /* FALLTHRU */
		case 'R':
		    sp->parsestate = sp->groundtable;
		    sp->nextstate = sp->parsestate[E2A(c)];
		    TRACE(("Reset to ground state (brokenLinuxOSC)\n"));
		    break;
		}
	    }
	}
#endif

#if OPT_BROKEN_ST
	/*
	 * Before patch #171, carriage control embedded within an OSC string
	 * would terminate it.  Some (buggy, of course) applications rely on
	 * this behavior.  Accommodate them by allowing one to compile xterm
	 * and emulate the old behavior.
	 */
	if (screen->brokenStringTerm
	    && sp->parsestate == sos_table
	    && c < 32) {
	    switch (c) {
	    case 5:		/* FALLTHRU */
	    case 8:		/* FALLTHRU */
	    case 9:		/* FALLTHRU */
	    case 10:		/* FALLTHRU */
	    case 11:		/* FALLTHRU */
	    case 12:		/* FALLTHRU */
	    case 13:		/* FALLTHRU */
	    case 14:		/* FALLTHRU */
	    case 15:		/* FALLTHRU */
	    case 24:
		sp->parsestate = sp->groundtable;
		sp->nextstate = sp->parsestate[E2A(c)];
		TRACE(("Reset to ground state (brokenStringTerm)\n"));
		break;
	    }
	}
#endif

#if OPT_C1_PRINT
	/*
	 * This is not completely foolproof, but will allow an application
	 * with values in the C1 range to use them as printable characters,
	 * provided that they are not intermixed with an escape sequence.
	 */
	if (screen->c1_printable
	    && (c >= 128 && c < 160)) {
	    sp->nextstate = (sp->parsestate == esc_table
			     ? CASE_ESC_IGNORE
			     : sp->parsestate[E2A(160)]);
	}
#endif

#if OPT_WIDE_CHARS
	/*
	 * If we have a C1 code and the c1_printable flag is not set, simply
	 * ignore it when it was translated from UTF-8.  That is because the
	 * value could not have been present as-is in the UTF-8.
	 *
	 * To see that CASE_IGNORE is a consistent value, note that it is
	 * always used for NUL and other uninteresting C0 controls.
	 */
#if OPT_C1_PRINT
	if (!screen->c1_printable)
#endif
	    if (screen->wide_chars
		&& (c >= 128 && c < 160)) {
		sp->nextstate = CASE_IGNORE;
	    }

	/*
	 * If this character is a different width than the last one, put the
	 * previous text into the buffer and draw it now.
	 */
	if (iswide((int) c) != sp->last_was_wide) {
	    WriteNow();
	}
#endif

	/*
	 * Accumulate string for printable text.  This may be 8/16-bit
	 * characters.
	 */
	if (sp->nextstate == CASE_PRINT) {
	    SafeAlloc(IChar, print_area, print_used, print_size);
	    if (new_string == 0) {
		fprintf(stderr,
			"Cannot allocate %u bytes for printable text\n",
			new_length);
		continue;
	    }
#if OPT_VT52_MODE
	    /*
	     * Strip output text to 7-bits for VT52.  We should do this for
	     * VT100 also (which is a 7-bit device), but xterm has been
	     * doing this for so long we shouldn't change this behavior.
	     */
	    if (screen->vtXX_level < 1)
		c &= 0x7f;
#endif
	    print_area = new_string;
	    print_size = new_length;
	    print_area[print_used++] = sp->lastchar = thischar = c;
#if OPT_WIDE_CHARS
	    sp->last_was_wide = iswide((int) c);
#endif
	    if (morePtyData(screen, VTbuffer)) {
		continue;
	    }
	}

	if (sp->nextstate == CASE_PRINT
	    || (laststate == CASE_PRINT && print_used)) {
	    WriteNow();
	}

	/*
	 * Accumulate string for APC, DCS, PM, OSC, SOS controls
	 * This should always be 8-bit characters.
	 */
	if (sp->parsestate == sos_table) {
	    SafeAlloc(Char, string_area, string_used, string_size);
	    if (new_string == 0) {
		fprintf(stderr,
			"Cannot allocate %u bytes for string mode %d\n",
			new_length, sp->string_mode);
		continue;
	    }
#if OPT_WIDE_CHARS
	    /*
	     * We cannot display codes above 255, but let's try to
	     * accommodate the application a little by not aborting the
	     * string.
	     */
	    if ((c & 0xffff) > 255) {
		sp->nextstate = CASE_PRINT;
		c = '?';
	    }
#endif
	    string_area = new_string;
	    string_size = new_length;
	    string_area[string_used++] = c;
	} else if (sp->parsestate != esc_table) {
	    /* if we were accumulating, we're not any more */
	    sp->string_mode = 0;
	    string_used = 0;
	}

	TRACE(("parse %04X -> %d %s\n", c, sp->nextstate, which_table(sp->parsestate)));

	switch (sp->nextstate) {
	case CASE_PRINT:
	    TRACE(("CASE_PRINT - printable characters\n"));
	    break;

	case CASE_GROUND_STATE:
	    TRACE(("CASE_GROUND_STATE - exit ignore mode\n"));
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_IGNORE:
	    TRACE(("CASE_IGNORE - Ignore character %02X\n", c));
	    break;

	case CASE_ENQ:
	    TRACE(("CASE_ENQ - answerback\n"));
	    for (count = 0; screen->answer_back[count] != 0; count++)
		unparseputc(screen->answer_back[count], screen->respond);
	    break;

	case CASE_BELL:
	    TRACE(("CASE_BELL - bell\n"));
	    if (sp->string_mode == OSC) {
		if (string_used)
		    string_area[--string_used] = '\0';
		do_osc(string_area, string_used, (int) c);
		sp->parsestate = sp->groundtable;
	    } else {
		/* bell */
		Bell(XkbBI_TerminalBell, 0);
	    }
	    break;

	case CASE_BS:
	    TRACE(("CASE_BS - backspace\n"));
	    CursorBack(screen, 1);
	    break;

	case CASE_CR:
	    /* CR */
	    CarriageReturn(screen);
	    break;

	case CASE_ESC:
	    if_OPT_VT52_MODE(screen, {
		sp->parsestate = vt52_esc_table;
		break;
	    });
	    sp->parsestate = esc_table;
	    break;

#if OPT_VT52_MODE
	case CASE_VT52_CUP:
	    TRACE(("CASE_VT52_CUP - VT52 cursor addressing\n"));
	    sp->vt52_cup = True;
	    nparam = 0;
	    break;

	case CASE_VT52_IGNORE:
	    TRACE(("CASE_VT52_IGNORE - VT52 ignore-character\n"));
	    sp->parsestate = vt52_ignore_table;
	    break;
#endif

	case CASE_VMOT:
	    /*
	     * form feed, line feed, vertical tab
	     */
	    xtermAutoPrint((int) c);
	    xtermIndex(screen, 1);
	    if (term->flags & LINEFEED)
		CarriageReturn(screen);
	    do_xevents();
	    break;

	case CASE_CBT:
	    /* cursor backward tabulation */
	    if ((count = param[0]) == DEFAULT)
		count = 1;
	    while ((count-- > 0)
		   && (TabToPrevStop(screen))) ;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CHT:
	    /* cursor forward tabulation */
	    if ((count = param[0]) == DEFAULT)
		count = 1;
	    while ((count-- > 0)
		   && (TabToNextStop(screen))) ;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_TAB:
	    /* tab */
	    TabToNextStop(screen);
	    break;

	case CASE_SI:
	    screen->curgl = 0;
	    if_OPT_VT52_MODE(screen, {
		sp->parsestate = sp->groundtable;
	    });
	    break;

	case CASE_SO:
	    screen->curgl = 1;
	    if_OPT_VT52_MODE(screen, {
		sp->parsestate = sp->groundtable;
	    });
	    break;

	case CASE_DECDHL:
	    xterm_DECDHL(c == '3');
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSWL:
	    xterm_DECSWL();
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECDWL:
	    xterm_DECDWL();
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SCR_STATE:
	    /* enter scr state */
	    sp->parsestate = scrtable;
	    break;

	case CASE_SCS0_STATE:
	    /* enter scs state 0 */
	    sp->scstype = 0;
	    sp->parsestate = scstable;
	    break;

	case CASE_SCS1_STATE:
	    /* enter scs state 1 */
	    sp->scstype = 1;
	    sp->parsestate = scstable;
	    break;

	case CASE_SCS2_STATE:
	    /* enter scs state 2 */
	    sp->scstype = 2;
	    sp->parsestate = scstable;
	    break;

	case CASE_SCS3_STATE:
	    /* enter scs state 3 */
	    sp->scstype = 3;
	    sp->parsestate = scstable;
	    break;

	case CASE_ESC_IGNORE:
	    /* unknown escape sequence */
	    sp->parsestate = eigtable;
	    break;

	case CASE_ESC_DIGIT:
	    /* digit in csi or dec mode */
	    if ((row = param[nparam - 1]) == DEFAULT)
		row = 0;
	    param[nparam - 1] = 10 * row + (c - '0');
	    if (param[nparam - 1] > 65535)
		param[nparam - 1] = 65535;
	    if (sp->parsestate == csi_table)
		sp->parsestate = csi2_table;
	    break;

	case CASE_ESC_SEMI:
	    /* semicolon in csi or dec mode */
	    if (nparam < NPARAM)
		param[nparam++] = DEFAULT;
	    if (sp->parsestate == csi_table)
		sp->parsestate = csi2_table;
	    break;

	case CASE_DEC_STATE:
	    /* enter dec mode */
	    sp->parsestate = dec_table;
	    break;

	case CASE_DEC2_STATE:
	    /* enter dec2 mode */
	    sp->parsestate = dec2_table;
	    break;

	case CASE_DEC3_STATE:
	    /* enter dec3 mode */
	    sp->parsestate = dec3_table;
	    break;

	case CASE_ICH:
	    TRACE(("CASE_ICH - insert char\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    InsertChar(screen, (unsigned) row);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CUU:
	    TRACE(("CASE_CUU - cursor up\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    CursorUp(screen, row);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CUD:
	    TRACE(("CASE_CUD - cursor down\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    CursorDown(screen, row);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CUF:
	    TRACE(("CASE_CUF - cursor forward\n"));
	    if ((col = param[0]) < 1)
		col = 1;
	    CursorForward(screen, col);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CUB:
	    TRACE(("CASE_CUB - cursor backward\n"));
	    if ((col = param[0]) < 1)
		col = 1;
	    CursorBack(screen, col);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CUP:
	    TRACE(("CASE_CUP - cursor position\n"));
	    if_OPT_XMC_GLITCH(screen, {
		Jump_XMC(screen);
	    });
	    if ((row = param[0]) < 1)
		row = 1;
	    if (nparam < 2 || (col = param[1]) < 1)
		col = 1;
	    CursorSet(screen, row - 1, col - 1, term->flags);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_VPA:
	    TRACE(("CASE_VPA - vertical position\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    CursorSet(screen, row - 1, screen->cur_col, term->flags);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_HPA:
	    TRACE(("CASE_HPA - horizontal position\n"));
	    if ((col = param[0]) < 1)
		col = 1;
	    CursorSet(screen, screen->cur_row, col - 1, term->flags);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_HP_BUGGY_LL:
	    TRACE(("CASE_HP_BUGGY_LL\n"));
	    /* Some HP-UX applications have the bug that they
	       assume ESC F goes to the lower left corner of
	       the screen, regardless of what terminfo says. */
	    if (screen->hp_ll_bc)
		CursorSet(screen, screen->max_row, 0, term->flags);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_ED:
	    TRACE(("CASE_ED - erase display\n"));
	    do_erase_display(screen, param[0], OFF_PROTECT);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_EL:
	    TRACE(("CASE_EL - erase line\n"));
	    do_erase_line(screen, param[0], OFF_PROTECT);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_ECH:
	    TRACE(("CASE_ECH - erase char\n"));
	    /* ECH */
	    ClearRight(screen, param[0] < 1 ? 1 : param[0]);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_IL:
	    TRACE(("CASE_IL - insert line\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    InsertLine(screen, row);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DL:
	    TRACE(("CASE_DL - delete line\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    DeleteLine(screen, row);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DCH:
	    TRACE(("CASE_DCH - delete char\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    DeleteChar(screen, (unsigned) row);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_TRACK_MOUSE:
	    /*
	     * A single parameter other than zero is always scroll-down.
	     * A zero-parameter is used to reset the mouse mode, and is
	     * not useful for scrolling anyway.
	     */
	    if (nparam > 1 || param[0] == 0) {
		TRACE(("CASE_TRACK_MOUSE\n"));
		/* Track mouse as long as in window and between
		 * specified rows
		 */
		TrackMouse(param[0],
			   param[2] - 1, param[1] - 1,
			   param[3] - 1, param[4] - 2);
	    } else {
		TRACE(("CASE_SD - scroll down\n"));
		/* SD */
		if ((count = param[0]) < 1)
		    count = 1;
		RevScroll(screen, count);
		do_xevents();
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECID:
	    TRACE(("CASE_DECID\n"));
	    if_OPT_VT52_MODE(screen, {
		unparseputc(ESC, screen->respond);
		unparseputc('/', screen->respond);
		unparseputc('Z', screen->respond);
		sp->parsestate = sp->groundtable;
		break;
	    });
	    param[0] = DEFAULT;	/* Default ID parameter */
	    /* FALLTHRU */
	case CASE_DA1:
	    TRACE(("CASE_DA1\n"));
	    if (param[0] <= 0) {	/* less than means DEFAULT */
		count = 0;
		reply.a_type = CSI;
		reply.a_pintro = '?';

		/* The first param corresponds to the highest
		 * operating level (i.e., service level) of the
		 * emulation.  A DEC terminal can be setup to
		 * respond with a different DA response, but
		 * there's no control sequence that modifies this.
		 * We set it via a resource.
		 */
		if (screen->terminal_id < 200) {
		    switch (screen->terminal_id) {
		    case 102:
			reply.a_param[count++] = 6;	/* VT102 */
			break;
		    case 101:
			reply.a_param[count++] = 1;	/* VT101 */
			reply.a_param[count++] = 0;	/* no options */
			break;
		    default:	/* VT100 */
			reply.a_param[count++] = 1;	/* VT100 */
			reply.a_param[count++] = 2;	/* AVO */
			break;
		    }
		} else {
		    reply.a_param[count++] = 60 + screen->terminal_id / 100;
		    reply.a_param[count++] = 1;		/* 132-columns */
		    reply.a_param[count++] = 2;		/* printer */
		    reply.a_param[count++] = 6;		/* selective-erase */
#if OPT_SUNPC_KBD
		    if (term->keyboard.type == keyboardIsVT220)
#endif
			reply.a_param[count++] = 8;	/* user-defined-keys */
		    reply.a_param[count++] = 9;		/* national replacement charsets */
		    reply.a_param[count++] = 15;	/* technical characters */
		    if_OPT_ISO_COLORS(screen, {
			reply.a_param[count++] = 22;	/* ANSI color, VT525 */
		    });
#if OPT_DEC_LOCATOR
		    reply.a_param[count++] = 29;	/* ANSI text locator */
#endif
		}
		reply.a_nparam = count;
		reply.a_inters = 0;
		reply.a_final = 'c';
		unparseseq(&reply, screen->respond);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DA2:
	    TRACE(("CASE_DA2\n"));
	    if (param[0] <= 0) {	/* less than means DEFAULT */
		count = 0;
		reply.a_type = CSI;
		reply.a_pintro = '>';

		if (screen->terminal_id >= 200)
		    reply.a_param[count++] = 1;		/* VT220 */
		else
		    reply.a_param[count++] = 0;		/* VT100 (nonstandard) */
		reply.a_param[count++] = XTERM_PATCH;	/* Version */
		reply.a_param[count++] = 0;	/* options (none) */
		reply.a_nparam = count;
		reply.a_inters = 0;
		reply.a_final = 'c';
		unparseseq(&reply, screen->respond);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECRPTUI:
	    TRACE(("CASE_DECRPTUI\n"));
	    if ((screen->terminal_id >= 400)
		&& (param[0] <= 0)) {	/* less than means DEFAULT */
		unparseputc1(DCS, screen->respond);
		unparseputc('!', screen->respond);
		unparseputc('|', screen->respond);
		unparseputc('0', screen->respond);
		unparseputc1(ST, screen->respond);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_TBC:
	    TRACE(("CASE_TBC - tab clear\n"));
	    if ((row = param[0]) <= 0)	/* less than means default */
		TabClear(term->tabs, screen->cur_col);
	    else if (row == 3)
		TabZonk(term->tabs);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SET:
	    TRACE(("CASE_SET - set mode\n"));
	    ansi_modes(term, bitset);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_RST:
	    TRACE(("CASE_RST - reset mode\n"));
	    ansi_modes(term, bitclr);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SGR:
	    for (row = 0; row < nparam; ++row) {
		if_OPT_XMC_GLITCH(screen, {
		    Mark_XMC(screen, param[row]);
		});
		TRACE(("CASE_SGR %d\n", param[row]));
		switch (param[row]) {
		case DEFAULT:
		case 0:
		    term->flags &=
			~(INVERSE | BOLD | BLINK | UNDERLINE | INVISIBLE);
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Colors();
		    });
		    break;
		case 1:	/* Bold                 */
		    term->flags |= BOLD;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG();
		    });
		    break;
		case 5:	/* Blink                */
		    term->flags |= BLINK;
		    StartBlinking(screen);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG();
		    });
		    break;
		case 4:	/* Underscore           */
		    term->flags |= UNDERLINE;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG();
		    });
		    break;
		case 7:
		    term->flags |= INVERSE;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedBG();
		    });
		    break;
		case 8:
		    term->flags |= INVISIBLE;
		    break;
		case 22:	/* reset 'bold' */
		    term->flags &= ~BOLD;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG();
		    });
		    break;
		case 24:
		    term->flags &= ~UNDERLINE;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG();
		    });
		    break;
		case 25:	/* reset 'blink' */
		    term->flags &= ~BLINK;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG();
		    });
		    break;
		case 27:
		    term->flags &= ~INVERSE;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedBG();
		    });
		    break;
		case 28:
		    term->flags &= ~INVISIBLE;
		    break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
		    if_OPT_ISO_COLORS(screen, {
			term->sgr_foreground = (param[row] - 30);
			term->sgr_extended = 0;
			setExtendedFG();
		    });
		    break;
		case 38:
		    /* This is more complicated than I'd
		       like, but it should properly eat all
		       the parameters for unsupported modes
		     */
		    if_OPT_ISO_COLORS(screen, {
			row++;
			if (row < nparam) {
			    switch (param[row]) {
			    case 5:
				row++;
				if (row < nparam &&
				    param[row] < NUM_ANSI_COLORS) {
				    term->sgr_foreground = param[row];
				    term->sgr_extended = 1;
				    setExtendedFG();
				}
				break;
			    default:
				row += 7;
				break;
			    }
			}
		    });
		    break;
		case 39:
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Foreground();
		    });
		    break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
		    if_OPT_ISO_COLORS(screen, {
			term->sgr_background = (param[row] - 40);
			setExtendedBG();
		    });
		    break;
		case 48:
		    if_OPT_ISO_COLORS(screen, {
			row++;
			if (row < nparam) {
			    switch (param[row]) {
			    case 5:
				row++;
				if (row < nparam &&
				    param[row] < NUM_ANSI_COLORS) {
				    term->sgr_background = param[row];
				    setExtendedBG();
				}
				break;
			    default:
				row += 7;
				break;
			    }
			}
		    });
		    break;
		case 49:
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Background();
		    });
		    break;
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
		    if_OPT_AIX_COLORS(screen, {
			term->sgr_foreground = (param[row] - 90 + 8);
			term->sgr_extended = 0;
			setExtendedFG();
		    });
		    break;
		case 100:
#if !OPT_AIX_COLORS
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Foreground();
			reset_SGR_Background();
		    });
		    break;
#endif
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
		    if_OPT_AIX_COLORS(screen, {
			term->sgr_background = (param[row] - 100 + 8);
			setExtendedBG();
		    });
		    break;
		}
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	    /* DSR (except for the '?') is a superset of CPR */
	case CASE_DSR:
	    sp->private_function = True;

	    /* FALLTHRU */
	case CASE_CPR:
	    TRACE(("CASE_CPR - cursor position\n"));
	    count = 0;
	    reply.a_type = CSI;
	    reply.a_pintro = sp->private_function ? '?' : 0;
	    reply.a_inters = 0;
	    reply.a_final = 'n';

	    switch (param[0]) {
	    case 5:
		/* operating status */
		reply.a_param[count++] = 0;	/* (no malfunction ;-) */
		break;
	    case 6:
		/* CPR */
		/* DECXCPR (with page=0) */
		reply.a_param[count++] = screen->cur_row + 1;
		reply.a_param[count++] = screen->cur_col + 1;
		reply.a_final = 'R';
		break;
	    case 15:
		/* printer status */
		reply.a_param[count++] = 13;	/* implement printer */
		break;
	    case 25:
		/* UDK status */
		reply.a_param[count++] = 20;	/* UDK always unlocked */
		break;
	    case 26:
		/* keyboard status */
		reply.a_param[count++] = 27;
		reply.a_param[count++] = 1;	/* North American */
		if (screen->terminal_id >= 400) {
		    reply.a_param[count++] = 0;		/* ready */
		    reply.a_param[count++] = 0;		/* LK201 */
		}
		break;
	    case 53:
		/* Locator status */
#if OPT_DEC_LOCATOR
		reply.a_param[count++] = 50;	/* locator ready */
#else
		reply.a_param[count++] = 53;	/* no locator */
#endif
		break;
	    }

	    if ((reply.a_nparam = count) != 0)
		unparseseq(&reply, screen->respond);

	    sp->parsestate = sp->groundtable;
	    sp->private_function = False;
	    break;

	case CASE_MC:
	    TRACE(("CASE_MC - media control\n"));
	    xtermMediaControl(param[0], False);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DEC_MC:
	    TRACE(("CASE_DEC_MC - DEC media control\n"));
	    xtermMediaControl(param[0], True);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_HP_MEM_LOCK:
	case CASE_HP_MEM_UNLOCK:
	    TRACE(("%s\n", ((sp->parsestate[c] == CASE_HP_MEM_LOCK)
			    ? "CASE_HP_MEM_LOCK"
			    : "CASE_HP_MEM_UNLOCK")));
	    if (screen->scroll_amt)
		FlushScroll(screen);
	    if (sp->parsestate[c] == CASE_HP_MEM_LOCK)
		set_tb_margins(screen, screen->cur_row, screen->bot_marg);
	    else
		set_tb_margins(screen, 0, screen->bot_marg);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSTBM:
	    TRACE(("CASE_DECSTBM - set scrolling region\n"));
	    if ((top = param[0]) < 1)
		top = 1;
	    if (nparam < 2 || (bot = param[1]) == DEFAULT
		|| bot > MaxRows(screen)
		|| bot == 0)
		bot = MaxRows(screen);
	    if (bot > top) {
		if (screen->scroll_amt)
		    FlushScroll(screen);
		set_tb_margins(screen, top - 1, bot - 1);
		CursorSet(screen, 0, 0, term->flags);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECREQTPARM:
	    TRACE(("CASE_DECREQTPARM\n"));
	    if (screen->terminal_id < 200) {	/* VT102 */
		if ((row = param[0]) == DEFAULT)
		    row = 0;
		if (row == 0 || row == 1) {
		    reply.a_type = CSI;
		    reply.a_pintro = 0;
		    reply.a_nparam = 7;
		    reply.a_param[0] = row + 2;
		    reply.a_param[1] = 1;	/* no parity */
		    reply.a_param[2] = 1;	/* eight bits */
		    reply.a_param[3] = 128;	/* transmit 38.4k baud */
		    reply.a_param[4] = 128;	/* receive 38.4k baud */
		    reply.a_param[5] = 1;	/* clock multiplier ? */
		    reply.a_param[6] = 0;	/* STP flags ? */
		    reply.a_inters = 0;
		    reply.a_final = 'x';
		    unparseseq(&reply, screen->respond);
		}
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSET:
	    /* DECSET */
#if OPT_VT52_MODE
	    if (screen->vtXX_level != 0)
#endif
		dpmodes(term, bitset);
	    sp->parsestate = sp->groundtable;
#if OPT_TEK4014
	    if (screen->TekEmu)
		return False;
#endif
	    break;

	case CASE_DECRST:
	    /* DECRST */
	    dpmodes(term, bitclr);
#if OPT_VT52_MODE
	    if (screen->vtXX_level == 0)
		sp->groundtable = vt52_table;
	    else if (screen->terminal_id >= 100)
		sp->groundtable = ansi_table;
#endif
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECALN:
	    TRACE(("CASE_DECALN - alignment test\n"));
	    if (screen->cursor_state)
		HideCursor();
	    set_tb_margins(screen, 0, screen->max_row);
	    CursorSet(screen, 0, 0, term->flags);
	    xtermParseRect(screen, 0, 0, &myRect);
	    ScrnFillRectangle(screen, &myRect, 'E', 0);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_GSETS:
	    TRACE(("CASE_GSETS(%d) = '%c'\n", sp->scstype, c));
	    if (screen->vtXX_level != 0)
		screen->gsets[sp->scstype] = c;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSC:
	    TRACE(("CASE_DECSC - save cursor\n"));
	    CursorSave(term);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECRC:
	    TRACE(("CASE_DECRC - restore cursor\n"));
	    CursorRestore(term);
	    if_OPT_ISO_COLORS(screen, {
		setExtendedFG();
	    });
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECKPAM:
	    TRACE(("CASE_DECKPAM\n"));
	    term->keyboard.flags |= MODE_DECKPAM;
	    update_appkeypad();
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECKPNM:
	    TRACE(("CASE_DECKPNM\n"));
	    term->keyboard.flags &= ~MODE_DECKPAM;
	    update_appkeypad();
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CSI_QUOTE_STATE:
	    sp->parsestate = csi_quo_table;
	    break;

#if OPT_VT52_MODE
	case CASE_VT52_FINISH:
	    TRACE(("CASE_VT52_FINISH terminal_id %d, vtXX_level %d\n",
		   screen->terminal_id,
		   screen->vtXX_level));
	    if (screen->terminal_id >= 100
		&& screen->vtXX_level == 0) {
		sp->groundtable =
		    sp->parsestate = ansi_table;
		screen->vtXX_level = screen->vt52_save_level;
		screen->curgl = screen->vt52_save_curgl;
		screen->curgr = screen->vt52_save_curgr;
		screen->curss = screen->vt52_save_curss;
		memmove(screen->gsets, screen->vt52_save_gsets, sizeof(screen->gsets));
	    }
	    break;
#endif

	case CASE_ANSI_LEVEL_1:
	    TRACE(("CASE_ANSI_LEVEL_1\n"));
	    set_ansi_conformance(screen, 1);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_ANSI_LEVEL_2:
	    TRACE(("CASE_ANSI_LEVEL_2\n"));
	    set_ansi_conformance(screen, 2);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_ANSI_LEVEL_3:
	    TRACE(("CASE_ANSI_LEVEL_3\n"));
	    set_ansi_conformance(screen, 3);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSCL:
	    TRACE(("CASE_DECSCL(%d,%d)\n", param[0], param[1]));
	    if (param[0] >= 61 && param[0] <= 65) {
		/*
		 * VT300, VT420, VT520 manuals claim that DECSCL does a hard
		 * reset (RIS).  VT220 manual states that it is a soft reset.
		 * Perhaps both are right (unlikely).  Kermit says it's soft.
		 */
		VTReset(False, False);
		screen->vtXX_level = param[0] - 60;
		if (param[0] > 61) {
		    if (param[1] == 1)
			show_8bit_control(False);
		    else if (param[1] == 0 || param[1] == 2)
			show_8bit_control(True);
		}
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSCA:
	    TRACE(("CASE_DECSCA\n"));
	    screen->protected_mode = DEC_PROTECT;
	    if (param[0] <= 0 || param[0] == 2)
		term->flags &= ~PROTECTED;
	    else if (param[0] == 1)
		term->flags |= PROTECTED;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSED:
	    TRACE(("CASE_DECSED\n"));
	    do_erase_display(screen, param[0], DEC_PROTECT);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSEL:
	    TRACE(("CASE_DECSEL\n"));
	    do_erase_line(screen, param[0], DEC_PROTECT);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_ST:
	    TRACE(("CASE_ST: End of String (%d bytes)\n", string_used));
	    sp->parsestate = sp->groundtable;
	    if (!string_used)
		break;
	    string_area[--string_used] = '\0';
	    switch (sp->string_mode) {
	    case APC:
		/* ignored */
		break;
	    case DCS:
		do_dcs(string_area, string_used);
		break;
	    case OSC:
		do_osc(string_area, string_used, ST);
		break;
	    case PM:
		/* ignored */
		break;
	    case SOS:
		/* ignored */
		break;
	    }
	    break;

	case CASE_SOS:
	    TRACE(("CASE_SOS: Start of String\n"));
	    sp->string_mode = SOS;
	    sp->parsestate = sos_table;
	    break;

	case CASE_PM:
	    TRACE(("CASE_PM: Privacy Message\n"));
	    sp->string_mode = PM;
	    sp->parsestate = sos_table;
	    break;

	case CASE_DCS:
	    TRACE(("CASE_DCS: Device Control String\n"));
	    sp->string_mode = DCS;
	    sp->parsestate = sos_table;
	    break;

	case CASE_APC:
	    TRACE(("CASE_APC: Application Program Command\n"));
	    sp->string_mode = APC;
	    sp->parsestate = sos_table;
	    break;

	case CASE_SPA:
	    TRACE(("CASE_SPA - start protected area\n"));
	    screen->protected_mode = ISO_PROTECT;
	    term->flags |= PROTECTED;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_EPA:
	    TRACE(("CASE_EPA - end protected area\n"));
	    term->flags &= ~PROTECTED;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SU:
	    TRACE(("CASE_SU - scroll up\n"));
	    if ((count = param[0]) < 1)
		count = 1;
	    xtermScroll(screen, count);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_IND:
	    TRACE(("CASE_IND - index\n"));
	    xtermIndex(screen, 1);
	    do_xevents();
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CPL:
	    TRACE(("CASE_CPL - cursor prev line\n"));
	    CursorPrevLine(screen, param[0]);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CNL:
	    TRACE(("CASE_NPL - cursor next line\n"));
	    CursorNextLine(screen, param[0]);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_NEL:
	    TRACE(("CASE_NEL\n"));
	    xtermIndex(screen, 1);
	    CarriageReturn(screen);
	    do_xevents();
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_HTS:
	    TRACE(("CASE_HTS - horizontal tab set\n"));
	    TabSet(term->tabs, screen->cur_col);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_RI:
	    TRACE(("CASE_RI - reverse index\n"));
	    RevIndex(screen, 1);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SS2:
	    TRACE(("CASE_SS2\n"));
	    screen->curss = 2;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SS3:
	    TRACE(("CASE_SS3\n"));
	    screen->curss = 3;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CSI_STATE:
	    /* enter csi state */
	    nparam = 1;
	    param[0] = DEFAULT;
	    sp->parsestate = csi_table;
	    break;

	case CASE_ESC_SP_STATE:
	    /* esc space */
	    sp->parsestate = esc_sp_table;
	    break;

	case CASE_CSI_EX_STATE:
	    /* csi exclamation */
	    sp->parsestate = csi_ex_table;
	    break;

#if OPT_DEC_LOCATOR
	case CASE_CSI_TICK_STATE:
	    /* csi tick (') */
	    sp->parsestate = csi_tick_table;
	    break;

	case CASE_DECEFR:
	    TRACE(("CASE_DECEFR - Enable Filter Rectangle\n"));
	    if (screen->send_mouse_pos == DEC_LOCATOR) {
		MotionOff(screen, term);
		if ((screen->loc_filter_top = param[0]) < 1)
		    screen->loc_filter_top = LOC_FILTER_POS;
		if (nparam < 2 || (screen->loc_filter_left = param[1]) < 1)
		    screen->loc_filter_left = LOC_FILTER_POS;
		if (nparam < 3 || (screen->loc_filter_bottom = param[2]) < 1)
		    screen->loc_filter_bottom = LOC_FILTER_POS;
		if (nparam < 4 || (screen->loc_filter_right = param[3]) < 1)
		    screen->loc_filter_right = LOC_FILTER_POS;
		InitLocatorFilter(term);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECELR:
	    MotionOff(screen, term);
	    if (param[0] <= 0 || param[0] > 2) {
		screen->send_mouse_pos = MOUSE_OFF;
		TRACE(("DECELR - Disable Locator Reports\n"));
	    } else {
		TRACE(("DECELR - Enable Locator Reports\n"));
		screen->send_mouse_pos = DEC_LOCATOR;
		if (param[0] == 2) {
		    screen->locator_reset = True;
		} else {
		    screen->locator_reset = False;
		}
		if (nparam < 2 || param[1] != 1) {
		    screen->locator_pixels = False;
		} else {
		    screen->locator_pixels = True;
		}
		screen->loc_filter = False;
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSLE:
	    TRACE(("DECSLE - Select Locator Events\n"));
	    for (count = 0; count < nparam; ++count) {
		switch (param[count]) {
		case DEFAULT:
		case 0:
		    MotionOff(screen, term);
		    screen->loc_filter = False;
		    screen->locator_events = 0;
		    break;
		case 1:
		    screen->locator_events |= LOC_BTNS_DN;
		    break;
		case 2:
		    screen->locator_events &= ~LOC_BTNS_DN;
		    break;
		case 3:
		    screen->locator_events |= LOC_BTNS_UP;
		    break;
		case 4:
		    screen->locator_events &= ~LOC_BTNS_UP;
		    break;
		}
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECRQLP:
	    TRACE(("DECRQLP - Request Locator Position\n"));
	    if (param[0] < 2) {
		/* Issue DECLRP Locator Position Report */
		GetLocatorPosition(term);
	    }
	    sp->parsestate = sp->groundtable;
	    break;
#endif /* OPT_DEC_LOCATOR */

#if OPT_DEC_RECTOPS
	case CASE_CSI_DOLLAR_STATE:
	    /* csi dollar ($) */
	    if (screen->vtXX_level >= 4)
		sp->parsestate = csi_dollar_table;
	    else
		sp->parsestate = eigtable;
	    break;

	case CASE_CSI_STAR_STATE:
	    /* csi dollar (*) */
	    if (screen->vtXX_level >= 4)
		sp->parsestate = csi_star_table;
	    else
		sp->parsestate = eigtable;
	    break;

	case CASE_DECCRA:
	    TRACE(("CASE_DECCRA - Copy rectangular area\n"));
	    xtermParseRect(screen, nparam, param, &myRect);
	    ScrnCopyRectangle(screen, &myRect, nparam - 5, param + 5);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECERA:
	    TRACE(("CASE_DECERA - Erase rectangular area\n"));
	    xtermParseRect(screen, nparam, param, &myRect);
	    ScrnFillRectangle(screen, &myRect, ' ', 0);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECFRA:
	    TRACE(("CASE_DECFRA - Fill rectangular area\n"));
	    if (nparam > 0
		&& ((param[0] >= 32 && param[0] <= 126)
		    || (param[0] >= 160 && param[0] <= 255))) {
		xtermParseRect(screen, nparam - 1, param + 1, &myRect);
		ScrnFillRectangle(screen, &myRect, param[0], term->flags);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSERA:
	    TRACE(("CASE_DECSERA - Selective erase rectangular area\n"));
	    xtermParseRect(screen, nparam > 4 ? 4 : nparam, param, &myRect);
	    ScrnWipeRectangle(screen, &myRect);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSACE:
	    TRACE(("CASE_DECSACE - Select attribute change extent\n"));
	    screen->cur_decsace = param[0];
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECCARA:
	    TRACE(("CASE_DECCARA - Change attributes in rectangular area\n"));
	    xtermParseRect(screen, nparam > 4 ? 4 : nparam, param, &myRect);
	    ScrnMarkRectangle(screen, &myRect, False, nparam - 4, param + 4);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECRARA:
	    TRACE(("CASE_DECRARA - Reverse attributes in rectangular area\n"));
	    xtermParseRect(screen, nparam > 4 ? 4 : nparam, param, &myRect);
	    ScrnMarkRectangle(screen, &myRect, True, nparam - 4, param + 4);
	    sp->parsestate = sp->groundtable;
	    break;
#else
	case CASE_CSI_DOLLAR_STATE:
	    /* csi dollar ($) */
	    sp->parsestate = eigtable;
	    break;

	case CASE_CSI_STAR_STATE:
	    /* csi dollar (*) */
	    sp->parsestate = eigtable;
	    break;
#endif /* OPT_DEC_RECTOPS */

	case CASE_S7C1T:
	    TRACE(("CASE_S7C1T\n"));
	    show_8bit_control(False);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_S8C1T:
	    TRACE(("CASE_S8C1T\n"));
#if OPT_VT52_MODE
	    if (screen->vtXX_level <= 1)
		break;
#endif
	    show_8bit_control(True);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_OSC:
	    TRACE(("CASE_OSC: Operating System Command\n"));
	    sp->parsestate = sos_table;
	    sp->string_mode = OSC;
	    break;

	case CASE_RIS:
	    TRACE(("CASE_RIS\n"));
	    VTReset(True, True);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSTR:
	    TRACE(("CASE_DECSTR\n"));
	    VTReset(False, False);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_REP:
	    TRACE(("CASE_REP\n"));
	    if (sp->lastchar >= 0 &&
		sp->groundtable[E2A(sp->lastchar)] == CASE_PRINT) {
		IChar repeated[2];
		count = (param[0] < 1) ? 1 : param[0];
		repeated[0] = sp->lastchar;
		while (count-- > 0) {
		    dotext(screen,
			   screen->gsets[(int) (screen->curgl)],
			   repeated, 1);
		}
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_LS2:
	    TRACE(("CASE_LS2\n"));
	    screen->curgl = 2;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_LS3:
	    TRACE(("CASE_LS3\n"));
	    screen->curgl = 3;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_LS3R:
	    TRACE(("CASE_LS3R\n"));
	    screen->curgr = 3;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_LS2R:
	    TRACE(("CASE_LS2R\n"));
	    screen->curgr = 2;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_LS1R:
	    TRACE(("CASE_LS1R\n"));
	    screen->curgr = 1;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_XTERM_SAVE:
	    savemodes(term);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_XTERM_RESTORE:
	    restoremodes(term);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_XTERM_WINOPS:
	    TRACE(("CASE_XTERM_WINOPS\n"));
	    if (screen->allowWindowOps)
		window_ops(term);
	    sp->parsestate = sp->groundtable;
	    break;
#if OPT_WIDE_CHARS
	case CASE_ESC_PERCENT:
	    sp->parsestate = esc_pct_table;
	    break;

	case CASE_UTF8:
	    /* If we did not set UTF-8 mode from resource or the
	     * command-line, allow it to be enabled/disabled by
	     * control sequence.
	     */
	    if (!screen->wide_chars) {
		WriteNow();
		ChangeToWide(screen);
	    }
	    if (screen->wide_chars
		&& screen->utf8_mode != uAlways) {
		switchPtyData(screen, c == 'G');
		TRACE(("UTF8 mode %s\n",
		       BtoS(screen->utf8_mode)));
	    } else {
		TRACE(("UTF8 mode NOT turned %s (%s)\n",
		       BtoS(c == 'G'),
		       (screen->utf8_mode == uAlways)
		       ? "UTF-8 mode set from command-line"
		       : "wideChars resource was not set"));
	    }
	    sp->parsestate = sp->groundtable;
	    break;
#endif

	case CASE_CSI_IGNORE:
	    sp->parsestate = cigtable;
	    break;
	}
	if (sp->parsestate == sp->groundtable)
	    sp->lastchar = thischar;
    } while (0);

#if OPT_WIDE_CHARS
    screen->utf8_inparse = (screen->utf8_mode != uFalse
			    && sp->parsestate != sos_table);
#endif

    return True;
}

static void
VTparse(void)
{
    TScreen *screen;

    /* We longjmp back to this point in VTReset() */
    (void) setjmp(vtjmpbuf);
    screen = &term->screen;
    memset(&myState, 0, sizeof(myState));
#if OPT_VT52_MODE
    myState.groundtable = screen->vtXX_level ? ansi_table : vt52_table;
#else
    myState.groundtable = ansi_table;
#endif
    myState.parsestate = myState.groundtable;
    myState.lastchar = -1;	/* not a legal IChar */
    myState.nextstate = -1;	/* not a legal state */

    for (;;) {
	if (!doparsing(doinput(), &myState))
	    return;
    }
}

static Char *v_buffer;		/* pointer to physical buffer */
static Char *v_bufstr = NULL;	/* beginning of area to write */
static Char *v_bufptr;		/* end of area to write */
static Char *v_bufend;		/* end of physical buffer */

/* Write data to the pty as typed by the user, pasted with the mouse,
   or generated by us in response to a query ESC sequence. */

int
v_write(int f, Char * data, unsigned len)
{
    int riten;
    unsigned c = len;

    if (v_bufstr == NULL && len > 0) {
	v_buffer = (Char *) XtMalloc(len);
	v_bufstr = v_buffer;
	v_bufptr = v_buffer;
	v_bufend = v_buffer + len;
    }
#ifdef DEBUG
    if (debug) {
	fprintf(stderr, "v_write called with %d bytes (%d left over)",
		len, v_bufptr - v_bufstr);
	if (len > 1 && len < 10)
	    fprintf(stderr, " \"%.*s\"", len, (char *) data);
	fprintf(stderr, "\n");
    }
#endif

#ifdef VMS
    if ((1 << f) != pty_mask)
	return (tt_write((char *) data, len));
#else /* VMS */
    if (!FD_ISSET(f, &pty_mask))
	return (write(f, (char *) data, len));
#endif /* VMS */

    /*
     * Append to the block we already have.
     * Always doing this simplifies the code, and
     * isn't too bad, either.  If this is a short
     * block, it isn't too expensive, and if this is
     * a long block, we won't be able to write it all
     * anyway.
     */

    if (len > 0) {
#if OPT_DABBREV
	term->screen.dabbrev_working = 0;	/* break dabbrev sequence */
#endif
	if (v_bufend < v_bufptr + len) {	/* we've run out of room */
	    if (v_bufstr != v_buffer) {
		/* there is unused space, move everything down */
		/* possibly overlapping memmove here */
#ifdef DEBUG
		if (debug)
		    fprintf(stderr, "moving data down %d\n",
			    v_bufstr - v_buffer);
#endif
		memmove(v_buffer, v_bufstr, (unsigned) (v_bufptr - v_bufstr));
		v_bufptr -= v_bufstr - v_buffer;
		v_bufstr = v_buffer;
	    }
	    if (v_bufend < v_bufptr + len) {
		/* still won't fit: get more space */
		/* Don't use XtRealloc because an error is not fatal. */
		int size = v_bufptr - v_buffer;		/* save across realloc */
		v_buffer = TypeRealloc(Char, size + len, v_buffer);
		if (v_buffer) {
#ifdef DEBUG
		    if (debug)
			fprintf(stderr, "expanded buffer to %d\n",
				size + len);
#endif
		    v_bufstr = v_buffer;
		    v_bufptr = v_buffer + size;
		    v_bufend = v_bufptr + len;
		} else {
		    /* no memory: ignore entire write request */
		    fprintf(stderr, "%s: cannot allocate buffer space\n",
			    xterm_name);
		    v_buffer = v_bufstr;	/* restore clobbered pointer */
		    c = 0;
		}
	    }
	}
	if (v_bufend >= v_bufptr + len) {
	    /* new stuff will fit */
	    memmove(v_bufptr, data, len);
	    v_bufptr += len;
	}
    }

    /*
     * Write out as much of the buffer as we can.
     * Be careful not to overflow the pty's input silo.
     * We are conservative here and only write
     * a small amount at a time.
     *
     * If we can't push all the data into the pty yet, we expect write
     * to return a non-negative number less than the length requested
     * (if some data written) or -1 and set errno to EAGAIN,
     * EWOULDBLOCK, or EINTR (if no data written).
     *
     * (Not all systems do this, sigh, so the code is actually
     * a little more forgiving.)
     */

#define MAX_PTY_WRITE 128	/* 1/2 POSIX minimum MAX_INPUT */

    if (v_bufptr > v_bufstr) {
#ifdef VMS
	riten = tt_write(v_bufstr,
			 ((v_bufptr - v_bufstr <= VMS_TERM_BUFFER_SIZE)
			  ? v_bufptr - v_bufstr
			  : VMS_TERM_BUFFER_SIZE));
	if (riten == 0)
	    return (riten);
#else /* VMS */
	riten = write(f, v_bufstr,
		      (size_t) ((v_bufptr - v_bufstr <= MAX_PTY_WRITE)
				? v_bufptr - v_bufstr
				: MAX_PTY_WRITE));
	if (riten < 0)
#endif /* VMS */
	{
#ifdef DEBUG
	    if (debug)
		perror("write");
#endif
	    riten = 0;
	}
#ifdef DEBUG
	if (debug)
	    fprintf(stderr, "write called with %d, wrote %d\n",
		    v_bufptr - v_bufstr <= MAX_PTY_WRITE ?
		    v_bufptr - v_bufstr : MAX_PTY_WRITE,
		    riten);
#endif
	v_bufstr += riten;
	if (v_bufstr >= v_bufptr)	/* we wrote it all */
	    v_bufstr = v_bufptr = v_buffer;
    }

    /*
     * If we have lots of unused memory allocated, return it
     */
    if (v_bufend - v_bufptr > 1024) {	/* arbitrary hysteresis */
	/* save pointers across realloc */
	int start = v_bufstr - v_buffer;
	int size = v_bufptr - v_buffer;
	unsigned allocsize = (unsigned) (size ? size : 1);

	v_buffer = TypeRealloc(Char, allocsize, v_buffer);
	if (v_buffer) {
	    v_bufstr = v_buffer + start;
	    v_bufptr = v_buffer + size;
	    v_bufend = v_buffer + allocsize;
#ifdef DEBUG
	    if (debug)
		fprintf(stderr, "shrunk buffer to %d\n", allocsize);
#endif
	} else {
	    /* should we print a warning if couldn't return memory? */
	    v_buffer = v_bufstr - start;	/* restore clobbered pointer */
	}
    }
    return (c);
}

#ifdef VMS
#define	ptymask()	(v_bufptr > v_bufstr ? pty_mask : 0)

static void
in_put(void)
{
    static PtySelect select_mask;
    static PtySelect write_mask;
    int update = VTbuffer->update;
    int size;

    int status;
    Dimension replyWidth, replyHeight;
    XtGeometryResult stat;

    TScreen *screen = &term->screen;
    char *cp;
    int i;

    select_mask = pty_mask;	/* force initial read */
    for (;;) {

	/* if the terminal changed size, resize the widget */
	if (tt_changed) {
	    tt_changed = False;

	    stat = XtMakeResizeRequest((Widget) term,
				       ((Dimension) FontWidth(screen)
					* (tt_width)
					+ 2 * screen->border
					+ screen->fullVwin.sb_info.width),
				       ((Dimension) FontHeight(screen)
					* (tt_length)
					+ 2 * screen->border),
				       &replyWidth, &replyHeight);

	    if (stat == XtGeometryYes || stat == XtGeometryDone) {
		term->core.width = replyWidth;
		term->core.height = replyHeight;

		ScreenResize(&term->screen, replyWidth, replyHeight,
			     &term->flags);
	    }
	    repairSizeHints();
	}

	if (eventMode == NORMAL
	    && readPtyData(screen, &select_mask, VTbuffer)) {
	    if (screen->scrollWidget
		&& screen->scrollttyoutput
		&& screen->topline < 0)
		/* Scroll to bottom */
		WindowScroll(screen, 0);
	    break;
	}
	if (screen->scroll_amt)
	    FlushScroll(screen);
	if (screen->cursor_set && CursorMoved(screen)) {
	    if (screen->cursor_state)
		HideCursor();
	    ShowCursor();
#if OPT_INPUT_METHOD
	    PreeditPosition(screen);
#endif
	} else if (screen->cursor_set != screen->cursor_state) {
	    if (screen->cursor_set)
		ShowCursor();
	    else
		HideCursor();
	}

	if (QLength(screen->display)) {
	    select_mask = X_mask;
	} else {
	    write_mask = ptymask();
	    XFlush(screen->display);
	    select_mask = Select_mask;
	    if (eventMode != NORMAL)
		select_mask = X_mask;
	}
	if (write_mask & ptymask()) {
	    v_write(screen->respond, 0, 0);	/* flush buffer */
	}

	if (select_mask & X_mask) {
	    xevents();
	    if (VTbuffer->update != update)
		break;
	}
    }
}
#else /* VMS */

static void
in_put(void)
{
    static PtySelect select_mask;
    static PtySelect write_mask;

    TScreen *screen = &term->screen;
    int i, time_select;
    int size;
    int update = VTbuffer->update;

    static struct timeval select_timeout;

#if OPT_BLINK_CURS
    /*
     * Compute the timeout for the blinking cursor to be much smaller than
     * the "on" or "off" interval.
     */
    int tick = ((screen->blink_on < screen->blink_off)
		? screen->blink_on
		: screen->blink_off);
    tick *= (1000 / 8);		/* 1000 for msec/usec, 8 for "much" smaller */
    if (tick < 1)
	tick = 1;
#endif

    for (;;) {
	if (eventMode == NORMAL
	    && (size = readPtyData(screen, &select_mask, VTbuffer)) != 0) {
	    if (screen->scrollWidget
		&& screen->scrollttyoutput
		&& screen->topline < 0)
		WindowScroll(screen, 0);	/* Scroll to bottom */
	    /* stop speed reading at some point to look for X stuff */
	    TRACE(("VTbuffer uses %d/%d\n",
		   VTbuffer->last - VTbuffer->buffer,
		   BUF_SIZE));
	    if ((VTbuffer->last - VTbuffer->buffer) > BUF_SIZE) {
		FD_CLR(screen->respond, &select_mask);
		break;
	    }
#if defined(HAVE_SCHED_YIELD)
	    /*
	     * If we've read a full (small/fragment) buffer, let the operating
	     * system have a turn, and we'll resume reading until we've either
	     * read only a fragment of the buffer, or we've filled the large
	     * buffer (see above).  Doing this helps keep up with large bursts
	     * of output.
	     */
	    if (size == FRG_SIZE) {
		select_timeout.tv_sec = 0;
		i = Select(max_plus1, &select_mask, &write_mask, 0,
			   &select_timeout);
		if (i > 0) {
		    sched_yield();
		} else
		    break;
	    } else {
		break;
	    }
#else
	    (void) size;	/* unused in this branch */
	    break;
#endif
	}
	/* update the screen */
	if (screen->scroll_amt)
	    FlushScroll(screen);
	if (screen->cursor_set && CursorMoved(screen)) {
	    if (screen->cursor_state)
		HideCursor();
	    ShowCursor();
#if OPT_INPUT_METHOD
	    PreeditPosition(screen);
#endif
	} else if (screen->cursor_set != screen->cursor_state) {
	    if (screen->cursor_set)
		ShowCursor();
	    else
		HideCursor();
	}

	XFlush(screen->display);	/* always flush writes before waiting */

	/* Update the masks and, unless X events are already in the queue,
	   wait for I/O to be possible. */
	XFD_COPYSET(&Select_mask, &select_mask);
	/* in selection mode xterm does not read pty */
	if (eventMode != NORMAL)
	    FD_CLR(screen->respond, &select_mask);

	if (v_bufptr > v_bufstr) {
	    XFD_COPYSET(&pty_mask, &write_mask);
	} else
	    FD_ZERO(&write_mask);
	select_timeout.tv_sec = 0;
	time_select = 0;

	/*
	 * if there's either an XEvent or an XtTimeout pending, just take
	 * a quick peek, i.e. timeout from the select() immediately.  If
	 * there's nothing pending, let select() block a little while, but
	 * for a shorter interval than the arrow-style scrollbar timeout.
	 * The blocking is optional, because it tends to increase the load
	 * on the host.
	 */
	if (XtAppPending(app_con)) {
	    select_timeout.tv_usec = 0;
	    time_select = 1;
	} else if (screen->awaitInput) {
	    select_timeout.tv_usec = 50000;
	    time_select = 1;
#if OPT_BLINK_CURS
	} else if ((screen->blink_timer != 0 &&
		    ((screen->select & FOCUS) || screen->always_highlight)) ||
		   (screen->cursor_state == BLINKED_OFF)) {
	    select_timeout.tv_usec = tick;
	    while (select_timeout.tv_usec > 1000000) {
		select_timeout.tv_usec -= 1000000;
		select_timeout.tv_sec++;
	    }
	    time_select = 1;
#endif
#if OPT_SESSION_MGT
	} else if (resource.sessionMgt) {
	    /*
	     * When session management is enabled, we should not block since
	     * session related events can arrive any time. 
	     */
	    select_timeout.tv_sec = 1;
	    select_timeout.tv_usec = 0;
	    time_select = 1;
#endif
	}
	if (need_cleanup)
	    Cleanup(0);
	i = Select(max_plus1, &select_mask, &write_mask, 0,
		   (time_select ? &select_timeout : 0));
	if (i < 0) {
	    if (errno != EINTR)
		SysError(ERROR_SELECT);
	    continue;
	}

	/* if there is room to write more data to the pty, go write more */
	if (FD_ISSET(screen->respond, &write_mask)) {
	    v_write(screen->respond, (Char *) 0, 0);	/* flush buffer */
	}

	/* if there are X events already in our queue, it
	   counts as being readable */
	if (XtAppPending(app_con) ||
	    FD_ISSET(ConnectionNumber(screen->display), &select_mask)) {
	    xevents();
	    if (VTbuffer->update != update)	/* HandleInterpret */
		break;
	}

    }
}
#endif /* VMS */

static IChar
doinput(void)
{
    TScreen *screen = &term->screen;

    while (!morePtyData(screen, VTbuffer))
	in_put();
    return nextPtyData(screen, VTbuffer);
}

#if OPT_INPUT_METHOD
/*
 *  For OverTheSpot, client has to inform the position for XIM preedit.
 */
static void
PreeditPosition(TScreen * screen)
{
    XPoint spot;
    XVaNestedList list;

    if (!screen->xic)
	return;
    spot.x = CurCursorX(screen, screen->cur_row, screen->cur_col);
    spot.y = CursorY(screen, screen->cur_row) + screen->fs_ascent;
    list = XVaCreateNestedList(0,
			       XNSpotLocation, &spot,
			       XNForeground, T_COLOR(screen, TEXT_FG),
			       XNBackground, T_COLOR(screen, TEXT_BG),
			       NULL);
    XSetICValues(screen->xic, XNPreeditAttributes, list, NULL);
    XFree(list);
}
#endif

/*
 * process a string of characters according to the character set indicated
 * by charset.  worry about end of line conditions (wraparound if selected).
 */
void
dotext(TScreen * screen,
       int charset,
       IChar * buf,		/* start of characters to process */
       Cardinal len)		/* end */
{
#if OPT_WIDE_CHARS
    Cardinal chars_chomped = 1;
#else
    int next_col, last_col, this_col;	/* must be signed */
#endif
    Cardinal offset;

#if OPT_WIDE_CHARS
    /* don't translate if we use UTF-8, and are not handling legacy support
     * for line-drawing characters.
     */
    if ((screen->utf8_mode == uFalse)
	|| (screen->vt100_graphics && charset == '0'))
#endif

	if (!xtermCharSetOut(buf, buf + len, charset))
	    return;

    if_OPT_XMC_GLITCH(screen, {
	Cardinal n;
	if (charset != '?') {
	    for (n = 0; n < len; n++) {
		if (buf[n] == XMC_GLITCH)
		    buf[n] = XMC_GLITCH + 1;
	    }
	}
    });

#if OPT_WIDE_CHARS
    for (offset = 0;
	 offset < len && (chars_chomped > 0 || screen->do_wrap);
	 offset += chars_chomped) {
	int width_available = MaxCols(screen) - screen->cur_col;
	int width_here = 0;
	int need_wrap = 0;
	chars_chomped = 0;

	if (screen->do_wrap && (term->flags & WRAPAROUND)) {
	    /* mark that we had to wrap this line */
	    ScrnSetWrapped(screen, screen->cur_row);
	    xtermAutoPrint('\n');
	    xtermIndex(screen, 1);
	    set_cur_col(screen, 0);
	    screen->do_wrap = 0;
	    width_available = MaxCols(screen) - screen->cur_col;
	}

	while (width_here <= width_available && chars_chomped < (len - offset)) {
	    if (!screen->utf8_mode
		|| (screen->vt100_graphics && charset == '0'))
		width_here++;
	    else
		width_here += my_wcwidth((int) buf[chars_chomped + offset]);
	    chars_chomped++;
	}

	if (width_here > width_available) {
	    chars_chomped--;
	    if (!screen->utf8_mode
		|| (screen->vt100_graphics && charset == '0'))
		width_here--;
	    else
		width_here -= my_wcwidth((int) buf[chars_chomped + offset]);
	    need_wrap = 1;
	} else if (width_here == width_available) {
	    need_wrap = 1;
	} else if (chars_chomped != (len - offset)) {
	    need_wrap = 1;
	}

	/*
	 * Split the wide characters back into separate arrays of 8-bit
	 * characters so we can use the existing interface.
	 *
	 * FIXME:  If we rewrote this interface, it would involve
	 * rewriting all of the memory-management for the screen
	 * buffers (perhaps this is simpler).
	 */
	if (chars_chomped != 0) {
	    static unsigned limit;
	    static Char *hibyte, *lobyte;
	    Bool both = False;
	    unsigned j, k;

	    if (chars_chomped >= limit) {
		limit = (chars_chomped + 1) * 2;
		lobyte = (Char *) XtRealloc((char *) lobyte, limit);
		hibyte = (Char *) XtRealloc((char *) hibyte, limit);
	    }
	    for (j = offset; j < offset + chars_chomped; j++) {
		k = j - offset;
		lobyte[k] = buf[j];
		if (buf[j] > 255) {
		    hibyte[k] = (buf[j] >> 8);
		    both = True;
		} else {
		    hibyte[k] = 0;
		}
	    }

	    WriteText(screen, PAIRED_CHARS(lobyte,
					   (both ? hibyte : 0)),
		      chars_chomped);
	}
	screen->do_wrap = need_wrap;
    }
#else

    for (offset = 0; offset < len; offset += this_col) {
	last_col = CurMaxCol(screen, screen->cur_row);
	this_col = last_col - screen->cur_col + 1;
	if (this_col <= 1) {
	    if (screen->do_wrap && (term->flags & WRAPAROUND)) {
		/* mark that we had to wrap this line */
		ScrnSetWrapped(screen, screen->cur_row);
		xtermAutoPrint('\n');
		xtermIndex(screen, 1);
		set_cur_col(screen, 0);
		screen->do_wrap = 0;
		this_col = last_col + 1;
	    } else
		this_col = 1;
	}
	if (offset + this_col > len) {
	    this_col = len - offset;
	}
	next_col = screen->cur_col + this_col;

	WriteText(screen, PAIRED_CHARS(buf + offset,
				       buf2 ? buf2 + offset : 0),
		  this_col);

	/*
	 * the call to WriteText updates screen->cur_col.
	 * If screen->cur_col != next_col, we must have
	 * hit the right margin, so set the do_wrap flag.
	 */
	screen->do_wrap = (screen->cur_col < (int) next_col);
    }

#endif
}

#if HANDLE_STRUCT_NOTIFY
/* Flag icon name with "*** "  on window output when iconified.
 * I'd like to do something like reverse video, but I don't
 * know how to tell this to window managers in general.
 *
 * mapstate can be IsUnmapped, !IsUnmapped, or -1;
 * -1 means no change; the other two are set by event handlers
 * and indicate a new mapstate.  !IsMapped is done in the handler.
 * we worry about IsUnmapped when output occurs.  -IAN!
 */
static int mapstate = -1;
#endif /* HANDLE_STRUCT_NOTIFY */

#if OPT_WIDE_CHARS
unsigned
visual_width(PAIRED_CHARS(Char * str, Char * str2), Cardinal len)
{
    /* returns the visual width of a string (doublewide characters count
       as 2, normalwide characters count as 1) */
    int my_len = 0;
    while (len) {
	int ch = *str;
	if (str2)
	    ch |= *str2 << 8;
	if (str)
	    str++;
	if (str2)
	    str2++;
	if (iswide(ch))
	    my_len += 2;
	else
	    my_len++;
	len--;
    }
    return my_len;
}
#endif

/*
 * write a string str of length len onto the screen at
 * the current cursor position.  update cursor position.
 */
static void
WriteText(TScreen * screen, PAIRED_CHARS(Char * str, Char * str2), Cardinal len)
{
    ScrnPtr PAIRED_CHARS(temp_str = 0, temp_str2 = 0);
    unsigned test;
    unsigned flags = term->flags;
    unsigned fg_bg = makeColorPair(term->cur_foreground, term->cur_background);
    unsigned cells = visual_width(PAIRED_CHARS(str, str2), len);
    GC currentGC;

    TRACE(("WriteText (%2d,%2d) (%d) %3d:%s\n",
	   screen->cur_row,
	   screen->cur_col,
	   curXtermChrSet(screen->cur_row),
	   len, visibleChars(PAIRED_CHARS(str, str2), len)));

    if (ScrnHaveSelection(screen)
	&& ScrnIsLineInSelection(screen, screen->cur_row - screen->topline)) {
	ScrnDisownSelection(screen);
    }

    if (screen->cur_row - screen->topline <= screen->max_row) {
	if (screen->cursor_state)
	    HideCursor();

	if (flags & INSERT) {
	    InsertChar(screen, cells);
	}
	if (!AddToRefresh(screen)) {

	    if (screen->scroll_amt)
		FlushScroll(screen);

	    if (flags & INVISIBLE) {
		if (cells > len) {
		    str = temp_str = TypeMallocN(Char, cells);
		    if (str == 0)
			return;
		}
		if_OPT_WIDE_CHARS(screen, {
		    if (cells > len) {
			str2 = temp_str2 = TypeMallocN(Char, cells);
		    }
		});
		len = cells;

		memset(str, ' ', len);
		if_OPT_WIDE_CHARS(screen, {
		    if (str2 != 0)
			memset(str2, 0, len);
		});
	    }

	    TRACE(("WriteText calling drawXtermText (%d,%d)\n",
		   screen->cur_col,
		   screen->cur_row));

	    test = flags;
	    checkVeryBoldColors(test, term->cur_foreground);

	    /* make sure that the correct GC is current */
	    currentGC = updatedXtermGC(screen, flags, fg_bg, False);

	    drawXtermText(screen, test & DRAWX_MASK, currentGC,
			  CurCursorX(screen, screen->cur_row, screen->cur_col),
			  CursorY(screen, screen->cur_row),
			  curXtermChrSet(screen->cur_row),
			  PAIRED_CHARS(str, str2), len, 0);

	    resetXtermGC(screen, flags, False);
	}
    }

    ScreenWrite(screen, PAIRED_CHARS(str, str2), flags, fg_bg, len);
    CursorForward(screen, (int) cells);
#if OPT_ZICONBEEP
    /* Flag icon name with "***"  on window output when iconified.
     */
    if (zIconBeep && mapstate == IsUnmapped && !zIconBeep_flagged) {
	static char *icon_name;
	static Arg args[] =
	{
	    {XtNiconName, (XtArgVal) & icon_name}
	};

	icon_name = NULL;
	XtGetValues(toplevel, args, XtNumber(args));

	if (icon_name != NULL) {
	    zIconBeep_flagged = True;
	    Changename(icon_name);
	}
	if (zIconBeep > 0) {
#if defined(HAVE_XKBBELL)
	    XkbBell(XtDisplay(toplevel), VShellWindow, zIconBeep, XkbBI_Info);
#else
	    XBell(XtDisplay(toplevel), zIconBeep);
#endif
	}
    }
    mapstate = -1;
#endif /* OPT_ZICONBEEP */
    if (temp_str != 0)
	free(temp_str);
    if_OPT_WIDE_CHARS(screen, {
	if (temp_str2 != 0)
	    free(temp_str2);
    });
    return;
}

#if HANDLE_STRUCT_NOTIFY
/* Flag icon name with "***"  on window output when iconified.
 */
static void
HandleStructNotify(Widget w GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XEvent * event,
		   Boolean * cont GCC_UNUSED)
{
    static char *icon_name;
    static Arg args[] =
    {
	{XtNiconName, (XtArgVal) & icon_name}
    };

    switch (event->type) {
    case MapNotify:
	TRACE(("HandleStructNotify(MapNotify)\n"));
#if OPT_ZICONBEEP
	if (zIconBeep_flagged) {
	    zIconBeep_flagged = False;
	    icon_name = NULL;
	    XtGetValues(toplevel, args, XtNumber(args));
	    if (icon_name != NULL) {
		char *buf = CastMallocN(char, strlen(icon_name));
		if (buf == NULL) {
		    zIconBeep_flagged = True;
		    return;
		}
		strcpy(buf, icon_name + 4);
		Changename(buf);
		free(buf);
	    }
	}
#endif /* OPT_ZICONBEEP */
	mapstate = !IsUnmapped;
	break;
    case UnmapNotify:
	TRACE(("HandleStructNotify(UnmapNotify)\n"));
	mapstate = IsUnmapped;
	break;
    case ConfigureNotify:
	TRACE(("HandleStructNotify(ConfigureNotify)\n"));
#if OPT_TOOLBAR
	/* the notify is for the top-level widget, but we care about vt100 */
	if (term->screen.Vshow) {
	    TScreen *screen = &term->screen;
	    struct _vtwin *Vwin = WhichVWin(&(term->screen));
	    TbInfo *info = &(Vwin->tb_info);
	    TbInfo save = *info;

	    if (info->menu_bar) {
		XtVaGetValues(info->menu_bar,
			      XtNheight, &info->menu_height,
			      XtNborderWidth, &info->menu_border,
			      (XtPointer) 0);

		if (save.menu_height != info->menu_height
		    || save.menu_border != info->menu_border) {

		    TRACE(("...menu_height %d\n", info->menu_height));
		    TRACE(("...menu_border %d\n", info->menu_border));
		    TRACE(("...had height  %d, border %d\n",
			   save.menu_height,
			   save.menu_border));

		    /*
		     * FIXME: Window manager still may be using the old values.
		     * Try to fool it.
		     */
		    XtMakeResizeRequest((Widget) term,
					screen->fullVwin.fullwidth,
					info->menu_height
					- save.menu_height
					+ screen->fullVwin.fullheight,
					NULL, NULL);
		    repairSizeHints();
		}
	    }
	}
#endif /* OPT_TOOLBAR */
	break;
    default:
	TRACE(("HandleStructNotify(event %d)\n", event->type));
	break;
    }
}
#endif /* HANDLE_STRUCT_NOTIFY */

#if OPT_BLINK_CURS
static void
SetCursorBlink(TScreen * screen, int enable)
{
    screen->cursor_blink = enable;
    if (DoStartBlinking(screen)) {
	StartBlinking(screen);
    } else {
#if !OPT_BLINK_TEXT
	StopBlinking(screen);
#endif
    }
    update_cursorblink();
}

void
ToggleCursorBlink(TScreen * screen)
{
    SetCursorBlink(screen, !(screen->cursor_blink));
}
#endif

/*
 * process ANSI modes set, reset
 */
static void
ansi_modes(XtermWidget termw,
	   void (*func) (unsigned *p, unsigned mask))
{
    int i;

    for (i = 0; i < nparam; ++i) {
	switch (param[i]) {
	case 2:		/* KAM (if set, keyboard locked */
	    (*func) (&termw->keyboard.flags, MODE_KAM);
	    break;

	case 4:		/* IRM                          */
	    (*func) (&termw->flags, INSERT);
	    break;

	case 12:		/* SRM (if set, local echo      */
	    (*func) (&termw->keyboard.flags, MODE_SRM);
	    break;

	case 20:		/* LNM                          */
	    (*func) (&termw->flags, LINEFEED);
	    update_autolinefeed();
	    break;
	}
    }
}

#define set_mousemode(mode) \
	screen->send_mouse_pos = (func == bitset) ? mode : MOUSE_OFF
#define set_mouseflag(f)		\
	((func == bitset)		\
	 ? SCREEN_FLAG_set(screen, f)	\
	 : SCREEN_FLAG_unset(screen, f))

/*
 * process DEC private modes set, reset
 */
static void
dpmodes(XtermWidget termw,
	void (*func) (unsigned *p, unsigned mask))
{
    TScreen *screen = &termw->screen;
    int i, j;

    for (i = 0; i < nparam; ++i) {
	TRACE(("%s %d\n", (func == bitset) ? "DECSET" : "DECRST", param[i]));
	switch (param[i]) {
	case 1:		/* DECCKM                       */
	    (*func) (&termw->keyboard.flags, MODE_DECCKM);
	    update_appcursor();
	    break;
	case 2:		/* DECANM - ANSI/VT52 mode      */
	    if (func == bitset) {	/* ANSI (VT100) */
		/*
		 * Setting DECANM should have no effect, since this function
		 * cannot be reached from vt52 mode.
		 */
		;
	    }
#if OPT_VT52_MODE
	    else if (screen->terminal_id >= 100) {	/* VT52 */
		TRACE(("DECANM terminal_id %d, vtXX_level %d\n",
		       screen->terminal_id,
		       screen->vtXX_level));
		screen->vt52_save_level = screen->vtXX_level;
		screen->vtXX_level = 0;
		screen->vt52_save_curgl = screen->curgl;
		screen->vt52_save_curgr = screen->curgr;
		screen->vt52_save_curss = screen->curss;
		memmove(screen->vt52_save_gsets, screen->gsets, sizeof(screen->gsets));
		resetCharsets(screen);
		nparam = 0;	/* ignore the remaining params, if any */
	    }
#endif
	    break;
	case 3:		/* DECCOLM                      */
	    if (screen->c132) {
		ClearScreen(screen);
		CursorSet(screen, 0, 0, termw->flags);
		if ((j = func == bitset ? 132 : 80) !=
		    ((termw->flags & IN132COLUMNS) ? 132 : 80) ||
		    j != MaxCols(screen))
		    RequestResize(termw, -1, j, True);
		(*func) (&termw->flags, IN132COLUMNS);
	    }
	    break;
	case 4:		/* DECSCLM (slow scroll)        */
	    if (func == bitset) {
		screen->jumpscroll = 0;
		if (screen->scroll_amt)
		    FlushScroll(screen);
	    } else
		screen->jumpscroll = 1;
	    (*func) (&termw->flags, SMOOTHSCROLL);
	    update_jumpscroll();
	    break;
	case 5:		/* DECSCNM                      */
	    j = termw->flags;
	    (*func) (&termw->flags, REVERSE_VIDEO);
	    if ((termw->flags ^ j) & REVERSE_VIDEO)
		ReverseVideo(termw);
	    /* update_reversevideo done in RevVid */
	    break;

	case 6:		/* DECOM                        */
	    (*func) (&termw->flags, ORIGIN);
	    CursorSet(screen, 0, 0, termw->flags);
	    break;

	case 7:		/* DECAWM                       */
	    (*func) (&termw->flags, WRAPAROUND);
	    update_autowrap();
	    break;
	case 8:		/* DECARM                       */
	    /* ignore autorepeat
	     * XAutoRepeatOn() and XAutoRepeatOff() can do this, but only
	     * for the whole display - not limited to a given window.
	     */
	    break;
	case SET_X10_MOUSE:	/* MIT bogus sequence           */
	    MotionOff(screen, termw);
	    set_mousemode(X10_MOUSE);
	    break;
#if OPT_TOOLBAR
	case 10:		/* rxvt */
	    ShowToolbar(func == bitset);
	    break;
#endif
#if OPT_BLINK_CURS
	case 12:		/* att610: Start/stop blinking cursor */
	    if (screen->cursor_blink_res) {
		screen->cursor_blink_esc = (func == bitset) ? ON : OFF;
		SetCursorBlink(screen, screen->cursor_blink);
	    }
	    break;
#endif
	case 18:		/* DECPFF: print form feed */
	    screen->printer_formfeed = (func == bitset) ? ON : OFF;
	    break;
	case 19:		/* DECPEX: print extent */
	    screen->printer_extent = (func == bitset) ? ON : OFF;
	    break;
	case 25:		/* DECTCEM: Show/hide cursor (VT200) */
	    screen->cursor_set = (func == bitset) ? ON : OFF;
	    break;
	case 30:		/* rxvt */
	    if (screen->fullVwin.sb_info.width != ((func == bitset) ? ON : OFF))
		ToggleScrollBar(termw);
	    break;
#if OPT_SHIFT_FONTS
	case 35:		/* rxvt */
	    term->misc.shift_fonts = (func == bitset) ? ON : OFF;
	    break;
#endif
	case 38:		/* DECTEK                       */
#if OPT_TEK4014
	    if (func == bitset && !(screen->inhibit & I_TEK)) {
		FlushLog(screen);
		screen->TekEmu = True;
	    }
#endif
	    break;
	case 40:		/* 132 column mode              */
	    screen->c132 = (func == bitset);
	    update_allow132();
	    break;
	case 41:		/* curses hack                  */
	    screen->curses = (func == bitset);
	    update_cursesemul();
	    break;
	case 42:		/* DECNRCM national charset (VT220) */
	    (*func) (&termw->flags, NATIONAL);
	    break;
	case 44:		/* margin bell                  */
	    screen->marginbell = (func == bitset);
	    if (!screen->marginbell)
		screen->bellarmed = -1;
	    update_marginbell();
	    break;
	case 45:		/* reverse wraparound   */
	    (*func) (&termw->flags, REVERSEWRAP);
	    update_reversewrap();
	    break;
#ifdef ALLOWLOGGING
	case 46:		/* logging              */
#ifdef ALLOWLOGFILEONOFF
	    /*
	     * if this feature is enabled, logging may be
	     * enabled and disabled via escape sequences.
	     */
	    if (func == bitset)
		StartLog(screen);
	    else
		CloseLog(screen);
#else
	    Bell(XkbBI_Info, 0);
	    Bell(XkbBI_Info, 0);
#endif /* ALLOWLOGFILEONOFF */
	    break;
#endif
	case 1049:		/* alternate buffer & cursor */
	    if (!termw->misc.titeInhibit) {
		if (func == bitset) {
		    CursorSave(termw);
		    ToAlternate(screen);
		    ClearScreen(screen);
		} else {
		    FromAlternate(screen);
		    CursorRestore(termw);
		}
	    } else if (termw->misc.tiXtraScroll) {
		if (func == bitset) {
		    xtermScroll(screen, screen->max_row);
		}
	    }
	    break;
	case 1047:
	case 47:		/* alternate buffer */
	    if (!termw->misc.titeInhibit) {
		if (func == bitset) {
		    ToAlternate(screen);
		} else {
		    if (screen->alternate
			&& (param[i] == 1047))
			ClearScreen(screen);
		    FromAlternate(screen);
		}
	    } else if (termw->misc.tiXtraScroll) {
		if (func == bitset) {
		    xtermScroll(screen, screen->max_row);
		}
	    }
	    break;
	case 66:		/* DECNKM */
	    (*func) (&termw->keyboard.flags, MODE_DECKPAM);
	    update_appkeypad();
	    break;
	case 67:		/* DECBKM */
	    /* back-arrow mapped to backspace or delete(D) */
	    (*func) (&termw->keyboard.flags, MODE_DECBKM);
	    TRACE(("DECSET DECBKM %s\n",
		   BtoS(termw->keyboard.flags & MODE_DECBKM)));
	    update_decbkm();
	    break;
	case SET_VT200_MOUSE:	/* xterm bogus sequence         */
	    MotionOff(screen, termw);
	    set_mousemode(VT200_MOUSE);
	    break;
	case SET_VT200_HIGHLIGHT_MOUSE:	/* xterm sequence w/hilite tracking */
	    MotionOff(screen, termw);
	    set_mousemode(VT200_HIGHLIGHT_MOUSE);
	    break;
	case SET_BTN_EVENT_MOUSE:
	    MotionOff(screen, termw);
	    set_mousemode(BTN_EVENT_MOUSE);
	    break;
	case SET_ANY_EVENT_MOUSE:
	    set_mousemode(ANY_EVENT_MOUSE);
	    if (screen->send_mouse_pos == MOUSE_OFF) {
		MotionOff(screen, term);
	    } else {
		MotionOn(screen, term);
	    }
	    break;
	case 1010:		/* rxvt */
	    screen->scrollttyoutput = (func == bitset) ? ON : OFF;
	    update_scrollttyoutput();
	    break;
	case 1011:		/* rxvt */
	    screen->scrollkey = (func == bitset) ? ON : OFF;
	    update_scrollkey();
	    break;
#if OPT_NUM_LOCK
	case 1035:
	    term->misc.real_NumLock = (func == bitset) ? ON : OFF;
	    update_num_lock();
	    break;
	case 1036:
	    screen->meta_sends_esc = (func == bitset) ? ON : OFF;
	    update_meta_esc();
	    break;
#endif
	case 1037:
	    screen->delete_is_del = (func == bitset) ? ON : OFF;
	    update_delete_del();
	    break;
	case 1048:
	    if (!termw->misc.titeInhibit) {
		if (func == bitset)
		    CursorSave(termw);
		else
		    CursorRestore(termw);
	    }
	    break;
	case 1051:
	    set_keyboard_type(keyboardIsSun, func == bitset);
	    break;
#if OPT_HP_FUNC_KEYS
	case 1052:
	    set_keyboard_type(keyboardIsHP, func == bitset);
	    break;
#endif
#if OPT_SCO_FUNC_KEYS
	case 1053:
	    set_keyboard_type(keyboardIsSCO, func == bitset);
	    break;
#endif
	case 1060:
	    set_keyboard_type(keyboardIsLegacy, func == bitset);
	    break;
#if OPT_SUNPC_KBD
	case 1061:
	    set_keyboard_type(keyboardIsVT220, func == bitset);
	    break;
#endif
#if OPT_READLINE
	case SET_BUTTON1_MOVE_POINT:
	    set_mouseflag(click1_moves);
	    break;
	case SET_BUTTON2_MOVE_POINT:
	    set_mouseflag(paste_moves);
	    break;
	case SET_DBUTTON3_DELETE:
	    set_mouseflag(dclick3_deletes);
	    break;
	case SET_PASTE_IN_BRACKET:
	    set_mouseflag(paste_brackets);
	    break;
	case SET_PASTE_QUOTE:
	    set_mouseflag(paste_quotes);
	    break;
	case SET_PASTE_LITERAL_NL:
	    set_mouseflag(paste_literal_nl);
	    break;
#endif /* OPT_READLINE */
	}
    }
}

/*
 * process xterm private modes save
 */
static void
savemodes(XtermWidget termw)
{
    TScreen *screen = &termw->screen;
    int i;

    for (i = 0; i < nparam; i++) {
	TRACE(("savemodes %d\n", param[i]));
	switch (param[i]) {
	case 1:		/* DECCKM                       */
	    DoSM(DP_DECCKM, termw->keyboard.flags & MODE_DECCKM);
	    break;
	case 3:		/* DECCOLM                      */
	    if (screen->c132)
		DoSM(DP_DECCOLM, termw->flags & IN132COLUMNS);
	    break;
	case 4:		/* DECSCLM (slow scroll)        */
	    DoSM(DP_DECSCLM, termw->flags & SMOOTHSCROLL);
	    break;
	case 5:		/* DECSCNM                      */
	    DoSM(DP_DECSCNM, termw->flags & REVERSE_VIDEO);
	    break;
	case 6:		/* DECOM                        */
	    DoSM(DP_DECOM, termw->flags & ORIGIN);
	    break;

	case 7:		/* DECAWM                       */
	    DoSM(DP_DECAWM, termw->flags & WRAPAROUND);
	    break;
	case 8:		/* DECARM                       */
	    /* ignore autorepeat */
	    break;
	case SET_X10_MOUSE:	/* mouse bogus sequence */
	    DoSM(DP_X_X10MSE, screen->send_mouse_pos);
	    break;
#if OPT_TOOLBAR
	case 10:		/* rxvt */
	    DoSM(DP_TOOLBAR, resource.toolBar);
	    break;
#endif
#if OPT_BLINK_CURS
	case 12:		/* att610: Start/stop blinking cursor */
	    if (screen->cursor_blink_res) {
		DoSM(DP_CRS_BLINK, screen->cursor_blink_esc);
	    }
	    break;
#endif
	case 18:		/* DECPFF: print form feed */
	    DoSM(DP_PRN_FORMFEED, screen->printer_formfeed);
	    break;
	case 19:		/* DECPEX: print extent */
	    DoSM(DP_PRN_EXTENT, screen->printer_extent);
	    break;
	case 25:		/* DECTCEM: Show/hide cursor (VT200) */
	    DoSM(DP_CRS_VISIBLE, screen->cursor_set);
	    break;
	case 40:		/* 132 column mode              */
	    DoSM(DP_X_DECCOLM, screen->c132);
	    break;
	case 41:		/* curses hack                  */
	    DoSM(DP_X_MORE, screen->curses);
	    break;
	case 44:		/* margin bell                  */
	    DoSM(DP_X_MARGIN, screen->marginbell);
	    break;
	case 45:		/* reverse wraparound   */
	    DoSM(DP_X_REVWRAP, termw->flags & REVERSEWRAP);
	    break;
#ifdef ALLOWLOGGING
	case 46:		/* logging              */
	    DoSM(DP_X_LOGGING, screen->logging);
	    break;
#endif
	case 1047:		/* alternate buffer             */
	    /* FALLTHRU */
	case 47:		/* alternate buffer             */
	    DoSM(DP_X_ALTSCRN, screen->alternate);
	    break;
	case SET_VT200_MOUSE:	/* mouse bogus sequence         */
	case SET_VT200_HIGHLIGHT_MOUSE:
	case SET_BTN_EVENT_MOUSE:
	case SET_ANY_EVENT_MOUSE:
	    DoSM(DP_X_MOUSE, screen->send_mouse_pos);
	    break;
	case 1048:
	    if (!termw->misc.titeInhibit) {
		CursorSave(termw);
	    }
	    break;
#if OPT_READLINE
	case SET_BUTTON1_MOVE_POINT:
	    SCREEN_FLAG_save(screen, click1_moves);
	    break;
	case SET_BUTTON2_MOVE_POINT:
	    SCREEN_FLAG_save(screen, paste_moves);
	    break;
	case SET_DBUTTON3_DELETE:
	    SCREEN_FLAG_save(screen, dclick3_deletes);
	    break;
	case SET_PASTE_IN_BRACKET:
	    SCREEN_FLAG_save(screen, paste_brackets);
	    break;
	case SET_PASTE_QUOTE:
	    SCREEN_FLAG_save(screen, paste_quotes);
	    break;
	case SET_PASTE_LITERAL_NL:
	    SCREEN_FLAG_save(screen, paste_literal_nl);
	    break;
#endif /* OPT_READLINE */
	}
    }
}

/*
 * process xterm private modes restore
 */
static void
restoremodes(XtermWidget termw)
{
    TScreen *screen = &termw->screen;
    int i, j;

    for (i = 0; i < nparam; i++) {
	TRACE(("restoremodes %d\n", param[i]));
	switch (param[i]) {
	case 1:		/* DECCKM                       */
	    bitcpy(&termw->keyboard.flags,
		   screen->save_modes[DP_DECCKM], MODE_DECCKM);
	    update_appcursor();
	    break;
	case 3:		/* DECCOLM                      */
	    if (screen->c132) {
		ClearScreen(screen);
		CursorSet(screen, 0, 0, termw->flags);
		if ((j = (screen->save_modes[DP_DECCOLM] & IN132COLUMNS)
		     ? 132 : 80) != ((termw->flags & IN132COLUMNS)
				     ? 132 : 80) || j != MaxCols(screen))
		    RequestResize(termw, -1, j, True);
		bitcpy(&termw->flags,
		       screen->save_modes[DP_DECCOLM],
		       IN132COLUMNS);
	    }
	    break;
	case 4:		/* DECSCLM (slow scroll)        */
	    if (screen->save_modes[DP_DECSCLM] & SMOOTHSCROLL) {
		screen->jumpscroll = 0;
		if (screen->scroll_amt)
		    FlushScroll(screen);
	    } else
		screen->jumpscroll = 1;
	    bitcpy(&termw->flags, screen->save_modes[DP_DECSCLM], SMOOTHSCROLL);
	    update_jumpscroll();
	    break;
	case 5:		/* DECSCNM                      */
	    if ((screen->save_modes[DP_DECSCNM] ^ termw->flags) & REVERSE_VIDEO) {
		bitcpy(&termw->flags, screen->save_modes[DP_DECSCNM], REVERSE_VIDEO);
		ReverseVideo(termw);
		/* update_reversevideo done in RevVid */
	    }
	    break;
	case 6:		/* DECOM                        */
	    bitcpy(&termw->flags, screen->save_modes[DP_DECOM], ORIGIN);
	    CursorSet(screen, 0, 0, termw->flags);
	    break;

	case 7:		/* DECAWM                       */
	    bitcpy(&termw->flags, screen->save_modes[DP_DECAWM], WRAPAROUND);
	    update_autowrap();
	    break;
	case 8:		/* DECARM                       */
	    /* ignore autorepeat */
	    break;
	case SET_X10_MOUSE:	/* MIT bogus sequence           */
	    DoRM(DP_X_X10MSE, screen->send_mouse_pos);
	    break;
#if OPT_TOOLBAR
	case 10:		/* rxvt */
	    DoRM(DP_TOOLBAR, resource.toolBar);
	    ShowToolbar(resource.toolBar);
	    break;
#endif
#if OPT_BLINK_CURS
	case 12:		/* att610: Start/stop blinking cursor */
	    if (screen->cursor_blink_res) {
		DoRM(DP_CRS_BLINK, screen->cursor_blink_esc);
		SetCursorBlink(screen, screen->cursor_blink);
	    }
	    break;
#endif
	case 18:		/* DECPFF: print form feed */
	    DoRM(DP_PRN_FORMFEED, screen->printer_formfeed);
	    break;
	case 19:		/* DECPEX: print extent */
	    DoRM(DP_PRN_EXTENT, screen->printer_extent);
	    break;
	case 25:		/* DECTCEM: Show/hide cursor (VT200) */
	    DoRM(DP_CRS_VISIBLE, screen->cursor_set);
	    break;
	case 40:		/* 132 column mode              */
	    DoRM(DP_X_DECCOLM, screen->c132);
	    update_allow132();
	    break;
	case 41:		/* curses hack                  */
	    DoRM(DP_X_MORE, screen->curses);
	    update_cursesemul();
	    break;
	case 44:		/* margin bell                  */
	    if ((DoRM(DP_X_MARGIN, screen->marginbell)) == 0)
		screen->bellarmed = -1;
	    update_marginbell();
	    break;
	case 45:		/* reverse wraparound   */
	    bitcpy(&termw->flags, screen->save_modes[DP_X_REVWRAP], REVERSEWRAP);
	    update_reversewrap();
	    break;
#ifdef ALLOWLOGGING
	case 46:		/* logging              */
#ifdef ALLOWLOGFILEONOFF
	    if (screen->save_modes[DP_X_LOGGING])
		StartLog(screen);
	    else
		CloseLog(screen);
#endif /* ALLOWLOGFILEONOFF */
	    /* update_logging done by StartLog and CloseLog */
	    break;
#endif
	case 1047:		/* alternate buffer */
	    /* FALLTHRU */
	case 47:		/* alternate buffer */
	    if (!termw->misc.titeInhibit) {
		if (screen->save_modes[DP_X_ALTSCRN])
		    ToAlternate(screen);
		else
		    FromAlternate(screen);
		/* update_altscreen done by ToAlt and FromAlt */
	    } else if (termw->misc.tiXtraScroll) {
		if (screen->save_modes[DP_X_ALTSCRN]) {
		    xtermScroll(screen, screen->max_row);
		}
	    }
	    break;
	case SET_VT200_MOUSE:	/* mouse bogus sequence         */
	case SET_VT200_HIGHLIGHT_MOUSE:
	case SET_BTN_EVENT_MOUSE:
	case SET_ANY_EVENT_MOUSE:
	    DoRM(DP_X_MOUSE, screen->send_mouse_pos);
	    break;
	case 1048:
	    if (!termw->misc.titeInhibit) {
		CursorRestore(termw);
	    }
	    break;
#if OPT_READLINE
	case SET_BUTTON1_MOVE_POINT:
	    SCREEN_FLAG_restore(screen, click1_moves);
	    break;
	case SET_BUTTON2_MOVE_POINT:
	    SCREEN_FLAG_restore(screen, paste_moves);
	    break;
	case SET_DBUTTON3_DELETE:
	    SCREEN_FLAG_restore(screen, dclick3_deletes);
	    break;
	case SET_PASTE_IN_BRACKET:
	    SCREEN_FLAG_restore(screen, paste_brackets);
	    break;
	case SET_PASTE_QUOTE:
	    SCREEN_FLAG_restore(screen, paste_quotes);
	    break;
	case SET_PASTE_LITERAL_NL:
	    SCREEN_FLAG_restore(screen, paste_literal_nl);
	    break;
#endif /* OPT_READLINE */
	}
    }
}

/*
 * Report window label (icon or title) in dtterm protocol
 * ESC ] code label ESC backslash
 */
static void
report_win_label(TScreen * screen,
		 int code,
		 XTextProperty * text,
		 Status ok)
{
    char **list;
    int length = 0;

    reply.a_type = ESC;
    unparseputc(ESC, screen->respond);
    unparseputc(']', screen->respond);
    unparseputc(code, screen->respond);

    if (ok) {
	if (XTextPropertyToStringList(text, &list, &length)) {
	    int n, c;
	    for (n = 0; n < length; n++) {
		char *s = list[n];
		while ((c = *s++) != '\0')
		    unparseputc(c, screen->respond);
	    }
	    XFreeStringList(list);
	}
	if (text->value != 0)
	    XFree(text->value);
    }

    unparseputc(ESC, screen->respond);
    unparseputc('\\', screen->respond);
}

/*
 * Window operations (from CDE dtterm description, as well as extensions).
 * See also "allowWindowOps" resource.
 */
static void
window_ops(XtermWidget termw)
{
    TScreen *screen = &termw->screen;
    XWindowChanges values;
    XWindowAttributes win_attrs;
    XTextProperty text;
    unsigned value_mask;
    unsigned root_width;
    unsigned root_height;

    TRACE(("window_ops %d\n", param[0]));
    switch (param[0]) {
    case 1:			/* Restore (de-iconify) window */
	XMapWindow(screen->display,
		   VShellWindow);
	break;

    case 2:			/* Minimize (iconify) window */
	XIconifyWindow(screen->display,
		       VShellWindow,
		       DefaultScreen(screen->display));
	break;

    case 3:			/* Move the window to the given position */
	values.x = param[1];
	values.y = param[2];
	value_mask = (CWX | CWY);
	XReconfigureWMWindow(screen->display,
			     VShellWindow,
			     DefaultScreen(screen->display),
			     value_mask,
			     &values);
	break;

    case 4:			/* Resize the window to given size in pixels */
	RequestResize(termw, param[1], param[2], False);
	break;

    case 5:			/* Raise the window to the front of the stack */
	XRaiseWindow(screen->display, VShellWindow);
	break;

    case 6:			/* Lower the window to the bottom of the stack */
	XLowerWindow(screen->display, VShellWindow);
	break;

    case 7:			/* Refresh the window */
	Redraw();
	break;

    case 8:			/* Resize the text-area, in characters */
	RequestResize(termw, param[1], param[2], True);
	break;

#if OPT_MAXIMIZE
    case 9:			/* Maximize or restore */
	RequestMaximize(termw, param[1]);
	break;
#endif

    case 11:			/* Report the window's state */
	XGetWindowAttributes(screen->display,
			     VWindow(screen),
			     &win_attrs);
	reply.a_type = CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 1;
	reply.a_param[0] = (win_attrs.map_state == IsViewable) ? 1 : 2;
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(&reply, screen->respond);
	break;

    case 13:			/* Report the window's position */
	XGetWindowAttributes(screen->display,
			     WMFrameWindow(termw),
			     &win_attrs);
	reply.a_type = CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 3;
	reply.a_param[0] = 3;
	reply.a_param[1] = win_attrs.x;
	reply.a_param[2] = win_attrs.y;
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(&reply, screen->respond);
	break;

    case 14:			/* Report the window's size in pixels */
	XGetWindowAttributes(screen->display,
			     VWindow(screen),
			     &win_attrs);
	reply.a_type = CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 3;
	reply.a_param[0] = 4;
	/*FIXME: find if dtterm uses
	 *    win_attrs.height or Height
	 *      win_attrs.width  or Width
	 */
	reply.a_param[1] = Height(screen);
	reply.a_param[2] = Width(screen);
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(&reply, screen->respond);
	break;

    case 18:			/* Report the text's size in characters */
	reply.a_type = CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 3;
	reply.a_param[0] = 8;
	reply.a_param[1] = MaxRows(screen);
	reply.a_param[2] = MaxCols(screen);
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(&reply, screen->respond);
	break;

#if OPT_MAXIMIZE
    case 19:			/* Report the screen's size, in characters */
	if (!QueryMaximize(term, &root_height, &root_width)) {
	    root_height = 0;
	    root_width = 0;
	}
	reply.a_type = CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 3;
	reply.a_param[0] = 9;
	reply.a_param[1] = root_height / FontHeight(screen);
	reply.a_param[2] = root_width / FontWidth(screen);
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(&reply, screen->respond);
	break;
#endif

    case 20:			/* Report the icon's label */
	report_win_label(screen, 'L', &text,
			 XGetWMIconName(screen->display, VShellWindow, &text));
	break;

    case 21:			/* Report the window's title */
	report_win_label(screen, 'l', &text,
			 XGetWMName(screen->display, VShellWindow, &text));
	break;

    default:			/* DECSLPP (24, 25, 36, 48, 72, 144) */
	if (param[0] >= 24)
	    RequestResize(termw, param[0], -1, True);
	break;
    }
}

/*
 * set a bit in a word given a pointer to the word and a mask.
 */
static void
bitset(unsigned *p, unsigned mask)
{
    *p |= mask;
}

/*
 * clear a bit in a word given a pointer to the word and a mask.
 */
static void
bitclr(unsigned *p, unsigned mask)
{
    *p &= ~mask;
}

/*
 * Copy bits from one word to another, given a mask
 */
static void
bitcpy(unsigned *p, unsigned q, unsigned mask)
{
    bitclr(p, mask);
    bitset(p, q & mask);
}

void
unparseputc1(int c, int fd)
{
    if (c >= 0x80 && c <= 0x9F) {
	if (!term->screen.control_eight_bits) {
	    unparseputc(A2E(ESC), fd);
	    c = A2E(c - 0x40);
	}
    }
    unparseputc(c, fd);
}

void
unparseseq(ANSI * ap, int fd)
{
    int c;
    int i;
    int inters;

    unparseputc1(c = ap->a_type, fd);
    if (c == ESC
	|| c == DCS
	|| c == CSI
	|| c == OSC
	|| c == PM
	|| c == APC
	|| c == SS3) {
	if (ap->a_pintro != 0)
	    unparseputc(ap->a_pintro, fd);
	for (i = 0; i < ap->a_nparam; ++i) {
	    if (i != 0)
		unparseputc(';', fd);
	    unparseputn((unsigned int) ap->a_param[i], fd);
	}
	if ((inters = ap->a_inters) != 0) {
	    for (i = 3; i >= 0; --i) {
		c = CharOf(inters >> (8 * i));
		if (c != 0)
		    unparseputc(c, fd);
	    }
	}
	unparseputc((char) ap->a_final, fd);
    }
}

static void
unparseputn(unsigned int n, int fd)
{
    unsigned int q;

    q = n / 10;
    if (q != 0)
	unparseputn(q, fd);
    unparseputc((char) ('0' + (n % 10)), fd);
}

void
unparseputc(int c, int fd)
{
    IChar buf[2];
    unsigned i = 1;

#if OPT_TCAP_QUERY
    /*
     * If we're returning a termcap string, it has to be translated since
     * a DCS must not contain any characters except for the normal 7-bit
     * printable ASCII (counting tab, carriage return, etc).  For now,
     * just use hexadecimal for the whole thing.
     */
    if (term->screen.tc_query >= 0) {
	char tmp[3];
	sprintf(tmp, "%02X", c & 0xFF);
	buf[0] = tmp[0];
	buf[1] = tmp[1];
	i = 2;
    } else
#endif
    if ((buf[0] = c) == '\r' && (term->flags & LINEFEED)) {
	buf[1] = '\n';
	i++;
    }
#ifdef VMS
    tt_write(&buf, i);
#else /* VMS */
    writePtyData(fd, buf, i);
#endif /* VMS */

    /* If send/receive mode is reset, we echo characters locally */
    if ((term->keyboard.flags & MODE_SRM) == 0) {
	(void) doparsing((unsigned) c, &myState);
    }
}

void
unparseputs(char *s, int fd)
{
    while (*s)
	unparseputc(*s++, fd);
}

void
ToggleAlternate(TScreen * screen)
{
    if (screen->alternate)
	FromAlternate(screen);
    else
	ToAlternate(screen);
}

static void
ToAlternate(TScreen * screen)
{
    if (screen->alternate)
	return;
    TRACE(("ToAlternate\n"));
    if (!screen->altbuf)
	screen->altbuf = Allocate(MaxRows(screen), MaxCols(screen),
				  &screen->abuf_address);
    SwitchBufs(screen);
    screen->alternate = True;
    update_altscreen();
}

static void
FromAlternate(TScreen * screen)
{
    if (!screen->alternate)
	return;
    TRACE(("FromAlternate\n"));
    if (screen->scroll_amt)
	FlushScroll(screen);
    screen->alternate = False;
    SwitchBufs(screen);
    update_altscreen();
}

static void
SwitchBufs(TScreen * screen)
{
    int rows, top;

    if (screen->cursor_state)
	HideCursor();

    rows = MaxRows(screen);
    SwitchBufPtrs(screen);

    if ((top = -screen->topline) < rows) {
	if (screen->scroll_amt)
	    FlushScroll(screen);
	if (top == 0)
	    XClearWindow(screen->display, VWindow(screen));
	else
	    XClearArea(screen->display,
		       VWindow(screen),
		       (int) OriginX(screen),
		       (int) top * FontHeight(screen) + screen->border,
		       (unsigned) Width(screen),
		       (unsigned) (rows - top) * FontHeight(screen),
		       False);
    }
    ScrnUpdate(screen, 0, 0, rows, MaxCols(screen), False);
}

/* swap buffer line pointers between alt and regular screens */
void
SwitchBufPtrs(TScreen * screen)
{
    size_t len = ScrnPointers(screen, (unsigned) MaxRows(screen));

    memcpy((char *) screen->save_ptr, (char *) screen->visbuf, len);
    memcpy((char *) screen->visbuf, (char *) screen->altbuf, len);
    memcpy((char *) screen->altbuf, (char *) screen->save_ptr, len);
}

void
VTRun(void)
{
    TScreen *screen = &term->screen;

    TRACE(("VTRun ...\n"));

    if (!screen->Vshow) {
	set_vt_visibility(True);
    }
    update_vttekmode();
    update_vtshow();
    update_tekshow();
    set_vthide_sensitivity();

    if (screen->allbuf == NULL)
	VTallocbuf();

    screen->cursor_state = OFF;
    screen->cursor_set = ON;
#if OPT_BLINK_CURS
    if (DoStartBlinking(screen))
	StartBlinking(screen);
#endif

#if OPT_TEK4014
    if (Tpushb > Tpushback) {
	fillPtyData(screen, VTbuffer, (char *) Tpushback, Tpushb - Tpushback);
	Tpushb = Tpushback;
    }
#endif
    if (!setjmp(VTend))
	VTparse();
    StopBlinking(screen);
    HideCursor();
    screen->cursor_set = OFF;
}

/*ARGSUSED*/
static void
VTExpose(Widget w GCC_UNUSED,
	 XEvent * event,
	 Region region GCC_UNUSED)
{
    TScreen *screen = &term->screen;

#ifdef DEBUG
    if (debug)
	fputs("Expose\n", stderr);
#endif /* DEBUG */
    if (event->type == Expose)
	HandleExposure(screen, event);
}

static void
VTGraphicsOrNoExpose(XEvent * event)
{
    TScreen *screen = &term->screen;
    if (screen->incopy <= 0) {
	screen->incopy = 1;
	if (screen->scrolls > 0)
	    screen->scrolls--;
    }
    if (event->type == GraphicsExpose)
	if (HandleExposure(screen, event))
	    screen->cursor_state = OFF;
    if ((event->type == NoExpose)
	|| ((XGraphicsExposeEvent *) event)->count == 0) {
	if (screen->incopy <= 0 && screen->scrolls > 0)
	    screen->scrolls--;
	if (screen->scrolls)
	    screen->incopy = -1;
	else
	    screen->incopy = 0;
    }
}

/*ARGSUSED*/
static void
VTNonMaskableEvent(Widget w GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XEvent * event,
		   Boolean * cont GCC_UNUSED)
{
    switch (event->type) {
    case GraphicsExpose:
    case NoExpose:
	VTGraphicsOrNoExpose(event);
	break;
    }
}

static void
VTResize(Widget w)
{
    if (XtIsRealized(w)) {
	ScreenResize(&term->screen, term->core.width, term->core.height, &term->flags);
    }
}

#define okDimension(src,dst) ((src <= 32767) && ((dst = src) == src))

static void
RequestResize(XtermWidget termw, int rows, int cols, int text)
{
#ifndef nothack
    XSizeHints sizehints;
    long supp;
#endif
    TScreen *screen = &termw->screen;
    unsigned long value;
    Dimension replyWidth, replyHeight;
    Dimension askedWidth, askedHeight;
    XtGeometryResult status;
    XWindowAttributes attrs;

    TRACE(("RequestResize(rows=%d, cols=%d, text=%d)\n", rows, cols, text));

    if ((askedWidth = cols) < cols
	|| (askedHeight = rows) < rows)
	return;

    if (askedHeight == 0
	|| askedWidth == 0
	|| term->misc.limit_resize > 0) {
	XGetWindowAttributes(XtDisplay(termw),
			     RootWindowOfScreen(XtScreen(termw)), &attrs);
    }

    if (text) {
	if ((value = rows) != 0) {
	    if (rows < 0)
		value = MaxRows(screen);
	    value *= FontHeight(screen);
	    value += (2 * screen->border);
	    if (!okDimension(value, askedHeight))
		return;
	}

	if ((value = cols) != 0) {
	    if (cols < 0)
		value = MaxCols(screen);
	    value *= FontWidth(screen);
	    value += (2 * screen->border) + ScrollbarWidth(screen);
	    if (!okDimension(value, askedWidth))
		return;
	}

    } else {
	if (rows < 0)
	    askedHeight = FullHeight(screen);
	if (cols < 0)
	    askedWidth = FullWidth(screen);
    }

    if (rows == 0)
	askedHeight = attrs.height;
    if (cols == 0)
	askedWidth = attrs.width;

    if (term->misc.limit_resize > 0) {
	Dimension high = term->misc.limit_resize * attrs.height;
	Dimension wide = term->misc.limit_resize * attrs.width;
	if (high < attrs.height)
	    high = attrs.height;
	if (askedHeight > high)
	    askedHeight = high;
	if (wide < attrs.width)
	    wide = attrs.width;
	if (askedWidth > wide)
	    askedWidth = wide;
    }
#ifndef nothack
    if (!XGetWMNormalHints(screen->display, VShellWindow,
			   &sizehints, &supp))
	bzero(&sizehints, sizeof(sizehints));
#endif

    status = XtMakeResizeRequest((Widget) termw,
				 askedWidth, askedHeight,
				 &replyWidth, &replyHeight);
    TRACE(("charproc.c XtMakeResizeRequest %dx%d -> %dx%d (status %d)\n",
	   askedHeight, askedWidth,
	   replyHeight, replyWidth,
	   status));

    if (status == XtGeometryYes ||
	status == XtGeometryDone) {
	ScreenResize(&termw->screen,
		     replyWidth,
		     replyHeight,
		     &termw->flags);
    }
#ifndef nothack
    /*
     * XtMakeResizeRequest() has the undesirable side-effect of clearing
     * the window manager's hints, even on a failed request.  This would
     * presumably be fixed if the shell did its own work.
     */
    if (sizehints.flags
	&& replyHeight
	&& replyWidth) {
	sizehints.height = replyHeight;
	sizehints.width = replyWidth;

	TRACE(("%s@%d -- ", __FILE__, __LINE__));
	TRACE_HINTS(&sizehints);
	XSetWMNormalHints(screen->display, VShellWindow, &sizehints);
	TRACE(("%s@%d -- ", __FILE__, __LINE__));
	TRACE_WM_HINTS(termw);
    }
#endif

    XSync(screen->display, False);	/* synchronize */
    if (XtAppPending(app_con))
	xevents();
}

static String xterm_trans =
"<ClientMessage>WM_PROTOCOLS: DeleteWindow()\n\
     <MappingNotify>: KeyboardMapping()\n";

int
VTInit(void)
{
    TScreen *screen = &term->screen;
    Widget vtparent = SHELL_OF(term);

    XtRealizeWidget(vtparent);
    XtOverrideTranslations(vtparent, XtParseTranslationTable(xterm_trans));
    (void) XSetWMProtocols(XtDisplay(vtparent), XtWindow(vtparent),
			   &wm_delete_window, 1);
    TRACE_TRANS("shell", vtparent);
    TRACE_TRANS("vt100", (Widget) (term));

    if (screen->allbuf == NULL)
	VTallocbuf();
    return (1);
}

static void
VTallocbuf(void)
{
    TScreen *screen = &term->screen;
    int nrows = MaxRows(screen);

    /* allocate screen buffer now, if necessary. */
    if (screen->scrollWidget)
	nrows += screen->savelines;
    screen->allbuf = Allocate(nrows, MaxCols(screen),
			      &screen->sbuf_address);
    if (screen->scrollWidget)
	screen->visbuf = &screen->allbuf[MAX_PTRS * screen->savelines];
    else
	screen->visbuf = screen->allbuf;
    return;
}

static void
VTClassInit(void)
{
    XtAddConverter(XtRString, XtRGravity, XmuCvtStringToGravity,
		   (XtConvertArgList) NULL, (Cardinal) 0);
}

/*
 * The whole wnew->screen struct is zeroed in VTInitialize.  Use these macros
 * where applicable for copying the pieces from the request widget into the
 * new widget.  We do not have to use them for wnew->misc, but the associated
 * traces are very useful for debugging.
 */
#if OPT_TRACE
#define init_Bres(name) \
	TRACE(("init " #name " = %s\n", \
		BtoS(wnew->name = request->name)))
#define init_Dres(name) \
	TRACE(("init " #name " = %f\n", \
		wnew->name = request->name))
#define init_Ires(name) \
	TRACE(("init " #name " = %d\n", \
		wnew->name = request->name))
#define init_Sres(name) \
	TRACE(("init " #name " = \"%s\"\n", \
		(wnew->name = x_strtrim(request->name)) != NULL \
			? wnew->name : "<null>"))
#define init_Tres(offset) \
	TRACE(("init screen.Tcolors[" #offset "] = %#lx\n", \
		fill_Tres(wnew, request, offset)))
#else
#define init_Bres(name) wnew->name = request->name
#define init_Dres(name) wnew->name = request->name
#define init_Ires(name) wnew->name = request->name
#define init_Sres(name) wnew->name = x_strtrim(request->name)
#define init_Tres(offset) fill_Tres(wnew, request, offset)
#endif

#if OPT_COLOR_RES
/*
 * Override the use of XtDefaultForeground/XtDefaultBackground to make some
 * colors, such as cursor color, use the actual foreground/background value
 * if there is no explicit resource value used.
 */
static Pixel
fill_Tres(XtermWidget target, XtermWidget source, int offset)
{
    char *name;
    ScrnColors temp;

    target->screen.Tcolors[offset] = source->screen.Tcolors[offset];
    target->screen.Tcolors[offset].mode = False;

    if ((name = x_strtrim(target->screen.Tcolors[offset].resource)) != 0)
	target->screen.Tcolors[offset].resource = name;

    if (name == 0) {
	target->screen.Tcolors[offset].value = target->dft_foreground;
    } else if (!x_strcasecmp(name, XtDefaultForeground)) {
	target->screen.Tcolors[offset].value =
	    ((offset == TEXT_FG || offset == TEXT_BG)
	     ? target->dft_foreground
	     : target->screen.Tcolors[TEXT_FG].value);
    } else if (!x_strcasecmp(name, XtDefaultBackground)) {
	target->screen.Tcolors[offset].value =
	    ((offset == TEXT_FG || offset == TEXT_BG)
	     ? target->dft_background
	     : target->screen.Tcolors[TEXT_BG].value);
    } else {
	if (AllocateTermColor(target, &temp, offset, name)) {
	    target->screen.Tcolors[offset].value = temp.colors[offset];
	}
    }
    return target->screen.Tcolors[offset].value;
}
#else
#define fill_Tres(target, source, offset) \
	target->screen.Tcolors[offset] = source->screen.Tcolors[offset]
#endif

#if OPT_WIDE_CHARS
static void
VTInitialize_locale(XtermWidget request)
{
    Bool is_utf8 = xtermEnvUTF8();

    TRACE(("VTInitialize_locale\n"));
    TRACE(("... request screen.utf8_mode = %d\n", request->screen.utf8_mode));

    if (request->screen.utf8_mode < 0)
	request->screen.utf8_mode = uFalse;

    if (request->screen.utf8_mode > 3)
	request->screen.utf8_mode = uDefault;

    request->screen.latin9_mode = 0;
    request->screen.unicode_font = 0;
#if OPT_LUIT_PROG
    request->misc.callfilter = 0;
    request->misc.use_encoding = 0;

    TRACE(("... setup for luit:\n"));
    TRACE(("... request misc.locale_str = \"%s\"\n", request->misc.locale_str));

    if (request->screen.utf8_mode == uFalse) {
	TRACE(("... command-line +u8 overrides\n"));
    } else
#if OPT_MINI_LUIT
    if (x_strcasecmp(request->misc.locale_str, "CHECKFONT") == 0) {
	int fl = (request->misc.default_font.f_n
		  ? strlen(request->misc.default_font.f_n)
		  : 0);
	if (fl > 11
	    && x_strcasecmp(request->misc.default_font.f_n + fl - 11,
			    "-ISO10646-1") == 0) {
	    request->screen.unicode_font = 1;
	    /* unicode font, use True */
#ifdef HAVE_LANGINFO_CODESET
	    if (!strcmp(xtermEnvEncoding(), "ANSI_X3.4-1968")
		|| !strcmp(xtermEnvEncoding(), "ISO-8859-1")) {
		if (request->screen.utf8_mode == uDefault)
		    request->screen.utf8_mode = uFalse;
	    } else if (!strcmp(xtermEnvEncoding(), "ISO-8859-15")) {
		if (request->screen.utf8_mode == uDefault)
		    request->screen.utf8_mode = uFalse;
		request->screen.latin9_mode = 1;
	    } else {
		request->misc.callfilter = is_utf8 ? 0 : 1;
		request->screen.utf8_mode = uAlways;
	    }
#else
	    request->misc.callfilter = is_utf8 ? 0 : 1;
	    request->screen.utf8_mode = uAlways;
#endif
	} else {
	    /* other encoding, use False */
	    if (request->screen.utf8_mode == uDefault) {
		request->screen.utf8_mode = is_utf8 ? uAlways : uFalse;
	    }
	}
    } else
#endif /* OPT_MINI_LUIT */
	if (x_strcasecmp(request->misc.locale_str, "TRUE") == 0 ||
	    x_strcasecmp(request->misc.locale_str, "ON") == 0 ||
	    x_strcasecmp(request->misc.locale_str, "YES") == 0 ||
	    x_strcasecmp(request->misc.locale_str, "AUTO") == 0 ||
	    strcmp(request->misc.locale_str, "1") == 0) {
	/* when true ... fully obeying LC_CTYPE locale */
	request->misc.callfilter = is_utf8 ? 0 : 1;
	request->screen.utf8_mode = uAlways;
    } else if (x_strcasecmp(request->misc.locale_str, "FALSE") == 0 ||
	       x_strcasecmp(request->misc.locale_str, "OFF") == 0 ||
	       x_strcasecmp(request->misc.locale_str, "NO") == 0 ||
	       strcmp(request->misc.locale_str, "0") == 0) {
	/* when false ... original value of utf8_mode is effective */
	if (request->screen.utf8_mode == uDefault) {
	    request->screen.utf8_mode = is_utf8 ? uAlways : uFalse;
	}
    } else if (x_strcasecmp(request->misc.locale_str, "MEDIUM") == 0 ||
	       x_strcasecmp(request->misc.locale_str, "SEMIAUTO") == 0) {
	/* when medium ... obeying locale only for UTF-8 and Asian */
	if (is_utf8) {
	    request->screen.utf8_mode = uAlways;
	} else if (
#ifdef MB_CUR_MAX
		      MB_CUR_MAX > 1 ||
#else
		      !strncmp(xtermEnvLocale(), "ja", 2) ||
		      !strncmp(xtermEnvLocale(), "ko", 2) ||
		      !strncmp(xtermEnvLocale(), "zh", 2) ||
#endif
		      !strncmp(xtermEnvLocale(), "th", 2) ||
		      !strncmp(xtermEnvLocale(), "vi", 2)) {
	    request->misc.callfilter = 1;
	    request->screen.utf8_mode = uAlways;
	} else {
	    request->screen.utf8_mode = uFalse;
	}
    } else if (x_strcasecmp(request->misc.locale_str, "UTF-8") == 0 ||
	       x_strcasecmp(request->misc.locale_str, "UTF8") == 0) {
	/* when UTF-8 ... UTF-8 mode */
	request->screen.utf8_mode = uAlways;
    } else {
	/* other words are regarded as encoding name passed to luit */
	request->misc.callfilter = 1;
	request->screen.utf8_mode = uAlways;
	request->misc.use_encoding = 1;
    }
    TRACE(("... updated misc.callfilter = %s\n", BtoS(request->misc.callfilter)));
    TRACE(("... updated misc.use_encoding = %s\n", BtoS(request->misc.use_encoding)));
#else
    if (request->screen.utf8_mode == uDefault) {
	request->screen.utf8_mode = is_utf8 ? uAlways : uFalse;
    }
#endif /* OPT_LUIT_PROG */

    request->screen.utf8_inparse = (request->screen.utf8_mode != uFalse);

    TRACE(("... updated screen.utf8_mode = %d\n", request->screen.utf8_mode));
    TRACE(("...VTInitialize_locale done\n"));
}
#endif

/* ARGSUSED */
static void
VTInitialize(Widget wrequest,
	     Widget new_arg,
	     ArgList args GCC_UNUSED,
	     Cardinal *num_args GCC_UNUSED)
{
    XtermWidget request = (XtermWidget) wrequest;
    XtermWidget wnew = (XtermWidget) new_arg;
    Widget my_parent = SHELL_OF(wnew);
    int i;
    char *s;

#if OPT_ISO_COLORS
    Bool color_ok;
#endif

#if OPT_COLOR_RES2 && (MAXCOLORS > MIN_ANSI_COLORS)
    static XtResource fake_resources[] =
    {
#if OPT_256_COLORS
# include <256colres.h>
#elif OPT_88_COLORS
# include <88colres.h>
#endif
    };
#endif /* OPT_COLOR_RES2 */

    TRACE(("VTInitialize\n"));

    /* Zero out the entire "screen" component of "wnew" widget, then do
     * field-by-field assignment of "screen" fields that are named in the
     * resource list.
     */
    bzero((char *) &wnew->screen, sizeof(wnew->screen));

    /* dummy values so that we don't try to Realize the parent shell with height
     * or width of 0, which is illegal in X.  The real size is computed in the
     * xtermWidget's Realize proc, but the shell's Realize proc is called first,
     * and must see a valid size.
     */
    wnew->core.height = wnew->core.width = 1;

    /*
     * The definition of -rv now is that it changes the definition of
     * XtDefaultForeground and XtDefaultBackground.  So, we no longer
     * need to do anything special.
     */
    wnew->screen.display = wnew->core.screen->display;

    /*
     * We use the default foreground/background colors to compare/check if a
     * color-resource has been set.
     */
#define MyBlackPixel(dpy) BlackPixel(dpy,DefaultScreen(dpy))
#define MyWhitePixel(dpy) WhitePixel(dpy,DefaultScreen(dpy))

    if (request->misc.re_verse) {
	wnew->dft_foreground = MyWhitePixel(wnew->screen.display);
	wnew->dft_background = MyBlackPixel(wnew->screen.display);
    } else {
	wnew->dft_foreground = MyBlackPixel(wnew->screen.display);
	wnew->dft_background = MyWhitePixel(wnew->screen.display);
    }
    init_Tres(TEXT_FG);
    init_Tres(TEXT_BG);

    TRACE(("Color resource initialization:\n"));
    TRACE(("   Default foreground %#lx\n", wnew->dft_foreground));
    TRACE(("   Default background %#lx\n", wnew->dft_background));
    TRACE(("   Screen foreground  %#lx\n", T_COLOR(&(wnew->screen), TEXT_FG)));
    TRACE(("   Screen background  %#lx\n", T_COLOR(&(wnew->screen), TEXT_BG)));

    wnew->screen.mouse_button = -1;
    wnew->screen.mouse_row = -1;
    wnew->screen.mouse_col = -1;

#if OPT_BOX_CHARS
    init_Bres(screen.force_box_chars);
    init_Bres(screen.force_all_chars);
#endif
    init_Bres(screen.free_bold_box);

    init_Bres(screen.c132);
    init_Bres(screen.curses);
    init_Bres(screen.hp_ll_bc);
#if OPT_XMC_GLITCH
    init_Ires(screen.xmc_glitch);
    init_Ires(screen.xmc_attributes);
    init_Bres(screen.xmc_inline);
    init_Bres(screen.move_sgr_ok);
#endif
#if OPT_BLINK_CURS
    init_Bres(screen.cursor_blink);
    init_Ires(screen.blink_on);
    init_Ires(screen.blink_off);
    wnew->screen.cursor_blink_res = wnew->screen.cursor_blink;
#endif
#if OPT_BLINK_TEXT
    init_Ires(screen.blink_as_bold);
#endif
    init_Ires(screen.border);
    init_Bres(screen.jumpscroll);
    init_Bres(screen.old_fkeys);
    init_Bres(screen.delete_is_del);
    wnew->keyboard.type = wnew->screen.old_fkeys
	? keyboardIsLegacy
	: keyboardIsDefault;
#ifdef ALLOWLOGGING
    init_Sres(screen.logfile);
#endif
    init_Bres(screen.bellOnReset);
    init_Bres(screen.marginbell);
    init_Bres(screen.multiscroll);
    init_Ires(screen.nmarginbell);
    init_Ires(screen.savelines);
    init_Ires(screen.scrollBarBorder);
    init_Ires(screen.scrolllines);
    init_Bres(screen.scrollttyoutput);
    init_Bres(screen.scrollkey);

    init_Sres(screen.term_id);
    for (s = request->screen.term_id; *s; s++) {
	if (!isalpha(CharOf(*s)))
	    break;
    }
    wnew->screen.terminal_id = atoi(s);
    if (wnew->screen.terminal_id < MIN_DECID)
	wnew->screen.terminal_id = MIN_DECID;
    if (wnew->screen.terminal_id > MAX_DECID)
	wnew->screen.terminal_id = MAX_DECID;
    TRACE(("term_id '%s' -> terminal_id %d\n",
	   wnew->screen.term_id,
	   wnew->screen.terminal_id));

    wnew->screen.vtXX_level = (wnew->screen.terminal_id / 100);
    init_Bres(screen.visualbell);
    init_Ires(screen.visualBellDelay);
    init_Bres(screen.poponbell);
    init_Ires(misc.limit_resize);
#if OPT_NUM_LOCK
    init_Bres(misc.real_NumLock);
    init_Bres(misc.alwaysUseMods);
    wnew->misc.num_lock = 0;
    wnew->misc.alt_left = 0;
    wnew->misc.alt_right = 0;
    wnew->misc.meta_trans = False;
    wnew->misc.meta_left = 0;
    wnew->misc.meta_right = 0;
#endif
#if OPT_SHIFT_FONTS
    init_Bres(misc.shift_fonts);
#endif
#if OPT_SUNPC_KBD
    init_Ires(misc.ctrl_fkeys);
#endif
#if OPT_TEK4014
    init_Bres(misc.tekInhibit);
    init_Bres(misc.tekSmall);
    init_Bres(screen.TekEmu);
#endif
#if OPT_TCAP_QUERY
    wnew->screen.tc_query = -1;
#endif
    wnew->misc.re_verse0 = request->misc.re_verse;
    init_Bres(misc.re_verse);
    init_Ires(screen.multiClickTime);
    init_Ires(screen.bellSuppressTime);
    init_Sres(screen.charClass);
    init_Bres(screen.cutNewline);
    init_Bres(screen.cutToBeginningOfLine);
    init_Bres(screen.highlight_selection);
    init_Bres(screen.trim_selection);
    init_Bres(screen.i18nSelections);
    init_Bres(screen.brokenSelections);
    init_Bres(screen.always_highlight);
    wnew->screen.pointer_cursor = request->screen.pointer_cursor;

    init_Sres(screen.answer_back);

    init_Sres(screen.printer_command);
    init_Bres(screen.printer_autoclose);
    init_Bres(screen.printer_extent);
    init_Bres(screen.printer_formfeed);
    init_Ires(screen.printer_controlmode);
#if OPT_PRINT_COLORS
    init_Ires(screen.print_attributes);
#endif

    init_Sres(screen.keyboard_dialect);

    init_Bres(screen.input_eight_bits);
    init_Bres(screen.output_eight_bits);
    init_Bres(screen.control_eight_bits);
    init_Bres(screen.backarrow_key);
    init_Bres(screen.meta_sends_esc);

    init_Bres(screen.allowSendEvent0);
    init_Bres(screen.allowWindowOp0);

    /* make a copy so that editres cannot change the resource after startup */
    wnew->screen.allowSendEvents = wnew->screen.allowSendEvent0;
    wnew->screen.allowWindowOps = wnew->screen.allowWindowOp0;

#ifndef NO_ACTIVE_ICON
    wnew->screen.fnt_icon = request->screen.fnt_icon;
    init_Bres(misc.active_icon);
    init_Ires(misc.icon_border_width);
    wnew->misc.icon_border_pixel = request->misc.icon_border_pixel;
#endif /* NO_ACTIVE_ICON */
    init_Bres(misc.titeInhibit);
    init_Bres(misc.tiXtraScroll);
    init_Bres(misc.dynamicColors);
    for (i = fontMenu_font1; i <= fontMenu_lastBuiltin; i++) {
	init_Sres(screen.MenuFontName(i));
    }
    /* set default in realize proc */
    wnew->screen.MenuFontName(fontMenu_fontdefault) = NULL;
    wnew->screen.MenuFontName(fontMenu_fontescape) = NULL;
    wnew->screen.MenuFontName(fontMenu_fontsel) = NULL;
    wnew->screen.menu_font_number = fontMenu_fontdefault;

#if OPT_BROKEN_OSC
    init_Bres(screen.brokenLinuxOSC);
#endif

#if OPT_BROKEN_ST
    init_Bres(screen.brokenStringTerm);
#endif

#if OPT_C1_PRINT
    init_Bres(screen.c1_printable);
#endif

#if OPT_DEC_CHRSET
    init_Bres(screen.font_doublesize);
    init_Ires(screen.cache_doublesize);
    if (wnew->screen.cache_doublesize > NUM_CHRSET)
	wnew->screen.cache_doublesize = NUM_CHRSET;
    if (wnew->screen.cache_doublesize == 0)
	wnew->screen.font_doublesize = False;
    TRACE(("Doublesize%s enabled, up to %d fonts\n",
	   wnew->screen.font_doublesize ? "" : " not",
	   wnew->screen.cache_doublesize));
#endif

#if OPT_ISO_COLORS || OPT_DEC_CHRSET || OPT_WIDE_CHARS
    wnew->num_ptrs = (OFF_ATTRS + 1);	/* OFF_FLAGS, OFF_CHARS, OFF_ATTRS */
#endif
#if OPT_ISO_COLORS
    init_Ires(screen.veryBoldColors);
    init_Bres(screen.boldColors);
    init_Bres(screen.colorAttrMode);
    init_Bres(screen.colorBDMode);
    init_Bres(screen.colorBLMode);
    init_Bres(screen.colorMode);
    init_Bres(screen.colorULMode);
    init_Bres(screen.italicULMode);
    init_Bres(screen.colorRVMode);

    for (i = 0, color_ok = False; i < MAXCOLORS; i++) {

#if OPT_COLOR_RES2 && (MAXCOLORS > MIN_ANSI_COLORS)
	/*
	 * Xt has a hardcoded limit on the maximum number of resources that can
	 * be used in a widget.  If we configure both luit (which implies
	 * wide-characters) and 256-colors, it goes over that limit.  Most
	 * people would not need a resource-file with 256-colors; the default
	 * values in our table are sufficient.  In that case, fake the resource
	 * setting by copying the default value from the table.  The #define's
	 * can be overridden to make these true resources.
	 */
	if (i >= MIN_ANSI_COLORS && i < NUM_ANSI_COLORS) {
	    wnew->screen.Acolors[i].resource
		= ((char *) fake_resources[i - MIN_ANSI_COLORS].default_addr);
	    if (wnew->screen.Acolors[i].resource == 0)
		wnew->screen.Acolors[i].resource = XtDefaultForeground;
	} else
#endif /* OPT_COLOR_RES2 */
	    wnew->screen.Acolors[i] = request->screen.Acolors[i];

#if OPT_COLOR_RES
	TRACE(("Acolors[%d] = %s\n", i, wnew->screen.Acolors[i].resource));
	wnew->screen.Acolors[i].mode = False;
	if (!x_strcasecmp(wnew->screen.Acolors[i].resource, XtDefaultForeground)) {
	    wnew->screen.Acolors[i].value = T_COLOR(&(wnew->screen), TEXT_FG);
	    wnew->screen.Acolors[i].mode = True;
	} else if (!x_strcasecmp(wnew->screen.Acolors[i].resource, XtDefaultBackground)) {
	    wnew->screen.Acolors[i].value = T_COLOR(&(wnew->screen), TEXT_BG);
	    wnew->screen.Acolors[i].mode = True;
	} else {
	    color_ok = True;
	}
#else
	TRACE(("Acolors[%d] = %#lx\n", i, request->screen.Acolors[i]));
	if (wnew->screen.Acolors[i] != wnew->dft_foreground &&
	    wnew->screen.Acolors[i] != T_COLOR(&(wnew->screen), TEXT_FG) &&
	    wnew->screen.Acolors[i] != T_COLOR(&(wnew->screen), TEXT_BG))
	    color_ok = True;
#endif
    }

    /*
     * Check if we're trying to use color in a monochrome screen.  Disable
     * color in that case, since that would make ANSI colors unusable.  A 4-bit
     * or 8-bit display is usable, so we do not have to check for anything more
     * specific.
     */
    if (color_ok) {
	Display *display = wnew->screen.display;
	XVisualInfo myTemplate, *visInfoPtr;
	int numFound;

	myTemplate.visualid = XVisualIDFromVisual(DefaultVisual(display,
								XDefaultScreen(display)));
	visInfoPtr = XGetVisualInfo(display, (long) VisualIDMask,
				    &myTemplate, &numFound);
	if (visInfoPtr == 0
	    || numFound == 0
	    || visInfoPtr->depth <= 1) {
	    TRACE(("disabling color since screen is monochrome\n"));
	    color_ok = False;
	} else {
	    XFree(visInfoPtr);
	}
    }

    /* If none of the colors are anything other than the foreground or
     * background, we'll assume this isn't color, no matter what the colorMode
     * resource says.  (There doesn't seem to be any good way to determine if
     * the resource lookup failed versus the user having misconfigured this).
     */
    if (!color_ok) {
	wnew->screen.colorMode = False;
	TRACE(("All colors are foreground or background: disable colorMode\n"));
    }
#if OPT_EXT_COLORS
    wnew->num_ptrs = (OFF_BGRND + 1);
#else
    wnew->num_ptrs = (OFF_COLOR + 1);
#endif
    wnew->sgr_foreground = -1;
    wnew->sgr_background = -1;
    wnew->sgr_extended = 0;
#endif /* OPT_ISO_COLORS */

    init_Tres(MOUSE_FG);
    init_Tres(MOUSE_BG);
    init_Tres(TEXT_CURSOR);
#if OPT_HIGHLIGHT_COLOR
    init_Tres(HIGHLIGHT_BG);
#endif

#if OPT_TEK4014
    /*
     * The Tek4014 window has no separate resources for foreground, background
     * and cursor color.  Since xterm always creates the vt100 widget first, we
     * can set the Tektronix colors here.  That lets us use escape sequences to
     * set its dynamic colors and get consistent behavior whether or not the
     * window is displayed.
     */
    T_COLOR(&(wnew->screen), TEK_BG) = T_COLOR(&(wnew->screen), TEXT_BG);
    T_COLOR(&(wnew->screen), TEK_FG) = T_COLOR(&(wnew->screen), TEXT_FG);
    T_COLOR(&(wnew->screen), TEK_CURSOR) = T_COLOR(&(wnew->screen), TEXT_CURSOR);
#endif

#if OPT_DEC_CHRSET
    wnew->num_ptrs = (OFF_CSETS + 1);
#endif

#if OPT_WIDE_CHARS
    VTInitialize_locale(request);

#if OPT_LUIT_PROG
    init_Bres(misc.callfilter);
    init_Bres(misc.use_encoding);
    init_Sres(misc.locale_str);
    init_Sres(misc.localefilter);
#endif

#if OPT_RENDERFONT
    init_Dres(misc.face_size);
    init_Sres(misc.face_name);
    init_Sres(misc.face_wide_name);
    init_Bres(misc.render_font);
    /* minor tweak to make debug traces consistent: */
    if (wnew->misc.render_font) {
	if (wnew->misc.face_name == 0) {
	    wnew->misc.render_font = False;
	    TRACE(("reset render_font since there is no face_name\n"));
	}
    }
#endif

    init_Bres(screen.vt100_graphics);
    init_Ires(screen.utf8_mode);
    init_Bres(screen.wide_chars);
    init_Bres(misc.mk_width);
    init_Bres(misc.cjk_width);
    if (request->screen.utf8_mode) {
	TRACE(("setting wide_chars on\n"));
	wnew->screen.wide_chars = True;
    } else {
	TRACE(("setting utf8_mode to 0\n"));
	wnew->screen.utf8_mode = uFalse;
    }
    TRACE(("initialized UTF-8 mode to %d\n", wnew->screen.utf8_mode));

#if OPT_MINI_LUIT
    if (request->screen.latin9_mode) {
	wnew->screen.latin9_mode = True;
    }
    if (request->screen.unicode_font) {
	wnew->screen.unicode_font = True;
    }
    TRACE(("initialized Latin9 mode to %d\n", wnew->screen.latin9_mode));
    TRACE(("initialized unicode_font to %d\n", wnew->screen.unicode_font));
#endif

    if (wnew->screen.wide_chars != False)
	wnew->num_ptrs = (OFF_COM2H + 1);

    decode_wcwidth((wnew->misc.cjk_width ? 2 : 0)
		   + (wnew->misc.mk_width ? 1 : 0)
		   + 1);
#endif /* OPT_WIDE_CHARS */

    init_Bres(screen.bold_mode);
    init_Bres(screen.underline);

    wnew->cur_foreground = 0;
    wnew->cur_background = 0;

    wnew->keyboard.flags = MODE_SRM;
    if (wnew->screen.backarrow_key)
	wnew->keyboard.flags |= MODE_DECBKM;
    TRACE(("initialized DECBKM %s\n",
	   BtoS(wnew->keyboard.flags & MODE_DECBKM)));

    /* look for focus related events on the shell, because we need
     * to care about the shell's border being part of our focus.
     */
    XtAddEventHandler(my_parent, EnterWindowMask, False,
		      HandleEnterWindow, (Opaque) NULL);
    XtAddEventHandler(my_parent, LeaveWindowMask, False,
		      HandleLeaveWindow, (Opaque) NULL);
    XtAddEventHandler(my_parent, FocusChangeMask, False,
		      HandleFocusChange, (Opaque) NULL);
    XtAddEventHandler((Widget) wnew, 0L, True,
		      VTNonMaskableEvent, (Opaque) NULL);
    XtAddEventHandler((Widget) wnew, PropertyChangeMask, False,
		      HandleBellPropertyChange, (Opaque) NULL);

#if HANDLE_STRUCT_NOTIFY
#if OPT_TOOLBAR
    wnew->VT100_TB_INFO(menu_bar) = request->VT100_TB_INFO(menu_bar);
    init_Ires(VT100_TB_INFO(menu_height));
#else
    /* Flag icon name with "***"  on window output when iconified.
     * Put in a handler that will tell us when we get Map/Unmap events.
     */
    if (zIconBeep)
#endif
	XtAddEventHandler(my_parent, StructureNotifyMask, False,
			  HandleStructNotify, (Opaque) 0);
#endif /* HANDLE_STRUCT_NOTIFY */

    wnew->screen.bellInProgress = False;

    set_character_class(wnew->screen.charClass);

    /* create it, but don't realize it */
    ScrollBarOn(wnew, True, False);

    /* make sure that the resize gravity acceptable */
    if (wnew->misc.resizeGravity != NorthWestGravity &&
	wnew->misc.resizeGravity != SouthWestGravity) {
	Cardinal nparams = 1;

	XtAppWarningMsg(app_con, "rangeError", "resizeGravity", "XTermError",
			"unsupported resizeGravity resource value (%d)",
			(String *) & (wnew->misc.resizeGravity), &nparams);
	wnew->misc.resizeGravity = SouthWestGravity;
    }
#ifndef NO_ACTIVE_ICON
    wnew->screen.whichVwin = &wnew->screen.fullVwin;
#if OPT_TEK4014
    wnew->screen.whichTwin = &wnew->screen.fullTwin;
#endif
#endif /* NO_ACTIVE_ICON */

    if (wnew->screen.savelines < 0)
	wnew->screen.savelines = 0;

    init_Bres(screen.awaitInput);

    wnew->flags = 0;
    if (!wnew->screen.jumpscroll)
	wnew->flags |= SMOOTHSCROLL;
    if (wnew->misc.reverseWrap)
	wnew->flags |= REVERSEWRAP;
    if (wnew->misc.autoWrap)
	wnew->flags |= WRAPAROUND;
    if (wnew->misc.re_verse != wnew->misc.re_verse0)
	wnew->flags |= REVERSE_VIDEO;
    if (wnew->screen.c132)
	wnew->flags |= IN132COLUMNS;

    wnew->initflags = wnew->flags;

    init_Ires(keyboard.modify_cursor_keys);

    init_Ires(misc.appcursorDefault);
    if (wnew->misc.appcursorDefault)
	wnew->keyboard.flags |= MODE_DECCKM;

    init_Ires(misc.appkeypadDefault);
    if (wnew->misc.appkeypadDefault)
	wnew->keyboard.flags |= MODE_DECKPAM;

    return;
}

static void
VTDestroy(Widget w)
{
    XtFree((char *) (((XtermWidget) w)->screen.selection_data));
}

/*ARGSUSED*/
static void
VTRealize(Widget w,
	  XtValueMask * valuemask,
	  XSetWindowAttributes * values)
{
    XtermWidget xw = (XtermWidget) w;
    TScreen *screen = &xw->screen;

    unsigned width, height;
    int xpos, ypos, pr;
    XSizeHints sizehints;
    Atom pid_atom;

    TRACE(("VTRealize\n"));

    TabReset(xw->tabs);

    screen->MenuFontName(fontMenu_fontdefault) = xw->misc.default_font.f_n;
    screen->fnt_norm = NULL;
    screen->fnt_bold = NULL;
#if OPT_WIDE_CHARS
    screen->fnt_dwd = NULL;
    screen->fnt_dwdb = NULL;
#endif
    if (!xtermLoadFont(xw,
		       &(xw->misc.default_font),
		       False, 0)) {
	if (XmuCompareISOLatin1(xw->misc.default_font.f_n, "fixed") != 0) {
	    fprintf(stderr,
		    "%s:  unable to open font \"%s\", trying \"fixed\"....\n",
		    xterm_name, xw->misc.default_font.f_n);
	    (void) xtermLoadFont(xw,
				 xtermFontName("fixed"),
				 False, 0);
	    screen->MenuFontName(fontMenu_fontdefault) = "fixed";
	}
    }

    /* really screwed if we couldn't open default font */
    if (!screen->fnt_norm) {
	fprintf(stderr, "%s:  unable to locate a suitable font\n",
		xterm_name);
	Exit(1);
    }
#if OPT_WIDE_CHARS
    if (xw->screen.utf8_mode) {
	TRACE(("check if this is a wide font, if not try again\n"));
	if (xtermLoadWideFonts(xw, False))
	    SetVTFont(xw, screen->menu_font_number, TRUE, NULL);
    }
#endif

    /* making cursor */
    if (!screen->pointer_cursor) {
	screen->pointer_cursor =
	    make_colored_cursor(XC_xterm,
				T_COLOR(screen, MOUSE_FG),
				T_COLOR(screen, MOUSE_BG));
    } else {
	recolor_cursor(screen->pointer_cursor,
		       T_COLOR(screen, MOUSE_FG),
		       T_COLOR(screen, MOUSE_BG));
    }

    /* set defaults */
    xpos = 1;
    ypos = 1;
    width = 80;
    height = 24;

    TRACE(("parsing geo_metry %s\n", NonNull(xw->misc.geo_metry)));
    pr = XParseGeometry(xw->misc.geo_metry, &xpos, &ypos,
			&width, &height);
    TRACE(("... position %d,%d size %dx%d\n", ypos, xpos, height, width));

    set_max_col(screen, (int) (width - 1));	/* units in character cells */
    set_max_row(screen, (int) (height - 1));	/* units in character cells */
    xtermUpdateFontInfo(xw, False);

    width = screen->fullVwin.fullwidth;
    height = screen->fullVwin.fullheight;

    TRACE(("... border widget %d parent %d shell %d\n",
	   BorderWidth(xw),
	   BorderWidth(XtParent(xw)),
	   BorderWidth(SHELL_OF(xw))));

    if ((pr & XValue) && (XNegative & pr)) {
	xpos += DisplayWidth(screen->display, DefaultScreen(screen->display))
	    - width - (BorderWidth(XtParent(xw)) * 2);
    }
    if ((pr & YValue) && (YNegative & pr)) {
	ypos += DisplayHeight(screen->display, DefaultScreen(screen->display))
	    - height - (BorderWidth(XtParent(xw)) * 2);
    }

    /* set up size hints for window manager; min 1 char by 1 char */
    bzero(&sizehints, sizeof(sizehints));
    xtermSizeHints(xw, &sizehints, (xw->misc.scrollbar
				    ? (screen->scrollWidget->core.width
				       + BorderWidth(screen->scrollWidget))
				    : 0));

    sizehints.x = xpos;
    sizehints.y = ypos;
    if ((XValue & pr) || (YValue & pr)) {
	sizehints.flags |= USSize | USPosition;
	sizehints.flags |= PWinGravity;
	switch (pr & (XNegative | YNegative)) {
	case 0:
	    sizehints.win_gravity = NorthWestGravity;
	    break;
	case XNegative:
	    sizehints.win_gravity = NorthEastGravity;
	    break;
	case YNegative:
	    sizehints.win_gravity = SouthWestGravity;
	    break;
	default:
	    sizehints.win_gravity = SouthEastGravity;
	    break;
	}
    } else {
	/* set a default size, but do *not* set position */
	sizehints.flags |= PSize;
    }
    sizehints.height = sizehints.base_height
	+ sizehints.height_inc * MaxRows(screen);
    sizehints.width = sizehints.base_width
	+ sizehints.width_inc * MaxCols(screen);

    if ((WidthValue & pr) || (HeightValue & pr))
	sizehints.flags |= USSize;
    else
	sizehints.flags |= PSize;

    /*
     * Note that the size-hints are for the shell, while the resize-request
     * is for the vt100 widget.  They are not the same size.
     */
    TRACE(("make resize request %dx%d\n", height, width));
    (void) XtMakeResizeRequest((Widget) xw,
			       (Dimension) width, (Dimension) height,
			       &xw->core.width, &xw->core.height);
    TRACE(("...made resize request %dx%d\n", xw->core.height, xw->core.width));

    /* XXX This is bogus.  We are parsing geometries too late.  This
     * is information that the shell widget ought to have before we get
     * realized, so that it can do the right thing.
     */
    if (sizehints.flags & USPosition)
	XMoveWindow(XtDisplay(xw), XtWindow(SHELL_OF(xw)),
		    sizehints.x, sizehints.y);

    TRACE(("%s@%d -- ", __FILE__, __LINE__));
    TRACE_HINTS(&sizehints);
    XSetWMNormalHints(XtDisplay(xw), XtWindow(SHELL_OF(xw)), &sizehints);
    TRACE(("%s@%d -- ", __FILE__, __LINE__));
    TRACE_WM_HINTS(xw);

    if ((pid_atom = XInternAtom(XtDisplay(xw), "_NET_WM_PID", False)) != None) {
	/* XChangeProperty format 32 really is "long" */
	unsigned long pid_l = (unsigned long) getpid();
	TRACE(("Setting _NET_WM_PID property to %lu\n", pid_l));
	XChangeProperty(XtDisplay(xw), VShellWindow,
			pid_atom, XA_CARDINAL, 32, PropModeReplace,
			(unsigned char *) &pid_l, 1);
    }

    XFlush(XtDisplay(xw));	/* get it out to window manager */

    /* use ForgetGravity instead of SouthWestGravity because translating
       the Expose events for ConfigureNotifys is too hard */
    values->bit_gravity = ((xw->misc.resizeGravity == NorthWestGravity)
			   ? NorthWestGravity
			   : ForgetGravity);
    xw->screen.fullVwin.window = XtWindow(xw) =
	XCreateWindow(XtDisplay(xw), XtWindow(XtParent(xw)),
		      xw->core.x, xw->core.y,
		      xw->core.width, xw->core.height, BorderWidth(xw),
		      (int) xw->core.depth,
		      InputOutput, CopyFromParent,
		      *valuemask | CWBitGravity, values);
    screen->event_mask = values->event_mask;

#ifndef NO_ACTIVE_ICON
    if (xw->misc.active_icon && screen->fnt_icon) {
	int iconX = 0, iconY = 0;
	Widget shell = SHELL_OF(xw);
	unsigned long mask;
	XGCValues xgcv;

	TRACE(("Initializing active-icon\n"));
	XtVaGetValues(shell, XtNiconX, &iconX, XtNiconY, &iconY, (XtPointer) 0);
	xtermComputeFontInfo(xw, &(screen->iconVwin), screen->fnt_icon, 0);

	/* since only one client is permitted to select for Button
	 * events, we have to let the window manager get 'em...
	 */
	values->event_mask &= ~(ButtonPressMask | ButtonReleaseMask);
	values->border_pixel = xw->misc.icon_border_pixel;

	screen->iconVwin.window =
	    XCreateWindow(XtDisplay(xw),
			  RootWindowOfScreen(XtScreen(shell)),
			  iconX, iconY,
			  screen->iconVwin.fullwidth,
			  screen->iconVwin.fullheight,
			  xw->misc.icon_border_width,
			  (int) xw->core.depth,
			  InputOutput, CopyFromParent,
			  *valuemask | CWBitGravity | CWBorderPixel,
			  values);
	XtVaSetValues(shell,
		      XtNiconWindow, screen->iconVwin.window,
		      (XtPointer) 0);
	XtRegisterDrawable(XtDisplay(xw), screen->iconVwin.window, w);

	mask = (GCFont | GCForeground | GCBackground |
		GCGraphicsExposures | GCFunction);

	xgcv.font = screen->fnt_icon->fid;
	xgcv.foreground = T_COLOR(screen, TEXT_FG);
	xgcv.background = T_COLOR(screen, TEXT_BG);
	xgcv.graphics_exposures = True;		/* default */
	xgcv.function = GXcopy;

	screen->iconVwin.normalGC =
	    screen->iconVwin.normalboldGC =
	    XtGetGC(shell, mask, &xgcv);

	xgcv.foreground = T_COLOR(screen, TEXT_BG);
	xgcv.background = T_COLOR(screen, TEXT_FG);

	screen->iconVwin.reverseGC =
	    screen->iconVwin.reverseboldGC =
	    XtGetGC(shell, mask, &xgcv);
#if OPT_TOOLBAR
	/*
	 * Toolbar is initialized before we get here.  Enable the menu item
	 * and set it properly.
	 */
	set_sensitivity(mw,
			vtMenuEntries[vtMenu_activeicon].widget,
			True);
	update_activeicon();
#endif
    } else {
	TRACE(("Disabled active-icon\n"));
	xw->misc.active_icon = False;
    }
#endif /* NO_ACTIVE_ICON */

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
    VTInitI18N();
#else
    xw->screen.xic = NULL;
#endif
#if OPT_NUM_LOCK
    VTInitModifiers();
#endif

    set_cursor_gcs(screen);

    /* Reset variables used by ANSI emulation. */

    resetCharsets(screen);

    XDefineCursor(screen->display, VShellWindow, screen->pointer_cursor);

    set_cur_col(screen, 0);
    set_cur_row(screen, 0);
    set_max_col(screen, Width(screen) / screen->fullVwin.f_width - 1);
    set_max_row(screen, Height(screen) / screen->fullVwin.f_height - 1);
    set_tb_margins(screen, 0, screen->max_row);

    memset(screen->sc, 0, sizeof(screen->sc));

    /* Mark screen buffer as unallocated.  We wait until the run loop so
       that the child process does not fork and exec with all the dynamic
       memory it will never use.  If we were to do it here, the
       swap space for new process would be huge for huge savelines. */
#if OPT_TEK4014
    if (!tekWidget)		/* if not called after fork */
#endif
	screen->visbuf = screen->allbuf = NULL;

    screen->do_wrap = 0;
    screen->scrolls = screen->incopy = 0;
    xtermSetCursorBox(screen);

    screen->savedlines = 0;

    if (xw->misc.scrollbar) {
	screen->fullVwin.sb_info.width = 0;
	ScrollBarOn(xw, False, True);
    }
    CursorSave(xw);
    return;
}

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD

/* limit this feature to recent XFree86 since X11R6.x core dumps */
#if defined(XtSpecificationRelease) && XtSpecificationRelease >= 6 && defined(X_HAVE_UTF8_STRING)
#define USE_XIM_INSTANTIATE_CB

static void
xim_instantiate_cb(Display * display,
		   XPointer client_data GCC_UNUSED,
		   XPointer call_data GCC_UNUSED)
{
    if (display != XtDisplay(term))
	return;

    VTInitI18N();
}

static void
xim_destroy_cb(XIM im GCC_UNUSED,
	       XPointer client_data GCC_UNUSED,
	       XPointer call_data GCC_UNUSED)
{
    term->screen.xic = NULL;

    XRegisterIMInstantiateCallback(XtDisplay(term), NULL, NULL, NULL,
				   xim_instantiate_cb, NULL);
}
#endif /* X11R6+ */

static void
xim_real_init(void)
{
    unsigned i, j;
    char *p, *s, *t, *ns, *end, buf[32];
    XIMStyles *xim_styles;
    XIMStyle input_style = 0;
    Bool found;
    static struct {
	char *name;
	unsigned long code;
    } known_style[] = {
	{
	    "OverTheSpot", (XIMPreeditPosition | XIMStatusNothing)
	},
	{
	    "OffTheSpot", (XIMPreeditArea | XIMStatusArea)
	},
	{
	    "Root", (XIMPreeditNothing | XIMStatusNothing)
	},
    };

    term->screen.xic = NULL;

    if (term->misc.cannot_im) {
	return;
    }

    if (!term->misc.input_method || !*term->misc.input_method) {
	if ((p = XSetLocaleModifiers("")) != NULL && *p)
	    term->screen.xim = XOpenIM(XtDisplay(term), NULL, NULL, NULL);
    } else {
	s = term->misc.input_method;
	i = 5 + strlen(s);
	t = (char *) MyStackAlloc(i, buf);
	if (t == NULL)
	    SysError(ERROR_VINIT);

	for (ns = s; ns && *s;) {
	    while (*s && isspace(CharOf(*s)))
		s++;
	    if (!*s)
		break;
	    if ((ns = end = strchr(s, ',')) == 0)
		end = s + strlen(s);
	    while ((end != s) && isspace(CharOf(end[-1])))
		end--;

	    if (end != s) {
		strcpy(t, "@im=");
		strncat(t, s, (unsigned) (end - s));

		if ((p = XSetLocaleModifiers(t)) != 0 && *p
		    && (term->screen.xim = XOpenIM(XtDisplay(term),
						   NULL,
						   NULL,
						   NULL)) != 0)
		    break;

	    }
	    s = ns + 1;
	}
	MyStackFree(t, buf);
    }

    if (term->screen.xim == NULL
	&& (p = XSetLocaleModifiers("@im=none")) != NULL
	&& *p) {
	term->screen.xim = XOpenIM(XtDisplay(term), NULL, NULL, NULL);
    }

    if (!term->screen.xim) {
	fprintf(stderr, "Failed to open input method\n");
	return;
    }
    TRACE(("VTInitI18N opened input method\n"));

    if (XGetIMValues(term->screen.xim, XNQueryInputStyle, &xim_styles, NULL)
	|| !xim_styles
	|| !xim_styles->count_styles) {
	fprintf(stderr, "input method doesn't support any style\n");
	XCloseIM(term->screen.xim);
	term->misc.cannot_im = True;
	return;
    }

    found = False;
    for (s = term->misc.preedit_type; s && !found;) {
	while (*s && isspace(CharOf(*s)))
	    s++;
	if (!*s)
	    break;
	if ((ns = end = strchr(s, ',')) != 0)
	    ns++;
	else
	    end = s + strlen(s);
	while ((end != s) && isspace(CharOf(end[-1])))
	    end--;

	if (end != s) {		/* just in case we have a spurious comma */
	    TRACE(("looking for style '%.*s'\n", end - s, s));
	    for (i = 0; i < XtNumber(known_style); i++) {
		if ((int) strlen(known_style[i].name) == (end - s)
		    && !strncmp(s, known_style[i].name, (unsigned) (end - s))) {
		    input_style = known_style[i].code;
		    for (j = 0; j < xim_styles->count_styles; j++) {
			if (input_style == xim_styles->supported_styles[j]) {
			    found = True;
			    break;
			}
		    }
		    if (found)
			break;
		}
	    }
	}

	s = ns;
    }
    XFree(xim_styles);

    if (!found) {
	fprintf(stderr,
		"input method doesn't support my preedit type (%s)\n",
		term->misc.preedit_type);
	XCloseIM(term->screen.xim);
	term->misc.cannot_im = True;
	return;
    }

    /*
     * Check for styles we do not yet support.
     */
    TRACE(("input_style %#lx\n", input_style));
    if (input_style == (XIMPreeditArea | XIMStatusArea)) {
	fprintf(stderr,
		"This program doesn't support the 'OffTheSpot' preedit type\n");
	XCloseIM(term->screen.xim);
	term->misc.cannot_im = True;
	return;
    }

    /*
     * For XIMPreeditPosition (or OverTheSpot), XIM client has to
     * prepare a font.
     * The font has to be locale-dependent XFontSet, whereas
     * XTerm use Unicode font.  This leads a problem that the
     * same font cannot be used for XIM preedit.
     */
    if (input_style != (XIMPreeditNothing | XIMStatusNothing)) {
	char **missing_charset_list;
	int missing_charset_count;
	char *def_string;
	XVaNestedList p_list;
	XPoint spot =
	{0, 0};
	XFontStruct **fonts;
	char **font_name_list;

	term->screen.fs = XCreateFontSet(XtDisplay(term),
					 term->misc.f_x,
					 &missing_charset_list,
					 &missing_charset_count,
					 &def_string);
	if (term->screen.fs == NULL) {
	    fprintf(stderr, "Preparation of font set "
		    "\"%s\" for XIM failed.\n", term->misc.f_x);
	    term->screen.fs = XCreateFontSet(XtDisplay(term),
					     DEFXIMFONT,
					     &missing_charset_list,
					     &missing_charset_count,
					     &def_string);
	}
	if (term->screen.fs == NULL) {
	    fprintf(stderr, "Preparation of default font set "
		    "\"%s\" for XIM failed.\n", DEFXIMFONT);
	    XCloseIM(term->screen.xim);
	    term->misc.cannot_im = True;
	    return;
	}
	(void) XExtentsOfFontSet(term->screen.fs);
	j = XFontsOfFontSet(term->screen.fs, &fonts, &font_name_list);
	for (i = 0, term->screen.fs_ascent = 0; i < j; i++) {
	    if (term->screen.fs_ascent < (*fonts)->ascent)
		term->screen.fs_ascent = (*fonts)->ascent;
	}
	p_list = XVaCreateNestedList(0,
				     XNSpotLocation, &spot,
				     XNFontSet, term->screen.fs,
				     NULL);
	term->screen.xic = XCreateIC(term->screen.xim,
				     XNInputStyle, input_style,
				     XNClientWindow, XtWindow(term),
				     XNFocusWindow, XtWindow(term),
				     XNPreeditAttributes, p_list,
				     NULL);
    } else {
	term->screen.xic = XCreateIC(term->screen.xim, XNInputStyle, input_style,
				     XNClientWindow, XtWindow(term),
				     XNFocusWindow, XtWindow(term),
				     NULL);
    }

    if (!term->screen.xic) {
	fprintf(stderr, "Failed to create input context\n");
	XCloseIM(term->screen.xim);
    }
#if defined(USE_XIM_INSTANTIATE_CB)
    else {
	XIMCallback destroy_cb;

	destroy_cb.callback = xim_destroy_cb;
	destroy_cb.client_data = NULL;
	if (XSetIMValues(term->screen.xim, XNDestroyCallback, &destroy_cb, NULL))
	    fprintf(stderr, "Could not set destroy callback to IM\n");
    }
#endif

    return;
}

static void
VTInitI18N(void)
{
    if (term->misc.open_im) {
	xim_real_init();

#if defined(USE_XIM_INSTANTIATE_CB)
	if (term->screen.xic == NULL && !term->misc.cannot_im) {
	    sleep(3);
	    XRegisterIMInstantiateCallback(XtDisplay(term), NULL, NULL, NULL,
					   xim_instantiate_cb, NULL);
	}
#endif
    }
}
#endif /* OPT_I18N_SUPPORT && OPT_INPUT_METHOD */

static Boolean
VTSetValues(Widget cur,
	    Widget request GCC_UNUSED,
	    Widget wnew,
	    ArgList args GCC_UNUSED,
	    Cardinal *num_args GCC_UNUSED)
{
    XtermWidget curvt = (XtermWidget) cur;
    XtermWidget newvt = (XtermWidget) wnew;
    Bool refresh_needed = False;
    Bool fonts_redone = False;

    if ((T_COLOR(&(curvt->screen), TEXT_BG) !=
	 T_COLOR(&(newvt->screen), TEXT_BG)) ||
	(T_COLOR(&(curvt->screen), TEXT_FG) !=
	 T_COLOR(&(newvt->screen), TEXT_FG)) ||
	(curvt->screen.MenuFontName(curvt->screen.menu_font_number) !=
	 newvt->screen.MenuFontName(newvt->screen.menu_font_number)) ||
	(curvt->misc.default_font.f_n != newvt->misc.default_font.f_n)) {
	if (curvt->misc.default_font.f_n != newvt->misc.default_font.f_n)
	    newvt->screen.MenuFontName(fontMenu_fontdefault) = newvt->misc.default_font.f_n;
	if (xtermLoadFont(newvt,
			  xtermFontName(newvt->screen.MenuFontName(curvt->screen.menu_font_number)),
			  True, newvt->screen.menu_font_number)) {
	    /* resizing does the redisplay, so don't ask for it here */
	    refresh_needed = True;
	    fonts_redone = True;
	} else if (curvt->misc.default_font.f_n != newvt->misc.default_font.f_n)
	    newvt->screen.MenuFontName(fontMenu_fontdefault) = curvt->misc.default_font.f_n;
    }
    if (!fonts_redone
	&& (T_COLOR(&(curvt->screen), TEXT_CURSOR) !=
	    T_COLOR(&(newvt->screen), TEXT_CURSOR))) {
	set_cursor_gcs(&newvt->screen);
	refresh_needed = True;
    }
    if (curvt->misc.re_verse != newvt->misc.re_verse) {
	newvt->flags ^= REVERSE_VIDEO;
	ReverseVideo(newvt);
	newvt->misc.re_verse = !newvt->misc.re_verse;	/* ReverseVideo toggles */
	refresh_needed = True;
    }
    if ((T_COLOR(&(curvt->screen), MOUSE_FG) !=
	 T_COLOR(&(newvt->screen), MOUSE_FG)) ||
	(T_COLOR(&(curvt->screen), MOUSE_BG) !=
	 T_COLOR(&(newvt->screen), MOUSE_BG))) {
	recolor_cursor(newvt->screen.pointer_cursor,
		       T_COLOR(&(newvt->screen), MOUSE_FG),
		       T_COLOR(&(newvt->screen), MOUSE_BG));
	refresh_needed = True;
    }
    if (curvt->misc.scrollbar != newvt->misc.scrollbar) {
	ToggleScrollBar(newvt);
    }

    return refresh_needed;
}

#define setGC(value) set_at = __LINE__, currentGC = value

#define OutsideSelection(screen,row,col)  \
	 ((row) > (screen)->endHRow || \
	  ((row) == (screen)->endHRow && \
	   (col) >= (screen)->endHCol) || \
	  (row) < (screen)->startHRow || \
	  ((row) == (screen)->startHRow && \
	   (col) < (screen)->startHCol))

/*
 * Shows cursor at new cursor position in screen.
 */
void
ShowCursor(void)
{
    TScreen *screen = &term->screen;
    int x, y;
    Char clo;
    unsigned flags;
    unsigned fg_bg = 0;
    GC currentGC;
    int set_at;
    Bool in_selection;
    Bool reversed;
    Pixel fg_pix;
    Pixel bg_pix;
    Pixel tmp;
#if OPT_HIGHLIGHT_COLOR
    Pixel hi_pix = T_COLOR(screen, HIGHLIGHT_BG);
#endif
#if OPT_WIDE_CHARS
    Char chi = 0;
    Char c1h = 0;
    Char c1l = 0;
    Char c2h = 0;
    Char c2l = 0;
    int base;
#endif
    int cursor_col;

    if (screen->cursor_state == BLINKED_OFF)
	return;

    if (eventMode != NORMAL)
	return;

    if (screen->cur_row - screen->topline > screen->max_row)
	return;

    screen->cursor_row = screen->cur_row;
    cursor_col = screen->cursor_col = screen->cur_col;
    screen->cursor_moved = False;

#ifndef NO_ACTIVE_ICON
    if (IsIcon(screen)) {
	screen->cursor_state = ON;
	return;
    }
#endif /* NO_ACTIVE_ICON */

#if OPT_WIDE_CHARS
    base =
#endif
	clo = SCRN_BUF_CHARS(screen, screen->cursor_row)[cursor_col];

    if_OPT_WIDE_CHARS(screen, {
	int my_col;
	chi = SCRN_BUF_WIDEC(screen, screen->cursor_row)[cursor_col];
	if (clo == HIDDEN_LO && chi == HIDDEN_HI && cursor_col > 0) {
	    /* if cursor points to non-initial part of wide character,
	     * back it up
	     */
	    --cursor_col;
	    clo = SCRN_BUF_CHARS(screen, screen->cursor_row)[cursor_col];
	    chi = SCRN_BUF_WIDEC(screen, screen->cursor_row)[cursor_col];
	}
	my_col = cursor_col;
	base = (chi << 8) | clo;
	if (iswide(base))
	    my_col += 1;
	c1l = SCRN_BUF_COM1L(screen, screen->cursor_row)[my_col];
	c1h = SCRN_BUF_COM1H(screen, screen->cursor_row)[my_col];
	c2l = SCRN_BUF_COM2L(screen, screen->cursor_row)[my_col];
	c2h = SCRN_BUF_COM2H(screen, screen->cursor_row)[my_col];
    });

    flags = SCRN_BUF_ATTRS(screen, screen->cursor_row)[cursor_col];

    if (clo == 0
#if OPT_WIDE_CHARS
	&& chi == 0
#endif
	) {
	clo = ' ';
    }

    /*
     * If the cursor happens to be on blanks, and the foreground color is set
     * but not the background, do not treat it as a colored cell.
     */
#if OPT_ISO_COLORS
    if ((flags & TERM_COLOR_FLAGS(term)) == BG_COLOR
#if OPT_WIDE_CHARS
	&& chi == 0
#endif
	&& clo == ' ') {
	flags &= ~TERM_COLOR_FLAGS(term);
    }
#endif

    /*
     * Compare the current cell to the last set of colors used for the
     * cursor and update the GC's if needed.
     */
    if_OPT_EXT_COLORS(screen, {
	fg_bg = (SCRN_BUF_FGRND(screen, screen->cursor_row)[cursor_col] << 8)
	    | (SCRN_BUF_BGRND(screen, screen->cursor_row)[cursor_col]);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	fg_bg = SCRN_BUF_COLOR(screen, screen->cursor_row)[cursor_col];
    });
    fg_pix = getXtermForeground(flags, extract_fg(fg_bg, flags));
    bg_pix = getXtermBackground(flags, extract_bg(fg_bg, flags));

    if (OutsideSelection(screen, screen->cur_row, screen->cur_col))
	in_selection = False;
    else
	in_selection = True;

    reversed = ReverseOrHilite(screen, flags, in_selection);

    /* This is like updatedXtermGC(), except that we have to worry about
     * whether the window has focus, since in that case we want just an
     * outline for the cursor.
     */
    if (screen->select || screen->always_highlight) {
	if (reversed) {		/* text is reverse video */
	    if (screen->cursorGC) {
		setGC(screen->cursorGC);
	    } else {
		if (flags & BOLDATTR(screen)) {
		    setGC(NormalBoldGC(screen));
		} else {
		    setGC(NormalGC(screen));
		}
	    }
#if OPT_HIGHLIGHT_COLOR
	    if (hi_pix != T_COLOR(screen, TEXT_FG)
		&& hi_pix != fg_pix
		&& hi_pix != bg_pix
		&& hi_pix != term->dft_foreground) {
		bg_pix = fg_pix;
		fg_pix = hi_pix;
	    }
#endif
	    EXCHANGE(fg_pix, bg_pix, tmp);
	} else {		/* normal video */
	    if (screen->reversecursorGC) {
		setGC(screen->reversecursorGC);
	    } else {
		if (flags & BOLDATTR(screen)) {
		    setGC(ReverseBoldGC(screen));
		} else {
		    setGC(ReverseGC(screen));
		}
	    }
	}
	if (T_COLOR(screen, TEXT_CURSOR) == term->dft_foreground) {
	    XSetBackground(screen->display, currentGC, fg_pix);
	}
	XSetForeground(screen->display, currentGC, bg_pix);
    } else {			/* not selected */
	if (reversed) {		/* text is reverse video */
#if OPT_HIGHLIGHT_COLOR
	    if (hi_pix != T_COLOR(screen, TEXT_FG)
		&& hi_pix != fg_pix
		&& hi_pix != bg_pix
		&& hi_pix != term->dft_foreground) {
		bg_pix = fg_pix;
		fg_pix = hi_pix;
	    }
#endif
	    setGC(ReverseGC(screen));
	    XSetForeground(screen->display, currentGC, bg_pix);
	    XSetBackground(screen->display, currentGC, fg_pix);
	} else {		/* normal video */
	    setGC(NormalGC(screen));
	    XSetForeground(screen->display, currentGC, fg_pix);
	    XSetBackground(screen->display, currentGC, bg_pix);
	}
    }

    if (screen->cursor_busy == 0
	&& (screen->cursor_state != ON || screen->cursor_GC != set_at)) {

	screen->cursor_GC = set_at;
	TRACE(("ShowCursor calling drawXtermText cur(%d,%d)\n",
	       screen->cur_row, screen->cur_col));

	drawXtermText(screen, flags & DRAWX_MASK, currentGC,
		      x = CurCursorX(screen, screen->cur_row, cursor_col),
		      y = CursorY(screen, screen->cur_row),
		      curXtermChrSet(screen->cur_row),
		      PAIRED_CHARS(&clo, &chi), 1, 0);

#if OPT_WIDE_CHARS
	if (c1l || c1h) {
	    drawXtermText(screen, (flags & DRAWX_MASK) | NOBACKGROUND,
			  currentGC, x, y,
			  curXtermChrSet(screen->cur_row),
			  PAIRED_CHARS(&c1l, &c1h), 1, iswide(base));

	    if (c2l || c2h)
		drawXtermText(screen, (flags & DRAWX_MASK) | NOBACKGROUND,
			      currentGC, x, y,
			      curXtermChrSet(screen->cur_row),
			      PAIRED_CHARS(&c2l, &c2h), 1, iswide(base));
	}
#endif

	if (!screen->select && !screen->always_highlight) {
	    screen->box->x = x;
	    screen->box->y = y;
	    XDrawLines(screen->display, VWindow(screen),
		       screen->cursoroutlineGC ? screen->cursoroutlineGC
		       : currentGC,
		       screen->box, NBOX, CoordModePrevious);
	}
    }
    screen->cursor_state = ON;
}

/*
 * hide cursor at previous cursor position in screen.
 */
void
HideCursor(void)
{
    TScreen *screen = &term->screen;
    GC currentGC;
    unsigned flags;
    unsigned fg_bg = 0;
    int x, y;
    Char clo;
    Bool in_selection;
#if OPT_WIDE_CHARS
    Char chi = 0;
    Char c1h = 0;
    Char c1l = 0;
    Char c2h = 0;
    Char c2l = 0;
    int base;
#endif
    int cursor_col;

    if (screen->cursor_state == OFF)	/* FIXME */
	return;
    if (screen->cursor_row - screen->topline > screen->max_row)
	return;

    cursor_col = screen->cursor_col;

#ifndef NO_ACTIVE_ICON
    if (IsIcon(screen)) {
	screen->cursor_state = OFF;
	return;
    }
#endif /* NO_ACTIVE_ICON */

#if OPT_WIDE_CHARS
    base =
#endif
	clo = SCRN_BUF_CHARS(screen, screen->cursor_row)[cursor_col];
    flags = SCRN_BUF_ATTRS(screen, screen->cursor_row)[cursor_col];

    if_OPT_WIDE_CHARS(screen, {
	int my_col;
	chi = SCRN_BUF_WIDEC(screen, screen->cursor_row)[cursor_col];
	if (clo == HIDDEN_LO && chi == HIDDEN_HI) {
	    /* if cursor points to non-initial part of wide character,
	     * back it up
	     */
	    --cursor_col;
	    clo = SCRN_BUF_CHARS(screen, screen->cursor_row)[cursor_col];
	    chi = SCRN_BUF_WIDEC(screen, screen->cursor_row)[cursor_col];
	}
	my_col = cursor_col;
	base = (chi << 8) | clo;
	if (iswide(base))
	    my_col += 1;
	c1l = SCRN_BUF_COM1L(screen, screen->cursor_row)[my_col];
	c1h = SCRN_BUF_COM1H(screen, screen->cursor_row)[my_col];
	c2l = SCRN_BUF_COM2L(screen, screen->cursor_row)[my_col];
	c2h = SCRN_BUF_COM2H(screen, screen->cursor_row)[my_col];
    });

    if_OPT_EXT_COLORS(screen, {
	fg_bg = (SCRN_BUF_FGRND(screen, screen->cursor_row)[cursor_col] << 8)
	    | (SCRN_BUF_BGRND(screen, screen->cursor_row)[cursor_col]);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	fg_bg = SCRN_BUF_COLOR(screen, screen->cursor_row)[cursor_col];
    });

    if (OutsideSelection(screen, screen->cursor_row, screen->cursor_col))
	in_selection = False;
    else
	in_selection = True;

    currentGC = updatedXtermGC(screen, flags, fg_bg, in_selection);

    if (clo == 0
#if OPT_WIDE_CHARS
	&& chi == 0
#endif
	) {
	clo = ' ';
    }

    TRACE(("HideCursor calling drawXtermText cur(%d,%d)\n",
	   screen->cursor_row, screen->cursor_col));
    drawXtermText(screen, flags & DRAWX_MASK, currentGC,
		  x = CurCursorX(screen, screen->cursor_row, cursor_col),
		  y = CursorY(screen, screen->cursor_row),
		  curXtermChrSet(screen->cursor_row),
		  PAIRED_CHARS(&clo, &chi), 1, 0);

#if OPT_WIDE_CHARS
    if (c1l || c1h) {
	drawXtermText(screen, (flags & DRAWX_MASK) | NOBACKGROUND,
		      currentGC, x, y,
		      curXtermChrSet(screen->cur_row),
		      PAIRED_CHARS(&c1l, &c1h), 1, iswide(base));

	if (c2l || c2h)
	    drawXtermText(screen, (flags & DRAWX_MASK) | NOBACKGROUND,
			  currentGC, x, y,
			  curXtermChrSet(screen->cur_row),
			  PAIRED_CHARS(&c2l, &c2h), 1, iswide(base));
    }
#endif
    screen->cursor_state = OFF;
    resetXtermGC(screen, flags, in_selection);
}

#if OPT_BLINK_CURS || OPT_BLINK_TEXT
static void
StartBlinking(TScreen * screen)
{
    if (screen->blink_timer == 0) {
	unsigned long interval = (screen->cursor_state == ON ?
				  screen->blink_on : screen->blink_off);
	if (interval == 0)	/* wow! */
	    interval = 1;	/* let's humor him anyway */
	screen->blink_timer = XtAppAddTimeOut(app_con,
					      interval,
					      HandleBlinking,
					      screen);
    }
}

static void
StopBlinking(TScreen * screen)
{
    if (screen->blink_timer)
	XtRemoveTimeOut(screen->blink_timer);
    screen->blink_timer = 0;
}

#if OPT_BLINK_TEXT
static Bool
ScrnHasBlinking(TScreen * screen, int row)
{
    Char *attrs = SCRN_BUF_ATTRS(screen, row);
    int col;
    Bool result = False;

    for (col = 0; col < MaxCols(screen); ++col) {
	if (attrs[col] & BLINK) {
	    result = True;
	    break;
	}
    }
    return result;
}
#endif

/*
 * Blink the cursor by alternately showing/hiding cursor.  We leave the timer
 * running all the time (even though that's a little inefficient) to make the
 * logic simple.
 */
static void
HandleBlinking(XtPointer closure, XtIntervalId * id GCC_UNUSED)
{
    TScreen *screen = (TScreen *) closure;
    Bool resume = False;

    screen->blink_timer = 0;
    screen->blink_state = !screen->blink_state;

#if OPT_BLINK_CURS
    if (DoStartBlinking(screen)) {
	if (screen->cursor_state == ON) {
	    if (screen->select || screen->always_highlight) {
		HideCursor();
		if (screen->cursor_state == OFF)
		    screen->cursor_state = BLINKED_OFF;
	    }
	} else if (screen->cursor_state == BLINKED_OFF) {
	    screen->cursor_state = OFF;
	    ShowCursor();
	    if (screen->cursor_state == OFF)
		screen->cursor_state = BLINKED_OFF;
	}
	resume = True;
    }
#endif

#if OPT_BLINK_TEXT
    /*
     * Inspect the line on the current screen to see if any have the BLINK flag
     * associated with them.  Prune off any that have had the corresponding
     * cells reset.  If any are left, repaint those lines with ScrnRefresh().
     */
    if (!(screen->blink_as_bold)) {
	int row;
	int first_row = screen->max_row;
	int last_row = -1;

	for (row = screen->max_row; row >= 0; row--) {
	    if (ScrnTstBlinked(screen, row)) {
		if (ScrnHasBlinking(screen, row)) {
		    resume = True;
		    if (row > last_row)
			last_row = row;
		    if (row < first_row)
			first_row = row;
		} else {
		    ScrnClrBlinked(screen, row);
		}
	    }
	}
	/*
	 * FIXME: this could be a little more efficient, e.g,. by limiting the
	 * columns which are updated.
	 */
	if (first_row <= last_row) {
	    ScrnRefresh(screen,
			first_row,
			0,
			last_row + 1 - first_row,
			MaxCols(screen),
			True);
	}
    }
#endif

    /*
     * If either the cursor or text is blinking, restart the timer.
     */
    if (resume)
	StartBlinking(screen);
}
#endif /* OPT_BLINK_CURS || OPT_BLINK_TEXT */

/*
 * Implement soft or hard (full) reset of the VTxxx emulation.  There are a
 * couple of differences from real DEC VTxxx terminals (to avoid breaking
 * applications which have come to rely on xterm doing this):
 *
 *	+ autowrap mode should be reset (instead it's reset to the resource
 *	  default).
 *	+ the popup menu offers a choice of resetting the savedLines, or not.
 *	  (but the control sequence does this anyway).
 */
void
VTReset(Bool full, Bool saved)
{
    TScreen *screen = &term->screen;

    if (!XtIsRealized((Widget) term)) {
	Bell(XkbBI_MinorError, 0);
	return;
    }

    if (saved) {
	screen->savedlines = 0;
	ScrollBarDrawThumb(screen->scrollWidget);
    }

    /* make cursor visible */
    screen->cursor_set = ON;

    /* reset scrolling region */
    set_tb_margins(screen, 0, screen->max_row);

    bitclr(&term->flags, ORIGIN);

    if_OPT_ISO_COLORS(screen, {
	reset_SGR_Colors();
    });

    /* Reset character-sets to initial state */
    resetCharsets(screen);

    /* Reset DECSCA */
    bitclr(&term->flags, PROTECTED);
    screen->protected_mode = OFF_PROTECT;

    if (full) {			/* RIS */
	if (screen->bellOnReset)
	    Bell(XkbBI_TerminalBell, 0);

	/* reset the mouse mode */
	screen->send_mouse_pos = MOUSE_OFF;
	waitingForTrackInfo = False;
	eventMode = NORMAL;

	TabReset(term->tabs);
	term->keyboard.flags = MODE_SRM;
#if OPT_INITIAL_ERASE
	if (term->keyboard.reset_DECBKM == 1)
	    term->keyboard.flags |= MODE_DECBKM;
	else if (term->keyboard.reset_DECBKM == 2)
#endif
	    if (term->screen.backarrow_key)
		term->keyboard.flags |= MODE_DECBKM;
	TRACE(("full reset DECBKM %s\n",
	       BtoS(term->keyboard.flags & MODE_DECBKM)));
	update_appcursor();
	update_appkeypad();
	update_decbkm();
	show_8bit_control(False);
	reset_decudk();

	FromAlternate(screen);
	ClearScreen(screen);
	screen->cursor_state = OFF;
	if (term->flags & REVERSE_VIDEO)
	    ReverseVideo(term);

	term->flags = term->initflags;
	update_reversevideo();
	update_autowrap();
	update_reversewrap();
	update_autolinefeed();

	screen->jumpscroll = !(term->flags & SMOOTHSCROLL);
	update_jumpscroll();

	if (screen->c132 && (term->flags & IN132COLUMNS)) {
	    Dimension reqWidth = (80 * FontWidth(screen)
				  + 2 * screen->border + ScrollbarWidth(screen));
	    Dimension reqHeight = (FontHeight(screen)
				   * MaxRows(screen) + 2 * screen->border);
	    Dimension replyWidth;
	    Dimension replyHeight;

	    TRACE(("Making resize-request to restore 80-columns %dx%d\n",
		   reqHeight, reqWidth));
	    XtMakeResizeRequest((Widget) term,
				reqWidth,
				reqHeight,
				&replyWidth, &replyHeight);
	    TRACE(("...result %dx%d\n", replyHeight, replyWidth));
	    repairSizeHints();
	    XSync(screen->display, False);	/* synchronize */
	    if (XtAppPending(app_con))
		xevents();
	}

	CursorSet(screen, 0, 0, term->flags);
	CursorSave(term);
    } else {			/* DECSTR */
	/*
	 * There's a tiny difference, to accommodate usage of xterm.
	 * We reset autowrap to the resource values rather than turning
	 * it off.
	 */
	term->keyboard.flags &= ~(MODE_DECCKM | MODE_KAM | MODE_DECKPAM);
	bitcpy(&term->flags, term->initflags, WRAPAROUND | REVERSEWRAP);
	bitclr(&term->flags, INSERT | INVERSE | BOLD | BLINK | UNDERLINE | INVISIBLE);
	if_OPT_ISO_COLORS(screen, {
	    reset_SGR_Colors();
	});
	update_appcursor();
	update_autowrap();
	update_reversewrap();

	CursorSave(term);
	screen->sc[screen->alternate != False].row =
	    screen->sc[screen->alternate != False].col = 0;
    }
    longjmp(vtjmpbuf, 1);	/* force ground state in parser */
}

/*
 * set_character_class - takes a string of the form
 *
 *   low[-high]:val[,low[-high]:val[...]]
 *
 * and sets the indicated ranges to the indicated values.
 */
static int
set_character_class(char *s)
{
    int i;			/* iterator, index into s */
    int len;			/* length of s */
    int acc;			/* accumulator */
    int low, high;		/* bounds of range [0..127] */
    int base;			/* 8, 10, 16 (octal, decimal, hex) */
    int numbers;		/* count of numbers per range */
    int digits;			/* count of digits in a number */
    static char *errfmt = "%s:  %s in range string \"%s\" (position %d)\n";

    if (!s || !s[0])
	return -1;

    base = 10;			/* in case we ever add octal, hex */
    low = high = -1;		/* out of range */

    for (i = 0, len = strlen(s), acc = 0, numbers = digits = 0;
	 i < len; i++) {
	Char c = s[i];

	if (isspace(c)) {
	    continue;
	} else if (isdigit(c)) {
	    acc = acc * base + (c - '0');
	    digits++;
	    continue;
	} else if (c == '-') {
	    low = acc;
	    acc = 0;
	    if (digits == 0) {
		fprintf(stderr, errfmt, ProgramName, "missing number", s, i);
		return (-1);
	    }
	    digits = 0;
	    numbers++;
	    continue;
	} else if (c == ':') {
	    if (numbers == 0)
		low = acc;
	    else if (numbers == 1)
		high = acc;
	    else {
		fprintf(stderr, errfmt, ProgramName, "too many numbers",
			s, i);
		return (-1);
	    }
	    digits = 0;
	    numbers++;
	    acc = 0;
	    continue;
	} else if (c == ',') {
	    /*
	     * now, process it
	     */

	    if (high < 0) {
		high = low;
		numbers++;
	    }
	    if (numbers != 2) {
		fprintf(stderr, errfmt, ProgramName, "bad value number",
			s, i);
	    } else if (SetCharacterClassRange(low, high, acc) != 0) {
		fprintf(stderr, errfmt, ProgramName, "bad range", s, i);
	    }

	    low = high = -1;
	    acc = 0;
	    digits = 0;
	    numbers = 0;
	    continue;
	} else {
	    fprintf(stderr, errfmt, ProgramName, "bad character", s, i);
	    return (-1);
	}			/* end if else if ... else */

    }

    if (low < 0 && high < 0)
	return (0);

    /*
     * now, process it
     */

    if (high < 0)
	high = low;
    if (numbers < 1 || numbers > 2) {
	fprintf(stderr, errfmt, ProgramName, "bad value number", s, i);
    } else if (SetCharacterClassRange(low, high, acc) != 0) {
	fprintf(stderr, errfmt, ProgramName, "bad range", s, i);
    }

    return (0);
}

/* ARGSUSED */
static void
HandleKeymapChange(Widget w,
		   XEvent * event GCC_UNUSED,
		   String * params,
		   Cardinal *param_count)
{
    static XtTranslations keymap, original;
    static XtResource key_resources[] =
    {
	{XtNtranslations, XtCTranslations, XtRTranslationTable,
	 sizeof(XtTranslations), 0, XtRTranslationTable, (XtPointer) NULL}
    };
    char mapName[1000];
    char mapClass[1000];
    char *pmapName;
    char *pmapClass;
    size_t len;

    if (*param_count != 1)
	return;

    if (original == NULL)
	original = w->core.tm.translations;

    if (strcmp(params[0], "None") == 0) {
	XtOverrideTranslations(w, original);
	return;
    }

    len = strlen(params[0]) + 7;

    pmapName = (char *) MyStackAlloc(len, mapName);
    pmapClass = (char *) MyStackAlloc(len, mapClass);
    if (pmapName == NULL
	|| pmapClass == NULL)
	SysError(ERROR_KMMALLOC1);

    (void) sprintf(pmapName, "%sKeymap", params[0]);
    (void) strcpy(pmapClass, pmapName);
    if (islower(CharOf(pmapClass[0])))
	pmapClass[0] = toupper(CharOf(pmapClass[0]));
    XtGetSubresources(w, (XtPointer) &keymap, pmapName, pmapClass,
		      key_resources, (Cardinal) 1, NULL, (Cardinal) 0);
    if (keymap != NULL)
	XtOverrideTranslations(w, keymap);

    MyStackFree(pmapName, mapName);
    MyStackFree(pmapClass, mapClass);
}

/* ARGSUSED */
static void
HandleBell(Widget w GCC_UNUSED,
	   XEvent * event GCC_UNUSED,
	   String * params,	/* [0] = volume */
	   Cardinal *param_count)	/* 0 or 1 */
{
    int percent = (*param_count) ? atoi(params[0]) : 0;

    Bell(XkbBI_TerminalBell, percent);
}

/* ARGSUSED */
static void
HandleVisualBell(Widget w GCC_UNUSED,
		 XEvent * event GCC_UNUSED,
		 String * params GCC_UNUSED,
		 Cardinal *param_count GCC_UNUSED)
{
    VisualBell();
}

/* ARGSUSED */
static void
HandleIgnore(Widget w,
	     XEvent * event,
	     String * params GCC_UNUSED,
	     Cardinal *param_count GCC_UNUSED)
{
    /* do nothing, but check for funny escape sequences */
    (void) SendMousePosition(w, event);
}

/* ARGSUSED */
static void
DoSetSelectedFont(Widget w GCC_UNUSED,
		  XtPointer client_data GCC_UNUSED,
		  Atom * selection GCC_UNUSED,
		  Atom * type,
		  XtPointer value,
		  unsigned long *length GCC_UNUSED,
		  int *format)
{
    char *val = (char *) value;
    int len;
    if (*type != XA_STRING || *format != 8) {
	Bell(XkbBI_MinorError, 0);
	return;
    }
    len = strlen(val);
    if (len > 0) {
	if (val[len - 1] == '\n')
	    val[len - 1] = '\0';
	/* Do some sanity checking to avoid sending a long selection
	   back to the server in an OpenFont that is unlikely to succeed.
	   XLFD allows up to 255 characters and no control characters;
	   we are a little more liberal here. */
	if (len > 1000 || strchr(val, '\n'))
	    return;
	if (!xtermLoadFont(term,
			   xtermFontName(val),
			   True,
			   fontMenu_fontsel))
	    Bell(XkbBI_MinorError, 0);
    }
}

void
FindFontSelection(char *atom_name, Bool justprobe)
{
    static AtomPtr *atoms;
    static int atomCount = 0;
    AtomPtr *pAtom;
    int a;
    Atom target;

    if (!atom_name)
	atom_name = "PRIMARY";

    for (pAtom = atoms, a = atomCount; a; a--, pAtom++) {
	if (strcmp(atom_name, XmuNameOfAtom(*pAtom)) == 0)
	    break;
    }
    if (!a) {
	atoms = (AtomPtr *) XtRealloc((char *) atoms,
				      sizeof(AtomPtr) * (atomCount + 1));
	*(pAtom = &atoms[atomCount++]) = XmuMakeAtom(atom_name);
    }

    target = XmuInternAtom(XtDisplay(term), *pAtom);
    if (justprobe) {
	term->screen.MenuFontName(fontMenu_fontsel) =
	    XGetSelectionOwner(XtDisplay(term), target) ? _Font_Selected_ : 0;
    } else {
	XtGetSelectionValue((Widget) term, target, XA_STRING,
			    DoSetSelectedFont, NULL,
			    XtLastTimestampProcessed(XtDisplay(term)));
    }
    return;
}

void
set_cursor_gcs(TScreen * screen)
{
    XGCValues xgcv;
    XtGCMask mask;
    Pixel cc = T_COLOR(screen, TEXT_CURSOR);
    Pixel fg = T_COLOR(screen, TEXT_FG);
    Pixel bg = T_COLOR(screen, TEXT_BG);
    GC new_cursorGC = NULL;
    GC new_cursorFillGC = NULL;
    GC new_reversecursorGC = NULL;
    GC new_cursoroutlineGC = NULL;

    /*
     * Let's see, there are three things that have "color":
     *
     *     background
     *     text
     *     cursorblock
     *
     * And, there are four situation when drawing a cursor, if we decide
     * that we like have a solid block of cursor color with the letter
     * that it is highlighting shown in the background color to make it
     * stand out:
     *
     *     selected window, normal video - background on cursor
     *     selected window, reverse video - foreground on cursor
     *     unselected window, normal video - foreground on background
     *     unselected window, reverse video - background on foreground
     *
     * Since the last two are really just normalGC and reverseGC, we only
     * need two new GC's.  Under monochrome, we get the same effect as
     * above by setting cursor color to foreground.
     */

#if OPT_ISO_COLORS
    /*
     * If we're using ANSI colors, the functions manipulating the SGR code will
     * use the same GC's.  To avoid having the cursor change color, we use the
     * Xlib calls rather than the Xt calls.
     *
     * Use the colorMode value to determine which we'll do (the VWindow may
     * not be set before the widget's realized, so it's tested separately).
     */
    if (screen->colorMode) {
	if (VWindow(screen) != 0 && (cc != bg) && (cc != fg)) {
	    /* we might have a colored foreground/background later */
	    xgcv.font = screen->fnt_norm->fid;
	    mask = (GCForeground | GCBackground | GCFont);
	    xgcv.foreground = fg;
	    xgcv.background = cc;
	    new_cursorGC = XCreateGC(screen->display, VWindow(screen), mask, &xgcv);

	    xgcv.foreground = cc;
	    xgcv.background = fg;
	    new_cursorFillGC =
		XCreateGC(screen->display, VWindow(screen), mask, &xgcv);

	    if (screen->always_highlight) {
		new_reversecursorGC = (GC) 0;
		new_cursoroutlineGC = (GC) 0;
	    } else {
		xgcv.foreground = bg;
		xgcv.background = cc;
		new_reversecursorGC =
		    XCreateGC(screen->display, VWindow(screen), mask, &xgcv);
		xgcv.foreground = cc;
		xgcv.background = bg;
		new_cursoroutlineGC =
		    XCreateGC(screen->display, VWindow(screen), mask, &xgcv);
	    }
	}
    } else
#endif
    if (cc != fg && cc != bg) {
	/* we have a colored cursor */
	xgcv.font = screen->fnt_norm->fid;
	mask = (GCForeground | GCBackground | GCFont);

	xgcv.foreground = fg;
	xgcv.background = cc;
	new_cursorGC = XtGetGC((Widget) term, mask, &xgcv);

	xgcv.foreground = cc;
	xgcv.background = fg;
	new_cursorFillGC = XtGetGC((Widget) term, mask, &xgcv);

	if (screen->always_highlight) {
	    new_reversecursorGC = (GC) 0;
	    new_cursoroutlineGC = (GC) 0;
	} else {
	    xgcv.foreground = bg;
	    xgcv.background = cc;
	    new_reversecursorGC = XtGetGC((Widget) term, mask, &xgcv);
	    xgcv.foreground = cc;
	    xgcv.background = bg;
	    new_cursoroutlineGC = XtGetGC((Widget) term, mask, &xgcv);
	}
    }
#if OPT_ISO_COLORS
    if (screen->colorMode) {
	if (screen->cursorGC)
	    XFreeGC(screen->display, screen->cursorGC);
	if (screen->fillCursorGC)
	    XFreeGC(screen->display, screen->fillCursorGC);
	if (screen->reversecursorGC)
	    XFreeGC(screen->display, screen->reversecursorGC);
	if (screen->cursoroutlineGC)
	    XFreeGC(screen->display, screen->cursoroutlineGC);
    } else
#endif
    {
	if (screen->cursorGC)
	    XtReleaseGC((Widget) term, screen->cursorGC);
	if (screen->fillCursorGC)
	    XtReleaseGC((Widget) term, screen->fillCursorGC);
	if (screen->reversecursorGC)
	    XtReleaseGC((Widget) term, screen->reversecursorGC);
	if (screen->cursoroutlineGC)
	    XtReleaseGC((Widget) term, screen->cursoroutlineGC);
    }

    screen->cursorGC = new_cursorGC;
    screen->fillCursorGC = new_cursorFillGC;
    screen->reversecursorGC = new_reversecursorGC;
    screen->cursoroutlineGC = new_cursoroutlineGC;
}
