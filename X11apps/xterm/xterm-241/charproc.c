/* $XTermId: charproc.c,v 1.869 2009/01/26 20:50:58 Jeff.Chua Exp $ */

/*

Copyright 1999-2008,2009 by Thomas E. Dickey

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

static IChar doinput(void);
static int set_character_class(char *s);
static void FromAlternate(XtermWidget /* xw */ );
static void RequestResize(XtermWidget termw, int rows, int cols, Bool text);
static void SwitchBufs(XtermWidget xw);
static void ToAlternate(XtermWidget /* xw */ );
static void VTallocbuf(void);
static void ansi_modes(XtermWidget termw,
		       void (*func) (unsigned *p, unsigned mask));
static void bitclr(unsigned *p, unsigned mask);
static void bitcpy(unsigned *p, unsigned q, unsigned mask);
static void bitset(unsigned *p, unsigned mask);
static void dpmodes(XtermWidget termw, void (*func) (unsigned *p, unsigned mask));
static void restoremodes(XtermWidget termw);
static void savemodes(XtermWidget termw);
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
         Shift <KeyPress> Select:select-cursor-start() select-cursor-end(SELECT, CUT_BUFFER0) \n\
         Shift <KeyPress> Insert:insert-selection(SELECT, CUT_BUFFER0) \n\
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
            ~Ctrl ~Meta <Btn2Up>:insert-selection(SELECT, CUT_BUFFER0) \n\
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
                         <BtnUp>:select-end(SELECT, CUT_BUFFER0) \n\
                       <BtnDown>:ignore() \
";				/* PROCURA added "Meta <Btn2Down>:clear-saved-lines()" */
/* *INDENT-OFF* */
static XtActionsRec actionsList[] = {
    { "allow-send-events",	HandleAllowSends },
    { "bell",			HandleBell },
    { "clear-saved-lines",	HandleClearSavedLines },
    { "create-menu",		HandleCreateMenu },
    { "delete-is-del",		HandleDeleteIsDEL },
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
    { "set-bellIsUrgent",	HandleBellIsUrgent },
    { "set-cursesemul",		HandleCursesEmul },
    { "set-jumpscroll",		HandleJumpscroll },
    { "set-keep-selection",	HandleKeepSelection },
    { "set-marginbell",		HandleMarginBell },
    { "set-old-function-keys",	HandleOldFunctionKeys },
    { "set-pop-on-bell",	HandleSetPopOnBell },
    { "set-reverse-video",	HandleReverseVideo },
    { "set-reversewrap",	HandleReverseWrap },
    { "set-scroll-on-key",	HandleScrollKey },
    { "set-scroll-on-tty-output", HandleScrollTtyOutput },
    { "set-scrollbar",		HandleScrollbar },
    { "set-select",		HandleSetSelect },
    { "set-sun-keyboard",	HandleSunKeyboard },
    { "set-titeInhibit",	HandleTiteInhibit },
    { "set-visual-bell",	HandleSetVisualBell },
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
#if OPT_EXEC_XTERM
    { "spawn-new-terminal",	HandleSpawnTerminal },
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
#if OPT_SUN_FUNC_KEYS
    { "set-sun-function-keys",	HandleSunFunctionKeys },
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
    { "set-utf8-title",		HandleUTF8Title },
#endif
};
/* *INDENT-ON* */

static XtResource resources[] =
{
    Bres(XtNallowSendEvents, XtCAllowSendEvents, screen.allowSendEvent0, False),
    Bres(XtNallowFontOps, XtCAllowFontOps, screen.allowFontOp0, True),
    Bres(XtNallowTcapOps, XtCAllowTcapOps, screen.allowTcapOp0, False),
    Bres(XtNallowTitleOps, XtCAllowTitleOps, screen.allowTitleOp0, True),
    Bres(XtNallowWindowOps, XtCAllowWindowOps, screen.allowWindowOp0, True),
    Bres(XtNaltIsNotMeta, XtCAltIsNotMeta, screen.alt_is_not_meta, False),
    Bres(XtNaltSendsEscape, XtCAltSendsEscape, screen.alt_sends_esc, False),
    Bres(XtNalwaysBoldMode, XtCAlwaysBoldMode, screen.always_bold_mode, False),
    Bres(XtNalwaysHighlight, XtCAlwaysHighlight, screen.always_highlight, False),
    Bres(XtNappcursorDefault, XtCAppcursorDefault, misc.appcursorDefault, False),
    Bres(XtNappkeypadDefault, XtCAppkeypadDefault, misc.appkeypadDefault, False),
    Bres(XtNautoWrap, XtCAutoWrap, misc.autoWrap, True),
    Bres(XtNawaitInput, XtCAwaitInput, screen.awaitInput, False),
    Bres(XtNfreeBoldBox, XtCFreeBoldBox, screen.free_bold_box, False),
    Bres(XtNbackarrowKey, XtCBackarrowKey, screen.backarrow_key, True),
    Bres(XtNbellIsUrgent, XtCBellIsUrgent, screen.bellIsUrgent, False),
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
    Bres(XtNkeepSelection, XtCKeepSelection, screen.keepSelection, False),
    Bres(XtNloginShell, XtCLoginShell, misc.login_shell, False),
    Bres(XtNmarginBell, XtCMarginBell, screen.marginbell, False),
    Bres(XtNmetaSendsEscape, XtCMetaSendsEscape, screen.meta_sends_esc, False),
    Bres(XtNmultiScroll, XtCMultiScroll, screen.multiscroll, False),
    Bres(XtNoldXtermFKeys, XtCOldXtermFKeys, screen.old_fkeys, False),
    Bres(XtNpopOnBell, XtCPopOnBell, screen.poponbell, False),
    Bres(XtNprinterAutoClose, XtCPrinterAutoClose, screen.printer_autoclose, False),
    Bres(XtNprinterExtent, XtCPrinterExtent, screen.printer_extent, False),
    Bres(XtNprinterFormFeed, XtCPrinterFormFeed, screen.printer_formfeed, False),
    Bres(XtNquietGrab, XtCQuietGrab, screen.quiet_grab, False),
    Bres(XtNreverseVideo, XtCReverseVideo, misc.re_verse, False),
    Bres(XtNreverseWrap, XtCReverseWrap, misc.reverseWrap, False),
    Bres(XtNscrollBar, XtCScrollBar, misc.scrollbar, False),
    Bres(XtNscrollKey, XtCScrollCond, screen.scrollkey, False),
    Bres(XtNscrollTtyOutput, XtCScrollCond, screen.scrollttyoutput, True),
    Bres(XtNselectToClipboard, XtCSelectToClipboard,
	 screen.selectToClipboard, False),
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
    Ires(XtNpointerMode, XtCPointerMode, screen.pointer_mode, DEF_POINTER_MODE),
    Ires(XtNprinterControlMode, XtCPrinterControlMode,
	 screen.printer_controlmode, 0),
    Ires(XtNvisualBellDelay, XtCVisualBellDelay, screen.visualBellDelay, 100),
    Ires(XtNsaveLines, XtCSaveLines, screen.savelines, SAVELINES),
    Ires(XtNscrollBarBorder, XtCScrollBarBorder, screen.scrollBarBorder, 1),
    Ires(XtNscrollLines, XtCScrollLines, screen.scrolllines, SCROLLLINES),

    Sres(XtNinitialFont, XtCInitialFont, screen.initial_font, NULL),
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
    Fres("iconFont", "IconFont", screen.fnt_icon.fs, XtDefaultFont),
    Cres("iconBorderColor", XtCBorderColor, misc.icon_border_pixel, XtDefaultBackground),
#endif				/* NO_ACTIVE_ICON */

#if OPT_BLINK_CURS
    Bres(XtNcursorBlink, XtCCursorBlink, screen.cursor_blink, False),
#endif
    Bres(XtNcursorUnderline, XtCCursorUnderline, screen.cursor_underline, False),

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

#if OPT_CLIP_BOLD
    Bres(XtNuseClipping, XtCUseClipping, screen.use_clipping, True),
#endif

#if OPT_DEC_CHRSET
    Bres(XtNfontDoublesize, XtCFontDoublesize, screen.font_doublesize, True),
    Ires(XtNcacheDoublesize, XtCCacheDoublesize, screen.cache_doublesize, NUM_CHRSET),
#endif

#if OPT_HIGHLIGHT_COLOR
    Tres(XtNhighlightColor, XtCHighlightColor, HIGHLIGHT_BG, XtDefaultForeground),
    Tres(XtNhighlightTextColor, XtCHighlightTextColor, HIGHLIGHT_FG, XtDefaultBackground),
    Bres(XtNhighlightReverse, XtCHighlightReverse, screen.hilite_reverse, True),
    Bres(XtNhighlightColorMode, XtCHighlightColorMode, screen.hilite_color, Maybe),
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

    CLICK_RES("2", screen.onClick[1], "word"),
    CLICK_RES("3", screen.onClick[2], "line"),
    CLICK_RES("4", screen.onClick[3], 0),
    CLICK_RES("5", screen.onClick[4], 0),

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
	 keyboard.modify_1st.cursor_keys, 2),
    Ires(XtNmodifyFunctionKeys, XtCModifyFunctionKeys,
	 keyboard.modify_1st.function_keys, 2),
    Ires(XtNmodifyKeypadKeys, XtCModifyKeypadKeys,
	 keyboard.modify_1st.keypad_keys, 0),
    Ires(XtNmodifyOtherKeys, XtCModifyOtherKeys,
	 keyboard.modify_1st.other_keys, 0),
    Ires(XtNmodifyStringKeys, XtCModifyStringKeys,
	 keyboard.modify_1st.string_keys, 0),
    Ires(XtNformatOtherKeys, XtCFormatOtherKeys,
	 keyboard.format_keys, 0),
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
    Bres(XtNtekStartup, XtCTekStartup, misc.TekEmu, False),
#endif

#if OPT_TOOLBAR
    Wres(XtNmenuBar, XtCMenuBar, VT100_TB_INFO(menu_bar), 0),
    Ires(XtNmenuHeight, XtCMenuHeight, VT100_TB_INFO(menu_height), 25),
#endif

#if OPT_WIDE_CHARS
    Bres(XtNcjkWidth, XtCCjkWidth, misc.cjk_width, False),
    Bres(XtNmkWidth, XtCMkWidth, misc.mk_width, False),
    Bres(XtNutf8Latin1, XtCUtf8Latin1, screen.utf8_latin1, False),
    Bres(XtNutf8Title, XtCUtf8Title, screen.utf8_title, False),
    Bres(XtNvt100Graphics, XtCVT100Graphics, screen.vt100_graphics, True),
    Bres(XtNwideChars, XtCWideChars, screen.wide_chars, False),
    Ires(XtNcombiningChars, XtCCombiningChars, screen.max_combining, 2),
    Ires(XtNmkSamplePass, XtCMkSamplePass, misc.mk_samplepass, 256),
    Ires(XtNmkSampleSize, XtCMkSampleSize, misc.mk_samplesize, 1024),
    Ires(XtNutf8, XtCUtf8, screen.utf8_mode, uDefault),
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
#define RES_FACESIZE(n) Dres(XtNfaceSize #n, XtCFaceSize #n, misc.face_size[n], "0.0")
    RES_FACESIZE(1),
    RES_FACESIZE(2),
    RES_FACESIZE(3),
    RES_FACESIZE(4),
    RES_FACESIZE(5),
    RES_FACESIZE(6),
    Dres(XtNfaceSize, XtCFaceSize, misc.face_size[0], DEFFACESIZE),
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

    TRACE_TRANS("BEFORE", w);
    XtAppAddActions(app_con, input_actions, XtNumber(input_actions));
    XtAugmentTranslations(w, XtParseTranslationTable(defaultTranslations));
    TRACE_TRANS("AFTER:", w);

#if OPT_EXTRA_PASTE
    if (term && term->keyboard.extra_translations)
	XtOverrideTranslations((Widget) term, XtParseTranslationTable(term->keyboard.extra_translations));
#endif
}

#if OPT_ISO_COLORS
/*
 * The terminal's foreground and background colors are set via two mechanisms:
 *	text (cur_foreground, cur_background values that are passed down to
 *		XDrawImageString and XDrawString)
 *	area (X11 graphics context used in XClearArea and XFillRectangle)
 */
void
SGR_Foreground(XtermWidget xw, int color)
{
    TScreen *screen = &xw->screen;
    Pixel fg;

    if (color >= 0) {
	xw->flags |= FG_COLOR;
    } else {
	xw->flags &= ~FG_COLOR;
    }
    fg = getXtermForeground(xw, xw->flags, color);
    xw->cur_foreground = color;

    setCgsFore(xw, WhichVWin(screen), gcNorm, fg);
    setCgsBack(xw, WhichVWin(screen), gcNormReverse, fg);

    setCgsFore(xw, WhichVWin(screen), gcBold, fg);
    setCgsBack(xw, WhichVWin(screen), gcBoldReverse, fg);
}

void
SGR_Background(XtermWidget xw, int color)
{
    TScreen *screen = &xw->screen;
    Pixel bg;

    /*
     * An indexing operation may have set screen->scroll_amt, which would
     * normally result in calling FlushScroll() in WriteText().  However,
     * if we're changing the background color now, then the new value
     * should not apply to the pending blank lines.
     */
    if (screen->scroll_amt && (color != xw->cur_background))
	FlushScroll(xw);

    if (color >= 0) {
	xw->flags |= BG_COLOR;
    } else {
	xw->flags &= ~BG_COLOR;
    }
    bg = getXtermBackground(xw, xw->flags, color);
    xw->cur_background = color;

    setCgsBack(xw, WhichVWin(screen), gcNorm, bg);
    setCgsFore(xw, WhichVWin(screen), gcNormReverse, bg);

    setCgsBack(xw, WhichVWin(screen), gcBold, bg);
    setCgsFore(xw, WhichVWin(screen), gcBoldReverse, bg);
}

/* Invoked after updating bold/underline flags, computes the extended color
 * index to use for foreground.  (See also 'extract_fg()').
 */
static void
setExtendedFG(XtermWidget xw)
{
    int fg = xw->sgr_foreground;

    if (xw->screen.colorAttrMode
	|| (fg < 0)) {
	if (xw->screen.colorULMode && (xw->flags & UNDERLINE))
	    fg = COLOR_UL;
	if (xw->screen.colorBDMode && (xw->flags & BOLD))
	    fg = COLOR_BD;
	if (xw->screen.colorBLMode && (xw->flags & BLINK))
	    fg = COLOR_BL;
    }

    /* This implements the IBM PC-style convention of 8-colors, with one
     * bit for bold, thus mapping the 0-7 codes to 8-15.  It won't make
     * much sense for 16-color applications, but we keep it to retain
     * compatiblity with ANSI-color applications.
     */
#if OPT_PC_COLORS		/* XXXJTL should be settable at runtime (resource or OSC?) */
    if (xw->screen.boldColors
	&& (!xw->sgr_extended)
	&& (fg >= 0)
	&& (fg < 8)
	&& (xw->flags & BOLD))
	fg |= 8;
#endif

    SGR_Foreground(xw, fg);
}

/* Invoked after updating inverse flag, computes the extended color
 * index to use for background.  (See also 'extract_bg()').
 */
static void
setExtendedBG(XtermWidget xw)
{
    int bg = xw->sgr_background;

    if (xw->screen.colorAttrMode
	|| (bg < 0)) {
	if (xw->screen.colorRVMode && (xw->flags & INVERSE))
	    bg = COLOR_RV;
    }

    SGR_Background(xw, bg);
}

static void
reset_SGR_Foreground(XtermWidget xw)
{
    xw->sgr_foreground = -1;
    xw->sgr_extended = False;
    setExtendedFG(xw);
}

static void
reset_SGR_Background(XtermWidget xw)
{
    xw->sgr_background = -1;
    setExtendedBG(xw);
}

