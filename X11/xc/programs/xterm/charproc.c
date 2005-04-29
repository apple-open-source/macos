/*
 * $Xorg: charproc.c,v 1.6 2001/02/09 02:06:02 xorgcvs Exp $
 */

/* $XFree86: xc/programs/xterm/charproc.c,v 3.152 2003/12/31 17:12:26 dickey Exp $ */

/*

Copyright 1999-2002,2003 by Thomas E. Dickey

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
#endif

#if OPT_INPUT_METHOD
#include <X11/Xlocale.h>
#endif

#include <stdio.h>
#include <ctype.h>

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

static IChar in_put(void);
static int set_character_class(char *s);
static void FromAlternate(TScreen * screen);
static void RequestResize(XtermWidget termw, int rows, int cols, int text);
static void SwitchBufs(TScreen * screen);
static void ToAlternate(TScreen * screen);
static void VTallocbuf(void);
static void WriteText(TScreen * screen, PAIRED_CHARS(Char * str, Char *
						     str2), Cardinal len);
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

#if OPT_BLINK_CURS
static void BlinkCursor(XtPointer closure, XtIntervalId * id);
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

#define doinput() (morePtyData(&VTbuffer) ? nextPtyData(&VTbuffer) : in_put())

static int nparam;
static ANSI reply;
static int param[NPARAM];

#ifdef UNUSED
static unsigned long ctotal;
static unsigned long ntotal;
#endif

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
                       <BtnDown>:bell(0) \
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
    { "meta-sends-escape",	HandleMetaEsc },
    { "set-num-lock",		HandleNumLock },
#endif
#if OPT_READLINE
    { "readline-button",	ReadLineButton },
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
};
/* *INDENT-ON* */

#ifdef VMS
#define OS_DEPENDENT_PRINT_COMMAND "print/delete/noflag"
#else /* VMS */
#define OS_DEPENDENT_PRINT_COMMAND "lpr"
#endif /* VMS */

static XtResource resources[] =
{
    Bres(XtNallowSendEvents, XtCAllowSendEvents, screen.allowSendEvents, FALSE),
    Bres(XtNallowWindowOps, XtCAllowWindowOps, screen.allowWindowOps, TRUE),
    Bres(XtNalwaysHighlight, XtCAlwaysHighlight, screen.always_highlight, FALSE),
    Bres(XtNappcursorDefault, XtCAppcursorDefault, misc.appcursorDefault, FALSE),
    Bres(XtNappkeypadDefault, XtCAppkeypadDefault, misc.appkeypadDefault, FALSE),
    Bres(XtNautoWrap, XtCAutoWrap, misc.autoWrap, TRUE),
    Bres(XtNawaitInput, XtCAwaitInput, screen.awaitInput, FALSE),
    Bres(XtNfreeBoldBox, XtCBoolean, screen.free_bold_box, FALSE),
    Bres(XtNbackarrowKey, XtCBackarrowKey, screen.backarrow_key, TRUE),
    Bres(XtNboldMode, XtCBoldMode, screen.bold_mode, TRUE),
    Bres(XtNbrokenSelections, XtCBrokenSelections, screen.brokenSelections, FALSE),
    Bres(XtNc132, XtCC132, screen.c132, FALSE),
    Bres(XtNcurses, XtCCurses, screen.curses, FALSE),
    Bres(XtNcutNewline, XtCCutNewline, screen.cutNewline, TRUE),
    Bres(XtNcutToBeginningOfLine, XtCCutToBeginningOfLine,
	 screen.cutToBeginningOfLine, TRUE),
    Bres(XtNdeleteIsDEL, XtCDeleteIsDEL, screen.delete_is_del, DEFDELETE_DEL),
    Bres(XtNdynamicColors, XtCDynamicColors, misc.dynamicColors, TRUE),
    Bres(XtNeightBitControl, XtCEightBitControl, screen.control_eight_bits, FALSE),
    Bres(XtNeightBitInput, XtCEightBitInput, screen.input_eight_bits, TRUE),
    Bres(XtNeightBitOutput, XtCEightBitOutput, screen.output_eight_bits, TRUE),
    Bres(XtNhighlightSelection, XtCHighlightSelection,
	 screen.highlight_selection, FALSE),
    Bres(XtNhpLowerleftBugCompat, XtCHpLowerleftBugCompat, screen.hp_ll_bc, FALSE),
    Bres(XtNi18nSelections, XtCI18nSelections, screen.i18nSelections, TRUE),
    Bres(XtNjumpScroll, XtCJumpScroll, screen.jumpscroll, TRUE),
    Bres(XtNloginShell, XtCLoginShell, misc.login_shell, FALSE),
    Bres(XtNmarginBell, XtCMarginBell, screen.marginbell, FALSE),
    Bres(XtNmetaSendsEscape, XtCMetaSendsEscape, screen.meta_sends_esc, FALSE),
    Bres(XtNmultiScroll, XtCMultiScroll, screen.multiscroll, FALSE),
    Bres(XtNoldXtermFKeys, XtCOldXtermFKeys, screen.old_fkeys, FALSE),
    Bres(XtNpopOnBell, XtCPopOnBell, screen.poponbell, FALSE),
    Bres(XtNprinterAutoClose, XtCPrinterAutoClose, screen.printer_autoclose, FALSE),
    Bres(XtNprinterExtent, XtCPrinterExtent, screen.printer_extent, FALSE),
    Bres(XtNprinterFormFeed, XtCPrinterFormFeed, screen.printer_formfeed, FALSE),
    Bres(XtNreverseVideo, XtCReverseVideo, misc.re_verse, FALSE),
    Bres(XtNreverseWrap, XtCReverseWrap, misc.reverseWrap, FALSE),
    Bres(XtNscrollBar, XtCScrollBar, misc.scrollbar, FALSE),
    Bres(XtNscrollKey, XtCScrollCond, screen.scrollkey, FALSE),
    Bres(XtNscrollTtyOutput, XtCScrollCond, screen.scrollttyoutput, TRUE),
    Bres(XtNsignalInhibit, XtCSignalInhibit, misc.signalInhibit, FALSE),
    Bres(XtNtiteInhibit, XtCTiteInhibit, misc.titeInhibit, FALSE),
    Bres(XtNtiXtraScroll, XtCTiXtraScroll, misc.tiXtraScroll, FALSE),
    Bres(XtNtrimSelection, XtCTrimSelection, screen.trim_selection, FALSE),
    Bres(XtNunderLine, XtCUnderLine, screen.underline, TRUE),
    Bres(XtNvisualBell, XtCVisualBell, screen.visualbell, FALSE),
    Cres(XtNcursorColor, XtCCursorColor, screen.cursorcolor, XtDefaultForeground),
    Cres(XtNforeground, XtCForeground, screen.foreground, XtDefaultForeground),
    Cres(XtNpointerColor, XtCPointerColor, screen.mousecolor, XtDefaultForeground),
    Cres(XtNbackground, XtCBackground, core.background_pixel, XtDefaultBackground),
    Cres(XtNpointerColorBackground, XtCBackground, screen.mousecolorback, XtDefaultBackground),
    Ires(XtNbellSuppressTime, XtCBellSuppressTime, screen.bellSuppressTime, BELLSUPPRESSMSEC),
    Ires(XtNinternalBorder, XtCBorderWidth, screen.border, DEFBORDER),
    Ires(XtNlimitResize, XtCLimitResize, misc.limit_resize, 1),
    Ires(XtNmultiClickTime, XtCMultiClickTime, screen.multiClickTime, MULTICLICKTIME),
    Ires(XtNnMarginBell, XtCColumn, screen.nmarginbell, N_MARGINBELL),
    Ires(XtNprinterControlMode, XtCPrinterControlMode,
	 screen.printer_controlmode, 0),
    Ires(XtNvisualBellDelay, XtCVisualBellDelay, screen.visualBellDelay, 100),
    Ires(XtNsaveLines, XtCSaveLines, screen.savelines, SAVELINES),
    Ires(XtNscrollLines, XtCScrollLines, screen.scrolllines, SCROLLLINES),
    Sres(XtNfont1, XtCFont1, screen.menu_font_names[fontMenu_font1], NULL),
    Sres(XtNfont2, XtCFont2, screen.menu_font_names[fontMenu_font2], NULL),
    Sres(XtNfont3, XtCFont3, screen.menu_font_names[fontMenu_font3], NULL),
    Sres(XtNfont4, XtCFont4, screen.menu_font_names[fontMenu_font4], NULL),
    Sres(XtNfont5, XtCFont5, screen.menu_font_names[fontMenu_font5], NULL),
    Sres(XtNfont6, XtCFont6, screen.menu_font_names[fontMenu_font6], NULL),
    Sres(XtNanswerbackString, XtCAnswerbackString, screen.answer_back, ""),
    Sres(XtNboldFont, XtCBoldFont, misc.default_font.f_b, DEFBOLDFONT),
    Sres(XtNcharClass, XtCCharClass, screen.charClass, NULL),
    Sres(XtNdecTerminalID, XtCDecTerminalID, screen.term_id, DFT_DECID),
    Sres(XtNfont, XtCFont, misc.default_font.f_n, DEFFONT),
    Sres(XtNgeometry, XtCGeometry, misc.geo_metry, NULL),
    Sres(XtNkeyboardDialect, XtCKeyboardDialect, screen.keyboard_dialect, DFT_KBD_DIALECT),
    Sres(XtNprinterCommand, XtCPrinterCommand, screen.printer_command, OS_DEPENDENT_PRINT_COMMAND),
    Sres(XtNtekGeometry, XtCGeometry, misc.T_geometry, NULL),

    {XtNresizeGravity, XtCResizeGravity, XtRGravity, sizeof(XtGravity),
     XtOffsetOf(XtermWidgetRec, misc.resizeGravity),
     XtRImmediate, (XtPointer) SouthWestGravity},

    {XtNpointerShape, XtCCursor, XtRCursor, sizeof(Cursor),
     XtOffsetOf(XtermWidgetRec, screen.pointer_cursor),
     XtRString, (XtPointer) "xterm"},

#ifdef ALLOWLOGGING
    Bres(XtNlogInhibit, XtCLogInhibit, misc.logInhibit, FALSE),
    Bres(XtNlogging, XtCLogging, misc.log_on, FALSE),
    Sres(XtNlogFile, XtCLogfile, screen.logfile, NULL),
#endif

#ifndef NO_ACTIVE_ICON
    Bres("activeIcon", "ActiveIcon", misc.active_icon, FALSE),
    Ires("iconBorderWidth", XtCBorderWidth, misc.icon_border_width, 2),
    Fres("iconFont", "IconFont", screen.fnt_icon, XtExtdefaultfont),
    Cres("iconBorderColor", XtCBorderColor, misc.icon_border_pixel, XtExtdefaultbackground),
#endif				/* NO_ACTIVE_ICON */

#if OPT_BLINK_CURS
    Bres(XtNcursorBlink, XtCCursorBlink, screen.cursor_blink, FALSE),
    Ires(XtNcursorOnTime, XtCCursorOnTime, screen.cursor_on, 600),
    Ires(XtNcursorOffTime, XtCCursorOffTime, screen.cursor_off, 300),
#endif

#if OPT_BOX_CHARS
    Bres(XtNforceBoxChars, XtCBoolean, screen.force_box_chars, FALSE),
#endif

#if OPT_BROKEN_OSC
    Bres(XtNbrokenLinuxOSC, XtCBrokenLinuxOSC, screen.brokenLinuxOSC, TRUE),
#endif

#if OPT_BROKEN_ST
    Bres(XtNbrokenStringTerm, XtCBrokenStringTerm, screen.brokenStringTerm, TRUE),
#endif

#if OPT_C1_PRINT
    Bres(XtNallowC1Printable, XtCAllowC1Printable, screen.c1_printable, FALSE),
#endif

#if OPT_DEC_CHRSET
    Bres(XtNfontDoublesize, XtCFontDoublesize, screen.font_doublesize, TRUE),
    Ires(XtNcacheDoublesize, XtCCacheDoublesize, screen.cache_doublesize, NUM_CHRSET),
#endif

#if OPT_HIGHLIGHT_COLOR
    Cres(XtNhighlightColor, XtCHighlightColor, screen.highlightcolor, XtDefaultForeground),
#endif				/* OPT_HIGHLIGHT_COLOR */

#if OPT_INPUT_METHOD
    Bres(XtNopenIm, XtCOpenIm, misc.open_im, TRUE),
    Sres(XtNinputMethod, XtCInputMethod, misc.input_method, NULL),
    Sres(XtNpreeditType, XtCPreeditType, misc.preedit_type,
	 "OverTheSpot,Root"),
#endif

#if OPT_ISO_COLORS
    Bres(XtNboldColors, XtCColorMode, screen.boldColors, TRUE),
    Ires(XtNveryBoldColors, XtCVeryBoldColors, screen.veryBoldColors, 0),
    Bres(XtNcolorMode, XtCColorMode, screen.colorMode, DFT_COLORMODE),

    Bres(XtNcolorAttrMode, XtCColorAttrMode, screen.colorAttrMode, FALSE),
    Bres(XtNcolorBDMode, XtCColorAttrMode, screen.colorBDMode, FALSE),
    Bres(XtNcolorBLMode, XtCColorAttrMode, screen.colorBLMode, FALSE),
    Bres(XtNcolorRVMode, XtCColorAttrMode, screen.colorRVMode, FALSE),
    Bres(XtNcolorULMode, XtCColorAttrMode, screen.colorULMode, FALSE),

    COLOR_RES("0", screen.Acolors[COLOR_0], DFT_COLOR("black")),
    COLOR_RES("1", screen.Acolors[COLOR_1], DFT_COLOR("red3")),
    COLOR_RES("2", screen.Acolors[COLOR_2], DFT_COLOR("green3")),
    COLOR_RES("3", screen.Acolors[COLOR_3], DFT_COLOR("yellow3")),
    COLOR_RES("4", screen.Acolors[COLOR_4], DFT_COLOR("DodgerBlue1")),
    COLOR_RES("5", screen.Acolors[COLOR_5], DFT_COLOR("magenta3")),
    COLOR_RES("6", screen.Acolors[COLOR_6], DFT_COLOR("cyan3")),
    COLOR_RES("7", screen.Acolors[COLOR_7], DFT_COLOR("gray90")),
    COLOR_RES("8", screen.Acolors[COLOR_8], DFT_COLOR("gray50")),
    COLOR_RES("9", screen.Acolors[COLOR_9], DFT_COLOR("red")),
    COLOR_RES("10", screen.Acolors[COLOR_10], DFT_COLOR("green")),
    COLOR_RES("11", screen.Acolors[COLOR_11], DFT_COLOR("yellow")),
    COLOR_RES("12", screen.Acolors[COLOR_12], DFT_COLOR("SteelBlue1")),
    COLOR_RES("13", screen.Acolors[COLOR_13], DFT_COLOR("magenta")),
    COLOR_RES("14", screen.Acolors[COLOR_14], DFT_COLOR("cyan")),
    COLOR_RES("15", screen.Acolors[COLOR_15], DFT_COLOR("white")),
    COLOR_RES("BD", screen.Acolors[COLOR_BD], DFT_COLOR(XtDefaultForeground)),
    COLOR_RES("BL", screen.Acolors[COLOR_BL], DFT_COLOR(XtDefaultForeground)),
    COLOR_RES("UL", screen.Acolors[COLOR_UL], DFT_COLOR(XtDefaultForeground)),
    COLOR_RES("RV", screen.Acolors[COLOR_RV], DFT_COLOR(XtDefaultForeground)),

#if OPT_256_COLORS
# include <256colres.h>
#elif OPT_88_COLORS
# include <88colres.h>
#endif

#endif				/* OPT_ISO_COLORS */

#if OPT_MOD_FKEYS
    Ires(XtNmodifyCursorKeys, XtCModifyCursorKeys,
	 keyboard.modify_cursor_keys, 2),
#endif

#if OPT_NUM_LOCK
    Bres(XtNalwaysUseMods, XtCAlwaysUseMods, misc.alwaysUseMods, FALSE),
    Bres(XtNnumLock, XtCNumLock, misc.real_NumLock, TRUE),
#endif

#if OPT_PRINT_COLORS
    Ires(XtNprintAttributes, XtCPrintAttributes, screen.print_attributes, 1),
#endif

#if OPT_SHIFT_FONTS
    Bres(XtNshiftFonts, XtCShiftFonts, misc.shift_fonts, TRUE),
#endif

#if OPT_SUNPC_KBD
    Ires(XtNctrlFKeys, XtCCtrlFKeys, misc.ctrl_fkeys, 10),
#endif

#if OPT_TEK4014
    Bres(XtNtekInhibit, XtCTekInhibit, misc.tekInhibit, FALSE),
    Bres(XtNtekSmall, XtCTekSmall, misc.tekSmall, FALSE),
    Bres(XtNtekStartup, XtCTekStartup, screen.TekEmu, FALSE),
#endif

#if OPT_TOOLBAR
    {XtNmenuBar, XtCMenuBar, XtRWidget, sizeof(Widget),
     XtOffsetOf(XtermWidgetRec, screen.fullVwin.menu_bar),
     XtRWidget, (XtPointer) 0},
    Ires(XtNmenuHeight, XtCMenuHeight, screen.fullVwin.menu_height, 25),
#endif

#if OPT_WIDE_CHARS
    Ires(XtNutf8, XtCUtf8, screen.utf8_mode, 3),
    Bres(XtNwideChars, XtCWideChars, screen.wide_chars, FALSE),
    Bres(XtNcjkWidth, XtCCjkWidth, misc.cjk_width, FALSE),
    Bres(XtNvt100Graphics, XtCVT100Graphics, screen.vt100_graphics, TRUE),
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
    Bres(XtNxmcInline, XtCXmcInline, screen.xmc_inline, FALSE),
    Bres(XtNxmcMoveSGR, XtCXmcMoveSGR, screen.move_sgr_ok, TRUE),
    Ires(XtNxmcAttributes, XtCXmcAttributes, screen.xmc_attributes, 1),
    Ires(XtNxmcGlitch, XtCXmcGlitch, screen.xmc_glitch, 0),
#endif

#ifdef SCROLLBAR_RIGHT
    Bres(XtNrightScrollBar, XtCRightScrollBar, misc.useRight, FALSE),
#endif

#ifdef XRENDERFONT
    Ires(XtNfaceSize, XtCFaceSize, misc.face_size, DEFFACESIZE),
    Sres(XtNfaceName, XtCFaceName, misc.face_name, DEFFACENAME),
#endif
};