static void
reset_SGR_Colors(XtermWidget xw)
{
    reset_SGR_Foreground(xw);
    reset_SGR_Background(xw);
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

#if OPT_MOD_FKEYS
static void
set_mod_fkeys(XtermWidget xw, int which, int what, Bool enabled)
{
#define SET_MOD_FKEYS(field) \
    xw->keyboard.modify_now.field = ((what == DEFAULT) && enabled) \
				     ? xw->keyboard.modify_1st.field \
				     : what; \
    TRACE(("set modify_now.%s to %d\n", #field, \
	   xw->keyboard.modify_now.field));

    switch (which) {
    case 1:
	SET_MOD_FKEYS(cursor_keys);
	break;
    case 2:
	SET_MOD_FKEYS(function_keys);
	break;
    case 3:
	SET_MOD_FKEYS(keypad_keys);
	break;
    case 4:
	SET_MOD_FKEYS(other_keys);
	break;
    case 5:
	SET_MOD_FKEYS(string_keys);
	break;
    }
}
#endif /* OPT_MOD_FKEYS */

#if OPT_TRACE
#define WHICH_TABLE(name) if (table == name) result = #name
static char *
which_table(Const PARSE_T * table)
{
    char *result = "?";
    /* *INDENT-OFF* */
    WHICH_TABLE (ansi_table);
    else WHICH_TABLE (cigtable);
    else WHICH_TABLE (csi2_table);
    else WHICH_TABLE (csi_ex_table);
    else WHICH_TABLE (csi_quo_table);
    else WHICH_TABLE (csi_table);
    else WHICH_TABLE (dec2_table);
    else WHICH_TABLE (dec3_table);
    else WHICH_TABLE (dec_table);
    else WHICH_TABLE (eigtable);
    else WHICH_TABLE (esc_sp_table);
    else WHICH_TABLE (esc_table);
    else WHICH_TABLE (scrtable);
    else WHICH_TABLE (scs96table);
    else WHICH_TABLE (scstable);
    else WHICH_TABLE (sos_table);
#if OPT_DEC_LOCATOR
    else WHICH_TABLE (csi_tick_table);
#endif
#if OPT_DEC_RECTOPS
    else WHICH_TABLE (csi_dollar_table);
    else WHICH_TABLE (csi_star_table);
#endif
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
		dotext(xw,					\
		       screen->gsets[(int) (screen->curss)],	\
		       print_area, 1);				\
		screen->curss = 0;				\
		single++;					\
	    }							\
	    if (print_used > single) {				\
		dotext(xw,					\
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
    int scssize;
    Bool private_function;	/* distinguish private-mode from standard */
    int string_mode;		/* nonzero iff we're processing a string */
    int lastchar;		/* positive iff we had a graphic character */
    int nextstate;
#if OPT_WIDE_CHARS
    int last_was_wide;
#endif
};

static struct ParseState myState;

static void
init_groundtable(TScreen * screen, struct ParseState *sp)
{
#if OPT_VT52_MODE
    if (!(screen->vtXX_level)) {
	sp->groundtable = vt52_table;
    } else if (screen->terminal_id >= 100)
#endif
    {
	sp->groundtable = ansi_table;
    }
}

static void
select_charset(struct ParseState *sp, int type, int size)
{
    TRACE(("select_charset %#x %d\n", type, size));
    sp->scstype = type;
    sp->scssize = size;
    if (size == 94) {
	sp->parsestate = scstable;
    } else {
	sp->parsestate = scs96table;
    }
}

static Boolean
doparsing(XtermWidget xw, unsigned c, struct ParseState *sp)
{
    /* Buffer for processing printable text */
    static IChar *print_area;
    static size_t print_size, print_used;

    /* Buffer for processing strings (e.g., OSC ... ST) */
    static Char *string_area;
    static size_t string_size, string_used;

    TScreen *screen = &xw->screen;
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
	    && my_wcwidth((int) c) == 0
	    && !isWideControl(c)) {
	    int prev, precomposed;

	    WriteNow();

	    prev = (int) XTERM_CELL(screen->last_written_row,
				    screen->last_written_col);
	    precomposed = do_precomposition(prev, (int) c);
	    TRACE(("do_precomposition (U+%04X [%d], U+%04X [%d]) -> U+%04X [%d]\n",
		   prev, my_wcwidth(prev),
		   (int) c, my_wcwidth((int) c),
		   precomposed, my_wcwidth(precomposed)));

	    /* substitute combined character with precomposed character
	     * only if it does not change the width of the base character
	     */
	    if (precomposed != -1 && my_wcwidth(precomposed) == my_wcwidth(prev)) {
		putXtermCell(screen,
			     screen->last_written_row,
			     screen->last_written_col, precomposed);
	    } else {
		addXtermCombining(screen,
				  screen->last_written_row,
				  screen->last_written_col, c);
	    }

	    if (!screen->scroll_amt)
		ScrnUpdate(xw,
			   screen->last_written_row,
			   screen->last_written_col, 1, 1, 1);
	    continue;
	}
#endif

	/* Intercept characters for printer controller mode */
	if (screen->printer_controlmode == 2) {
	    if ((c = (unsigned) xtermPrinterControl((int) c)) == 0)
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
		param[nparam++] = (int) (c & 0x7f) - 32;
	    if (nparam < 2)
		continue;
	    sp->vt52_cup = False;
	    if ((row = param[0]) < 0)
		row = 0;
	    if ((col = param[1]) < 0)
		col = 0;
	    CursorSet(screen, row, col, xw->flags);
	    sp->parsestate = vt52_table;
	    param[0] = 0;
	    param[1] = 0;
	    continue;
	}
#endif

	laststate = sp->nextstate;
	if (c == ANSI_DEL
	    && sp->parsestate == sp->groundtable
	    && sp->scssize == 96
	    && sp->scstype != 0) {
	    /*
	     * Handle special case of shifts for 96-character sets by checking
	     * if we have a DEL.  The other special case for SPACE will always
	     * be printable.
	     */
	    sp->nextstate = CASE_PRINT;
	} else
#if OPT_WIDE_CHARS
	if (c > 255) {
	    /*
	     * The parsing tables all have 256 entries.  If we're supporting
	     * wide characters, we handle them by treating them the same as
	     * printing characters.
	     */
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
	    print_area[print_used++] = c;
	    sp->lastchar = thischar = (int) c;
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
	    string_area[string_used++] = CharOf(c);
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
		unparseputc(xw, screen->answer_back[count]);
	    unparse_end(xw);
	    break;

	case CASE_BELL:
	    TRACE(("CASE_BELL - bell\n"));
	    if (sp->string_mode == ANSI_OSC) {
		if (string_used)
		    string_area[--string_used] = '\0';
		do_osc(xw, string_area, string_used, (int) c);
		sp->parsestate = sp->groundtable;
	    } else {
		/* bell */
		Bell(XkbBI_TerminalBell, 0);
	    }
	    break;

	case CASE_BS:
	    TRACE(("CASE_BS - backspace\n"));
	    CursorBack(xw, 1);
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
	    xtermAutoPrint(c);
	    xtermIndex(xw, 1);
	    if (xw->flags & LINEFEED)
		CarriageReturn(screen);
	    else
		do_xevents();
	    break;

	case CASE_CBT:
	    /* cursor backward tabulation */
	    if ((count = param[0]) == DEFAULT)
		count = 1;
	    while ((count-- > 0)
		   && (TabToPrevStop(xw))) ;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CHT:
	    /* cursor forward tabulation */
	    if ((count = param[0]) == DEFAULT)
		count = 1;
	    while ((count-- > 0)
		   && (TabToNextStop(xw))) ;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_TAB:
	    /* tab */
	    TabToNextStop(xw);
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
	    xterm_DECDHL(xw, c == '3');
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSWL:
	    xterm_DECSWL(xw);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECDWL:
	    xterm_DECDWL(xw);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SCR_STATE:
	    /* enter scr state */
	    sp->parsestate = scrtable;
	    break;

	case CASE_SCS0_STATE:
	    /* enter scs state 0 */
	    select_charset(sp, 0, 94);
	    break;

	case CASE_SCS1_STATE:
	    /* enter scs state 1 */
	    select_charset(sp, 1, 94);
	    break;

	case CASE_SCS2_STATE:
	    /* enter scs state 2 */
	    select_charset(sp, 2, 94);
	    break;

	case CASE_SCS3_STATE:
	    /* enter scs state 3 */
	    select_charset(sp, 3, 94);
	    break;

	case CASE_SCS1A_STATE:
	    /* enter scs state 1 */
	    select_charset(sp, 1, 96);
	    break;

	case CASE_SCS2A_STATE:
	    /* enter scs state 2 */
	    select_charset(sp, 2, 96);
	    break;

	case CASE_SCS3A_STATE:
	    /* enter scs state 3 */
	    select_charset(sp, 3, 96);
	    break;

	case CASE_ESC_IGNORE:
	    /* unknown escape sequence */
	    sp->parsestate = eigtable;
	    break;

	case CASE_ESC_DIGIT:
	    /* digit in csi or dec mode */
	    if ((row = param[nparam - 1]) == DEFAULT)
		row = 0;
	    param[nparam - 1] = (10 * row) + ((int) c - '0');
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
	    InsertChar(xw, (unsigned) row);
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
	    CursorBack(xw, col);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CUP:
	    TRACE(("CASE_CUP - cursor position\n"));
	    if_OPT_XMC_GLITCH(screen, {
		Jump_XMC(xw);
	    });
	    if ((row = param[0]) < 1)
		row = 1;
	    if (nparam < 2 || (col = param[1]) < 1)
		col = 1;
	    CursorSet(screen, row - 1, col - 1, xw->flags);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_VPA:
	    TRACE(("CASE_VPA - vertical position\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    CursorSet(screen, row - 1, screen->cur_col, xw->flags);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_HPA:
	    TRACE(("CASE_HPA - horizontal position\n"));
	    if ((col = param[0]) < 1)
		col = 1;
	    CursorSet(screen, screen->cur_row, col - 1, xw->flags);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_HP_BUGGY_LL:
	    TRACE(("CASE_HP_BUGGY_LL\n"));
	    /* Some HP-UX applications have the bug that they
	       assume ESC F goes to the lower left corner of
	       the screen, regardless of what terminfo says. */
	    if (screen->hp_ll_bc)
		CursorSet(screen, screen->max_row, 0, xw->flags);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_ED:
	    TRACE(("CASE_ED - erase display\n"));
	    do_erase_display(xw, param[0], OFF_PROTECT);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_EL:
	    TRACE(("CASE_EL - erase line\n"));
	    do_erase_line(xw, param[0], OFF_PROTECT);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_ECH:
	    TRACE(("CASE_ECH - erase char\n"));
	    /* ECH */
	    ClearRight(xw, param[0] < 1 ? 1 : param[0]);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_IL:
	    TRACE(("CASE_IL - insert line\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    InsertLine(xw, row);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DL:
	    TRACE(("CASE_DL - delete line\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    DeleteLine(xw, row);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DCH:
	    TRACE(("CASE_DCH - delete char\n"));
	    if ((row = param[0]) < 1)
		row = 1;
	    DeleteChar(xw, (unsigned) row);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_TRACK_MOUSE:
	    /*
	     * A single parameter other than zero is always scroll-down.
	     * A zero-parameter is used to reset the mouse mode, and is
	     * not useful for scrolling anyway.
	     */
	    if (nparam > 1 || param[0] == 0) {
		CELL start;

		TRACE(("CASE_TRACK_MOUSE\n"));
		/* Track mouse as long as in window and between
		 * specified rows
		 */
		start.row = param[2] - 1;
		start.col = param[1] - 1;
		TrackMouse(xw,
			   param[0],
			   &start,
			   param[3] - 1, param[4] - 2);
	    } else {
		TRACE(("CASE_SD - scroll down\n"));
		/* SD */
		if ((count = param[0]) < 1)
		    count = 1;
		RevScroll(xw, count);
		do_xevents();
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECID:
	    TRACE(("CASE_DECID\n"));
	    if_OPT_VT52_MODE(screen, {
		unparseputc(xw, ANSI_ESC);
		unparseputc(xw, '/');
		unparseputc(xw, 'Z');
		unparse_end(xw);
		sp->parsestate = sp->groundtable;
		break;
	    });
	    param[0] = DEFAULT;	/* Default ID parameter */
	    /* FALLTHRU */
	case CASE_DA1:
	    TRACE(("CASE_DA1\n"));
	    if (param[0] <= 0) {	/* less than means DEFAULT */
		count = 0;
		reply.a_type = ANSI_CSI;
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
		    reply.a_param[count++] = (ParmType) (60
							 + screen->terminal_id
							 / 100);
		    reply.a_param[count++] = 1;		/* 132-columns */
		    reply.a_param[count++] = 2;		/* printer */
		    reply.a_param[count++] = 6;		/* selective-erase */
#if OPT_SUNPC_KBD
		    if (xw->keyboard.type == keyboardIsVT220)
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
		reply.a_nparam = (ParmType) count;
		reply.a_inters = 0;
		reply.a_final = 'c';
		unparseseq(xw, &reply);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DA2:
	    TRACE(("CASE_DA2\n"));
	    if (param[0] <= 0) {	/* less than means DEFAULT */
		count = 0;
		reply.a_type = ANSI_CSI;
		reply.a_pintro = '>';

		if (screen->terminal_id >= 200)
		    reply.a_param[count++] = 1;		/* VT220 */
		else
		    reply.a_param[count++] = 0;		/* VT100 (nonstandard) */
		reply.a_param[count++] = XTERM_PATCH;	/* Version */
		reply.a_param[count++] = 0;	/* options (none) */
		reply.a_nparam = (ParmType) count;
		reply.a_inters = 0;
		reply.a_final = 'c';
		unparseseq(xw, &reply);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECRPTUI:
	    TRACE(("CASE_DECRPTUI\n"));
	    if ((screen->terminal_id >= 400)
		&& (param[0] <= 0)) {	/* less than means DEFAULT */
		unparseputc1(xw, ANSI_DCS);
		unparseputc(xw, '!');
		unparseputc(xw, '|');
		unparseputc(xw, '0');
		unparseputc1(xw, ANSI_ST);
		unparse_end(xw);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_TBC:
	    TRACE(("CASE_TBC - tab clear\n"));
	    if ((row = param[0]) <= 0)	/* less than means default */
		TabClear(xw->tabs, screen->cur_col);
	    else if (row == 3)
		TabZonk(xw->tabs);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SET:
	    TRACE(("CASE_SET - set mode\n"));
	    ansi_modes(xw, bitset);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_RST:
	    TRACE(("CASE_RST - reset mode\n"));
	    ansi_modes(xw, bitclr);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SGR:
	    for (row = 0; row < nparam; ++row) {
		if_OPT_XMC_GLITCH(screen, {
		    Mark_XMC(xw, param[row]);
		});
		TRACE(("CASE_SGR %d\n", param[row]));
		switch (param[row]) {
		case DEFAULT:
		case 0:
		    xw->flags &=
			~(INVERSE | BOLD | BLINK | UNDERLINE | INVISIBLE);
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Colors(xw);
		    });
		    break;
		case 1:	/* Bold                 */
		    xw->flags |= BOLD;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 5:	/* Blink                */
		    xw->flags |= BLINK;
		    StartBlinking(screen);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 4:	/* Underscore           */
		    xw->flags |= UNDERLINE;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 7:
		    xw->flags |= INVERSE;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedBG(xw);
		    });
		    break;
		case 8:
		    xw->flags |= INVISIBLE;
		    break;
		case 22:	/* reset 'bold' */
		    xw->flags &= ~BOLD;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 24:
		    xw->flags &= ~UNDERLINE;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 25:	/* reset 'blink' */
		    xw->flags &= ~BLINK;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 27:
		    xw->flags &= ~INVERSE;
		    if_OPT_ISO_COLORS(screen, {
			setExtendedBG(xw);
		    });
		    break;
		case 28:
		    xw->flags &= ~INVISIBLE;
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
			xw->sgr_foreground = (param[row] - 30);
			xw->sgr_extended = False;
			setExtendedFG(xw);
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
				    xw->sgr_foreground = param[row];
				    xw->sgr_extended = True;
				    setExtendedFG(xw);
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
			reset_SGR_Foreground(xw);
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
			xw->sgr_background = (param[row] - 40);
			setExtendedBG(xw);
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
				    xw->sgr_background = param[row];
				    setExtendedBG(xw);
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
			reset_SGR_Background(xw);
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
			xw->sgr_foreground = (param[row] - 90 + 8);
			xw->sgr_extended = False;
			setExtendedFG(xw);
		    });
		    break;
		case 100:
#if !OPT_AIX_COLORS
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Foreground(xw);
			reset_SGR_Background(xw);
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
			xw->sgr_background = (param[row] - 100 + 8);
			setExtendedBG(xw);
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
	    reply.a_type = ANSI_CSI;
	    reply.a_pintro = CharOf(sp->private_function ? '?' : 0);
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
		reply.a_param[count++] = (ParmType) (screen->cur_row + 1);
		reply.a_param[count++] = (ParmType) (screen->cur_col + 1);
		reply.a_final = 'R';
		break;
	    case 15:
		/* printer status */
		if (screen->terminal_id >= 200) {	/* VT220 */
		    reply.a_param[count++] = 13;	/* implement printer */
		}
		break;
	    case 25:
		/* UDK status */
		if (screen->terminal_id >= 200) {	/* VT220 */
		    reply.a_param[count++] = 20;	/* UDK always unlocked */
		}
		break;
	    case 26:
		/* keyboard status */
		if (screen->terminal_id >= 200) {	/* VT220 */
		    reply.a_param[count++] = 27;
		    reply.a_param[count++] = 1;		/* North American */
		    if (screen->terminal_id >= 400) {
			reply.a_param[count++] = 0;	/* ready */
			reply.a_param[count++] = 0;	/* LK201 */
		    }
		}
		break;
	    case 53:
		/* Locator status */
		if (screen->terminal_id >= 200) {	/* VT220 */
#if OPT_DEC_LOCATOR
		    reply.a_param[count++] = 50;	/* locator ready */
#else
		    reply.a_param[count++] = 53;	/* no locator */
#endif
		}
		break;
	    default:
		break;
	    }

	    if ((reply.a_nparam = (ParmType) count) != 0)
		unparseseq(xw, &reply);

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
		FlushScroll(xw);
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
		    FlushScroll(xw);
		set_tb_margins(screen, top - 1, bot - 1);
		CursorSet(screen, 0, 0, xw->flags);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECREQTPARM:
	    TRACE(("CASE_DECREQTPARM\n"));
	    if (screen->terminal_id < 200) {	/* VT102 */
		if ((row = param[0]) == DEFAULT)
		    row = 0;
		if (row == 0 || row == 1) {
		    reply.a_type = ANSI_CSI;
		    reply.a_pintro = 0;
		    reply.a_nparam = 7;
		    reply.a_param[0] = (ParmType) (row + 2);
		    reply.a_param[1] = 1;	/* no parity */
		    reply.a_param[2] = 1;	/* eight bits */
		    reply.a_param[3] = 128;	/* transmit 38.4k baud */
		    reply.a_param[4] = 128;	/* receive 38.4k baud */
		    reply.a_param[5] = 1;	/* clock multiplier ? */
		    reply.a_param[6] = 0;	/* STP flags ? */
		    reply.a_inters = 0;
		    reply.a_final = 'x';
		    unparseseq(xw, &reply);
		}
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSET:
	    /* DECSET */
#if OPT_VT52_MODE
	    if (screen->vtXX_level != 0)
#endif
		dpmodes(xw, bitset);
	    sp->parsestate = sp->groundtable;
#if OPT_TEK4014
	    if (TEK4014_ACTIVE(xw))
		return False;
#endif
	    break;

	case CASE_DECRST:
	    /* DECRST */
	    dpmodes(xw, bitclr);
	    init_groundtable(screen, sp);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECALN:
	    TRACE(("CASE_DECALN - alignment test\n"));
	    if (screen->cursor_state)
		HideCursor();
	    set_tb_margins(screen, 0, screen->max_row);
	    CursorSet(screen, 0, 0, xw->flags);
	    xtermParseRect(xw, 0, 0, &myRect);
	    ScrnFillRectangle(xw, &myRect, 'E', 0, False);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_GSETS:
	    TRACE(("CASE_GSETS(%d) = '%c'\n", sp->scstype, c));
	    if (screen->vtXX_level != 0)
		screen->gsets[sp->scstype] = CharOf(c);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSC:
	    TRACE(("CASE_DECSC - save cursor\n"));
	    CursorSave(xw);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECRC:
	    TRACE(("CASE_DECRC - restore cursor\n"));
	    CursorRestore(xw);
	    if_OPT_ISO_COLORS(screen, {
		setExtendedFG(xw);
	    });
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECKPAM:
	    TRACE(("CASE_DECKPAM\n"));
	    xw->keyboard.flags |= MODE_DECKPAM;
	    update_appkeypad();
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECKPNM:
	    TRACE(("CASE_DECKPNM\n"));
	    xw->keyboard.flags &= ~MODE_DECKPAM;
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
		VTReset(xw, False, False);
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
		xw->flags &= ~PROTECTED;
	    else if (param[0] == 1)
		xw->flags |= PROTECTED;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSED:
	    TRACE(("CASE_DECSED\n"));
	    do_erase_display(xw, param[0], DEC_PROTECT);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSEL:
	    TRACE(("CASE_DECSEL\n"));
	    do_erase_line(xw, param[0], DEC_PROTECT);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_ST:
	    TRACE(("CASE_ST: End of String (%d bytes)\n", string_used));
	    sp->parsestate = sp->groundtable;
	    if (!string_used)
		break;
	    string_area[--string_used] = '\0';
	    switch (sp->string_mode) {
	    case ANSI_APC:
		/* ignored */
		break;
	    case ANSI_DCS:
		do_dcs(xw, string_area, string_used);
		break;
	    case ANSI_OSC:
		do_osc(xw, string_area, string_used, ANSI_ST);
		break;
	    case ANSI_PM:
		/* ignored */
		break;
	    case ANSI_SOS:
		/* ignored */
		break;
	    }
	    break;

	case CASE_SOS:
	    TRACE(("CASE_SOS: Start of String\n"));
	    sp->string_mode = ANSI_SOS;
	    sp->parsestate = sos_table;
	    break;

	case CASE_PM:
	    TRACE(("CASE_PM: Privacy Message\n"));
	    sp->string_mode = ANSI_PM;
	    sp->parsestate = sos_table;
	    break;

	case CASE_DCS:
	    TRACE(("CASE_DCS: Device Control String\n"));
	    sp->string_mode = ANSI_DCS;
	    sp->parsestate = sos_table;
	    break;

	case CASE_APC:
	    TRACE(("CASE_APC: Application Program Command\n"));
	    sp->string_mode = ANSI_APC;
	    sp->parsestate = sos_table;
	    break;

	case CASE_SPA:
	    TRACE(("CASE_SPA - start protected area\n"));
	    screen->protected_mode = ISO_PROTECT;
	    xw->flags |= PROTECTED;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_EPA:
	    TRACE(("CASE_EPA - end protected area\n"));
	    xw->flags &= ~PROTECTED;
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_SU:
	    TRACE(("CASE_SU - scroll up\n"));
	    if ((count = param[0]) < 1)
		count = 1;
	    xtermScroll(xw, count);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_IND:
	    TRACE(("CASE_IND - index\n"));
	    xtermIndex(xw, 1);
	    do_xevents();
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CPL:
	    TRACE(("CASE_CPL - cursor prev line\n"));
	    CursorPrevLine(screen, param[0]);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_CNL:
	    TRACE(("CASE_CNL - cursor next line\n"));
	    CursorNextLine(screen, param[0]);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_NEL:
	    TRACE(("CASE_NEL\n"));
	    xtermIndex(xw, 1);
	    CarriageReturn(screen);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_HTS:
	    TRACE(("CASE_HTS - horizontal tab set\n"));
	    TabSet(xw->tabs, screen->cur_col);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_RI:
	    TRACE(("CASE_RI - reverse index\n"));
	    RevIndex(xw, 1);
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
		MotionOff(screen, xw);
		if ((screen->loc_filter_top = param[0]) < 1)
		    screen->loc_filter_top = LOC_FILTER_POS;
		if (nparam < 2 || (screen->loc_filter_left = param[1]) < 1)
		    screen->loc_filter_left = LOC_FILTER_POS;
		if (nparam < 3 || (screen->loc_filter_bottom = param[2]) < 1)
		    screen->loc_filter_bottom = LOC_FILTER_POS;
		if (nparam < 4 || (screen->loc_filter_right = param[3]) < 1)
		    screen->loc_filter_right = LOC_FILTER_POS;
		InitLocatorFilter(xw);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECELR:
	    MotionOff(screen, xw);
	    if (param[0] <= 0 || param[0] > 2) {
		screen->send_mouse_pos = MOUSE_OFF;
		TRACE(("DECELR - Disable Locator Reports\n"));
	    } else {
		TRACE(("DECELR - Enable Locator Reports\n"));
		screen->send_mouse_pos = DEC_LOCATOR;
		xtermShowPointer(xw, True);
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
		    MotionOff(screen, xw);
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
		GetLocatorPosition(xw);
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
	    xtermParseRect(xw, nparam, param, &myRect);
	    ScrnCopyRectangle(xw, &myRect, nparam - 5, param + 5);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECERA:
	    TRACE(("CASE_DECERA - Erase rectangular area\n"));
	    xtermParseRect(xw, nparam, param, &myRect);
	    ScrnFillRectangle(xw, &myRect, ' ', 0, True);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECFRA:
	    TRACE(("CASE_DECFRA - Fill rectangular area\n"));
	    if (nparam > 0
		&& ((param[0] >= 32 && param[0] <= 126)
		    || (param[0] >= 160 && param[0] <= 255))) {
		xtermParseRect(xw, nparam - 1, param + 1, &myRect);
		ScrnFillRectangle(xw, &myRect, param[0], xw->flags, True);
	    }
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSERA:
	    TRACE(("CASE_DECSERA - Selective erase rectangular area\n"));
	    xtermParseRect(xw, nparam > 4 ? 4 : nparam, param, &myRect);
	    ScrnWipeRectangle(xw, &myRect);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSACE:
	    TRACE(("CASE_DECSACE - Select attribute change extent\n"));
	    screen->cur_decsace = param[0];
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECCARA:
	    TRACE(("CASE_DECCARA - Change attributes in rectangular area\n"));
	    xtermParseRect(xw, nparam > 4 ? 4 : nparam, param, &myRect);
	    ScrnMarkRectangle(xw, &myRect, False, nparam - 4, param + 4);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECRARA:
	    TRACE(("CASE_DECRARA - Reverse attributes in rectangular area\n"));
	    xtermParseRect(xw, nparam > 4 ? 4 : nparam, param, &myRect);
	    ScrnMarkRectangle(xw, &myRect, True, nparam - 4, param + 4);
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
	    sp->string_mode = ANSI_OSC;
	    break;

	case CASE_RIS:
	    TRACE(("CASE_RIS\n"));
	    VTReset(xw, True, True);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_DECSTR:
	    TRACE(("CASE_DECSTR\n"));
	    VTReset(xw, False, False);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_REP:
	    TRACE(("CASE_REP\n"));
	    if (sp->lastchar >= 0 &&
		sp->lastchar < 256 &&
		sp->groundtable[E2A(sp->lastchar)] == CASE_PRINT) {
		IChar repeated[2];
		count = (param[0] < 1) ? 1 : param[0];
		repeated[0] = (IChar) sp->lastchar;
		while (count-- > 0) {
		    dotext(xw,
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
	    savemodes(xw);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_XTERM_RESTORE:
	    restoremodes(xw);
	    sp->parsestate = sp->groundtable;
	    break;

	case CASE_XTERM_WINOPS:
	    TRACE(("CASE_XTERM_WINOPS\n"));
	    if (screen->allowWindowOps)
		window_ops(xw);
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
		ChangeToWide(xw);
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
#if OPT_MOD_FKEYS
	case CASE_SET_MOD_FKEYS:
	    TRACE(("CASE_SET_MOD_FKEYS\n"));
	    if (nparam >= 1) {
		set_mod_fkeys(xw, param[0], nparam > 1 ? param[1] : DEFAULT, True);
	    } else {
		for (row = 1; row <= 5; ++row)
		    set_mod_fkeys(xw, row, DEFAULT, True);
	    }
	    break;
	case CASE_SET_MOD_FKEYS0:
	    TRACE(("CASE_SET_MOD_FKEYS0\n"));
	    if (nparam >= 1 && param[0] != DEFAULT) {
		set_mod_fkeys(xw, param[0], -1, False);
	    } else {
		xw->keyboard.modify_now.function_keys = -1;
	    }
	    break;
#endif
	case CASE_HIDE_POINTER:
	    TRACE(("CASE_HIDE_POINTER\n"));
	    if (nparam >= 1 && param[0] != DEFAULT) {
		screen->pointer_mode = param[0];
	    } else {
		screen->pointer_mode = DEF_POINTER_MODE;
	    }
	    break;

	case CASE_CSI_IGNORE:
	    sp->parsestate = cigtable;
	    break;
	}
	if (sp->parsestate == sp->groundtable)
	    sp->lastchar = thischar;
    } while (0);

#if OPT_WIDE_CHARS
    screen->utf8_inparse = (Boolean) ((screen->utf8_mode != uFalse)
				      && (sp->parsestate != sos_table));
#endif

    return True;
}

static void
VTparse(XtermWidget xw)
{
    TScreen *screen;

    /* We longjmp back to this point in VTReset() */
    (void) setjmp(vtjmpbuf);
    screen = &xw->screen;
    memset(&myState, 0, sizeof(myState));
    myState.scssize = 94;	/* number of printable/nonspace ASCII */
    myState.lastchar = -1;	/* not a legal IChar */
    myState.nextstate = -1;	/* not a legal state */

    init_groundtable(screen, &myState);
    myState.parsestate = myState.groundtable;

    do {
    } while (doparsing(xw, doinput(), &myState));
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
		unsigned size = (unsigned) (v_bufptr - v_buffer);
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
    return ((int) c);
}

#ifdef VMS
#define	ptymask()	(v_bufptr > v_bufstr ? pty_mask : 0)

static void
in_put(XtermWidget xw)
{
    static PtySelect select_mask;
    static PtySelect write_mask;
    int update = VTbuffer->update;
    int size;

    int status;
    Dimension replyWidth, replyHeight;
    XtGeometryResult stat;

    TScreen *screen = &xw->screen;
    char *cp;
    int i;

    select_mask = pty_mask;	/* force initial read */
    for (;;) {

	/* if the terminal changed size, resize the widget */
	if (tt_changed) {
	    tt_changed = False;

	    stat = REQ_RESIZE((Widget) xw,
			      ((Dimension) FontWidth(screen)
			       * (tt_width)
			       + 2 * screen->border
			       + screen->fullVwin.sb_info.width),
			      ((Dimension) FontHeight(screen)
			       * (tt_length)
			       + 2 * screen->border),
			      &replyWidth, &replyHeight);

	    if (stat == XtGeometryYes || stat == XtGeometryDone) {
		xw->core.width = replyWidth;
		xw->core.height = replyHeight;

		ScreenResize(xw, replyWidth, replyHeight, &xw->flags);
	    }
	    repairSizeHints();
	}

	if (screen->eventMode == NORMAL
	    && readPtyData(screen, &select_mask, VTbuffer)) {
	    if (screen->scrollWidget
		&& screen->scrollttyoutput
		&& screen->topline < 0)
		/* Scroll to bottom */
		WindowScroll(xw, 0);
	    break;
	}
	if (screen->scroll_amt)
	    FlushScroll(xw);
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
	    if (screen->eventMode != NORMAL)
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
in_put(XtermWidget xw)
{
    static PtySelect select_mask;
    static PtySelect write_mask;

    TScreen *screen = &xw->screen;
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
	if (screen->eventMode == NORMAL
	    && (size = readPtyData(screen, &select_mask, VTbuffer)) != 0) {
	    if (screen->scrollWidget
		&& screen->scrollttyoutput
		&& screen->topline < 0)
		WindowScroll(xw, 0);	/* Scroll to bottom */
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
	    FlushScroll(xw);
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
	if (screen->eventMode != NORMAL)
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
	    if (ice_fd >= 0)
		FD_SET(ice_fd, &select_mask);
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
    TScreen *screen = TScreenOf(term);

    while (!morePtyData(screen, VTbuffer))
	in_put(term);
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
    spot.x = (short) CurCursorX(screen, screen->cur_row, screen->cur_col);
    spot.y = (short) (CursorY(screen, screen->cur_row) + screen->fs_ascent);
    list = XVaCreateNestedList(0,
			       XNSpotLocation, &spot,
			       XNForeground, T_COLOR(screen, TEXT_FG),
			       XNBackground, T_COLOR(screen, TEXT_BG),
			       NULL);
    XSetICValues(screen->xic, XNPreeditAttributes, list, NULL);
    XFree(list);
}
#endif

static void
WrapLine(XtermWidget xw)
{
    TScreen *screen = &(xw->screen);

    /* mark that we had to wrap this line */
    ScrnSetFlag(screen, screen->cur_row, LINEWRAPPED);
    xtermAutoPrint('\n');
    xtermIndex(xw, 1);
    set_cur_col(screen, 0);
}

/*
 * process a string of characters according to the character set indicated
 * by charset.  worry about end of line conditions (wraparound if selected).
 */
void
dotext(XtermWidget xw,
       int charset,
       IChar * buf,		/* start of characters to process */
       Cardinal len)		/* end */
{
    TScreen *screen = &(xw->screen);
#if OPT_WIDE_CHARS
    Cardinal chars_chomped = 1;
    int next_col = screen->cur_col;
#else
    int next_col, last_col, this_col;	/* must be signed */
#endif
    Cardinal offset;

#if OPT_WIDE_CHARS
    /* don't translate if we use UTF-8, and are not handling legacy support
     * for line-drawing characters.
     */
    if ((screen->utf8_mode == uFalse)
	|| (screen->vt100_graphics))
#endif
	if (!xtermCharSetOut(xw, buf, buf + len, charset))
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
	Boolean need_wrap = False;
	int last_chomp = 0;
	chars_chomped = 0;

	if (screen->do_wrap) {
	    screen->do_wrap = False;
	    if ((xw->flags & WRAPAROUND)) {
		WrapLine(xw);
		width_available = MaxCols(screen) - screen->cur_col;
		next_col = screen->cur_col;
	    }
	}

	while (width_here <= width_available && chars_chomped < (len - offset)) {
	    if (!screen->utf8_mode
		|| (screen->vt100_graphics && charset == '0'))
		last_chomp = 1;
	    else
		last_chomp = my_wcwidth((int) buf[chars_chomped + offset]);
	    width_here += last_chomp;
	    chars_chomped++;
	}

	if (width_here > width_available) {
	    if (last_chomp > MaxCols(screen))
		break;		/* give up - it is too big */
	    chars_chomped--;
	    width_here -= last_chomp;
	    if (chars_chomped > 0) {
		need_wrap = True;
	    }
	} else if (width_here == width_available) {
	    need_wrap = True;
	} else if (chars_chomped != (len - offset)) {
	    need_wrap = True;
	}

	/*
	 * Split the wide characters back into separate arrays of 8-bit
	 * characters so we can use the existing interface.
	 *
	 * FIXME:  If we rewrote this interface, it would involve
	 * rewriting all of the memory-management for the screen
	 * buffers (perhaps this is simpler).
	 */
	if (chars_chomped != 0 && next_col <= screen->max_col) {
	    static unsigned limit;
	    static Char *hibyte, *lobyte;
	    Bool both = False;
	    unsigned j, k;

	    if (chars_chomped >= limit) {
		limit = (chars_chomped + 1) * 2;
		lobyte = (Char *) XtRealloc((char *) lobyte, limit);
		hibyte = (Char *) XtRealloc((char *) hibyte, limit);
	    }
	    for (j = offset, k = 0; j < offset + chars_chomped; j++) {
		if (buf[j] == HIDDEN_CHAR)
		    continue;
		lobyte[k] = LO_BYTE(buf[j]);
		if (buf[j] > 255) {
		    hibyte[k] = HI_BYTE(buf[j]);
		    both = True;
		} else {
		    hibyte[k] = 0;
		}
		++k;
	    }

	    WriteText(xw, PAIRED_CHARS(lobyte,
				       (both ? hibyte : 0)),
		      k);
#ifdef NO_LEAKS
	    if (limit != 0) {
		limit = 0;
		XtFree((char *) lobyte);
		XtFree((char *) hibyte);
		lobyte = 0;
		hibyte = 0;
	    }
#endif
	}
	next_col += width_here;
	screen->do_wrap = need_wrap;
    }
#else /* ! OPT_WIDE_CHARS */

    for (offset = 0; offset < len; offset += this_col) {
	last_col = CurMaxCol(screen, screen->cur_row);
	this_col = last_col - screen->cur_col + 1;
	if (this_col <= 1) {
	    if (screen->do_wrap) {
		screen->do_wrap = False;
		if ((xw->flags & WRAPAROUND)) {
		    WrapLine(xw);
		}
	    }
	    this_col = 1;
	}
	if (offset + this_col > len) {
	    this_col = len - offset;
	}
	next_col = screen->cur_col + this_col;

	WriteText(xw, PAIRED_CHARS(buf + offset,
				   buf2 ? buf2 + offset : 0),
		  (unsigned) this_col);

	/*
	 * The call to WriteText updates screen->cur_col.
	 * If screen->cur_col is less than next_col, we must have
	 * hit the right margin - so set the do_wrap flag.
	 */
	screen->do_wrap = (screen->cur_col < next_col);
    }

#endif /* OPT_WIDE_CHARS */
}

#if OPT_WIDE_CHARS
unsigned
visual_width(PAIRED_CHARS(Char * str, Char * str2), Cardinal len)
{
    /* returns the visual width of a string (doublewide characters count
       as 2, normalwide characters count as 1) */
    unsigned my_len = 0;
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
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    switch (event->type) {
    case MapNotify:
	TRACE(("HandleStructNotify(MapNotify)\n"));
#if OPT_ZICONBEEP
	if (screen->zIconBeep_flagged) {
	    screen->zIconBeep_flagged = False;
	    icon_name = NULL;
	    XtGetValues(toplevel, args, XtNumber(args));
	    if (icon_name != NULL) {
		char *buf = CastMallocN(char, strlen(icon_name));
		if (buf == NULL) {
		    screen->zIconBeep_flagged = True;
		    return;
		}
		strcpy(buf, icon_name + 4);
		ChangeIconName(buf);
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
	if (event->xconfigure.window == XtWindow(toplevel)) {
	    int height, width;

	    height = event->xconfigure.height;
	    width = event->xconfigure.width;
	    TRACE(("HandleStructNotify(ConfigureNotify) %d,%d %dx%d\n",
		   event->xconfigure.y, event->xconfigure.x,
		   event->xconfigure.height, event->xconfigure.width));

#if OPT_TOOLBAR
	    /*
	     * The notification is for the top-level widget, but we care about
	     * vt100 (ignore the tek4014 window).
	     */
	    if (xw->screen.Vshow) {
		VTwin *Vwin = WhichVWin(&(xw->screen));
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
			 * FIXME:  Window manager still may be using the old
			 * values.  Try to fool it.
			 */
			REQ_RESIZE((Widget) xw,
				   screen->fullVwin.fullwidth,
				   (Dimension) (info->menu_height
						- save.menu_height
						+ screen->fullVwin.fullheight),
				   NULL, NULL);
			repairSizeHints();
		    }
		}
	    }
#else
	    if (height != xw->hints.height || width != xw->hints.width)
		RequestResize(xw, height, width, False);
#endif /* OPT_TOOLBAR */
	}
	break;
    case ReparentNotify:
	TRACE(("HandleStructNotify(ReparentNotify)\n"));
	break;
    default:
	TRACE(("HandleStructNotify(event %s)\n",
	       visibleEventType(event->type)));
	break;
    }
}
#endif /* HANDLE_STRUCT_NOTIFY */

#if OPT_BLINK_CURS
static void
SetCursorBlink(TScreen * screen, Boolean enable)
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
    SetCursorBlink(screen, (Boolean) (!(screen->cursor_blink)));
}
#endif

/*
 * process ANSI modes set, reset
 */
static void
ansi_modes(XtermWidget xw,
	   void (*func) (unsigned *p, unsigned mask))
{
    int i;

    for (i = 0; i < nparam; ++i) {
	switch (param[i]) {
	case 2:		/* KAM (if set, keyboard locked */
	    (*func) (&xw->keyboard.flags, MODE_KAM);
	    break;

	case 4:		/* IRM                          */
	    (*func) (&xw->flags, INSERT);
	    break;

	case 12:		/* SRM (if set, local echo      */
	    (*func) (&xw->keyboard.flags, MODE_SRM);
	    break;

	case 20:		/* LNM                          */
	    (*func) (&xw->flags, LINEFEED);
	    update_autolinefeed();
	    break;
	}
    }
}

#define IsSM() (func == bitset)

#define set_bool_mode(flag) \
	flag = (Boolean) IsSM()

static void
really_set_mousemode(XtermWidget xw,
		     Bool enabled,
		     XtermMouseModes mode)
{
    xw->screen.send_mouse_pos = enabled ? mode : MOUSE_OFF;
    if (xw->screen.send_mouse_pos != MOUSE_OFF)
	xtermShowPointer(xw, True);
}

#define set_mousemode(mode) really_set_mousemode(xw, IsSM(), mode)

#if OPT_READLINE
#define set_mouseflag(f)		\
	(IsSM()				\
	 ? SCREEN_FLAG_set(screen, f)	\
	 : SCREEN_FLAG_unset(screen, f))
#endif

/*
 * process DEC private modes set, reset
 */
static void
dpmodes(XtermWidget xw,
	void (*func) (unsigned *p, unsigned mask))
{
    TScreen *screen = &xw->screen;
    int i, j;
    unsigned myflags;

    for (i = 0; i < nparam; ++i) {
	TRACE(("%s %d\n", IsSM()? "DECSET" : "DECRST", param[i]));
	switch (param[i]) {
	case 1:		/* DECCKM                       */
	    (*func) (&xw->keyboard.flags, MODE_DECCKM);
	    update_appcursor();
	    break;
	case 2:		/* DECANM - ANSI/VT52 mode      */
	    if (IsSM()) {	/* ANSI (VT100) */
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
		ClearScreen(xw);
		CursorSet(screen, 0, 0, xw->flags);
		if ((j = IsSM()? 132 : 80) !=
		    ((xw->flags & IN132COLUMNS) ? 132 : 80) ||
		    j != MaxCols(screen))
		    RequestResize(xw, -1, j, True);
		(*func) (&xw->flags, IN132COLUMNS);
	    }
	    break;
	case 4:		/* DECSCLM (slow scroll)        */
	    if (IsSM()) {
		screen->jumpscroll = 0;
		if (screen->scroll_amt)
		    FlushScroll(xw);
	    } else
		screen->jumpscroll = 1;
	    (*func) (&xw->flags, SMOOTHSCROLL);
	    update_jumpscroll();
	    break;
	case 5:		/* DECSCNM                      */
	    myflags = xw->flags;
	    (*func) (&xw->flags, REVERSE_VIDEO);
	    if ((xw->flags ^ myflags) & REVERSE_VIDEO)
		ReverseVideo(xw);
	    /* update_reversevideo done in RevVid */
	    break;

	case 6:		/* DECOM                        */
	    (*func) (&xw->flags, ORIGIN);
	    CursorSet(screen, 0, 0, xw->flags);
	    break;

	case 7:		/* DECAWM                       */
	    (*func) (&xw->flags, WRAPAROUND);
	    update_autowrap();
	    break;
	case 8:		/* DECARM                       */
	    /* ignore autorepeat
	     * XAutoRepeatOn() and XAutoRepeatOff() can do this, but only
	     * for the whole display - not limited to a given window.
	     */
	    break;
	case SET_X10_MOUSE:	/* MIT bogus sequence           */
	    MotionOff(screen, xw);
	    set_mousemode(X10_MOUSE);
	    break;
#if OPT_TOOLBAR
	case 10:		/* rxvt */
	    ShowToolbar(IsSM());
	    break;
#endif
#if OPT_BLINK_CURS
	case 12:		/* att610: Start/stop blinking cursor */
	    if (screen->cursor_blink_res) {
		set_bool_mode(screen->cursor_blink_esc);
		SetCursorBlink(screen, screen->cursor_blink);
	    }
	    break;
#endif
	case 18:		/* DECPFF: print form feed */
	    set_bool_mode(screen->printer_formfeed);
	    break;
	case 19:		/* DECPEX: print extent */
	    set_bool_mode(screen->printer_extent);
	    break;
	case 25:		/* DECTCEM: Show/hide cursor (VT200) */
	    set_bool_mode(screen->cursor_set);
	    break;
	case 30:		/* rxvt */
	    if (screen->fullVwin.sb_info.width != (IsSM()? ON : OFF))
		ToggleScrollBar(xw);
	    break;
#if OPT_SHIFT_FONTS
	case 35:		/* rxvt */
	    set_bool_mode(xw->misc.shift_fonts);
	    break;
#endif
	case 38:		/* DECTEK                       */
#if OPT_TEK4014
	    if (IsSM() && !(screen->inhibit & I_TEK)) {
		FlushLog(screen);
		TEK4014_ACTIVE(xw) = True;
	    }
#endif
	    break;
	case 40:		/* 132 column mode              */
	    set_bool_mode(screen->c132);
	    update_allow132();
	    break;
	case 41:		/* curses hack                  */
	    set_bool_mode(screen->curses);
	    update_cursesemul();
	    break;
	case 42:		/* DECNRCM national charset (VT220) */
	    (*func) (&xw->flags, NATIONAL);
	    break;
	case 44:		/* margin bell                  */
	    set_bool_mode(screen->marginbell);
	    if (!screen->marginbell)
		screen->bellarmed = -1;
	    update_marginbell();
	    break;
	case 45:		/* reverse wraparound   */
	    (*func) (&xw->flags, REVERSEWRAP);
	    update_reversewrap();
	    break;
#ifdef ALLOWLOGGING
	case 46:		/* logging              */
#ifdef ALLOWLOGFILEONOFF
	    /*
	     * if this feature is enabled, logging may be
	     * enabled and disabled via escape sequences.
	     */
	    if (IsSM())
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
	    if (!xw->misc.titeInhibit) {
		if (IsSM()) {
		    CursorSave(xw);
		    ToAlternate(xw);
		    ClearScreen(xw);
		} else {
		    FromAlternate(xw);
		    CursorRestore(xw);
		}
	    } else if (xw->misc.tiXtraScroll) {
		if (IsSM()) {
		    xtermScroll(xw, screen->max_row);
		}
	    }
	    break;
	case 1047:
	    /* FALLTHRU */
	case 47:		/* alternate buffer */
	    if (!xw->misc.titeInhibit) {
		if (IsSM()) {
		    ToAlternate(xw);
		} else {
		    if (screen->alternate
			&& (param[i] == 1047))
			ClearScreen(xw);
		    FromAlternate(xw);
		}
	    } else if (xw->misc.tiXtraScroll) {
		if (IsSM()) {
		    xtermScroll(xw, screen->max_row);
		}
	    }
	    break;
	case 66:		/* DECNKM */
	    (*func) (&xw->keyboard.flags, MODE_DECKPAM);
	    update_appkeypad();
	    break;
	case 67:		/* DECBKM */
	    /* back-arrow mapped to backspace or delete(D) */
	    (*func) (&xw->keyboard.flags, MODE_DECBKM);
	    TRACE(("DECSET DECBKM %s\n",
		   BtoS(xw->keyboard.flags & MODE_DECBKM)));
	    update_decbkm();
	    break;
	case SET_VT200_MOUSE:	/* xterm bogus sequence         */
	    MotionOff(screen, xw);
	    set_mousemode(VT200_MOUSE);
	    break;
	case SET_VT200_HIGHLIGHT_MOUSE:	/* xterm sequence w/hilite tracking */
	    MotionOff(screen, xw);
	    set_mousemode(VT200_HIGHLIGHT_MOUSE);
	    break;
	case SET_BTN_EVENT_MOUSE:
	    MotionOff(screen, xw);
	    set_mousemode(BTN_EVENT_MOUSE);
	    break;
	case SET_ANY_EVENT_MOUSE:
	    set_mousemode(ANY_EVENT_MOUSE);
	    if (screen->send_mouse_pos == MOUSE_OFF) {
		MotionOff(screen, xw);
	    } else {
		MotionOn(screen, xw);
	    }
	    break;
#if OPT_FOCUS_EVENT
	case SET_FOCUS_EVENT_MOUSE:
	    set_bool_mode(screen->send_focus_pos);
	    break;
#endif
	case 1010:		/* rxvt */
	    set_bool_mode(screen->scrollttyoutput);
	    update_scrollttyoutput();
	    break;
	case 1011:		/* rxvt */
	    set_bool_mode(screen->scrollkey);
	    update_scrollkey();
	    break;
	case 1034:
	    set_bool_mode(xw->screen.input_eight_bits);
	    update_alt_esc();
	    break;
#if OPT_NUM_LOCK
	case 1035:
	    set_bool_mode(xw->misc.real_NumLock);
	    update_num_lock();
	    break;
	case 1036:
	    set_bool_mode(screen->meta_sends_esc);
	    update_meta_esc();
	    break;
#endif
	case 1037:
	    set_bool_mode(screen->delete_is_del);
	    update_delete_del();
	    break;
#if OPT_NUM_LOCK
	case 1039:
	    set_bool_mode(screen->alt_sends_esc);
	    update_alt_esc();
	    break;
#endif
	case 1040:
	    set_bool_mode(screen->keepSelection);
	    update_keepSelection();
	    break;
	case 1041:
	    set_bool_mode(screen->selectToClipboard);
	    update_selectToClipboard();
	    break;
	case 1042:
	    set_bool_mode(screen->bellIsUrgent);
	    update_bellIsUrgent();
	    break;
	case 1043:
	    set_bool_mode(screen->poponbell);
	    update_poponbell();
	    break;
	case 1048:
	    if (!xw->misc.titeInhibit) {
		if (IsSM())
		    CursorSave(xw);
		else
		    CursorRestore(xw);
	    }
	    break;
#if OPT_TCAP_FKEYS
	case 1050:
	    set_keyboard_type(xw, keyboardIsTermcap, IsSM());
	    break;
#endif
#if OPT_SUN_FUNC_KEYS
	case 1051:
	    set_keyboard_type(xw, keyboardIsSun, IsSM());
	    break;
#endif
#if OPT_HP_FUNC_KEYS
	case 1052:
	    set_keyboard_type(xw, keyboardIsHP, IsSM());
	    break;
#endif
#if OPT_SCO_FUNC_KEYS
	case 1053:
	    set_keyboard_type(xw, keyboardIsSCO, IsSM());
	    break;
#endif
	case 1060:
	    set_keyboard_type(xw, keyboardIsLegacy, IsSM());
	    break;
#if OPT_SUNPC_KBD
	case 1061:
	    set_keyboard_type(xw, keyboardIsVT220, IsSM());
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
savemodes(XtermWidget xw)
{
    TScreen *screen = &xw->screen;
    int i;

    for (i = 0; i < nparam; i++) {
	TRACE(("savemodes %d\n", param[i]));
	switch (param[i]) {
	case 1:		/* DECCKM                       */
	    DoSM(DP_DECCKM, xw->keyboard.flags & MODE_DECCKM);
	    break;
	case 3:		/* DECCOLM                      */
	    if (screen->c132)
		DoSM(DP_DECCOLM, xw->flags & IN132COLUMNS);
	    break;
	case 4:		/* DECSCLM (slow scroll)        */
	    DoSM(DP_DECSCLM, xw->flags & SMOOTHSCROLL);
	    break;
	case 5:		/* DECSCNM                      */
	    DoSM(DP_DECSCNM, xw->flags & REVERSE_VIDEO);
	    break;
	case 6:		/* DECOM                        */
	    DoSM(DP_DECOM, xw->flags & ORIGIN);
	    break;
	case 7:		/* DECAWM                       */
	    DoSM(DP_DECAWM, xw->flags & WRAPAROUND);
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
	case 42:		/* DECNRCM national charset (VT220) */
	    /* do nothing */
	    break;
	case 44:		/* margin bell                  */
	    DoSM(DP_X_MARGIN, screen->marginbell);
	    break;
	case 45:		/* reverse wraparound   */
	    DoSM(DP_X_REVWRAP, xw->flags & REVERSEWRAP);
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
#if OPT_FOCUS_EVENT
	case SET_FOCUS_EVENT_MOUSE:
	    DoSM(DP_X_FOCUS, screen->send_focus_pos);
	    break;
#endif
	case 1048:
	    if (!xw->misc.titeInhibit) {
		CursorSave(xw);
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
restoremodes(XtermWidget xw)
{
    TScreen *screen = &xw->screen;
    int i, j;

    for (i = 0; i < nparam; i++) {
	TRACE(("restoremodes %d\n", param[i]));
	switch (param[i]) {
	case 1:		/* DECCKM                       */
	    bitcpy(&xw->keyboard.flags,
		   screen->save_modes[DP_DECCKM], MODE_DECCKM);
	    update_appcursor();
	    break;
	case 3:		/* DECCOLM                      */
	    if (screen->c132) {
		ClearScreen(xw);
		CursorSet(screen, 0, 0, xw->flags);
		if ((j = (screen->save_modes[DP_DECCOLM] & IN132COLUMNS)
		     ? 132 : 80) != ((xw->flags & IN132COLUMNS)
				     ? 132 : 80) || j != MaxCols(screen))
		    RequestResize(xw, -1, j, True);
		bitcpy(&xw->flags,
		       screen->save_modes[DP_DECCOLM],
		       IN132COLUMNS);
	    }
	    break;
	case 4:		/* DECSCLM (slow scroll)        */
	    if (screen->save_modes[DP_DECSCLM] & SMOOTHSCROLL) {
		screen->jumpscroll = 0;
		if (screen->scroll_amt)
		    FlushScroll(xw);
	    } else
		screen->jumpscroll = 1;
	    bitcpy(&xw->flags, screen->save_modes[DP_DECSCLM], SMOOTHSCROLL);
	    update_jumpscroll();
	    break;
	case 5:		/* DECSCNM                      */
	    if ((screen->save_modes[DP_DECSCNM] ^ xw->flags) & REVERSE_VIDEO) {
		bitcpy(&xw->flags, screen->save_modes[DP_DECSCNM], REVERSE_VIDEO);
		ReverseVideo(xw);
		/* update_reversevideo done in RevVid */
	    }
	    break;
	case 6:		/* DECOM                        */
	    bitcpy(&xw->flags, screen->save_modes[DP_DECOM], ORIGIN);
	    CursorSet(screen, 0, 0, xw->flags);
	    break;

	case 7:		/* DECAWM                       */
	    bitcpy(&xw->flags, screen->save_modes[DP_DECAWM], WRAPAROUND);
	    update_autowrap();
	    break;
	case 8:		/* DECARM                       */
	    /* ignore autorepeat */
	    break;
	case SET_X10_MOUSE:	/* MIT bogus sequence           */
	    DoRM0(DP_X_X10MSE, screen->send_mouse_pos);
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
	    bitcpy(&xw->flags, screen->save_modes[DP_X_REVWRAP], REVERSEWRAP);
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
	    if (!xw->misc.titeInhibit) {
		if (screen->save_modes[DP_X_ALTSCRN])
		    ToAlternate(xw);
		else
		    FromAlternate(xw);
		/* update_altscreen done by ToAlt and FromAlt */
	    } else if (xw->misc.tiXtraScroll) {
		if (screen->save_modes[DP_X_ALTSCRN]) {
		    xtermScroll(xw, screen->max_row);
		}
	    }
	    break;
	case SET_VT200_MOUSE:	/* mouse bogus sequence         */
	case SET_VT200_HIGHLIGHT_MOUSE:
	case SET_BTN_EVENT_MOUSE:
	case SET_ANY_EVENT_MOUSE:
	    DoRM0(DP_X_MOUSE, screen->send_mouse_pos);
	    break;
#if OPT_FOCUS_EVENT
	case SET_FOCUS_EVENT_MOUSE:
	    DoRM(DP_X_FOCUS, screen->send_focus_pos);
	    break;
#endif
	case 1048:
	    if (!xw->misc.titeInhibit) {
		CursorRestore(xw);
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
report_win_label(XtermWidget xw,
		 int code,
		 XTextProperty * text,
		 Status ok)
{
    char **list;
    int length = 0;

    reply.a_type = ANSI_ESC;
    unparseputc(xw, ANSI_ESC);
    unparseputc(xw, ']');
    unparseputc(xw, code);

    if (ok) {
	if (XTextPropertyToStringList(text, &list, &length)) {
	    int n, c;
	    for (n = 0; n < length; n++) {
		char *s = list[n];
		while ((c = *s++) != '\0')
		    unparseputc(xw, c);
	    }
	    XFreeStringList(list);
	}
	if (text->value != 0)
	    XFree(text->value);
    }

    unparseputc(xw, ANSI_ESC);
    unparseputc(xw, '\\');	/* should be ST */
    unparse_end(xw);
}

/*
 * Window operations (from CDE dtterm description, as well as extensions).
 * See also "allowWindowOps" resource.
 */
static void
window_ops(XtermWidget xw)
{
    TScreen *screen = &xw->screen;
    XWindowChanges values;
    XWindowAttributes win_attrs;
    XTextProperty text;
    unsigned value_mask;
#if OPT_MAXIMIZE
    unsigned root_width;
    unsigned root_height;
#endif

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
	RequestResize(xw, param[1], param[2], False);
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
	RequestResize(xw, param[1], param[2], True);
	break;

#if OPT_MAXIMIZE
    case 9:			/* Maximize or restore */
	RequestMaximize(xw, param[1]);
	break;
#endif

    case 11:			/* Report the window's state */
	XGetWindowAttributes(screen->display,
			     VWindow(screen),
			     &win_attrs);
	reply.a_type = ANSI_CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 1;
	reply.a_param[0] = (ParmType) ((win_attrs.map_state == IsViewable)
				       ? 1
				       : 2);
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(xw, &reply);
	break;

    case 13:			/* Report the window's position */
	XGetWindowAttributes(screen->display,
			     WMFrameWindow(xw),
			     &win_attrs);
	reply.a_type = ANSI_CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 3;
	reply.a_param[0] = 3;
	reply.a_param[1] = (ParmType) win_attrs.x;
	reply.a_param[2] = (ParmType) win_attrs.y;
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(xw, &reply);
	break;

    case 14:			/* Report the window's size in pixels */
	XGetWindowAttributes(screen->display,
			     VWindow(screen),
			     &win_attrs);
	reply.a_type = ANSI_CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 3;
	reply.a_param[0] = 4;
	/*FIXME: find if dtterm uses
	 *    win_attrs.height or Height
	 *      win_attrs.width  or Width
	 */
	reply.a_param[1] = (ParmType) Height(screen);
	reply.a_param[2] = (ParmType) Width(screen);
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(xw, &reply);
	break;

    case 18:			/* Report the text's size in characters */
	reply.a_type = ANSI_CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 3;
	reply.a_param[0] = 8;
	reply.a_param[1] = (ParmType) MaxRows(screen);
	reply.a_param[2] = (ParmType) MaxCols(screen);
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(xw, &reply);
	break;

#if OPT_MAXIMIZE
    case 19:			/* Report the screen's size, in characters */
	if (!QueryMaximize(xw, &root_height, &root_width)) {
	    root_height = 0;
	    root_width = 0;
	}
	reply.a_type = ANSI_CSI;
	reply.a_pintro = 0;
	reply.a_nparam = 3;
	reply.a_param[0] = 9;
	reply.a_param[1] = (ParmType) (root_height / FontHeight(screen));
	reply.a_param[2] = (ParmType) (root_width / FontWidth(screen));
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(xw, &reply);
	break;
#endif

    case 20:			/* Report the icon's label */
	report_win_label(xw, 'L', &text,
			 XGetWMIconName(screen->display, VShellWindow, &text));
	break;

    case 21:			/* Report the window's title */
	report_win_label(xw, 'l', &text,
			 XGetWMName(screen->display, VShellWindow, &text));
	break;

    default:			/* DECSLPP (24, 25, 36, 48, 72, 144) */
	if (param[0] >= 24)
	    RequestResize(xw, param[0], -1, True);
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
unparseputc1(XtermWidget xw, int c)
{
    if (c >= 0x80 && c <= 0x9F) {
	if (!xw->screen.control_eight_bits) {
	    unparseputc(xw, A2E(ANSI_ESC));
	    c = A2E(c - 0x40);
	}
    }
    unparseputc(xw, c);
}

void
unparseseq(XtermWidget xw, ANSI * ap)
{
    int c;
    int i;
    int inters;

    unparseputc1(xw, c = ap->a_type);
    if (c == ANSI_ESC
	|| c == ANSI_DCS
	|| c == ANSI_CSI
	|| c == ANSI_OSC
	|| c == ANSI_PM
	|| c == ANSI_APC
	|| c == ANSI_SS3) {
	if (ap->a_pintro != 0)
	    unparseputc(xw, ap->a_pintro);
	for (i = 0; i < ap->a_nparam; ++i) {
	    if (i != 0)
		unparseputc(xw, ';');
	    unparseputn(xw, (unsigned int) ap->a_param[i]);
	}
	if ((inters = ap->a_inters) != 0) {
	    for (i = 3; i >= 0; --i) {
		c = CharOf(inters >> (8 * i));
		if (c != 0)
		    unparseputc(xw, c);
	    }
	}
	unparseputc(xw, (char) ap->a_final);
    }
    unparse_end(xw);
}

void
unparseputn(XtermWidget xw, unsigned int n)
{
    unsigned int q;

    q = n / 10;
    if (q != 0)
	unparseputn(xw, q);
    unparseputc(xw, (char) ('0' + (n % 10)));
}

void
unparseputs(XtermWidget xw, char *s)
{
    while (*s)
	unparseputc(xw, *s++);
}

void
unparseputc(XtermWidget xw, int c)
{
    IChar *buf = xw->screen.unparse_bfr;
    unsigned len;

    if ((xw->screen.unparse_len + 2) >= sizeof(xw->screen.unparse_bfr))
	unparse_end(xw);

    len = xw->screen.unparse_len;

#if OPT_TCAP_QUERY
    /*
     * If we're returning a termcap string, it has to be translated since
     * a DCS must not contain any characters except for the normal 7-bit
     * printable ASCII (counting tab, carriage return, etc).  For now,
     * just use hexadecimal for the whole thing.
     */
    if (xw->screen.tc_query_code >= 0) {
	char tmp[3];
	sprintf(tmp, "%02X", c & 0xFF);
	buf[len++] = CharOf(tmp[0]);
	buf[len++] = CharOf(tmp[1]);
    } else
#endif
    if ((buf[len++] = (IChar) c) == '\r' && (xw->flags & LINEFEED)) {
	buf[len++] = '\n';
    }

    xw->screen.unparse_len = len;

    /* If send/receive mode is reset, we echo characters locally */
    if ((xw->keyboard.flags & MODE_SRM) == 0) {
	(void) doparsing(xw, (unsigned) c, &myState);
    }
}

void
unparse_end(XtermWidget xw)
{
    if (xw->screen.unparse_len) {
#ifdef VMS
	tt_write(xw->screen.unparse_bfr, xw->screen.unparse_len);
#else /* VMS */
	writePtyData(xw->screen.respond, xw->screen.unparse_bfr, xw->screen.unparse_len);
#endif /* VMS */
	xw->screen.unparse_len = 0;
    }
}

void
ToggleAlternate(XtermWidget xw)
{
    if (xw->screen.alternate)
	FromAlternate(xw);
    else
	ToAlternate(xw);
}

static void
ToAlternate(XtermWidget xw)
{
    TScreen *screen = &(xw->screen);

    if (!screen->alternate) {
	TRACE(("ToAlternate\n"));
	if (!screen->altbuf)
	    screen->altbuf = Allocate(MaxRows(screen), MaxCols(screen),
				      &screen->abuf_address);
	SwitchBufs(xw);
	screen->alternate = True;
	update_altscreen();
    }
}

static void
FromAlternate(XtermWidget xw)
{
    TScreen *screen = &(xw->screen);

    if (screen->alternate) {
	TRACE(("FromAlternate\n"));
	if (screen->scroll_amt)
	    FlushScroll(xw);
	screen->alternate = False;
	SwitchBufs(xw);
	update_altscreen();
    }
}

static void
SwitchBufs(XtermWidget xw)
{
    TScreen *screen = &(xw->screen);
    int rows, top;

    if (screen->cursor_state)
	HideCursor();

    rows = MaxRows(screen);
    SwitchBufPtrs(screen);

    if ((top = INX2ROW(screen, 0)) < rows) {
	if (screen->scroll_amt)
	    FlushScroll(xw);
	XClearArea(screen->display,
		   VWindow(screen),
		   (int) OriginX(screen),
		   (int) top * FontHeight(screen) + screen->border,
		   (unsigned) Width(screen),
		   (unsigned) ((rows - top) * FontHeight(screen)),
		   False);
    }
    ScrnUpdate(xw, 0, 0, rows, MaxCols(screen), False);
}

Bool
CheckBufPtrs(TScreen * screen)
{
    return (screen->visbuf != 0
	    && screen->altbuf != 0);
}

/*
 * Swap buffer line pointers between alternate and regular screens.
 * visbuf contains pointers from allbuf or altbuf for the visible screen,
 * and pointers from allbuf for the saved lines.  That makes it simple to
 * scroll back over the saved lines without juggling pointers for the
 * regular and alternate screens.
 */
void
SwitchBufPtrs(TScreen * screen)
{
    if (CheckBufPtrs(screen)) {
	size_t len = ScrnPointers(screen, (unsigned) MaxRows(screen));

	memcpy((char *) screen->save_ptr, (char *) screen->visbuf, len);
	memcpy((char *) screen->visbuf, (char *) screen->altbuf, len);
	memcpy((char *) screen->altbuf, (char *) screen->save_ptr, len);
    }
}

void
VTRun(void)
{
    TScreen *screen = TScreenOf(term);

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
    screen->is_running = True;
    if (!setjmp(VTend))
	VTparse(term);
    StopBlinking(screen);
    HideCursor();
    screen->cursor_set = OFF;
    TRACE(("... VTRun\n"));
}

/*ARGSUSED*/
static void
VTExpose(Widget w GCC_UNUSED,
	 XEvent * event,
	 Region region GCC_UNUSED)
{
#ifdef DEBUG
    if (debug)
	fputs("Expose\n", stderr);
#endif /* DEBUG */
    if (event->type == Expose)
	HandleExposure(term, event);
}

static void
VTGraphicsOrNoExpose(XEvent * event)
{
    TScreen *screen = TScreenOf(term);
    if (screen->incopy <= 0) {
	screen->incopy = 1;
	if (screen->scrolls > 0)
	    screen->scrolls--;
    }
    if (event->type == GraphicsExpose)
	if (HandleExposure(term, event))
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
	XtermWidget xw = (XtermWidget) w;
	ScreenResize(xw, xw->core.width, xw->core.height, &xw->flags);
    }
}

#define okDimension(src,dst) ((src <= 32767) \
			  && ((dst = (Dimension) src) == src))

static void
RequestResize(XtermWidget xw, int rows, int cols, Bool text)
{
    TScreen *screen = &xw->screen;
    unsigned long value;
    Dimension replyWidth, replyHeight;
    Dimension askedWidth, askedHeight;
    XtGeometryResult status;
    XWindowAttributes attrs;

    TRACE(("RequestResize(rows=%d, cols=%d, text=%d)\n", rows, cols, text));

    if ((askedWidth = (Dimension) cols) < cols
	|| (askedHeight = (Dimension) rows) < rows)
	return;

    if (askedHeight == 0
	|| askedWidth == 0
	|| xw->misc.limit_resize > 0) {
	XGetWindowAttributes(XtDisplay(xw),
			     RootWindowOfScreen(XtScreen(xw)), &attrs);
    }

    if (text) {
	if ((value = (unsigned long) rows) != 0) {
	    if (rows < 0)
		value = (unsigned long) MaxRows(screen);
	    value *= (unsigned long) FontHeight(screen);
	    value += (unsigned long) (2 * screen->border);
	    if (!okDimension(value, askedHeight))
		return;
	}

	if ((value = (unsigned long) cols) != 0) {
	    if (cols < 0)
		value = (unsigned long) MaxCols(screen);
	    value *= (unsigned long) FontWidth(screen);
	    value += (unsigned long) ((2 * screen->border)
				      + ScrollbarWidth(screen));
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
	askedHeight = (Dimension) attrs.height;
    if (cols == 0)
	askedWidth = (Dimension) attrs.width;

    if (xw->misc.limit_resize > 0) {
	Dimension high = (Dimension) (xw->misc.limit_resize * attrs.height);
	Dimension wide = (Dimension) (xw->misc.limit_resize * attrs.width);
	if (high < attrs.height)
	    high = (Dimension) attrs.height;
	if (askedHeight > high)
	    askedHeight = high;
	if (wide < attrs.width)
	    wide = (Dimension) attrs.width;
	if (askedWidth > wide)
	    askedWidth = wide;
    }
#ifndef nothack
    getXtermSizeHints(xw);
#endif

    status = REQ_RESIZE((Widget) xw,
			askedWidth, askedHeight,
			&replyWidth, &replyHeight);

    if (status == XtGeometryYes ||
	status == XtGeometryDone) {
	ScreenResize(xw, replyWidth, replyHeight, &xw->flags);
    }
#ifndef nothack
    /*
     * XtMakeResizeRequest() has the undesirable side-effect of clearing
     * the window manager's hints, even on a failed request.  This would
     * presumably be fixed if the shell did its own work.
     */
    if (xw->hints.flags
	&& replyHeight
	&& replyWidth) {
	xw->hints.height = replyHeight;
	xw->hints.width = replyWidth;

	TRACE(("%s@%d -- ", __FILE__, __LINE__));
	TRACE_HINTS(&xw->hints);
	XSetWMNormalHints(screen->display, VShellWindow, &xw->hints);
	TRACE(("%s@%d -- ", __FILE__, __LINE__));
	TRACE_WM_HINTS(xw);
    }
#endif

    XSync(screen->display, False);	/* synchronize */
    if (XtAppPending(app_con))
	xevents();

    TRACE(("...RequestResize done\n"));
}

static String xterm_trans =
"<ClientMessage>WM_PROTOCOLS: DeleteWindow()\n\
     <MappingNotify>: KeyboardMapping()\n";

int
VTInit(void)
{
    TScreen *screen = TScreenOf(term);
    Widget vtparent = SHELL_OF(term);

    TRACE(("VTInit {{\n"));

    XtRealizeWidget(vtparent);
    XtOverrideTranslations(vtparent, XtParseTranslationTable(xterm_trans));
    (void) XSetWMProtocols(XtDisplay(vtparent), XtWindow(vtparent),
			   &wm_delete_window, 1);
    TRACE_TRANS("shell", vtparent);
    TRACE_TRANS("vt100", (Widget) (term));

    if (screen->allbuf == NULL)
	VTallocbuf();

    TRACE(("...}} VTInit\n"));
    return (1);
}

static void
VTallocbuf(void)
{
    TScreen *screen = TScreenOf(term);
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
#define init_Dres2(name,i) \
	TRACE(("init " #name "[%d] = %f\n", i, \
		wnew->name[i] = request->name[i]))
#define init_Ires(name) \
	TRACE(("init " #name " = %d\n", \
		wnew->name = request->name))
#define init_Sres(name) \
	TRACE(("init " #name " = \"%s\"\n", \
		(wnew->name = x_strtrim(request->name)) != NULL \
			? wnew->name : "<null>"))
#define init_Sres2(name,i) \
	TRACE(("init " #name "[%d] = \"%s\"\n", i, \
		(wnew->name(i) = x_strtrim(request->name(i))) != NULL \
			? wnew->name(i) : "<null>"))
#define init_Tres(offset) \
	TRACE(("init screen.Tcolors[" #offset "] = %#lx\n", \
		fill_Tres(wnew, request, offset)))
#else
#define init_Bres(name)    wnew->name = request->name
#define init_Dres2(name,i) wnew->name[i] = request->name[i]
#define init_Ires(name)    wnew->name = request->name
#define init_Sres(name)    wnew->name = x_strtrim(request->name)
#define init_Sres2(name,i) wnew->name(i) = x_strtrim(request->name(i))
#define init_Tres(offset)  fill_Tres(wnew, request, offset)
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
    } else if (isDefaultForeground(name)) {
	target->screen.Tcolors[offset].value =
	    ((offset == TEXT_FG || offset == TEXT_BG)
	     ? target->dft_foreground
	     : target->screen.Tcolors[TEXT_FG].value);
    } else if (isDefaultBackground(name)) {
	target->screen.Tcolors[offset].value =
	    ((offset == TEXT_FG || offset == TEXT_BG)
	     ? target->dft_background
	     : target->screen.Tcolors[TEXT_BG].value);
    } else {
	memset(&temp, 0, sizeof(temp));
	if (AllocateTermColor(target, &temp, offset, name)) {
	    if (COLOR_DEFINED(&(temp), offset))
		free(temp.names[offset]);
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
		  ? (int) strlen(request->misc.default_font.f_n)
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
		request->misc.callfilter = (Boolean) (is_utf8 ? 0 : 1);
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
	request->misc.callfilter = (Boolean) (is_utf8 ? 0 : 1);
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

    request->screen.utf8_inparse = (Boolean) (request->screen.utf8_mode != uFalse);

    TRACE(("... updated screen.utf8_mode = %d\n", request->screen.utf8_mode));
    TRACE(("...VTInitialize_locale done\n"));
}
#endif

static void
ParseOnClicks(XtermWidget wnew, XtermWidget wreq, Cardinal item)
{
    /* *INDENT-OFF* */
    static struct {
	const String	name;
	SelectUnit	code;
    } table[] = {
    	{ "char",	Select_CHAR },
    	{ "word",	Select_WORD },
    	{ "line",	Select_LINE },
    	{ "group",	Select_GROUP },
    	{ "page",	Select_PAGE },
    	{ "all",	Select_ALL },
#if OPT_SELECT_REGEX
    	{ "regex",	Select_REGEX },
#endif
    };
    /* *INDENT-ON* */

    String res = wreq->screen.onClick[item];
    String next = x_skip_nonblanks(res);
    Cardinal n;

    wnew->screen.selectMap[item] = NSELECTUNITS;
    for (n = 0; n < XtNumber(table); ++n) {
	if (!x_strncasecmp(table[n].name, res, (unsigned) (next - res))) {
	    wnew->screen.selectMap[item] = table[n].code;
#if OPT_SELECT_REGEX
	    if (table[n].code == Select_REGEX) {
		wnew->screen.selectExpr[item] = x_strtrim(next);
		TRACE(("Parsed regex \"%s\"\n", wnew->screen.selectExpr[item]));
	    }
#endif
	    break;
	}
    }
}

/* ARGSUSED */
static void
VTInitialize(Widget wrequest,
	     Widget new_arg,
	     ArgList args GCC_UNUSED,
	     Cardinal *num_args GCC_UNUSED)
{
#define Kolor(name) wnew->screen.name.resource
#define TxtFg(name) !x_strcasecmp(Kolor(Tcolors[TEXT_FG]), Kolor(name))
#define TxtBg(name) !x_strcasecmp(Kolor(Tcolors[TEXT_BG]), Kolor(name))
#define DftFg(name) isDefaultForeground(Kolor(name))
#define DftBg(name) isDefaultBackground(Kolor(name))

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

    /* DESCO Sys#67660
     * Zero out the entire "keyboard" component of "wnew" widget.
     */
    bzero((char *) &wnew->keyboard, sizeof(wnew->keyboard));

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
    init_Bres(screen.cursor_underline);
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
    init_Bres(screen.bellIsUrgent);
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
    wnew->misc.alt_mods = 0;
    wnew->misc.meta_mods = 0;
    wnew->misc.other_mods = 0;
#endif
#if OPT_SHIFT_FONTS
    init_Bres(misc.shift_fonts);
#endif
#if OPT_SUNPC_KBD
    init_Ires(misc.ctrl_fkeys);
#endif
#if OPT_TEK4014
    TEK4014_SHOWN(wnew) = False;	/* not a resource... */
    init_Bres(misc.tekInhibit);
    init_Bres(misc.tekSmall);
    init_Bres(misc.TekEmu);
#endif
#if OPT_TCAP_QUERY
    wnew->screen.tc_query_code = -1;
#endif
    wnew->misc.re_verse0 = request->misc.re_verse;
    init_Bres(misc.re_verse);
    init_Ires(screen.multiClickTime);
    init_Ires(screen.bellSuppressTime);
    init_Sres(screen.charClass);

    init_Bres(screen.always_highlight);
    init_Bres(screen.brokenSelections);
    init_Bres(screen.cutNewline);
    init_Bres(screen.cutToBeginningOfLine);
    init_Bres(screen.highlight_selection);
    init_Bres(screen.i18nSelections);
    init_Bres(screen.keepSelection);
    init_Bres(screen.selectToClipboard);
    init_Bres(screen.trim_selection);

    wnew->screen.pointer_cursor = request->screen.pointer_cursor;
    init_Ires(screen.pointer_mode);

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
    init_Bres(screen.alt_is_not_meta);
    init_Bres(screen.alt_sends_esc);
    init_Bres(screen.meta_sends_esc);

    init_Bres(screen.allowSendEvent0);
    init_Bres(screen.allowFontOp0);
    init_Bres(screen.allowTcapOp0);
    init_Bres(screen.allowTitleOp0);
    init_Bres(screen.allowWindowOp0);

    /* make a copy so that editres cannot change the resource after startup */
    wnew->screen.allowSendEvents = wnew->screen.allowSendEvent0;
    wnew->screen.allowFontOps = wnew->screen.allowFontOp0;
    wnew->screen.allowTcapOps = wnew->screen.allowTcapOp0;
    wnew->screen.allowTitleOps = wnew->screen.allowTitleOp0;
    wnew->screen.allowWindowOps = wnew->screen.allowWindowOp0;

    init_Bres(screen.quiet_grab);

#ifndef NO_ACTIVE_ICON
    wnew->screen.fnt_icon.fs = request->screen.fnt_icon.fs;
    init_Bres(misc.active_icon);
    init_Ires(misc.icon_border_width);
    wnew->misc.icon_border_pixel = request->misc.icon_border_pixel;
#endif /* NO_ACTIVE_ICON */
    init_Bres(misc.titeInhibit);
    init_Bres(misc.tiXtraScroll);
    init_Bres(misc.dynamicColors);
    for (i = fontMenu_font1; i <= fontMenu_lastBuiltin; i++) {
	init_Sres2(screen.MenuFontName, i);
    }
    wnew->screen.MenuFontName(fontMenu_default) = wnew->misc.default_font.f_n;
    wnew->screen.MenuFontName(fontMenu_fontescape) = NULL;
    wnew->screen.MenuFontName(fontMenu_fontsel) = NULL;

    wnew->screen.menu_font_number = fontMenu_default;
    init_Sres(screen.initial_font);
    if (wnew->screen.initial_font != 0) {
	int result = xtermGetFont(wnew->screen.initial_font);
	if (result >= 0)
	    wnew->screen.menu_font_number = result;
    }
#if OPT_BROKEN_OSC
    init_Bres(screen.brokenLinuxOSC);
#endif

#if OPT_BROKEN_ST
    init_Bres(screen.brokenStringTerm);
#endif

#if OPT_C1_PRINT
    init_Bres(screen.c1_printable);
#endif

#if OPT_CLIP_BOLD
    init_Bres(screen.use_clipping);
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

#if OPT_WIDE_CHARS
    wnew->num_ptrs = (OFF_CHARS + 1);	/* minimum needed for cell */
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
	if (DftFg(Acolors[i])) {
	    wnew->screen.Acolors[i].value = T_COLOR(&(wnew->screen), TEXT_FG);
	    wnew->screen.Acolors[i].mode = True;
	} else if (DftBg(Acolors[i])) {
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
    wnew->sgr_foreground = -1;
    wnew->sgr_background = -1;
    wnew->sgr_extended = False;
#endif /* OPT_ISO_COLORS */

    /*
     * Decode the resources that control the behavior on multiple mouse clicks.
     * A single click is always bound to normal character selection, but the
     * other flavors can be changed.
     */
    for (i = 0; i < NSELECTUNITS; ++i) {
	int ck = (i + 1);
	wnew->screen.maxClicks = ck;
	if (i == Select_CHAR)
	    wnew->screen.selectMap[i] = Select_CHAR;
	else if (request->screen.onClick[i] != 0)
	    ParseOnClicks(wnew, request, (unsigned) i);
	else if (i <= Select_LINE)
	    wnew->screen.selectMap[i] = (SelectUnit) i;
	else
	    break;
	TRACE(("on%dClicks %s=%d\n", ck,
	       NonNull(request->screen.onClick[i]),
	       wnew->screen.selectMap[i]));
	if (wnew->screen.selectMap[i] == NSELECTUNITS)
	    break;
    }
    TRACE(("maxClicks %d\n", wnew->screen.maxClicks));

    init_Tres(MOUSE_FG);
    init_Tres(MOUSE_BG);
    init_Tres(TEXT_CURSOR);
#if OPT_HIGHLIGHT_COLOR
    init_Tres(HIGHLIGHT_BG);
    init_Tres(HIGHLIGHT_FG);
    init_Bres(screen.hilite_reverse);
    init_Bres(screen.hilite_color);
    if (wnew->screen.hilite_color == Maybe) {
	wnew->screen.hilite_color = False;
#if OPT_COLOR_RES
	/*
	 * If the highlight text/background are both set, and if they are
	 * not equal to either the text/background or background/text, then
	 * set the highlightColorMode automatically.
	 */
	if (!DftFg(Tcolors[HIGHLIGHT_BG])
	    && !DftBg(Tcolors[HIGHLIGHT_FG])
	    && !TxtFg(Tcolors[HIGHLIGHT_BG])
	    && !TxtBg(Tcolors[HIGHLIGHT_FG])
	    && !TxtBg(Tcolors[HIGHLIGHT_BG])
	    && !TxtFg(Tcolors[HIGHLIGHT_FG])) {
	    TRACE(("...setting hilite_color automatically\n"));
	    wnew->screen.hilite_color = True;
	}
#endif
    }
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

#if OPT_WIDE_CHARS
    VTInitialize_locale(request);
    init_Bres(screen.utf8_latin1);
    init_Bres(screen.utf8_title);

#if OPT_LUIT_PROG
    init_Bres(misc.callfilter);
    init_Bres(misc.use_encoding);
    init_Sres(misc.locale_str);
    init_Sres(misc.localefilter);
#endif

#if OPT_RENDERFONT
    for (i = 0; i <= fontMenu_lastBuiltin; ++i) {
	init_Dres2(misc.face_size, i);
    }
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

    init_Ires(screen.utf8_inparse);
    init_Ires(screen.utf8_mode);
    init_Ires(screen.max_combining);

    if (wnew->screen.max_combining < 0) {
	wnew->screen.max_combining = 0;
    }
    if (wnew->screen.max_combining > 5) {
	wnew->screen.max_combining = 5;
    }

    init_Bres(screen.vt100_graphics);
    init_Bres(screen.wide_chars);
    init_Bres(misc.mk_width);
    init_Bres(misc.cjk_width);

    init_Ires(misc.mk_samplesize);
    init_Ires(misc.mk_samplepass);

    if (wnew->misc.mk_samplesize > 0xffff)
	wnew->misc.mk_samplesize = 0xffff;
    if (wnew->misc.mk_samplesize < 0)
	wnew->misc.mk_samplesize = 0;

    if (wnew->misc.mk_samplepass > wnew->misc.mk_samplesize)
	wnew->misc.mk_samplepass = wnew->misc.mk_samplesize;
    if (wnew->misc.mk_samplepass < 0)
	wnew->misc.mk_samplepass = 0;

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
	wnew->num_ptrs = OFF_FINAL + (wnew->screen.max_combining * 2);

    decode_wcwidth((wnew->misc.cjk_width ? 2 : 0)
		   + (wnew->misc.mk_width ? 1 : 0)
		   + 1,
		   wnew->misc.mk_samplesize,
		   wnew->misc.mk_samplepass);
#endif /* OPT_WIDE_CHARS */

    init_Bres(screen.always_bold_mode);
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
    if (resource.zIconBeep)
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
	char value[80];
	char *temp[2];
	Cardinal nparams = 1;

	sprintf(temp[0] = value, "%d", wnew->misc.resizeGravity);
	temp[1] = 0;
	XtAppWarningMsg(app_con, "rangeError", "resizeGravity", "XTermError",
			"unsupported resizeGravity resource value (%s)",
			temp, &nparams);
	wnew->misc.resizeGravity = SouthWestGravity;
    }
#ifndef NO_ACTIVE_ICON
    wnew->screen.whichVwin = &wnew->screen.fullVwin;
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

#if OPT_MOD_FKEYS
    init_Ires(keyboard.modify_1st.cursor_keys);
    init_Ires(keyboard.modify_1st.function_keys);
    init_Ires(keyboard.modify_1st.keypad_keys);
    init_Ires(keyboard.modify_1st.other_keys);
    init_Ires(keyboard.modify_1st.string_keys);
    init_Ires(keyboard.format_keys);
    wnew->keyboard.modify_now = wnew->keyboard.modify_1st;
#endif

    init_Ires(misc.appcursorDefault);
    if (wnew->misc.appcursorDefault)
	wnew->keyboard.flags |= MODE_DECCKM;

    init_Ires(misc.appkeypadDefault);
    if (wnew->misc.appkeypadDefault)
	wnew->keyboard.flags |= MODE_DECKPAM;

    return;
}

void
releaseCursorGCs(XtermWidget xw)
{
    TScreen *screen = &xw->screen;
    VTwin *win = WhichVWin(screen);
    int n;

    for_each_curs_gc(n) {
	freeCgs(xw, win, (CgsEnum) n);
    }
}

void
releaseWindowGCs(XtermWidget xw, VTwin * win)
{
    int n;

    for_each_text_gc(n) {
	freeCgs(xw, win, (CgsEnum) n);
    }
}

#define TRACE_FREE_LEAK(name) \
	if (name) { \
	    free(name); \
	    name = 0; \
	    TRACE(("freed " #name "\n")); \
	}

#define FREE_LEAK(name) \
	if (name) { \
	    free(name); \
	    name = 0; \
	}

#ifdef NO_LEAKS
#if OPT_RENDERFONT
static void
xtermCloseXft(TScreen * screen, XftFont ** pub)
{
    if (*pub != 0) {
	XftFontClose(screen->display, *pub);
	*pub = 0;
    }
}
#endif
#endif

static void
VTDestroy(Widget w GCC_UNUSED)
{
#ifdef NO_LEAKS
    XtermWidget xw = (XtermWidget) w;
    TScreen *screen = &xw->screen;
    Cardinal n;

    StopBlinking(screen);

    if (screen->scrollWidget) {
	XtUninstallTranslations(screen->scrollWidget);
	XtDestroyWidget(screen->scrollWidget);
    }

    TRACE_FREE_LEAK(screen->save_ptr);
    TRACE_FREE_LEAK(screen->sbuf_address);
    TRACE_FREE_LEAK(screen->allbuf);
    TRACE_FREE_LEAK(screen->abuf_address);
    TRACE_FREE_LEAK(screen->altbuf);
    TRACE_FREE_LEAK(screen->keyboard_dialect);
    TRACE_FREE_LEAK(screen->term_id);
#if OPT_WIDE_CHARS
    TRACE_FREE_LEAK(screen->draw_buf);
#if OPT_LUIT_PROG
    TRACE_FREE_LEAK(xw->misc.locale_str);
    TRACE_FREE_LEAK(xw->misc.localefilter);
#endif
#endif
#if OPT_INPUT_METHOD
    if (screen->xim) {
	XCloseIM(screen->xim);
	TRACE(("freed screen->xim\n"));
    }
#endif
    releaseCursorGCs(xw);
    releaseWindowGCs(xw, &(screen->fullVwin));
#ifndef NO_ACTIVE_ICON
    releaseWindowGCs(xw, &(screen->iconVwin));
#endif
    XtUninstallTranslations((Widget) xw);
#if OPT_TOOLBAR
    XtUninstallTranslations((Widget) XtParent(xw));
#endif
    XtUninstallTranslations((Widget) SHELL_OF(xw));

    if (screen->hidden_cursor)
	XFreeCursor(screen->display, screen->hidden_cursor);

    xtermCloseFonts(xw, screen->fnts);
    noleaks_cachedCgs(xw);

#if OPT_RENDERFONT
    for (n = 0; n < NMENUFONTS; ++n) {
	xtermCloseXft(screen, &(screen->renderFontNorm[n]));
	xtermCloseXft(screen, &(screen->renderFontBold[n]));
	xtermCloseXft(screen, &(screen->renderFontItal[n]));
	xtermCloseXft(screen, &(screen->renderWideNorm[n]));
	xtermCloseXft(screen, &(screen->renderWideBold[n]));
	xtermCloseXft(screen, &(screen->renderWideItal[n]));
    }
#endif

#if 0				/* some strings may be owned by X libraries */
    for (n = 0; n <= fontMenu_lastBuiltin; ++n) {
	int k;
	for (k = 0; k < fMAX; ++k) {
	    char *s = screen->menu_font_names[n][k];
	    if (s != 0)
		free(s);
	}
    }
#endif

#if OPT_COLOR_RES
    /* free local copies of resource strings */
    for (n = 0; n < NCOLORS; ++n) {
	FREE_LEAK(screen->Tcolors[n].resource);
    }
#endif
#if OPT_SELECT_REGEX
    for (n = 0; n < NSELECTUNITS; ++n) {
	FREE_LEAK(screen->selectExpr[n]);
    }
#endif

    if (screen->selection_atoms)
	XtFree((char *) (screen->selection_atoms));

    XtFree((char *) (screen->selection_data));

    TRACE_FREE_LEAK(xw->keyboard.extra_translations);
    TRACE_FREE_LEAK(xw->keyboard.shell_translations);
    TRACE_FREE_LEAK(xw->keyboard.xterm_translations);
#endif /* defined(NO_LEAKS) */
}

/*ARGSUSED*/
static void
VTRealize(Widget w,
	  XtValueMask * valuemask,
	  XSetWindowAttributes * values)
{
    XtermWidget xw = (XtermWidget) w;
    TScreen *screen = &xw->screen;

    const VTFontNames *myfont;
    unsigned width, height;
    int xpos, ypos, pr;
    Atom pid_atom;
    int i;

    TRACE(("VTRealize\n"));

    TabReset(xw->tabs);

    if (screen->menu_font_number == fontMenu_default) {
	myfont = &(xw->misc.default_font);
    } else {
	myfont = xtermFontName(screen->MenuFontName(screen->menu_font_number));
    }
    memset(screen->fnts, 0, sizeof(screen->fnts));

    if (!xtermLoadFont(xw,
		       myfont,
		       False,
		       screen->menu_font_number)) {
	if (XmuCompareISOLatin1(myfont->f_n, DEFFONT) != 0) {
	    fprintf(stderr,
		    "%s:  unable to open font \"%s\", trying \"%s\"....\n",
		    xterm_name, myfont->f_n, DEFFONT);
	    (void) xtermLoadFont(xw,
				 xtermFontName(DEFFONT),
				 False,
				 screen->menu_font_number);
	    screen->MenuFontName(screen->menu_font_number) = DEFFONT;
	}
    }

    /* really screwed if we couldn't open default font */
    if (!screen->fnts[fNorm].fs) {
	fprintf(stderr, "%s:  unable to locate a suitable font\n",
		xterm_name);
	Exit(1);
    }
#if OPT_WIDE_CHARS
    if (xw->screen.utf8_mode) {
	TRACE(("check if this is a wide font, if not try again\n"));
	if (xtermLoadWideFonts(xw, False))
	    SetVTFont(xw, screen->menu_font_number, True, NULL);
    }
#endif

    /* making cursor */
    if (!screen->pointer_cursor) {
	screen->pointer_cursor =
	    make_colored_cursor(XC_xterm,
				T_COLOR(screen, MOUSE_FG),
				T_COLOR(screen, MOUSE_BG));
    } else {
	recolor_cursor(screen,
		       screen->pointer_cursor,
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
	xpos += (DisplayWidth(screen->display, DefaultScreen(screen->display))
		 - (int) width
		 - (BorderWidth(XtParent(xw)) * 2));
    }
    if ((pr & YValue) && (YNegative & pr)) {
	ypos += (DisplayHeight(screen->display, DefaultScreen(screen->display))
		 - (int) height
		 - (BorderWidth(XtParent(xw)) * 2));
    }

    /* set up size hints for window manager; min 1 char by 1 char */
    getXtermSizeHints(xw);
    xtermSizeHints(xw, (xw->misc.scrollbar
			? (screen->scrollWidget->core.width
			   + BorderWidth(screen->scrollWidget))
			: 0));

    xw->hints.x = xpos;
    xw->hints.y = ypos;
    if ((XValue & pr) || (YValue & pr)) {
	xw->hints.flags |= USSize | USPosition;
	xw->hints.flags |= PWinGravity;
	switch (pr & (XNegative | YNegative)) {
	case 0:
	    xw->hints.win_gravity = NorthWestGravity;
	    break;
	case XNegative:
	    xw->hints.win_gravity = NorthEastGravity;
	    break;
	case YNegative:
	    xw->hints.win_gravity = SouthWestGravity;
	    break;
	default:
	    xw->hints.win_gravity = SouthEastGravity;
	    break;
	}
    } else {
	/* set a default size, but do *not* set position */
	xw->hints.flags |= PSize;
    }
    xw->hints.height = xw->hints.base_height
	+ xw->hints.height_inc * MaxRows(screen);
    xw->hints.width = xw->hints.base_width
	+ xw->hints.width_inc * MaxCols(screen);

    if ((WidthValue & pr) || (HeightValue & pr))
	xw->hints.flags |= USSize;
    else
	xw->hints.flags |= PSize;

    /*
     * Note that the size-hints are for the shell, while the resize-request
     * is for the vt100 widget.  They are not the same size.
     */
    (void) REQ_RESIZE((Widget) xw,
		      (Dimension) width, (Dimension) height,
		      &xw->core.width, &xw->core.height);

    /* XXX This is bogus.  We are parsing geometries too late.  This
     * is information that the shell widget ought to have before we get
     * realized, so that it can do the right thing.
     */
    if (xw->hints.flags & USPosition)
	XMoveWindow(XtDisplay(xw), XtWindow(SHELL_OF(xw)),
		    xw->hints.x, xw->hints.y);

    TRACE(("%s@%d -- ", __FILE__, __LINE__));
    TRACE_HINTS(&xw->hints);
    XSetWMNormalHints(XtDisplay(xw), XtWindow(SHELL_OF(xw)), &xw->hints);
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
    if (xw->misc.active_icon && screen->fnt_icon.fs) {
	int iconX = 0, iconY = 0;
	Widget shell = SHELL_OF(xw);
	VTwin *win = &(screen->iconVwin);

	TRACE(("Initializing active-icon\n"));
	XtVaGetValues(shell, XtNiconX, &iconX, XtNiconY, &iconY, (XtPointer) 0);
	xtermComputeFontInfo(xw, &(screen->iconVwin), screen->fnt_icon.fs, 0);

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

	setCgsFont(xw, win, gcNorm, &(screen->fnt_icon));
	setCgsFore(xw, win, gcNorm, T_COLOR(screen, TEXT_FG));
	setCgsBack(xw, win, gcNorm, T_COLOR(screen, TEXT_BG));

	copyCgs(xw, win, gcBold, gcNorm);

	setCgsFont(xw, win, gcNormReverse, &(screen->fnt_icon));
	setCgsFore(xw, win, gcNormReverse, T_COLOR(screen, TEXT_BG));
	setCgsBack(xw, win, gcNormReverse, T_COLOR(screen, TEXT_FG));

	copyCgs(xw, win, gcBoldReverse, gcNormReverse);

#if OPT_TOOLBAR
	/*
	 * Toolbar is initialized before we get here.  Enable the menu item
	 * and set it properly.
	 */
	SetItemSensitivity(vtMenuEntries[vtMenu_activeicon].widget, True);
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
    VTInitModifiers(xw);
#if OPT_EXTRA_PASTE
    if (xw->keyboard.extra_translations) {
	XtOverrideTranslations((Widget) xw,
			       XtParseTranslationTable(xw->keyboard.extra_translations));
    }
#endif
#endif

    set_cursor_gcs(xw);

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

    screen->do_wrap = False;
    screen->scrolls = screen->incopy = 0;
    xtermSetCursorBox(screen);

    screen->savedlines = 0;

    for (i = 0; i < 2; ++i) {
	screen->alternate = (Boolean) (!screen->alternate);
	CursorSave(xw);
    }

    /*
     * Do this last, since it may change the layout via a resize.
     */
    if (xw->misc.scrollbar) {
	screen->fullVwin.sb_info.width = 0;
	ScrollBarOn(xw, False, True);
    }
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
	j = (unsigned) XFontsOfFontSet(term->screen.fs, &fonts, &font_name_list);
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
    Boolean refresh_needed = False;
    Boolean fonts_redone = False;

    if ((T_COLOR(&(curvt->screen), TEXT_BG) !=
	 T_COLOR(&(newvt->screen), TEXT_BG)) ||
	(T_COLOR(&(curvt->screen), TEXT_FG) !=
	 T_COLOR(&(newvt->screen), TEXT_FG)) ||
	(curvt->screen.MenuFontName(curvt->screen.menu_font_number) !=
	 newvt->screen.MenuFontName(newvt->screen.menu_font_number)) ||
	(curvt->misc.default_font.f_n != newvt->misc.default_font.f_n)) {
	if (curvt->misc.default_font.f_n != newvt->misc.default_font.f_n)
	    newvt->screen.MenuFontName(fontMenu_default) = newvt->misc.default_font.f_n;
	if (xtermLoadFont(newvt,
			  xtermFontName(newvt->screen.MenuFontName(curvt->screen.menu_font_number)),
			  True, newvt->screen.menu_font_number)) {
	    /* resizing does the redisplay, so don't ask for it here */
	    refresh_needed = True;
	    fonts_redone = True;
	} else if (curvt->misc.default_font.f_n != newvt->misc.default_font.f_n)
	    newvt->screen.MenuFontName(fontMenu_default) = curvt->misc.default_font.f_n;
    }
    if (!fonts_redone
	&& (T_COLOR(&(curvt->screen), TEXT_CURSOR) !=
	    T_COLOR(&(newvt->screen), TEXT_CURSOR))) {
	set_cursor_gcs(newvt);
	refresh_needed = True;
    }
    if (curvt->misc.re_verse != newvt->misc.re_verse) {
	newvt->flags ^= REVERSE_VIDEO;
	ReverseVideo(newvt);
	/* ReverseVideo toggles */
	newvt->misc.re_verse = (Boolean) (!newvt->misc.re_verse);
	refresh_needed = True;
    }
    if ((T_COLOR(&(curvt->screen), MOUSE_FG) !=
	 T_COLOR(&(newvt->screen), MOUSE_FG)) ||
	(T_COLOR(&(curvt->screen), MOUSE_BG) !=
	 T_COLOR(&(newvt->screen), MOUSE_BG))) {
	recolor_cursor(&(newvt->screen),
		       newvt->screen.pointer_cursor,
		       T_COLOR(&(newvt->screen), MOUSE_FG),
		       T_COLOR(&(newvt->screen), MOUSE_BG));
	refresh_needed = True;
    }
    if (curvt->misc.scrollbar != newvt->misc.scrollbar) {
	ToggleScrollBar(newvt);
    }

    return refresh_needed;
}

#define setGC(code) set_at = __LINE__, currentCgs = code

#define OutsideSelection(screen,srow,scol)  \
	 ((srow) > (screen)->endH.row || \
	  ((srow) == (screen)->endH.row && \
	   (scol) >= (screen)->endH.col) || \
	  (srow) < (screen)->startH.row || \
	  ((srow) == (screen)->startH.row && \
	   (scol) < (screen)->startH.col))

/*
 * Shows cursor at new cursor position in screen.
 */
void
ShowCursor(void)
{
    XtermWidget xw = term;
    TScreen *screen = &xw->screen;
    int x, y;
    Char clo;
    unsigned flags;
    unsigned fg_bg = 0;
    GC currentGC;
    CgsEnum currentCgs = gcMAX;
    VTwin *currentWin = WhichVWin(screen);
    int set_at;
    Bool in_selection;
    Bool reversed;
    Bool filled;
    Pixel fg_pix;
    Pixel bg_pix;
    Pixel tmp;
#if OPT_HIGHLIGHT_COLOR
    Pixel selbg_pix = T_COLOR(screen, HIGHLIGHT_BG);
    Pixel selfg_pix = T_COLOR(screen, HIGHLIGHT_FG);
    Boolean use_selbg;
    Boolean use_selfg;
#endif
#if OPT_WIDE_CHARS
    Char chi = 0;
    int base;
    int off;
    int my_col = 0;
#endif
    int cursor_col;

    if (screen->cursor_state == BLINKED_OFF)
	return;

    if (screen->eventMode != NORMAL)
	return;

    if (INX2ROW(screen, screen->cur_row) > screen->max_row)
	return;

    screen->cursorp.row = screen->cur_row;
    cursor_col = screen->cursorp.col = screen->cur_col;
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
	clo = SCRN_BUF_CHARS(screen, screen->cursorp.row)[cursor_col];

    if_OPT_WIDE_CHARS(screen, {
	chi = SCRN_BUF_WIDEC(screen, screen->cursorp.row)[cursor_col];
	if (clo == HIDDEN_LO && chi == HIDDEN_HI && cursor_col > 0) {
	    /* if cursor points to non-initial part of wide character,
	     * back it up
	     */
	    --cursor_col;
	    clo = SCRN_BUF_CHARS(screen, screen->cursorp.row)[cursor_col];
	    chi = SCRN_BUF_WIDEC(screen, screen->cursorp.row)[cursor_col];
	}
	my_col = cursor_col;
	base = (chi << 8) | clo;
	if (iswide(base))
	    my_col += 1;
    });

    flags = SCRN_BUF_ATTRS(screen, screen->cursorp.row)[cursor_col];

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
    if ((flags & TERM_COLOR_FLAGS(xw)) == BG_COLOR
#if OPT_WIDE_CHARS
	&& chi == 0
#endif
	&& clo == ' ') {
	flags &= ~TERM_COLOR_FLAGS(xw);
    }
#endif

    /*
     * Compare the current cell to the last set of colors used for the
     * cursor and update the GC's if needed.
     */
    (void) fg_bg;
    if_OPT_EXT_COLORS(screen, {
	fg_bg = PACK_FGBG(screen, screen->cursorp.row, cursor_col);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	fg_bg = SCRN_BUF_COLOR(screen, screen->cursorp.row)[cursor_col];
    });
    fg_pix = getXtermForeground(xw, flags, extract_fg(xw, fg_bg, flags));
    bg_pix = getXtermBackground(xw, flags, extract_bg(xw, fg_bg, flags));

    if (OutsideSelection(screen, screen->cur_row, screen->cur_col))
	in_selection = False;
    else
	in_selection = True;

    reversed = ReverseOrHilite(screen, flags, in_selection);

    /* This is like updatedXtermGC(), except that we have to worry about
     * whether the window has focus, since in that case we want just an
     * outline for the cursor.
     */
    filled = (screen->select || screen->always_highlight) && !screen->cursor_underline;
#if OPT_HIGHLIGHT_COLOR
    use_selbg = isNotForeground(xw, fg_pix, bg_pix, selbg_pix);
    use_selfg = isNotBackground(xw, fg_pix, bg_pix, selfg_pix);
#endif
    if (filled) {
	if (reversed) {		/* text is reverse video */
	    if (getCgsGC(xw, currentWin, gcVTcursNormal)) {
		setGC(gcVTcursNormal);
	    } else {
		if (flags & BOLDATTR(screen)) {
		    setGC(gcBold);
		} else {
		    setGC(gcNorm);
		}
	    }
	    EXCHANGE(fg_pix, bg_pix, tmp);
#if OPT_HIGHLIGHT_COLOR
	    if (screen->hilite_reverse) {
		if (use_selbg && !use_selfg)
		    fg_pix = bg_pix;
		if (use_selfg && !use_selbg)
		    bg_pix = fg_pix;
		if (use_selbg)
		    bg_pix = selbg_pix;
		if (use_selfg)
		    fg_pix = selfg_pix;
	    }
#endif
	} else {		/* normal video */
	    if (getCgsGC(xw, currentWin, gcVTcursReverse)) {
		setGC(gcVTcursReverse);
	    } else {
		if (flags & BOLDATTR(screen)) {
		    setGC(gcBoldReverse);
		} else {
		    setGC(gcNormReverse);
		}
	    }
	}
	if (T_COLOR(screen, TEXT_CURSOR) == xw->dft_foreground) {
	    setCgsBack(xw, currentWin, currentCgs, fg_pix);
	}
	setCgsFore(xw, currentWin, currentCgs, bg_pix);
    } else {			/* not selected */
	if (reversed) {		/* text is reverse video */
	    EXCHANGE(fg_pix, bg_pix, tmp);
	    setGC(gcNormReverse);
	} else {		/* normal video */
	    setGC(gcNorm);
	}
#if OPT_HIGHLIGHT_COLOR
	if (screen->hilite_reverse) {
	    if (in_selection && !reversed) {
		;		/* really INVERSE ... */
	    } else if (in_selection || reversed) {
		if (use_selbg) {
		    if (use_selfg) {
			bg_pix = fg_pix;
		    } else {
			fg_pix = bg_pix;
		    }
		}
		if (use_selbg) {
		    bg_pix = selbg_pix;
		}
		if (use_selfg) {
		    fg_pix = selfg_pix;
		}
	    }
	} else {
	    if (in_selection) {
		if (use_selbg) {
		    bg_pix = selbg_pix;
		}
		if (use_selfg) {
		    fg_pix = selfg_pix;
		}
	    }
	}
#endif
	setCgsFore(xw, currentWin, currentCgs, fg_pix);
	setCgsBack(xw, currentWin, currentCgs, bg_pix);
    }

    if (screen->cursor_busy == 0
	&& (screen->cursor_state != ON || screen->cursor_GC != set_at)) {

	screen->cursor_GC = set_at;
	TRACE(("ShowCursor calling drawXtermText cur(%d,%d) %s\n",
	       screen->cur_row, screen->cur_col,
	       (filled ? "filled" : "outline")));

	currentGC = getCgsGC(xw, currentWin, currentCgs);
	drawXtermText(xw, flags & DRAWX_MASK, currentGC,
		      x = CurCursorX(screen, screen->cur_row, cursor_col),
		      y = CursorY(screen, screen->cur_row),
		      curXtermChrSet(xw, screen->cur_row),
		      PAIRED_CHARS(&clo, &chi), 1, 0);

#if OPT_WIDE_CHARS
	if_OPT_WIDE_CHARS(screen, {
	    for (off = OFF_FINAL; off < MAX_PTRS; off += 2) {
		clo = SCREEN_PTR(screen, screen->cursorp.row, off + 0)[my_col];
		chi = SCREEN_PTR(screen, screen->cursorp.row, off + 1)[my_col];
		if (!(clo || chi))
		    break;
		drawXtermText(xw, (flags & DRAWX_MASK) | NOBACKGROUND,
			      currentGC, x, y,
			      curXtermChrSet(xw, screen->cur_row),
			      PAIRED_CHARS(&clo, &chi), 1, iswide(base));
	    }
	});
#endif

	if (!filled) {
	    GC outlineGC = getCgsGC(xw, currentWin, gcVTcursOutline);
	    if (outlineGC == 0)
		outlineGC = currentGC;

	    screen->box->x = (short) x;
	    if (!screen->cursor_underline)
		screen->box->y = (short) y;
	    else
		screen->box->y = (short) (y + FontHeight(screen) - 2);
	    XDrawLines(screen->display, VWindow(screen), outlineGC,
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
    XtermWidget xw = term;
    TScreen *screen = &xw->screen;
    GC currentGC;
    unsigned flags;
    unsigned fg_bg = 0;
    int x, y;
    Char clo;
    Bool in_selection;
#if OPT_WIDE_CHARS
    Char chi = 0;
    int base;
    int off;
    int my_col = 0;
#endif
    int cursor_col;

    if (screen->cursor_state == OFF)	/* FIXME */
	return;
    if (INX2ROW(screen, screen->cursorp.row) > screen->max_row)
	return;

    cursor_col = screen->cursorp.col;

#ifndef NO_ACTIVE_ICON
    if (IsIcon(screen)) {
	screen->cursor_state = OFF;
	return;
    }
#endif /* NO_ACTIVE_ICON */

#if OPT_WIDE_CHARS
    base =
#endif
	clo = SCRN_BUF_CHARS(screen, screen->cursorp.row)[cursor_col];
    flags = SCRN_BUF_ATTRS(screen, screen->cursorp.row)[cursor_col];

    if_OPT_WIDE_CHARS(screen, {
	chi = SCRN_BUF_WIDEC(screen, screen->cursorp.row)[cursor_col];
	if (clo == HIDDEN_LO && chi == HIDDEN_HI) {
	    /* if cursor points to non-initial part of wide character,
	     * back it up
	     */
	    --cursor_col;
	    clo = SCRN_BUF_CHARS(screen, screen->cursorp.row)[cursor_col];
	    chi = SCRN_BUF_WIDEC(screen, screen->cursorp.row)[cursor_col];
	}
	my_col = cursor_col;
	base = (chi << 8) | clo;
	if (iswide(base))
	    my_col += 1;
    });

    if_OPT_EXT_COLORS(screen, {
	fg_bg = PACK_FGBG(screen, screen->cursorp.row, cursor_col);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	fg_bg = SCRN_BUF_COLOR(screen, screen->cursorp.row)[cursor_col];
    });

    if (OutsideSelection(screen, screen->cursorp.row, screen->cursorp.col))
	in_selection = False;
    else
	in_selection = True;

    currentGC = updatedXtermGC(xw, flags, fg_bg, in_selection);

    if (clo == 0
#if OPT_WIDE_CHARS
	&& chi == 0
#endif
	) {
	clo = ' ';
    }

    TRACE(("HideCursor calling drawXtermText cur(%d,%d)\n",
	   screen->cursorp.row, screen->cursorp.col));
    drawXtermText(xw, flags & DRAWX_MASK, currentGC,
		  x = CurCursorX(screen, screen->cursorp.row, cursor_col),
		  y = CursorY(screen, screen->cursorp.row),
		  curXtermChrSet(xw, screen->cursorp.row),
		  PAIRED_CHARS(&clo, &chi), 1, 0);

#if OPT_WIDE_CHARS
    if_OPT_WIDE_CHARS(screen, {
	for (off = OFF_FINAL; off < MAX_PTRS; off += 2) {
	    clo = SCREEN_PTR(screen, screen->cursorp.row, off + 0)[my_col];
	    chi = SCREEN_PTR(screen, screen->cursorp.row, off + 1)[my_col];
	    if (!(clo || chi))
		break;
	    drawXtermText(xw, (flags & DRAWX_MASK) | NOBACKGROUND,
			  currentGC, x, y,
			  curXtermChrSet(xw, screen->cur_row),
			  PAIRED_CHARS(&clo, &chi), 1, iswide(base));
	}
    });
#endif
    screen->cursor_state = OFF;
    resetXtermGC(xw, flags, in_selection);
}

#if OPT_BLINK_CURS || OPT_BLINK_TEXT
static void
StartBlinking(TScreen * screen)
{
    if (screen->blink_timer == 0) {
	unsigned long interval = (unsigned long) ((screen->cursor_state == ON)
						  ? screen->blink_on
						  : screen->blink_off);
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
	    ScrnRefresh(term,
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
VTReset(XtermWidget xw, Bool full, Bool saved)
{
    TScreen *screen = &xw->screen;

    if (!XtIsRealized((Widget) xw) || (CURRENT_EMU() != (Widget) xw)) {
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

    bitclr(&xw->flags, ORIGIN);

    if_OPT_ISO_COLORS(screen, {
	reset_SGR_Colors(xw);
    });

    /* Reset character-sets to initial state */
    resetCharsets(screen);

#if OPT_MOD_FKEYS
    /* Reset modifier-resources to initial state */
    xw->keyboard.modify_now = xw->keyboard.modify_1st;
#endif

    /* Reset DECSCA */
    bitclr(&xw->flags, PROTECTED);
    screen->protected_mode = OFF_PROTECT;

    if (full) {			/* RIS */
	if (screen->bellOnReset)
	    Bell(XkbBI_TerminalBell, 0);

	/* reset the mouse mode */
	screen->send_mouse_pos = MOUSE_OFF;
	screen->send_focus_pos = OFF;
	screen->waitingForTrackInfo = False;
	screen->eventMode = NORMAL;

	xtermShowPointer(xw, True);

	TabReset(xw->tabs);
	xw->keyboard.flags = MODE_SRM;
#if OPT_INITIAL_ERASE
	if (xw->keyboard.reset_DECBKM == 1)
	    xw->keyboard.flags |= MODE_DECBKM;
	else if (xw->keyboard.reset_DECBKM == 2)
#endif
	    if (xw->screen.backarrow_key)
		xw->keyboard.flags |= MODE_DECBKM;
	TRACE(("full reset DECBKM %s\n",
	       BtoS(xw->keyboard.flags & MODE_DECBKM)));
	update_appcursor();
	update_appkeypad();
	update_decbkm();
	show_8bit_control(False);
	reset_decudk();

	FromAlternate(xw);
	ClearScreen(xw);
	screen->cursor_state = OFF;
	if (xw->flags & REVERSE_VIDEO)
	    ReverseVideo(xw);

	xw->flags = xw->initflags;
	update_reversevideo();
	update_autowrap();
	update_reversewrap();
	update_autolinefeed();

	screen->jumpscroll = (Boolean) (!(xw->flags & SMOOTHSCROLL));
	update_jumpscroll();

	if (screen->c132 && (xw->flags & IN132COLUMNS)) {
	    Dimension reqWidth = (Dimension) (80 * FontWidth(screen)
					      + 2 * screen->border
					      + ScrollbarWidth(screen));
	    Dimension reqHeight = (Dimension) (FontHeight(screen)
					       * MaxRows(screen)
					       + 2 * screen->border);
	    Dimension replyWidth;
	    Dimension replyHeight;

	    TRACE(("Making resize-request to restore 80-columns %dx%d\n",
		   reqHeight, reqWidth));
	    REQ_RESIZE((Widget) xw,
		       reqWidth,
		       reqHeight,
		       &replyWidth, &replyHeight);
	    repairSizeHints();
	    XSync(screen->display, False);	/* synchronize */
	    if (XtAppPending(app_con))
		xevents();
	}

	CursorSet(screen, 0, 0, xw->flags);
	CursorSave(xw);
    } else {			/* DECSTR */
	/*
	 * There's a tiny difference, to accommodate usage of xterm.
	 * We reset autowrap to the resource values rather than turning
	 * it off.
	 */
	xw->keyboard.flags &= ~(MODE_DECCKM | MODE_KAM | MODE_DECKPAM);
	bitcpy(&xw->flags, xw->initflags, WRAPAROUND | REVERSEWRAP);
	bitclr(&xw->flags, INSERT | INVERSE | BOLD | BLINK | UNDERLINE | INVISIBLE);
	if_OPT_ISO_COLORS(screen, {
	    reset_SGR_Colors(xw);
	});
	update_appcursor();
	update_autowrap();
	update_reversewrap();

	CursorSave(xw);
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

    for (i = 0, len = (int) strlen(s), acc = 0, numbers = digits = 0;
	 i < len; i++) {
	Char c = CharOf(s[i]);

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
	pmapClass[0] = x_toupper(pmapClass[0]);
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
    XtermWidget xw;

    TRACE(("Handle ignore for %p\n", w));
    if ((xw = getXtermWidget(w)) != 0) {
	/* do nothing, but check for funny escape sequences */
	(void) SendMousePosition(xw, event);
    }
}

/* ARGSUSED */
static void
DoSetSelectedFont(Widget w,
		  XtPointer client_data GCC_UNUSED,
		  Atom * selection GCC_UNUSED,
		  Atom * type,
		  XtPointer value,
		  unsigned long *length,
		  int *format)
{
    XtermWidget xw = getXtermWidget(w);

    if ((xw == 0) || *type != XA_STRING || *format != 8) {
	Bell(XkbBI_MinorError, 0);
    } else {
	Boolean failed = False;
	int oldFont = xw->screen.menu_font_number;
	char *save = xw->screen.MenuFontName(fontMenu_fontsel);
	char *val;
	char *test = 0;
	char *used = 0;
	unsigned len = *length;
	unsigned tst;

	/*
	 * Some versions of X deliver null-terminated selections, some do not.
	 */
	for (tst = 0; tst < len; ++tst) {
	    if (((char *) value)[tst] == '\0') {
		len = tst;
		break;
	    }
	}

	if (len > 0 && (val = TypeMallocN(char, len + 1)) != 0) {
	    memcpy(val, value, len);
	    val[len] = '\0';
	    used = x_strtrim(val);
	    TRACE(("DoSetSelectedFont(%s)\n", val));
	    /* Do some sanity checking to avoid sending a long selection
	       back to the server in an OpenFont that is unlikely to succeed.
	       XLFD allows up to 255 characters and no control characters;
	       we are a little more liberal here. */
	    if (len < 1000
		&& !strchr(val, '\n')
		&& (test = x_strdup(val)) != 0) {
		xw->screen.MenuFontName(fontMenu_fontsel) = test;
		if (!xtermLoadFont(term,
				   xtermFontName(val),
				   True,
				   fontMenu_fontsel)) {
		    failed = True;
		    free(test);
		    xw->screen.MenuFontName(fontMenu_fontsel) = save;
		}
	    } else {
		failed = True;
	    }
	    if (failed) {
		(void) xtermLoadFont(term,
				     xtermFontName(xw->screen.MenuFontName(oldFont)),
				     True,
				     oldFont);
		Bell(XkbBI_MinorError, 0);
	    }
	    if (used != val)
		free(used);
	    free(val);
	}
    }
}

void
FindFontSelection(XtermWidget xw, const char *atom_name, Bool justprobe)
{
    static AtomPtr *atoms;
    unsigned int atomCount = 0;
    AtomPtr *pAtom;
    unsigned a;
    Atom target;

    if (!atom_name)
	atom_name = (xw->screen.mappedSelect
		     ? xw->screen.mappedSelect[0]
		     : "PRIMARY");
    TRACE(("FindFontSelection(%s)\n", atom_name));

    for (pAtom = atoms, a = atomCount; a; a--, pAtom++) {
	if (strcmp(atom_name, XmuNameOfAtom(*pAtom)) == 0)
	    break;
    }
    if (!a) {
	atoms = (AtomPtr *) XtRealloc((char *) atoms,
				      sizeof(AtomPtr) * (atomCount + 1));
	*(pAtom = &atoms[atomCount++]) = XmuMakeAtom(atom_name);
    }

    target = XmuInternAtom(XtDisplay(xw), *pAtom);
    if (justprobe) {
	xw->screen.MenuFontName(fontMenu_fontsel) =
	    XGetSelectionOwner(XtDisplay(xw), target) ? _Font_Selected_ : 0;
    } else {
	XtGetSelectionValue((Widget) xw, target, XA_STRING,
			    DoSetSelectedFont, NULL,
			    XtLastTimestampProcessed(XtDisplay(xw)));
    }
    return;
}

void
set_cursor_gcs(XtermWidget xw)
{
    TScreen *screen = &(xw->screen);
    VTwin *win = WhichVWin(screen);

    Pixel cc = T_COLOR(screen, TEXT_CURSOR);
    Pixel fg = T_COLOR(screen, TEXT_FG);
    Pixel bg = T_COLOR(screen, TEXT_BG);
    Boolean changed = False;

    /*
     * Let's see, there are three things that have "color":
     *
     *     background
     *     text
     *     cursorblock
     *
     * And, there are four situations when drawing a cursor, if we decide
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

    TRACE(("set_cursor_gcs cc=%#lx, fg=%#lx, bg=%#lx\n", cc, fg, bg));
    if (win != 0 && (cc != bg)) {
	/* set the fonts to the current one */
	setCgsFont(xw, win, gcVTcursNormal, 0);
	setCgsFont(xw, win, gcVTcursFilled, 0);
	setCgsFont(xw, win, gcVTcursReverse, 0);
	setCgsFont(xw, win, gcVTcursOutline, 0);

	/* we have a colored cursor */
	setCgsFore(xw, win, gcVTcursNormal, fg);
	setCgsBack(xw, win, gcVTcursNormal, cc);

	setCgsFore(xw, win, gcVTcursFilled, cc);
	setCgsBack(xw, win, gcVTcursFilled, fg);

	if (screen->always_highlight) {
	    /* both GC's use the same color */
	    setCgsFore(xw, win, gcVTcursReverse, bg);
	    setCgsBack(xw, win, gcVTcursReverse, cc);

	    setCgsFore(xw, win, gcVTcursOutline, bg);
	    setCgsBack(xw, win, gcVTcursOutline, cc);
	} else {
	    setCgsFore(xw, win, gcVTcursReverse, bg);
	    setCgsBack(xw, win, gcVTcursReverse, cc);

	    setCgsFore(xw, win, gcVTcursOutline, cc);
	    setCgsBack(xw, win, gcVTcursOutline, bg);
	}
	changed = True;
    }

    if (changed) {
	TRACE(("...set_cursor_gcs - done\n"));
    }
}

#ifdef NO_LEAKS
void
noleaks_charproc(void)
{
    if (v_buffer != 0)
	free(v_buffer);
}
#endif