static Boolean VTSetValues(Widget cur, Widget request, Widget new_arg,
			   ArgList args, Cardinal * num_args);
static void VTClassInit(void);
static void VTDestroy(Widget w);
static void VTExpose(Widget w, XEvent * event, Region region);
static void VTInitialize(Widget wrequest, Widget new_arg, ArgList args,
			 Cardinal * num_args);
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
	FALSE,			/* class_inited                 */
	VTInitialize,		/* initialize                   */
	NULL,			/* initialize_hook              */
	VTRealize,		/* realize                      */
	actionsList,		/* actions                      */
	XtNumber(actionsList),	/* num_actions                  */
	resources,		/* resources                    */
	XtNumber(resources),	/* num_resources                */
	NULLQUARK,		/* xrm_class                    */
	TRUE,			/* compress_motion              */
	FALSE,			/* compress_exposure            */
	TRUE,			/* compress_enterleave          */
	FALSE,			/* visible_interest             */
	VTDestroy,		/* destroy                      */
	VTResize,		/* resize                       */
	VTExpose,		/* expose                       */
	VTSetValues,		/* set_values                   */
	NULL,			/* set_values_hook              */
	NULL,			/* set_values_almost            */
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
    XtAppAddActions(app_con, actionsList, XtNumber(actionsList));
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
    register TScreen *screen = &term->screen;
    Pixel fg;

    /* FIXME HideCursor(); */
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
    register TScreen *screen = &term->screen;
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

	/* allocate larger buffer if needed/possible */
#define SafeAlloc(type, area, used, size) \
		type *new_string = area; \
		unsigned new_length = size; \
		if (new_length == 0) { \
		    new_length = 256; \
		    new_string = (type *)malloc(new_length * sizeof(type)); \
		} else if (used+1 >= new_length) { \
		    new_length = size * 2; \
		    new_string = (type *)malloc(new_length * sizeof(type)); \
		    if (new_string != 0 \
		     && area != 0 \
		     && used != 0) \
			memcpy(new_string, area, used * sizeof(type)); \
		}

#define	MotionOff( s, t )							\
	if ((s)->event_mask & PointerMotionMask) {				\
	    (s)->event_mask |= ButtonMotionMask;				\
	    (s)->event_mask &= ~PointerMotionMask;				\
	    XSelectInput(XtDisplay((t)), XtWindow((t)), (s)->event_mask);	\
	}

extern int last_written_col, last_written_row;

static void
VTparse(void)
{
    /* Buffer for processing printable text */
    static IChar *print_area;
    static size_t print_size, print_used;

    /* Buffer for processing strings (e.g., OSC ... ST) */
    static Char *string_area;
    static size_t string_size, string_used;

#if OPT_VT52_MODE
    static Bool vt52_cup = FALSE;
#endif

    Const PARSE_T *groundtable = ansi_table;
    TScreen *screen = &term->screen;
    Const PARSE_T *parsestate;
    unsigned int c;
    Char *cp;
    int row, col, top, bot, scstype, count;
    Bool private_function;	/* distinguish private-mode from standard */
    int string_mode;		/* nonzero iff we're processing a string */
    int lastchar;		/* positive iff we had a graphic character */
    int nextstate;
    int laststate;
#if OPT_WIDE_CHARS
    int last_was_wide;
#endif

    /* We longjmp back to this point in VTReset() */
    (void) setjmp(vtjmpbuf);
#if OPT_VT52_MODE
    groundtable = screen->vtXX_level ? ansi_table : vt52_table;
#else
    groundtable = ansi_table;
#endif
    parsestate = groundtable;
    scstype = 0;
    private_function = False;
    string_mode = 0;
    lastchar = -1;		/* not a legal IChar */
    nextstate = -1;		/* not a legal state */
#if OPT_WIDE_CHARS
    last_was_wide = 0;
#endif

    for (;;) {
	int thischar = -1;
	c = doinput();

#if OPT_WIDE_CHARS
	if (screen->wide_chars
	    && my_wcwidth(c) == 0) {
	    unsigned single = 0;
	    int prev, precomposed;

	    if (screen->curss) {
		dotext(screen, screen->gsets[(int) (screen->curss)],
		       print_area, 1);
		screen->curss = 0;
		single++;
	    }
	    if (print_used > single) {
		dotext(screen,
		       screen->gsets[(int) (screen->curgl)],
		       print_area + single,
		       print_used - single);
	    }
	    print_used = 0;

	    prev = getXtermCell(screen, last_written_row, last_written_col);
	    precomposed = do_precomposition(prev, c);

	    if (precomposed != -1) {
		putXtermCell(screen, last_written_row, last_written_col, precomposed);
	    } else {
		addXtermCombining(screen,
				  last_written_row,
				  last_written_col, c);
	    }
	    if (!screen->scroll_amt)
		ScrnRefresh(screen,
			    last_written_row,
			    last_written_col, 1, 1, 1);
	    continue;
	}
#endif

	/* Intercept characters for printer controller mode */
	if (screen->printer_controlmode == 2) {
	    if ((c = xtermPrinterControl(c)) == 0)
		continue;
	}

	/*
	 * VT52 is a little ugly in the one place it has a parameterized
	 * control sequence, since the parameter falls after the character
	 * that denotes the type of sequence.
	 */
#if OPT_VT52_MODE
	if (vt52_cup) {
	    param[nparam++] = (c & 0x7f) - 32;
	    if (nparam < 2)
		continue;
	    vt52_cup = FALSE;
	    if ((row = param[0]) < 0)
		row = 0;
	    if ((col = param[1]) < 0)
		col = 0;
	    CursorSet(screen, row, col, term->flags);
	    parsestate = vt52_table;
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
	laststate = nextstate;
#if OPT_WIDE_CHARS
	if (c > 255) {
	    if (parsestate == groundtable) {
		nextstate = CASE_PRINT;
	    } else if (parsestate == sos_table) {
		c &= 0xffff;
		if (c > 255) {
		    TRACE(("Found code > 255 while in SOS state: %04X\n", c));
		    c = '?';
		}
	    } else {
		nextstate = CASE_GROUND_STATE;
	    }
	} else
#endif
	    nextstate = parsestate[E2A(c)];

#if OPT_BROKEN_OSC
	/*
	 * Linux console palette escape sequences start with an OSC, but do
	 * not terminate correctly.  Some scripts do not check before writing
	 * them, making xterm appear to hang (it's awaiting a valid string
	 * terminator).  Just ignore these if we see them - there's no point
	 * in emulating bad code.
	 */
	if (screen->brokenLinuxOSC
	    && parsestate == sos_table) {
	    if (string_used) {
		switch (string_area[0]) {
		case 'P':
		    if (string_used <= 7)
			break;
		    /* FALLTHRU */
		case 'R':
		    parsestate = groundtable;
		    nextstate = parsestate[E2A(c)];
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
	    && parsestate == sos_table
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
		parsestate = groundtable;
		nextstate = parsestate[E2A(c)];
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
	    nextstate = parsestate[E2A(160)];
	    if (nextstate == CASE_ESC_SP_STATE)
		nextstate = CASE_ESC_IGNORE;
	}
#endif

#if OPT_WIDE_CHARS
	/* if this character is a different width than
	   the last one, put the previous text into
	   the buffer and draw it now */

	if (iswide(c) != last_was_wide) {
	    unsigned single = 0;

	    if (screen->curss) {
		dotext(screen,
		       screen->gsets[(int) (screen->curss)],
		       print_area, 1);
		screen->curss = 0;
		single++;
	    }
	    if (print_used > single) {
		dotext(screen,
		       screen->gsets[(int) (screen->curgl)],
		       print_area + single,
		       print_used - single);
	    }
	    print_used = 0;
	}
#endif

	/*
	 * Accumulate string for printable text.  This may be 8/16-bit
	 * characters.
	 */
	if (nextstate == CASE_PRINT) {
	    SafeAlloc(IChar, print_area, print_used, print_size);
	    if (new_string == 0) {
		fprintf(stderr,
			"Cannot allocate %d bytes for printable text\n",
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
	    print_area[print_used++] = lastchar = thischar = c;
#if OPT_WIDE_CHARS
	    last_was_wide = iswide(c);
#endif
	    if (morePtyData(&VTbuffer)) {
		continue;
	    }
	}

	if (nextstate == CASE_PRINT
	    || (laststate == CASE_PRINT && print_used)) {
	    unsigned single = 0;

	    if (screen->curss) {
		dotext(screen,
		       screen->gsets[(int) (screen->curss)],
		       print_area, 1);
		screen->curss = 0;
		single++;
	    }
	    if (print_used > single) {
		dotext(screen,
		       screen->gsets[(int) (screen->curgl)],
		       print_area + single,
		       print_used - single);
	    }
	    print_used = 0;
	}

	/*
	 * Accumulate string for APC, DCS, PM, OSC, SOS controls
	 * This should always be 8-bit characters.
	 */
	if (parsestate == sos_table) {
	    SafeAlloc(Char, string_area, string_used, string_size);
	    if (new_string == 0) {
		fprintf(stderr,
			"Cannot allocate %d bytes for string mode %d\n",
			new_length, string_mode);
		continue;
	    }
#if OPT_WIDE_CHARS
	    /*
	     * We cannot display codes above 255, but let's try to
	     * accommodate the application a little by not aborting the
	     * string.
	     */
	    if ((c & 0xffff) > 255) {
		nextstate = CASE_PRINT;
		c = '?';
	    }
#endif
	    string_area = new_string;
	    string_size = new_length;
	    string_area[string_used++] = c;
	} else if (parsestate != esc_table) {
	    /* if we were accumulating, we're not any more */
	    string_mode = 0;
	    string_used = 0;
	}

	TRACE(("parse %04X -> %d\n", c, nextstate));

	switch (nextstate) {
	case CASE_PRINT:
	    /* printable characters (see above) */
	    break;

	case CASE_GROUND_STATE:
	    /* exit ignore mode */
	    parsestate = groundtable;
	    break;

	case CASE_IGNORE:
	    /* Ignore character */
	    break;

	case CASE_ENQ:
	    for (count = 0; screen->answer_back[count] != 0; count++)
		unparseputc(screen->answer_back[count], screen->respond);
	    break;

	case CASE_BELL:
	    if (string_mode == OSC) {
		if (string_used)
		    string_area[--string_used] = '\0';
		do_osc(string_area, string_used, c);
		parsestate = groundtable;
	    } else {
		/* bell */
		Bell(XkbBI_TerminalBell, 0);
	    }
	    break;

	case CASE_BS:
	    /* backspace */
	    CursorBack(screen, 1);
	    break;

	case CASE_CR:
	    /* carriage return */
	    CarriageReturn(screen);
	    break;

	case CASE_ESC:
	    /* escape */
	    if_OPT_VT52_MODE(screen, {
		parsestate = vt52_esc_table;
		break;
	    });
	    parsestate = esc_table;
	    break;

#if OPT_VT52_MODE
	case CASE_VT52_CUP:
	    vt52_cup = TRUE;
	    nparam = 0;
	    break;

	case CASE_VT52_IGNORE:
	    parsestate = vt52_ignore_table;
	    break;
#endif

	case CASE_VMOT:
	    /*
	     * form feed, line feed, vertical tab
	     */
	    xtermAutoPrint(c);
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
		   && (TabToPrevStop())) ;
	    parsestate = groundtable;
	    break;

	case CASE_CHT:
	    /* cursor forward tabulation */
	    if ((count = param[0]) == DEFAULT)
		count = 1;
	    while ((count-- > 0)
		   && (TabToNextStop())) ;
	    parsestate = groundtable;
	    break;

	case CASE_TAB:
	    /* tab */
	    TabToNextStop();
	    break;

	case CASE_SI:
	    screen->curgl = 0;
	    if_OPT_VT52_MODE(screen, {
		parsestate = groundtable;
	    });
	    break;

	case CASE_SO:
	    screen->curgl = 1;
	    if_OPT_VT52_MODE(screen, {
		parsestate = groundtable;
	    });
	    break;

	case CASE_DECDHL:
	    xterm_DECDHL(c == '3');
	    parsestate = groundtable;
	    break;

	case CASE_DECSWL:
	    xterm_DECSWL();
	    parsestate = groundtable;
	    break;

	case CASE_DECDWL:
	    xterm_DECDWL();
	    parsestate = groundtable;
	    break;

	case CASE_SCR_STATE:
	    /* enter scr state */
	    parsestate = scrtable;
	    break;

	case CASE_SCS0_STATE:
	    /* enter scs state 0 */
	    scstype = 0;
	    parsestate = scstable;
	    break;

	case CASE_SCS1_STATE:
	    /* enter scs state 1 */
	    scstype = 1;
	    parsestate = scstable;
	    break;

	case CASE_SCS2_STATE:
	    /* enter scs state 2 */
	    scstype = 2;
	    parsestate = scstable;
	    break;

	case CASE_SCS3_STATE:
	    /* enter scs state 3 */
	    scstype = 3;
	    parsestate = scstable;
	    break;

	case CASE_ESC_IGNORE:
	    /* unknown escape sequence */
	    parsestate = eigtable;
	    break;

	case CASE_ESC_DIGIT:
	    /* digit in csi or dec mode */
	    if ((row = param[nparam - 1]) == DEFAULT)
		row = 0;
	    param[nparam - 1] = 10 * row + (c - '0');
	    if (param[nparam - 1] > 65535)
		param[nparam - 1] = 65535;
	    if (parsestate == csi_table)
		parsestate = csi2_table;
	    break;

	case CASE_ESC_SEMI:
	    /* semicolon in csi or dec mode */
	    if (nparam < NPARAM)
		param[nparam++] = DEFAULT;
	    if (parsestate == csi_table)
		parsestate = csi2_table;
	    break;

	case CASE_DEC_STATE:
	    /* enter dec mode */
	    parsestate = dec_table;
	    break;

	case CASE_DEC2_STATE:
	    /* enter dec2 mode */
	    parsestate = dec2_table;
	    break;

	case CASE_DEC3_STATE:
	    /* enter dec3 mode */
	    parsestate = dec3_table;
	    break;

	case CASE_ICH:
	    /* ICH */
	    if ((row = param[0]) < 1)
		row = 1;
	    InsertChar(screen, row);
	    parsestate = groundtable;
	    break;

	case CASE_CUU:
	    /* CUU */
	    if ((row = param[0]) < 1)
		row = 1;
	    CursorUp(screen, row);
	    parsestate = groundtable;
	    break;

	case CASE_CUD:
	    /* CUD */
	    if ((row = param[0]) < 1)
		row = 1;
	    CursorDown(screen, row);
	    parsestate = groundtable;
	    break;

	case CASE_CUF:
	    /* CUF */
	    if ((col = param[0]) < 1)
		col = 1;
	    CursorForward(screen, col);
	    parsestate = groundtable;
	    break;

	case CASE_CUB:
	    /* CUB */
	    if ((col = param[0]) < 1)
		col = 1;
	    CursorBack(screen, col);
	    parsestate = groundtable;
	    break;

	case CASE_CUP:
	    /* CUP | HVP */
	    if_OPT_XMC_GLITCH(screen, {
		Jump_XMC(screen);
	    });
	    if ((row = param[0]) < 1)
		row = 1;
	    if (nparam < 2 || (col = param[1]) < 1)
		col = 1;
	    CursorSet(screen, row - 1, col - 1, term->flags);
	    parsestate = groundtable;
	    break;

	case CASE_VPA:
	    if ((row = param[0]) < 1)
		row = 1;
	    CursorSet(screen, row - 1, screen->cur_col, term->flags);
	    parsestate = groundtable;
	    break;

	case CASE_HPA:
	    /* HPA | CHA */
	    if ((col = param[0]) < 1)
		col = 1;
	    CursorSet(screen, screen->cur_row, col - 1, term->flags);
	    parsestate = groundtable;
	    break;

	case CASE_HP_BUGGY_LL:
	    /* Some HP-UX applications have the bug that they
	       assume ESC F goes to the lower left corner of
	       the screen, regardless of what terminfo says. */
	    if (screen->hp_ll_bc)
		CursorSet(screen, screen->max_row, 0, term->flags);
	    parsestate = groundtable;
	    break;

	case CASE_ED:
	    /* ED */
	    do_erase_display(screen, param[0], OFF_PROTECT);
	    parsestate = groundtable;
	    break;

	case CASE_EL:
	    /* EL */
	    do_erase_line(screen, param[0], OFF_PROTECT);
	    parsestate = groundtable;
	    break;

	case CASE_ECH:
	    /* ECH */
	    ClearRight(screen, param[0] < 1 ? 1 : param[0]);
	    parsestate = groundtable;
	    break;

	case CASE_IL:
	    /* IL */
	    if ((row = param[0]) < 1)
		row = 1;
	    InsertLine(screen, row);
	    parsestate = groundtable;
	    break;

	case CASE_DL:
	    /* DL */
	    if ((row = param[0]) < 1)
		row = 1;
	    DeleteLine(screen, row);
	    parsestate = groundtable;
	    break;

	case CASE_DCH:
	    /* DCH */
	    if ((row = param[0]) < 1)
		row = 1;
	    DeleteChar(screen, row);
	    parsestate = groundtable;
	    break;

	case CASE_TRACK_MOUSE:
	    if (screen->send_mouse_pos == VT200_HIGHLIGHT_MOUSE
		|| nparam > 1) {
		/* Track mouse as long as in window and between
		 * specified rows
		 */
		TrackMouse(param[0],
			   param[2] - 1, param[1] - 1,
			   param[3] - 1, param[4] - 2);
	    } else {
		/* SD */
		if ((count = param[0]) < 1)
		    count = 1;
		RevScroll(screen, count);
		do_xevents();
	    }
	    parsestate = groundtable;
	    break;

	case CASE_DECID:
	    if_OPT_VT52_MODE(screen, {
		unparseputc(ESC, screen->respond);
		unparseputc('/', screen->respond);
		unparseputc('Z', screen->respond);
		parsestate = groundtable;
		break;
	    });
	    param[0] = -1;	/* Default ID parameter */
	    /* FALLTHRU */
	case CASE_DA1:
	    /* DA1 */
	    if (param[0] <= 1) {	/* less than means DEFAULT */
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
	    parsestate = groundtable;
	    break;

	case CASE_DA2:
	    /* DA2 */
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
	    parsestate = groundtable;
	    break;

	case CASE_DECRPTUI:
	    /* DECRPTUI */
	    if ((screen->terminal_id >= 400)
		&& (param[0] <= 0)) {	/* less than means DEFAULT */
		unparseputc1(DCS, screen->respond);
		unparseputc('!', screen->respond);
		unparseputc('|', screen->respond);
		unparseputc('0', screen->respond);
		unparseputc1(ST, screen->respond);
	    }
	    parsestate = groundtable;
	    break;

	case CASE_TBC:
	    /* TBC */
	    if ((row = param[0]) <= 0)	/* less than means default */
		TabClear(term->tabs, screen->cur_col);
	    else if (row == 3)
		TabZonk(term->tabs);
	    parsestate = groundtable;
	    break;

	case CASE_SET:
	    /* SET */
	    ansi_modes(term, bitset);
	    parsestate = groundtable;
	    break;

	case CASE_RST:
	    /* RST */
	    ansi_modes(term, bitclr);
	    parsestate = groundtable;
	    break;

	case CASE_SGR:
	    /* SGR */
	    for (row = 0; row < nparam; ++row) {
		if_OPT_XMC_GLITCH(screen, {
		    Mark_XMC(screen, param[row]);
		});
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
	    parsestate = groundtable;
	    break;

	    /* DSR (except for the '?') is a superset of CPR */
	case CASE_DSR:
	    private_function = True;

	    /* FALLTHRU */
	case CASE_CPR:
	    count = 0;
	    reply.a_type = CSI;
	    reply.a_pintro = private_function ? '?' : 0;
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

	    parsestate = groundtable;
	    private_function = False;
	    break;

	case CASE_MC:
	    xtermMediaControl(param[0], FALSE);
	    parsestate = groundtable;
	    break;

	case CASE_DEC_MC:
	    xtermMediaControl(param[0], TRUE);
	    parsestate = groundtable;
	    break;

	case CASE_HP_MEM_LOCK:
	case CASE_HP_MEM_UNLOCK:
	    if (screen->scroll_amt)
		FlushScroll(screen);
	    if (parsestate[c] == CASE_HP_MEM_LOCK)
		screen->top_marg = screen->cur_row;
	    else
		screen->top_marg = 0;
	    parsestate = groundtable;
	    break;

	case CASE_DECSTBM:
	    /* DECSTBM - set scrolling region */
	    if ((top = param[0]) < 1)
		top = 1;
	    if (nparam < 2 || (bot = param[1]) == DEFAULT
		|| bot > screen->max_row + 1
		|| bot == 0)
		bot = screen->max_row + 1;
	    if (bot > top) {
		if (screen->scroll_amt)
		    FlushScroll(screen);
		screen->top_marg = top - 1;
		screen->bot_marg = bot - 1;
		CursorSet(screen, 0, 0, term->flags);
	    }
	    parsestate = groundtable;
	    break;

	case CASE_DECREQTPARM:
	    /* DECREQTPARM */
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
	    parsestate = groundtable;
	    break;

	case CASE_DECSET:
	    /* DECSET */
#if OPT_VT52_MODE
	    if (screen->vtXX_level != 0)
#endif
		dpmodes(term, bitset);
	    parsestate = groundtable;
#if OPT_TEK4014
	    if (screen->TekEmu)
		return;
#endif
	    break;

	case CASE_DECRST:
	    /* DECRST */
	    dpmodes(term, bitclr);
#if OPT_VT52_MODE
	    if (screen->vtXX_level == 0)
		groundtable = vt52_table;
	    else if (screen->terminal_id >= 100)
		groundtable = ansi_table;
#endif
	    parsestate = groundtable;
	    break;

	case CASE_DECALN:
	    /* DECALN */
	    if (screen->cursor_state)
		HideCursor();
	    for (row = screen->max_row; row >= 0; row--) {
		bzero(SCRN_BUF_ATTRS(screen, row),
		      col = screen->max_col + 1);
		for (cp = SCRN_BUF_CHARS(screen, row); col > 0; col--)
		    *cp++ = (Char) 'E';
		if_OPT_WIDE_CHARS(screen, {
		    bzero(SCRN_BUF_WIDEC(screen, row),
			  screen->max_col + 1);
		});
	    }
	    ScrnRefresh(screen, 0, 0, screen->max_row + 1,
			screen->max_col + 1, False);
	    parsestate = groundtable;
	    break;

	case CASE_GSETS:
	    TRACE(("CASE_GSETS(%d) = '%c'\n", scstype, c));
	    if (screen->vtXX_level != 0)
		screen->gsets[scstype] = c;
	    parsestate = groundtable;
	    break;

	case CASE_DECSC:
	    /* DECSC */
	    CursorSave(term);
	    parsestate = groundtable;
	    break;

	case CASE_DECRC:
	    /* DECRC */
	    CursorRestore(term);
	    if_OPT_ISO_COLORS(screen, {
		setExtendedFG();
	    });
	    parsestate = groundtable;
	    break;

	case CASE_DECKPAM:
	    /* DECKPAM */
	    term->keyboard.flags |= MODE_DECKPAM;
	    update_appkeypad();
	    parsestate = groundtable;
	    break;

	case CASE_DECKPNM:
	    /* DECKPNM */
	    term->keyboard.flags &= ~MODE_DECKPAM;
	    update_appkeypad();
	    parsestate = groundtable;
	    break;

	case CASE_CSI_QUOTE_STATE:
	    parsestate = csi_quo_table;
	    break;

#if OPT_VT52_MODE
	case CASE_VT52_FINISH:
	    TRACE(("CASE_VT52_FINISH terminal_id %d, vtXX_level %d\n",
		   screen->terminal_id,
		   screen->vtXX_level));
	    if (screen->terminal_id >= 100
		&& screen->vtXX_level == 0) {
		groundtable =
		    parsestate = ansi_table;
		screen->vtXX_level = screen->vt52_save_level;
		screen->curgl = screen->vt52_save_curgl;
		screen->curgr = screen->vt52_save_curgr;
		screen->curss = screen->vt52_save_curss;
		memmove(screen->gsets, screen->vt52_save_gsets, sizeof(screen->gsets));
	    }
	    break;
#endif

	case CASE_ANSI_LEVEL_1:
	    set_ansi_conformance(screen, 1);
	    parsestate = groundtable;
	    break;

	case CASE_ANSI_LEVEL_2:
	    set_ansi_conformance(screen, 2);
	    parsestate = groundtable;
	    break;

	case CASE_ANSI_LEVEL_3:
	    set_ansi_conformance(screen, 3);
	    parsestate = groundtable;
	    break;

	case CASE_DECSCL:
	    if (param[0] >= 61 && param[0] <= 65) {
		VTReset(TRUE, TRUE);
		screen->vtXX_level = param[0] - 60;
		if (param[0] > 61) {
		    if (param[1] == 1)
			show_8bit_control(False);
		    else if (param[1] == 0 || param[1] == 2)
			show_8bit_control(True);
		}
	    }
	    parsestate = groundtable;
	    break;

	case CASE_DECSCA:
	    screen->protected_mode = DEC_PROTECT;
	    if (param[0] <= 0 || param[0] == 2)
		term->flags &= ~PROTECTED;
	    else if (param[0] == 1)
		term->flags |= PROTECTED;
	    parsestate = groundtable;
	    break;

	case CASE_DECSED:
	    /* DECSED */
	    do_erase_display(screen, param[0], DEC_PROTECT);
	    parsestate = groundtable;
	    break;

	case CASE_DECSEL:
	    /* DECSEL */
	    do_erase_line(screen, param[0], DEC_PROTECT);
	    parsestate = groundtable;
	    break;

	case CASE_ST:
	    if (!string_used)
		break;
	    string_area[--string_used] = '\0';
	    switch (string_mode) {
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
	    parsestate = groundtable;
	    break;

	case CASE_SOS:
	    TRACE(("begin SOS: Start of String\n"));
	    string_mode = SOS;
	    parsestate = sos_table;
	    break;

	case CASE_PM:
	    TRACE(("begin PM: Privacy Message\n"));
	    string_mode = PM;
	    parsestate = sos_table;
	    break;

	case CASE_DCS:
	    TRACE(("begin DCS: Device Control String\n"));
	    string_mode = DCS;
	    parsestate = sos_table;
	    break;

	case CASE_APC:
	    TRACE(("begin APC: Application Program Command\n"));
	    string_mode = APC;
	    parsestate = sos_table;
	    break;

	case CASE_SPA:
	    screen->protected_mode = ISO_PROTECT;
	    term->flags |= PROTECTED;
	    parsestate = groundtable;
	    break;

	case CASE_EPA:
	    term->flags &= ~PROTECTED;
	    parsestate = groundtable;
	    break;

	case CASE_SU:
	    /* SU */
	    if ((count = param[0]) < 1)
		count = 1;
	    xtermScroll(screen, count);
	    parsestate = groundtable;
	    break;

	case CASE_IND:
	    /* IND */
	    xtermIndex(screen, 1);
	    do_xevents();
	    parsestate = groundtable;
	    break;

	case CASE_CPL:
	    /* cursor prev line */
	    CursorPrevLine(screen, param[0]);
	    parsestate = groundtable;
	    break;

	case CASE_CNL:
	    /* cursor next line */
	    CursorNextLine(screen, param[0]);
	    parsestate = groundtable;
	    break;

	case CASE_NEL:
	    /* NEL */
	    xtermIndex(screen, 1);
	    CarriageReturn(screen);
	    do_xevents();
	    parsestate = groundtable;
	    break;

	case CASE_HTS:
	    /* HTS */
	    TabSet(term->tabs, screen->cur_col);
	    parsestate = groundtable;
	    break;

	case CASE_RI:
	    /* RI */
	    RevIndex(screen, 1);
	    parsestate = groundtable;
	    break;

	case CASE_SS2:
	    /* SS2 */
	    screen->curss = 2;
	    parsestate = groundtable;
	    break;

	case CASE_SS3:
	    /* SS3 */
	    screen->curss = 3;
	    parsestate = groundtable;
	    break;

	case CASE_CSI_STATE:
	    /* enter csi state */
	    nparam = 1;
	    param[0] = DEFAULT;
	    parsestate = csi_table;
	    break;

	case CASE_ESC_SP_STATE:
	    /* esc space */
	    parsestate = esc_sp_table;
	    break;

	case CASE_CSI_EX_STATE:
	    /* csi exclamation */
	    parsestate = csi_ex_table;
	    break;

#if OPT_DEC_LOCATOR
	case CASE_CSI_TICK_STATE:
	    /* csi tick (') */
	    parsestate = csi_tick_table;
	    break;

	case CASE_DECEFR:
	    TRACE(("DECEFR - Enable Filter Rectangle\n"));
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
	    parsestate = groundtable;
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
		    screen->locator_reset = TRUE;
		} else {
		    screen->locator_reset = FALSE;
		}
		if (nparam < 2 || param[1] != 1) {
		    screen->locator_pixels = FALSE;
		} else {
		    screen->locator_pixels = TRUE;
		}
		screen->loc_filter = FALSE;
	    }
	    parsestate = groundtable;
	    break;

	case CASE_DECSLE:
	    TRACE(("DECSLE - Select Locator Events\n"));
	    for (count = 0; count < nparam; ++count) {
		switch (param[count]) {
		case DEFAULT:
		case 0:
		    MotionOff(screen, term);
		    screen->loc_filter = FALSE;
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
	    parsestate = groundtable;
	    break;

	case CASE_DECRQLP:
	    TRACE(("DECRQLP - Request Locator Position\n"));
	    if (param[0] < 2) {
		/* Issue DECLRP Locator Position Report */
		GetLocatorPosition(term);
	    }
	    parsestate = groundtable;
	    break;
#endif /* OPT_DEC_LOCATOR */

	case CASE_S7C1T:
	    show_8bit_control(False);
	    parsestate = groundtable;
	    break;

	case CASE_S8C1T:
#if OPT_VT52_MODE
	    if (screen->vtXX_level <= 1)
		break;
#endif
	    show_8bit_control(True);
	    parsestate = groundtable;
	    break;

	case CASE_OSC:
	    TRACE(("begin OSC: Operating System Command\n"));
	    parsestate = sos_table;
	    string_mode = OSC;
	    break;

	case CASE_RIS:
	    /* RIS */
	    VTReset(TRUE, TRUE);
	    parsestate = groundtable;
	    break;

	case CASE_DECSTR:
	    /* DECSTR */
	    VTReset(FALSE, FALSE);
	    parsestate = groundtable;
	    break;

	case CASE_REP:
	    /* REP */
	    if (lastchar >= 0 &&
		groundtable[E2A(lastchar)] == CASE_PRINT) {
		IChar repeated[2];
		count = (param[0] < 1) ? 1 : param[0];
		repeated[0] = lastchar;
		while (count-- > 0) {
		    dotext(screen,
			   screen->gsets[(int) (screen->curgl)],
			   repeated, 1);
		}
	    }
	    parsestate = groundtable;
	    break;

	case CASE_LS2:
	    /* LS2 */
	    screen->curgl = 2;
	    parsestate = groundtable;
	    break;

	case CASE_LS3:
	    /* LS3 */
	    screen->curgl = 3;
	    parsestate = groundtable;
	    break;

	case CASE_LS3R:
	    /* LS3R */
	    screen->curgr = 3;
	    parsestate = groundtable;
	    break;

	case CASE_LS2R:
	    /* LS2R */
	    screen->curgr = 2;
	    parsestate = groundtable;
	    break;

	case CASE_LS1R:
	    /* LS1R */
	    screen->curgr = 1;
	    parsestate = groundtable;
	    break;

	case CASE_XTERM_SAVE:
	    savemodes(term);
	    parsestate = groundtable;
	    break;

	case CASE_XTERM_RESTORE:
	    restoremodes(term);
	    parsestate = groundtable;
	    break;

	case CASE_XTERM_WINOPS:
	    if (screen->allowWindowOps)
		window_ops(term);
	    parsestate = groundtable;
	    break;
#if OPT_WIDE_CHARS
	case CASE_ESC_PERCENT:
	    parsestate = esc_pct_table;
	    break;

	case CASE_UTF8:
	    /* If we did not set UTF-8 mode from resource or the
	     * command-line, allow it to be enabled/disabled by
	     * control sequence.  To keep the logic simple, require
	     * that wide_chars have been set during initialization
	     * so that we have been maintaining data structures for
	     * wide characters, anyway.  Otherwise we would have to
	     * reallocate everything the first time we turned UTF-8
	     * mode on.
	     */
	    if (screen->wide_chars
		&& screen->utf8_mode != 2) {
		screen->utf8_mode = (c == 'G');
		TRACE(("UTF8 mode %s\n",
		       screen->utf8_mode ? "ON" : "OFF"));
	    } else {
		TRACE(("UTF8 mode NOT turned %s (%s)\n",
		       (c == 'G') ? "ON" : "OFF",
		       (screen->utf8_mode == 2)
		       ? "UTF-8 mode set from command-line"
		       : "wideChars resource was not set"));
	    }
	    parsestate = groundtable;
	    break;
#endif

	case CASE_CSI_IGNORE:
	    parsestate = cigtable;
	    break;
	}
	if (parsestate == groundtable)
	    lastchar = thischar;
    }
}

static Char *v_buffer;		/* pointer to physical buffer */
static Char *v_bufstr = NULL;	/* beginning of area to write */
static Char *v_bufptr;		/* end of area to write */
static Char *v_bufend;		/* end of physical buffer */

/* Write data to the pty as typed by the user, pasted with the mouse,
   or generated by us in response to a query ESC sequence. */

int
v_write(int f, Char * data, int len)
{
    int riten;
    int c = len;

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
	if (v_bufend < v_bufptr + len) {	/* we've run out of room */
	    if (v_bufstr != v_buffer) {
		/* there is unused space, move everything down */
		/* possibly overlapping memmove here */
#ifdef DEBUG
		if (debug)
		    fprintf(stderr, "moving data down %d\n",
			    v_bufstr - v_buffer);
#endif
		memmove(v_buffer, v_bufstr, v_bufptr - v_bufstr);
		v_bufptr -= v_bufstr - v_buffer;
		v_bufstr = v_buffer;
	    }
	    if (v_bufend < v_bufptr + len) {
		/* still won't fit: get more space */
		/* Don't use XtRealloc because an error is not fatal. */
		int size = v_bufptr - v_buffer;		/* save across realloc */
		v_buffer = (Char *) realloc(v_buffer, size + len);
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
		      ((v_bufptr - v_bufstr <= MAX_PTY_WRITE)
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
	int allocsize = size ? size : 1;

	v_buffer = (Char *) realloc(v_buffer, allocsize);
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
static int select_mask;
static int write_mask;
static int pty_read_bytes;

#define	ptymask()	(v_bufptr > v_bufstr ? pty_mask : 0)

static IChar
in_put(void)
{
    int status;
    Dimension replyWidth, replyHeight;
    XtGeometryResult stat;

    register TScreen *screen = &term->screen;
    register char *cp;
    register int i;

    select_mask = pty_mask;	/* force initial read */
    for (;;) {

	/* if the terminal changed size, resize the widget */
	if (tt_changed) {
	    tt_changed = FALSE;

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
	}

	if ((select_mask & pty_mask) && (eventMode == NORMAL)) {
#ifdef ALLOWLOGGING
	    if (screen->logging) {
		FlushLog(screen);
	    }
#endif

	    if (read_queue.flink != 0) {
		VTbuffer.cnt = tt_read(VTbuffer.ptr = VTbuffer.buf);
		if (VTbuffer.cnt == 0)
		    Panic("input: read returned zero\n", 0);
		else {
		    /* strip parity bit */
		    for (i = VTbuffer.cnt, cp = VTbuffer.ptr; i > 0; i--)
			*cp++ &= 0177;	/* originally CHAR */
		    if (screen->scrollWidget && screen->scrollttyoutput &&
			screen->topline < 0)
			/* Scroll to bottom */
			WindowScroll(screen, 0);
		    break;
		}
	    } else {
		sys$hiber();
	    }

	}
	if (screen->scroll_amt)
	    FlushScroll(screen);
	if (screen->cursor_set && (screen->cursor_col != screen->cur_col
				   || screen->cursor_row != screen->cur_row)) {
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
	    if (VTbuffer.cnt <= 0) {
		VTbuffer.cnt = 0;
		VTbuffer.ptr = VTbuffer.buf;
	    }
	    xevents();
	    if (VTbuffer.cnt > 0)
		break;
	}
    }
    VTbuffer.cnt--;
    return (*VTbuffer.ptr++);
}
#else /* VMS */
static fd_set select_mask;
static fd_set write_mask;
static int pty_read_bytes;

static IChar
in_put(void)
{
    register TScreen *screen = &term->screen;
    register int i, time_select;
    static struct timeval select_timeout;
#if OPT_BLINK_CURS
#define	TICK	(1000/8)
#define	MIN(a,b)	(((a)<(b))?(a):(b))
#define	MAX(a,b)	(((a)>(b))?(a):(b))
    int tick = MAX(1, MIN(screen->cursor_on, screen->cursor_off)) * TICK;
#endif

    for (;;) {
	if (eventMode == NORMAL
	    && getPtyData(screen, &select_mask, &VTbuffer)) {
	    if (screen->scrollWidget
		&& screen->scrollttyoutput
		&& screen->topline < 0)
		WindowScroll(screen, 0);	/* Scroll to bottom */
	    pty_read_bytes += VTbuffer.cnt;
	    /* stop speed reading at some point to look for X stuff */
	    /* (4096 is just a random large number.) */
	    if (pty_read_bytes > 4096)
		FD_CLR(screen->respond, &select_mask);
	    break;
	}
	pty_read_bytes = 0;
	/* update the screen */
	if (screen->scroll_amt)
	    FlushScroll(screen);
	if (screen->cursor_set && (screen->cursor_col != screen->cur_col
				   || screen->cursor_row != screen->cur_row)) {
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
	} else if ((screen->cursor_blink &&
		    ((screen->select & FOCUS) || screen->always_highlight)) ||
		   (screen->cursor_state == BLINKED_OFF)) {
	    select_timeout.tv_usec = tick;
	    while (select_timeout.tv_usec > 1000000) {
		select_timeout.tv_usec -= 1000000;
		select_timeout.tv_sec++;
	    }
	    time_select = 1;
#endif
	}
	i = select(max_plus1, &select_mask, &write_mask, 0,
		   time_select ? &select_timeout : 0);
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
	    if (VTbuffer.cnt > 0)	/* HandleInterpret */
		break;
	}

    }
    return nextPtyData(&VTbuffer);
}
#endif /* VMS */

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
			       XNForeground, screen->foreground,
			       XNBackground, term->core.background_pixel,
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
    Cardinal chars_chomped;
#else
    int next_col, last_col, this_col;	/* must be signed */
#endif
    Cardinal offset;

#if OPT_WIDE_CHARS
    /* don't translate if we use UTF-8, and are not handling legacy support
     * for line-drawing characters.
     */
    if (!screen->utf8_mode
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
    for (offset = 0; offset < len; offset += chars_chomped) {
	int width_available = screen->max_col - screen->cur_col + 1;
	int width_here = 0, need_wrap = 0;
	chars_chomped = 0;

	if (screen->do_wrap && (term->flags & WRAPAROUND)) {
	    /* mark that we had to wrap this line */
	    ScrnSetWrapped(screen, screen->cur_row);
	    xtermAutoPrint('\n');
	    xtermIndex(screen, 1);
	    screen->cur_col = 0;
	    screen->do_wrap = 0;
	    width_available = screen->max_col - screen->cur_col + 1;
	}

	while (width_here <= width_available && chars_chomped < (len - offset)) {
	    if (!screen->utf8_mode
		|| (screen->vt100_graphics && charset == '0'))
		width_here++;
	    else
		width_here += my_wcwidth(buf[chars_chomped + offset]);
	    chars_chomped++;
	}

	if (width_here > width_available) {
	    chars_chomped--;
	    if (!screen->utf8_mode
		|| (screen->vt100_graphics && charset == '0'))
		width_here--;
	    else
		width_here -= my_wcwidth(buf[chars_chomped + offset]);
	    need_wrap = 1;
	}

	if (width_here == width_available) {
	    need_wrap = 1;
	}

	if (chars_chomped != (len - offset)) {
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
	{
	    static unsigned limit;
	    static Char *hibyte, *lobyte;
	    Boolean both = False;
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
		screen->cur_col = 0;
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
int
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
    unsigned test;
    unsigned flags = term->flags;
    int fg_bg = makeColorPair(term->cur_foreground, term->cur_background);
    GC currentGC;

    TRACE(("WriteText (%2d,%2d) (%d) %3d:%s\n",
	   screen->cur_row,
	   screen->cur_col,
	   curXtermChrSet(screen->cur_row),
	   len, visibleChars(PAIRED_CHARS(str, str2), len)));

    if (screen->cur_row - screen->topline <= screen->max_row) {
	if (screen->cursor_state)
	    HideCursor();

	if (flags & INSERT) {
	    InsertChar(screen, visual_width(PAIRED_CHARS(str, str2), len));
	}
	if (!AddToRefresh(screen)) {
	    /* make sure that the correct GC is current */
	    currentGC = updatedXtermGC(screen, flags, fg_bg, False);

	    if (screen->scroll_amt)
		FlushScroll(screen);

	    if (flags & INVISIBLE) {
		memset(str, ' ', len);
		if_OPT_WIDE_CHARS(screen, {
		    if (str2 != 0)
			memset(str2, ' ', len);
		});
	    }

	    TRACE(("WriteText calling drawXtermText (%d,%d)\n",
		   screen->cur_col,
		   screen->cur_row));

	    test = flags;
	    checkVeryBoldColors(test, term->cur_foreground);

	    drawXtermText(screen, test & DRAWX_MASK, currentGC,
			  CurCursorX(screen, screen->cur_row, screen->cur_col),
			  CursorY(screen, screen->cur_row),
			  curXtermChrSet(screen->cur_row),
			  PAIRED_CHARS(str, str2), len, 0);

	    resetXtermGC(screen, flags, False);

	    /*
	     * The following statements compile data to compute the
	     * average number of characters written on each call to
	     * XText.  The data may be examined via the use of a
	     * "hidden" escape sequence.
	     */
#ifdef UNUSED
	    ctotal += len;
	    ++ntotal;
#endif
	}
    }
    ScreenWrite(screen, PAIRED_CHARS(str, str2), flags, fg_bg, len);
    CursorForward(screen, visual_width(PAIRED_CHARS(str, str2), len));
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
		char *buf = (char *) malloc(strlen(icon_name) + 1);
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
	if (term->screen.Vshow) {
#ifndef NO_ACTIVE_ICON
	    struct _vtwin *Vwin = term->screen.whichVwin;
#else
	    struct _vtwin *Vwin = &(term->screen.fullVwin);
#endif
	    if (Vwin->menu_bar) {
		XtVaGetValues(Vwin->menu_bar,
			      XtNheight, &Vwin->menu_height,
			      (XtPointer) 0);
		TRACE(("...menu_height %d\n", Vwin->menu_height));
	    }
	}
#endif
	break;
    default:
	TRACE(("HandleStructNotify(event %d)\n", event->type));
	break;
    }
}
#endif /* HANDLE_STRUCT_NOTIFY */

#if OPT_BLINK_CURS
static void
SetCursorBlink(register TScreen * screen, int enable)
{
    ShowCursor();
    if (enable) {
	screen->cursor_blink = TRUE;
	StartBlinking(screen);
    } else {
	screen->cursor_blink = FALSE;
	StopBlinking(screen);
    }
    update_cursorblink();
}

void
ToggleCursorBlink(register TScreen * screen)
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
    register int i;

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
    register TScreen *screen = &termw->screen;
    register int i, j;

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
		    j != screen->max_col + 1)
		    RequestResize(termw, -1, j, TRUE);
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
#if OPT_BLINK_CURS
	case 12:		/* att610: Start/stop blinking cursor */
	    SetCursorBlink(screen, (func == bitset) ? ON : OFF);
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
#ifdef ALLOWLOGGING
		if (screen->logging && TekPtyData()) {
		    FlushLog(screen);
		    screen->logstart = DecodedData(Tbuffer);
		}
#endif
		screen->TekEmu = TRUE;
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
		   (termw->keyboard.flags & MODE_DECBKM)
		   ? "on"
		   : "off"));
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
		screen->event_mask |= ButtonMotionMask;
		screen->event_mask &= ~PointerMotionMask;
	    } else {
		screen->event_mask &= ~ButtonMotionMask;
		screen->event_mask |= PointerMotionMask;
	    }
	    XSelectInput(XtDisplay(termw), XtWindow(term), screen->event_mask);
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
	    break;
	case 1036:
	    screen->meta_sends_esc = (func == bitset) ? ON : OFF;
	    break;
#endif
	case 1037:
	    screen->delete_is_del = (func == bitset) ? ON : OFF;
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
    register TScreen *screen = &termw->screen;
    register int i;

    for (i = 0; i < nparam; i++) {
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
#if OPT_BLINK_CURS
	case 12:		/* att610: Start/stop blinking cursor */
	    DoSM(DP_CRS_BLINK, screen->cursor_blink);
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
    register TScreen *screen = &termw->screen;
    register int i, j;

    for (i = 0; i < nparam; i++) {
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
				     ? 132 : 80) || j != screen->max_col + 1)
		    RequestResize(termw, -1, j, TRUE);
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
#if OPT_BLINK_CURS
	case 12:		/* att610: Start/stop blinking cursor */
	    DoRM(DP_CRS_BLINK, screen->cursor_blink);
	    SetCursorBlink(screen, screen->cursor_blink);
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
	XReconfigureWMWindow(
				screen->display,
				VShellWindow,
				DefaultScreen(screen->display),
				value_mask,
				&values);
	break;

    case 4:			/* Resize the window to given size in pixels */
	RequestResize(termw, param[1], param[2], FALSE);
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
	RequestResize(termw, param[1], param[2], TRUE);
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
	reply.a_param[1] = screen->max_row + 1;
	reply.a_param[2] = screen->max_col + 1;
	reply.a_inters = 0;
	reply.a_final = 't';
	unparseseq(&reply, screen->respond);
	break;

#if OPT_MAXIMIZE
    case 19:			/* Report the screen's size, in characters */
	if (!QueryMaximize(screen, &root_height, &root_width)) {
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
	    RequestResize(termw, param[0], -1, TRUE);
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
unparseseq(register ANSI * ap, int fd)
{
    register int c;
    register int i;
    register int inters;

    unparseputc1(c = ap->a_type, fd);
    if (c == ESC
	|| c == DCS
	|| c == CSI
	|| c == OSC
	|| c == PM
	|| c == APC
	|| c == SS3) {
	if (ap->a_pintro != 0)
	    unparseputc((char) ap->a_pintro, fd);
	for (i = 0; i < ap->a_nparam; ++i) {
	    if (i != 0)
		unparseputc(';', fd);
	    unparseputn((unsigned int) ap->a_param[i], fd);
	}
	inters = ap->a_inters;
	for (i = 3; i >= 0; --i) {
	    c = CharOf(inters >> (8 * i));
	    if (c != 0)
		unparseputc(c, fd);
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
    register TScreen *screen = &term->screen;
    IChar buf[2];
    register int i = 1;

#if OPT_TCAP_QUERY
    /*
     * If we're returning a termcap string, it has to be translated since
     * a DCS must not contain any characters except for the normal 7-bit
     * printable ASCII (counting tab, carriage return, etc).  For now,
     * just use hexadecimal for the whole thing.
     */
    if (screen->tc_query >= 0) {
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
	dotext(screen, screen->gsets[(int) (screen->curgl)], buf, i);
    }
}

void
unparseputs(char *s, int fd)
{
    while (*s)
	unparseputc(*s++, fd);
}

void
ToggleAlternate(register TScreen * screen)
{
    if (screen->alternate)
	FromAlternate(screen);
    else
	ToAlternate(screen);
}

static void
ToAlternate(register TScreen * screen)
{
    if (screen->alternate)
	return;
    TRACE(("ToAlternate\n"));
    if (!screen->altbuf)
	screen->altbuf = Allocate(screen->max_row + 1, screen->max_col
				  + 1, &screen->abuf_address);
    SwitchBufs(screen);
    screen->alternate = TRUE;
    update_altscreen();
}

static void
FromAlternate(register TScreen * screen)
{
    if (!screen->alternate)
	return;
    TRACE(("FromAlternate\n"));
    if (screen->scroll_amt)
	FlushScroll(screen);
    screen->alternate = FALSE;
    SwitchBufs(screen);
    update_altscreen();
}

static void
SwitchBufs(register TScreen * screen)
{
    register int rows, top;

    if (screen->cursor_state)
	HideCursor();

    rows = screen->max_row + 1;
    SwitchBufPtrs(screen);
    TrackText(0, 0, 0, 0);	/* remove any highlighting */

    if ((top = -screen->topline) < rows) {
	if (screen->scroll_amt)
	    FlushScroll(screen);
	if (top == 0)
	    XClearWindow(screen->display, VWindow(screen));
	else
	    XClearArea(
			  screen->display,
			  VWindow(screen),
			  (int) OriginX(screen),
			  (int) top * FontHeight(screen) + screen->border,
			  (unsigned) Width(screen),
			  (unsigned) (rows - top) * FontHeight(screen),
			  FALSE);
    }
    ScrnRefresh(screen, 0, 0, rows, screen->max_col + 1, False);
}

/* swap buffer line pointers between alt and regular screens */
void
SwitchBufPtrs(register TScreen * screen)
{
    size_t len = ScrnPointers(screen, screen->max_row + 1);

    memcpy((char *) screen->save_ptr, (char *) screen->visbuf, len);
    memcpy((char *) screen->visbuf, (char *) screen->altbuf, len);
    memcpy((char *) screen->altbuf, (char *) screen->save_ptr, len);
}

void
VTRun(void)
{
    register TScreen *screen = &term->screen;
#if OPT_TEK4014
    register int i;
#endif

    if (!screen->Vshow) {
	set_vt_visibility(TRUE);
    }
    update_vttekmode();
    update_vtshow();
    update_tekshow();
    set_vthide_sensitivity();

    if (screen->allbuf == NULL)
	VTallocbuf();

    screen->cursor_state = OFF;
    screen->cursor_set = ON;
    StartBlinking(screen);

    initPtyData(&VTbuffer);
#if OPT_TEK4014
    while (Tpushb > Tpushback) {
	*(VTbuffer.ptr)++ = *--Tpushb;
	VTbuffer.cnt++;
    }
    VTbuffer.cnt += (i = (Tbuffer ? Tbuffer->cnt : 0));
    for (; i > 0; i--)
	*(VTbuffer.ptr)++ = *(Tbuffer->ptr)++;
    VTbuffer.ptr = DecodedData(&VTbuffer);
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
    register TScreen *screen = &term->screen;

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
    register TScreen *screen = &term->screen;
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
    if (XtIsRealized(w))
	ScreenResize(&term->screen, term->core.width, term->core.height, &term->flags);
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
		value = screen->max_row + 1;
	    value *= FontHeight(screen);
	    value += (2 * screen->border);
	    if (!okDimension(value, askedHeight))
		return;
	}

	if ((value = cols) != 0) {
	    if (cols < 0)
		value = screen->max_col + 1;
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

    status = XtMakeResizeRequest(
				    (Widget) termw,
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
	XSetWMNormalHints(screen->display, VShellWindow, &sizehints);
    }
#endif

    XSync(screen->display, FALSE);	/* synchronize */
    if (XtAppPending(app_con))
	xevents();
}

static String xterm_trans =
"<ClientMessage>WM_PROTOCOLS: DeleteWindow()\n\
     <MappingNotify>: KeyboardMapping()\n";

int
VTInit(void)
{
    register TScreen *screen = &term->screen;
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
    register TScreen *screen = &term->screen;
    int nrows = screen->max_row + 1;

    /* allocate screen buffer now, if necessary. */
    if (screen->scrollWidget)
	nrows += screen->savelines;
    screen->allbuf = Allocate(nrows, screen->max_col + 1,
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

#if OPT_TRACE
#define init_Bres(name) \
	TRACE(("init " #name " = %s\n", \
		(wnew->name = request->name) != FALSE \
			? "on" : "off"))
#define init_Cres(name) \
	TRACE(("init " #name " = %#lx\n", \
		wnew->name = request->name))
#define init_Ires(name) \
	TRACE(("init " #name " = %d\n", \
		wnew->name = request->name))
#define init_Sres(name) \
	TRACE(("init " #name " = \"%s\"\n", \
		(wnew->name = x_strtrim(request->name)) != NULL \
			? wnew->name : "<null>"))
#else
#define init_Bres(name) wnew->name = request->name
#define init_Cres(name) wnew->name = request->name
#define init_Ires(name) wnew->name = request->name
#define init_Sres(name) wnew->name = x_strtrim(request->name)
#endif

#if OPT_WIDE_CHARS
static void
VTInitialize_locale(XtermWidget request)
{
    char *locale;
    Boolean is_utf8;

    if ((locale = getenv("LC_ALL")) == 0 || *locale == '\0')
	if ((locale = getenv("LC_CTYPE")) == 0 || *locale == '\0')
	    if ((locale = getenv("LANG")) == 0 || *locale == '\0')
		locale = "";
#ifdef HAVE_LANGINFO_CODESET	/* FIXME: not available yet */
    is_utf8 = (strcmp(nl_langinfo(CODESET), "UTF-8") == 0);
#else
    is_utf8 = (strstr(locale, "UTF-8") != NULL);
#endif

#if OPT_LUIT_PROG
    request->misc.callfilter = 0;
    request->misc.use_encoding = 0;

    if (x_strcasecmp(request->misc.locale_str, "TRUE") == 0 ||
	x_strcasecmp(request->misc.locale_str, "ON") == 0 ||
	x_strcasecmp(request->misc.locale_str, "YES") == 0 ||
	x_strcasecmp(request->misc.locale_str, "AUTO") == 0 ||
	strcmp(request->misc.locale_str, "1") == 0) {
	/* when true ... fully obeying LC_CTYPE locale */
	request->misc.callfilter = is_utf8 ? 0 : 1;
	request->screen.utf8_mode = 2;
    } else if (x_strcasecmp(request->misc.locale_str, "FALSE") == 0 ||
	       x_strcasecmp(request->misc.locale_str, "OFF") == 0 ||
	       x_strcasecmp(request->misc.locale_str, "NO") == 0 ||
	       strcmp(request->misc.locale_str, "0") == 0) {
	/* when false ... original value of utf8_mode is effective */
	if (request->screen.utf8_mode == 3) {
	    request->screen.utf8_mode = is_utf8 ? 2 : 0;
	}
    } else if (x_strcasecmp(request->misc.locale_str, "MEDIUM") == 0 ||
	       x_strcasecmp(request->misc.locale_str, "SEMIAUTO") == 0) {
	/* when medium ... obeying locale only for UTF-8 and Asian */
	if (is_utf8) {
	    request->screen.utf8_mode = 2;
	} else if (
#ifdef MB_CUR_MAX
		      MB_CUR_MAX > 1 ||
#else
		      !strncmp(locale, "ja", 2) ||
		      !strncmp(locale, "ko", 2) ||
		      !strncmp(locale, "zh", 2) ||
#endif
		      !strncmp(locale, "th", 2) ||
		      !strncmp(locale, "vi", 2)) {
	    request->misc.callfilter = 1;
	    request->screen.utf8_mode = 2;
	} else {
	    request->screen.utf8_mode = 0;
	}
    } else if (x_strcasecmp(request->misc.locale_str, "UTF-8") == 0 ||
	       x_strcasecmp(request->misc.locale_str, "UTF8") == 0) {
	/* when UTF-8 ... UTF-8 mode */
	request->screen.utf8_mode = 2;
    } else {
	/* other words are regarded as encoding name passed to luit */
	request->misc.callfilter = 1;
	request->screen.utf8_mode = 2;
	request->misc.use_encoding = 1;
    }
#else
    if (request->screen.utf8_mode == 3) {
	request->screen.utf8_mode = is_utf8 ? 2 : 0;
    }
#endif /* OPT_LUIT_PROG */

}
#endif

/* ARGSUSED */
static void
VTInitialize(Widget wrequest,
	     Widget new_arg,
	     ArgList args GCC_UNUSED,
	     Cardinal * num_args GCC_UNUSED)
{
    XtermWidget request = (XtermWidget) wrequest;
    XtermWidget wnew = (XtermWidget) new_arg;
    Widget my_parent = SHELL_OF(wnew);
    int i;
#if OPT_ISO_COLORS
    Boolean color_ok;
#endif
    char *s;

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
    TRACE(("Color resource initialization:\n"));
    TRACE(("   Default foreground %#lx\n", wnew->dft_foreground));
    TRACE(("   Default background %#lx\n", wnew->dft_background));
    TRACE(("   Screen foreground  %#lx\n", request->screen.foreground));
    TRACE(("   Screen background  %#lx\n", request->core.background_pixel));

    wnew->screen.mouse_button = -1;
    wnew->screen.mouse_row = -1;
    wnew->screen.mouse_col = -1;

#if OPT_BOX_CHARS
    init_Bres(screen.force_box_chars);
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
    init_Cres(screen.foreground);
    init_Cres(screen.cursorcolor);
#if OPT_BLINK_CURS
    init_Bres(screen.cursor_blink);
    init_Ires(screen.cursor_on);
    init_Ires(screen.cursor_off);
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
    init_Bres(screen.marginbell);
    init_Cres(screen.mousecolor);
    init_Cres(screen.mousecolorback);
    init_Bres(screen.multiscroll);
    init_Ires(screen.nmarginbell);
    init_Ires(screen.savelines);
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
    init_Bres(screen.allowSendEvents);
    init_Bres(screen.allowWindowOps);
#ifndef NO_ACTIVE_ICON
    wnew->screen.fnt_icon = request->screen.fnt_icon;
#endif /* NO_ACTIVE_ICON */
    init_Bres(misc.titeInhibit);
    init_Bres(misc.tiXtraScroll);
    init_Bres(misc.dynamicColors);
    for (i = fontMenu_font1; i <= fontMenu_lastBuiltin; i++) {
	init_Sres(screen.menu_font_names[i]);
    }
    /* set default in realize proc */
    wnew->screen.menu_font_names[fontMenu_fontdefault] = NULL;
    wnew->screen.menu_font_names[fontMenu_fontescape] = NULL;
    wnew->screen.menu_font_names[fontMenu_fontsel] = NULL;
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
    init_Bres(screen.colorRVMode);

    for (i = 0, color_ok = False; i < MAXCOLORS; i++) {
	wnew->screen.Acolors[i] = request->screen.Acolors[i];
#if OPT_COLOR_RES
	TRACE(("Acolors[%d] = %s\n", i, request->screen.Acolors[i].resource));
	wnew->screen.Acolors[i].mode = False;
	if (strcmp(wnew->screen.Acolors[i].resource, XtDefaultForeground))
	    color_ok = True;
#else
	TRACE(("Acolors[%d] = %#lx\n", i, request->screen.Acolors[i]));
	if (wnew->screen.Acolors[i] != wnew->dft_foreground
	    && wnew->screen.Acolors[i] != request->screen.foreground
	    && wnew->screen.Acolors[i] != request->core.background_pixel)
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

#if OPT_HIGHLIGHT_COLOR
    init_Cres(screen.highlightcolor);
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

    init_Bres(screen.vt100_graphics);
    init_Bres(screen.wide_chars);
    init_Bres(misc.cjk_width);
    if (request->screen.utf8_mode) {
	wnew->screen.wide_chars = True;
	wnew->screen.utf8_mode = 2;	/* disable further change */
	xtermLoadVTFonts(wnew, "utf8Fonts", "Utf8Fonts");
    } else {
	wnew->screen.utf8_mode = 0;
    }
    TRACE(("initialized UTF-8 mode to %d\n", wnew->screen.utf8_mode));

    if (wnew->screen.wide_chars != False)
	wnew->num_ptrs = (OFF_COM2H + 1);
#endif /* OPT_WIDE_CHARS */

    init_Bres(screen.bold_mode);
    init_Bres(screen.underline);
#ifdef XRENDERFONT
    wnew->screen.renderFont = 0;
    wnew->screen.renderFontBold = 0;
    wnew->screen.renderDraw = 0;
#endif

    wnew->cur_foreground = 0;
    wnew->cur_background = 0;

    wnew->keyboard.flags = MODE_SRM;
    if (wnew->screen.backarrow_key)
	wnew->keyboard.flags |= MODE_DECBKM;
    TRACE(("initialized DECBKM %s\n",
	   (wnew->keyboard.flags & MODE_DECBKM) ? "on" : "off"));

    /* look for focus related events on the shell, because we need
     * to care about the shell's border being part of our focus.
     */
    XtAddEventHandler(my_parent, EnterWindowMask, FALSE,
		      HandleEnterWindow, (Opaque) NULL);
    XtAddEventHandler(my_parent, LeaveWindowMask, FALSE,
		      HandleLeaveWindow, (Opaque) NULL);
    XtAddEventHandler(my_parent, FocusChangeMask, FALSE,
		      HandleFocusChange, (Opaque) NULL);
    XtAddEventHandler((Widget) wnew, 0L, TRUE,
		      VTNonMaskableEvent, (Opaque) NULL);
    XtAddEventHandler((Widget) wnew, PropertyChangeMask, FALSE,
		      HandleBellPropertyChange, (Opaque) NULL);

#if HANDLE_STRUCT_NOTIFY
#if OPT_TOOLBAR
    wnew->screen.fullVwin.menu_bar = request->screen.fullVwin.menu_bar;
    init_Ires(screen.fullVwin.menu_height);
#else
    /* Flag icon name with "***"  on window output when iconified.
     * Put in a handler that will tell us when we get Map/Unmap events.
     */
    if (zIconBeep)
#endif
	XtAddEventHandler(my_parent, StructureNotifyMask, FALSE,
			  HandleStructNotify, (XtPointer) 0);
#endif /* HANDLE_STRUCT_NOTIFY */

    wnew->screen.bellInProgress = FALSE;

    set_character_class(wnew->screen.charClass);

    /* create it, but don't realize it */
    ScrollBarOn(wnew, TRUE, FALSE);

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
    unsigned int width, height;
    register TScreen *screen = &term->screen;
    int xpos, ypos, pr;
    XSizeHints sizehints;
    int scrollbar_width;

    TRACE(("VTRealize\n"));

    TabReset(term->tabs);

    screen->menu_font_names[fontMenu_fontdefault] = term->misc.default_font.f_n;
    screen->fnt_norm = NULL;
    screen->fnt_bold = NULL;
#if OPT_WIDE_CHARS
    screen->fnt_dwd = NULL;
    screen->fnt_dwdb = NULL;
#endif
    if (!xtermLoadFont(screen,
		       &(term->misc.default_font),
		       False, 0)) {
	if (XmuCompareISOLatin1(term->misc.default_font.f_n, "fixed") != 0) {
	    fprintf(stderr,
		    "%s:  unable to open font \"%s\", trying \"fixed\"....\n",
		    xterm_name, term->misc.default_font.f_n);
	    (void) xtermLoadFont(screen,
				 xtermFontName("fixed"),
				 False, 0);
	    screen->menu_font_names[fontMenu_fontdefault] = "fixed";
	}
    }

    /* really screwed if we couldn't open default font */
    if (!screen->fnt_norm) {
	fprintf(stderr, "%s:  unable to locate a suitable font\n",
		xterm_name);
	Exit(1);
    }

    /* making cursor */
    if (!screen->pointer_cursor)
	screen->pointer_cursor = make_colored_cursor(XC_xterm,
						     screen->mousecolor,
						     screen->mousecolorback);
    else
	recolor_cursor(screen->pointer_cursor,
		       screen->mousecolor, screen->mousecolorback);

    scrollbar_width = (term->misc.scrollbar ?
		       screen->scrollWidget->core.width : 0);

    /* set defaults */
    xpos = 1;
    ypos = 1;
    width = 80;
    height = 24;
    pr = XParseGeometry(term->misc.geo_metry, &xpos, &ypos,
			&width, &height);
    screen->max_col = (width - 1);	/* units in character cells */
    screen->max_row = (height - 1);	/* units in character cells */
    xtermUpdateFontInfo(&term->screen, False);

    width = screen->fullVwin.fullwidth;
    height = screen->fullVwin.fullheight;

    if ((pr & XValue) && (XNegative & pr))
	xpos += DisplayWidth(screen->display, DefaultScreen(screen->display))
	    - width - (XtParent(term)->core.border_width * 2);
    if ((pr & YValue) && (YNegative & pr))
	ypos += DisplayHeight(screen->display, DefaultScreen(screen->display))
	    - height - (XtParent(term)->core.border_width * 2);

    /* set up size hints for window manager; min 1 char by 1 char */
    bzero(&sizehints, sizeof(sizehints));
    sizehints.base_width = 2 * screen->border + scrollbar_width;
    sizehints.base_height = 2 * screen->border;
    sizehints.width_inc = FontWidth(screen);
    sizehints.height_inc = FontHeight(screen);
    sizehints.min_width = sizehints.base_width + sizehints.width_inc;
    sizehints.min_height = sizehints.base_height + sizehints.height_inc;
    sizehints.flags = (PBaseSize | PMinSize | PResizeInc);
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
    sizehints.width = width;
    sizehints.height = height;
    if ((WidthValue & pr) || (HeightValue & pr))
	sizehints.flags |= USSize;
    else
	sizehints.flags |= PSize;

    (void) XtMakeResizeRequest((Widget) term,
			       (Dimension) width, (Dimension) height,
			       &term->core.width, &term->core.height);

    /* XXX This is bogus.  We are parsing geometries too late.  This
     * is information that the shell widget ought to have before we get
     * realized, so that it can do the right thing.
     */
    if (sizehints.flags & USPosition)
	XMoveWindow(XtDisplay(term), XtWindow(XtParent(term)),
		    sizehints.x, sizehints.y);

    XSetWMNormalHints(XtDisplay(term), XtWindow(XtParent(term)),
		      &sizehints);
    XFlush(XtDisplay(term));	/* get it out to window manager */

    /* use ForgetGravity instead of SouthWestGravity because translating
       the Expose events for ConfigureNotifys is too hard */
    values->bit_gravity = ((term->misc.resizeGravity == NorthWestGravity)
			   ? NorthWestGravity
			   : ForgetGravity);
    term->screen.fullVwin.window = XtWindow(term) =
	XCreateWindow(XtDisplay(term), XtWindow(XtParent(term)),
		      term->core.x, term->core.y,
		      term->core.width, term->core.height, term->core.border_width,
		      (int) term->core.depth,
		      InputOutput, CopyFromParent,
		      *valuemask | CWBitGravity, values);
    screen->event_mask = values->event_mask;

#ifndef NO_ACTIVE_ICON
    if (term->misc.active_icon && screen->fnt_icon) {
	int iconX = 0, iconY = 0;
	Widget shell = XtParent(term);
	unsigned long mask;
	XGCValues xgcv;

	XtVaGetValues(shell, XtNiconX, &iconX, XtNiconY, &iconY, (XtPointer) 0);
	xtermComputeFontInfo(screen, &(screen->iconVwin), screen->fnt_icon, 0);

	/* since only one client is permitted to select for Button
	 * events, we have to let the window manager get 'em...
	 */
	values->event_mask &= ~(ButtonPressMask | ButtonReleaseMask);
	values->border_pixel = term->misc.icon_border_pixel;

	screen->iconVwin.window =
	    XCreateWindow(XtDisplay(term),
			  RootWindowOfScreen(XtScreen(shell)),
			  iconX, iconY,
			  screen->iconVwin.fullwidth,
			  screen->iconVwin.fullheight,
			  term->misc.icon_border_width,
			  (int) term->core.depth,
			  InputOutput, CopyFromParent,
			  *valuemask | CWBitGravity | CWBorderPixel,
			  values);
	XtVaSetValues(shell,
		      XtNiconWindow, screen->iconVwin.window,
		      (XtPointer) 0);
	XtRegisterDrawable(XtDisplay(term), screen->iconVwin.window, w);

	mask = (GCFont | GCForeground | GCBackground |
		GCGraphicsExposures | GCFunction);

	xgcv.font = screen->fnt_icon->fid;
	xgcv.foreground = screen->foreground;
	xgcv.background = term->core.background_pixel;
	xgcv.graphics_exposures = TRUE;		/* default */
	xgcv.function = GXcopy;

	screen->iconVwin.normalGC =
	    screen->iconVwin.normalboldGC =
	    XtGetGC(shell, mask, &xgcv);

	xgcv.foreground = term->core.background_pixel;
	xgcv.background = screen->foreground;

	screen->iconVwin.reverseGC =
	    screen->iconVwin.reverseboldGC =
	    XtGetGC(shell, mask, &xgcv);
    } else {
	term->misc.active_icon = False;
    }
#endif /* NO_ACTIVE_ICON */

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
    VTInitI18N();
#else
    term->screen.xic = NULL;
#endif
#if OPT_NUM_LOCK
    VTInitModifiers();
#endif

    set_cursor_gcs(screen);

    /* Reset variables used by ANSI emulation. */

    resetCharsets(screen);

    XDefineCursor(screen->display, VShellWindow, screen->pointer_cursor);

    screen->cur_col = screen->cur_row = 0;
    screen->max_col = Width(screen) / screen->fullVwin.f_width - 1;
    screen->top_marg = 0;
    screen->bot_marg = screen->max_row = Height(screen) /
	screen->fullVwin.f_height - 1;

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

    if (term->misc.scrollbar) {
	screen->fullVwin.sb_info.width = 0;
	ScrollBarOn(term, FALSE, TRUE);
    }
    CursorSave(term);
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
    XIM xim = (XIM) NULL;
    XIMStyles *xim_styles;
    XIMStyle input_style = 0;
    Boolean found;
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

    if (!term->misc.open_im) {
	return;
    }

    if (!term->misc.input_method || !*term->misc.input_method) {
	if ((p = XSetLocaleModifiers("")) != NULL && *p)
	    xim = XOpenIM(XtDisplay(term), NULL, NULL, NULL);
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
		strncat(t, s, end - s);

		if ((p = XSetLocaleModifiers(t)) != 0 && *p
		    && (xim = XOpenIM(XtDisplay(term), NULL, NULL, NULL)) != 0)
		    break;

	    }
	    s = ns + 1;
	}
	MyStackFree(t, buf);
    }

    if (xim == NULL && (p = XSetLocaleModifiers("@im=none")) != NULL && *p)
	xim = XOpenIM(XtDisplay(term), NULL, NULL, NULL);

    if (!xim) {
	fprintf(stderr, "Failed to open input method\n");
	return;
    }
    TRACE(("VTInitI18N opened input method\n"));

    if (XGetIMValues(xim, XNQueryInputStyle, &xim_styles, NULL)
	|| !xim_styles
	|| !xim_styles->count_styles) {
	fprintf(stderr, "input method doesn't support any style\n");
	XCloseIM(xim);
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
		    && !strncmp(s, known_style[i].name, end - s)) {
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
	XCloseIM(xim);
	return;
    }

    /*
     * Check for styles we do not yet support.
     */
    if (input_style == (XIMPreeditArea | XIMStatusArea)) {
	fprintf(stderr,
		"This program doesn't support the 'OffTheSpot' preedit type\n");
	XCloseIM(xim);
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
	    XCloseIM(xim);
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
	term->screen.xic = XCreateIC(xim,
				     XNInputStyle, input_style,
				     XNClientWindow, XtWindow(term),
				     XNFocusWindow, XtWindow(term),
				     XNPreeditAttributes, p_list,
				     NULL);
    } else {
	term->screen.xic = XCreateIC(xim, XNInputStyle, input_style,
				     XNClientWindow, XtWindow(term),
				     XNFocusWindow, XtWindow(term),
				     NULL);
    }

    if (!term->screen.xic) {
	fprintf(stderr, "Failed to create input context\n");
	XCloseIM(xim);
    }
#if defined(USE_XIM_INSTANTIATE_CB)
    else {
	XIMCallback destroy_cb;

	destroy_cb.callback = xim_destroy_cb;
	destroy_cb.client_data = NULL;
	if (XSetIMValues(xim, XNDestroyCallback, &destroy_cb, NULL))
	    fprintf(stderr, "Could not set destroy callback to IM\n");
    }
#endif

    return;
}

static void
VTInitI18N(void)
{
    xim_real_init();

#if defined(USE_XIM_INSTANTIATE_CB)
    if (term->screen.xic == NULL)
	XRegisterIMInstantiateCallback(XtDisplay(term), NULL, NULL, NULL,
				       xim_instantiate_cb, NULL);
#endif
}
#endif /* OPT_I18N_SUPPORT && OPT_INPUT_METHOD */

static Boolean
VTSetValues(Widget cur,
	    Widget request GCC_UNUSED,
	    Widget wnew,
	    ArgList args GCC_UNUSED,
	    Cardinal * num_args GCC_UNUSED)
{
    XtermWidget curvt = (XtermWidget) cur;
    XtermWidget newvt = (XtermWidget) wnew;
    Boolean refresh_needed = FALSE;
    Boolean fonts_redone = FALSE;

    if (curvt->core.background_pixel != newvt->core.background_pixel
	|| curvt->screen.foreground != newvt->screen.foreground
	|| curvt->screen.menu_font_names[curvt->screen.menu_font_number]
	!= newvt->screen.menu_font_names[newvt->screen.menu_font_number]
	|| curvt->misc.default_font.f_n != newvt->misc.default_font.f_n) {
	if (curvt->misc.default_font.f_n != newvt->misc.default_font.f_n)
	    newvt->screen.menu_font_names[fontMenu_fontdefault] = newvt->misc.default_font.f_n;
	if (xtermLoadFont(&newvt->screen,
			  xtermFontName(newvt->screen.menu_font_names[curvt->screen.menu_font_number]),
			  TRUE, newvt->screen.menu_font_number)) {
	    /* resizing does the redisplay, so don't ask for it here */
	    refresh_needed = TRUE;
	    fonts_redone = TRUE;
	} else if (curvt->misc.default_font.f_n != newvt->misc.default_font.f_n)
	    newvt->screen.menu_font_names[fontMenu_fontdefault] = curvt->misc.default_font.f_n;
    }
    if (!fonts_redone
	&& curvt->screen.cursorcolor != newvt->screen.cursorcolor) {
	set_cursor_gcs(&newvt->screen);
	refresh_needed = TRUE;
    }
    if (curvt->misc.re_verse != newvt->misc.re_verse) {
	newvt->flags ^= REVERSE_VIDEO;
	ReverseVideo(newvt);
	newvt->misc.re_verse = !newvt->misc.re_verse;	/* ReverseVideo toggles */
	refresh_needed = TRUE;
    }
    if (curvt->screen.mousecolor != newvt->screen.mousecolor
	|| curvt->screen.mousecolorback != newvt->screen.mousecolorback) {
	recolor_cursor(newvt->screen.pointer_cursor,
		       newvt->screen.mousecolor,
		       newvt->screen.mousecolorback);
	refresh_needed = TRUE;
    }
    if (curvt->misc.scrollbar != newvt->misc.scrollbar) {
	ToggleScrollBar(newvt);
    }

    return refresh_needed;
}

/*
 * Shows cursor at new cursor position in screen.
 */
void
ShowCursor(void)
{
    register TScreen *screen = &term->screen;
    register int x, y, flags;
    Char clo;
    int fg_bg = 0;
    GC currentGC;
    Boolean in_selection;
    Boolean reversed;
    Pixel fg_pix;
    Pixel bg_pix;
    Pixel tmp;
#if OPT_HIGHLIGHT_COLOR
    Pixel hi_pix = screen->highlightcolor;
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

    flags = SCRN_BUF_ATTRS(screen, screen->cursor_row)[cursor_col];

    if (clo == 0
#if OPT_WIDE_CHARS
	&& chi == 0
#endif
	) {
	clo = ' ';
    }

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

    if (screen->cur_row > screen->endHRow ||
	(screen->cur_row == screen->endHRow &&
	 screen->cur_col >= screen->endHCol) ||
	screen->cur_row < screen->startHRow ||
	(screen->cur_row == screen->startHRow &&
	 screen->cur_col < screen->startHCol))
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
		currentGC = screen->cursorGC;
	    } else {
		if (flags & (BOLD | BLINK)) {
		    currentGC = NormalBoldGC(screen);
		} else {
		    currentGC = NormalGC(screen);
		}
	    }
#if OPT_HIGHLIGHT_COLOR
	    if (hi_pix != screen->foreground
		&& hi_pix != fg_pix
		&& hi_pix != bg_pix
		&& hi_pix != term->dft_foreground) {
		bg_pix = fg_pix;
		fg_pix = hi_pix;
	    }
#endif
	    EXCHANGE(fg_pix, bg_pix, tmp)
	} else {		/* normal video */
	    if (screen->reversecursorGC) {
		currentGC = screen->reversecursorGC;
	    } else {
		if (flags & (BOLD | BLINK)) {
		    currentGC = ReverseBoldGC(screen);
		} else {
		    currentGC = ReverseGC(screen);
		}
	    }
	}
	if (screen->cursorcolor == term->dft_foreground) {
	    XSetBackground(screen->display, currentGC, fg_pix);
	}
	XSetForeground(screen->display, currentGC, bg_pix);
    } else {			/* not selected */
	if (reversed) {		/* text is reverse video */
#if OPT_HIGHLIGHT_COLOR
	    if (hi_pix != screen->foreground
		&& hi_pix != fg_pix
		&& hi_pix != bg_pix
		&& hi_pix != term->dft_foreground) {
		bg_pix = fg_pix;
		fg_pix = hi_pix;
	    }
#endif
	    currentGC = ReverseGC(screen);
	    XSetForeground(screen->display, currentGC, bg_pix);
	    XSetBackground(screen->display, currentGC, fg_pix);
	} else {		/* normal video */
	    currentGC = NormalGC(screen);
	    XSetForeground(screen->display, currentGC, fg_pix);
	    XSetBackground(screen->display, currentGC, bg_pix);
	}
    }

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
    screen->cursor_state = ON;
}

/*
 * hide cursor at previous cursor position in screen.
 */
void
HideCursor(void)
{
    register TScreen *screen = &term->screen;
    GC currentGC;
    register int flags;
    register int fg_bg = 0;
    int x, y;
    Char clo;
    Boolean in_selection;
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

    if (screen->cursor_row > screen->endHRow ||
	(screen->cursor_row == screen->endHRow &&
	 screen->cursor_col >= screen->endHCol) ||
	screen->cursor_row < screen->startHRow ||
	(screen->cursor_row == screen->startHRow &&
	 screen->cursor_col < screen->startHCol))
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

#if OPT_BLINK_CURS
static void
StartBlinking(TScreen * screen)
{
    if (screen->cursor_blink
	&& screen->cursor_timer == 0) {
	unsigned long interval = (screen->cursor_state == ON ?
				  screen->cursor_on : screen->cursor_off);
	if (interval == 0)	/* wow! */
	    interval = 1;	/* let's humor him anyway */
	screen->cursor_timer = XtAppAddTimeOut(app_con,
					       interval,
					       BlinkCursor,
					       screen);
    }
}

static void
StopBlinking(TScreen * screen)
{
    if (screen->cursor_timer)
	XtRemoveTimeOut(screen->cursor_timer);
    screen->cursor_timer = 0;
}

/*
 * Blink the cursor by alternately showing/hiding cursor.  We leave the timer
 * running all the time (even though that's a little inefficient) to make the
 * logic simple.
 */
static void
BlinkCursor(XtPointer closure, XtIntervalId * id GCC_UNUSED)
{
    TScreen *screen = (TScreen *) closure;

    screen->cursor_timer = 0;
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
    StartBlinking(screen);
}
#endif /* OPT_BLINK_CURS */

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
    register TScreen *screen = &term->screen;

    if (saved) {
	screen->savedlines = 0;
	ScrollBarDrawThumb(screen->scrollWidget);
    }

    /* make cursor visible */
    screen->cursor_set = ON;

    /* reset scrolling region */
    screen->top_marg = 0;
    screen->bot_marg = screen->max_row;

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
	Bell(XkbBI_TerminalBell, 0);

	/* reset the mouse mode */
	screen->send_mouse_pos = MOUSE_OFF;
	waitingForTrackInfo = FALSE;
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
	       (term->keyboard.flags & MODE_DECBKM) ? "on" : "off"));
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
	    Dimension junk;
	    XtMakeResizeRequest(
				   (Widget) term,
				   (Dimension) 80 * FontWidth(screen)
				   + 2 * screen->border + ScrollbarWidth(screen),
				   (Dimension) FontHeight(screen)
				   * (screen->max_row + 1) + 2 * screen->border,
				   &junk, &junk);
	    XSync(screen->display, FALSE);	/* synchronize */
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
set_character_class(register char *s)
{
    register int i;		/* iterator, index into s */
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
		   Cardinal * param_count)
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
    XtGetSubresources(w, (XtPointer) & keymap, pmapName, pmapClass,
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
	   Cardinal * param_count)	/* 0 or 1 */
{
    int percent = (*param_count) ? atoi(params[0]) : 0;

    Bell(XkbBI_TerminalBell, percent);
}

/* ARGSUSED */
static void
HandleVisualBell(Widget w GCC_UNUSED,
		 XEvent * event GCC_UNUSED,
		 String * params GCC_UNUSED,
		 Cardinal * param_count GCC_UNUSED)
{
    VisualBell();
}

/* ARGSUSED */
static void
HandleIgnore(Widget w,
	     XEvent * event,
	     String * params GCC_UNUSED,
	     Cardinal * param_count GCC_UNUSED)
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
	if (!xtermLoadFont(&term->screen,
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
	term->screen.menu_font_names[fontMenu_fontsel] =
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
    Pixel cc = screen->cursorcolor;
    Pixel fg = screen->foreground;
    Pixel bg = term->core.background_pixel;
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
