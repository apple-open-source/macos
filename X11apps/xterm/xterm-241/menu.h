/* $XTermId: menu.h,v 1.111 2007/11/26 18:09:53 tom Exp $ */

/*

Copyright 1999-2006,2007 by Thomas E. Dickey

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the above listed
copyright holder(s) not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD
TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


Copyright 1989  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

#ifndef included_menu_h
#define included_menu_h

#include <xterm.h>

typedef struct _MenuEntry {
    char *name;
    void (*function) PROTO_XT_CALLBACK_ARGS;
    Widget widget;
} MenuEntry;

extern MenuEntry mainMenuEntries[], vtMenuEntries[];
extern MenuEntry fontMenuEntries[];
#if OPT_TEK4014
extern MenuEntry tekMenuEntries[];
#endif

extern void Handle8BitControl      PROTO_XT_ACTIONS_ARGS;
extern void HandleAllow132         PROTO_XT_ACTIONS_ARGS;
extern void HandleAllowSends       PROTO_XT_ACTIONS_ARGS;
extern void HandleAltEsc           PROTO_XT_ACTIONS_ARGS;
extern void HandleAltScreen        PROTO_XT_ACTIONS_ARGS;
extern void HandleAppCursor        PROTO_XT_ACTIONS_ARGS;
extern void HandleAppKeypad        PROTO_XT_ACTIONS_ARGS;
extern void HandleAutoLineFeed     PROTO_XT_ACTIONS_ARGS;
extern void HandleAutoWrap         PROTO_XT_ACTIONS_ARGS;
extern void HandleBackarrow        PROTO_XT_ACTIONS_ARGS;
extern void HandleBellIsUrgent     PROTO_XT_ACTIONS_ARGS;
extern void HandleClearSavedLines  PROTO_XT_ACTIONS_ARGS;
extern void HandleCreateMenu       PROTO_XT_ACTIONS_ARGS;
extern void HandleCursesEmul       PROTO_XT_ACTIONS_ARGS;
extern void HandleCursorBlink      PROTO_XT_ACTIONS_ARGS;
extern void HandleDeleteIsDEL      PROTO_XT_ACTIONS_ARGS;
extern void HandleFontBoxChars     PROTO_XT_ACTIONS_ARGS;
extern void HandleFontDoublesize   PROTO_XT_ACTIONS_ARGS;
extern void HandleFontLoading      PROTO_XT_ACTIONS_ARGS;
extern void HandleHardReset        PROTO_XT_ACTIONS_ARGS;
extern void HandleHpFunctionKeys   PROTO_XT_ACTIONS_ARGS;
extern void HandleJumpscroll       PROTO_XT_ACTIONS_ARGS;
extern void HandleLogging          PROTO_XT_ACTIONS_ARGS;
extern void HandleMarginBell       PROTO_XT_ACTIONS_ARGS;
extern void HandleMetaEsc          PROTO_XT_ACTIONS_ARGS;
extern void HandleNumLock          PROTO_XT_ACTIONS_ARGS;
extern void HandleOldFunctionKeys  PROTO_XT_ACTIONS_ARGS;
extern void HandlePopupMenu        PROTO_XT_ACTIONS_ARGS;
extern void HandlePrintControlMode PROTO_XT_ACTIONS_ARGS;
extern void HandlePrintScreen      PROTO_XT_ACTIONS_ARGS;
extern void HandleQuit             PROTO_XT_ACTIONS_ARGS;
extern void HandleRedraw           PROTO_XT_ACTIONS_ARGS;
extern void HandleRenderFont       PROTO_XT_ACTIONS_ARGS;
extern void HandleReverseVideo     PROTO_XT_ACTIONS_ARGS;
extern void HandleReverseWrap      PROTO_XT_ACTIONS_ARGS;
extern void HandleScoFunctionKeys  PROTO_XT_ACTIONS_ARGS;
extern void HandleScrollKey        PROTO_XT_ACTIONS_ARGS;
extern void HandleScrollTtyOutput  PROTO_XT_ACTIONS_ARGS;
extern void HandleScrollbar        PROTO_XT_ACTIONS_ARGS;
extern void HandleSecure           PROTO_XT_ACTIONS_ARGS;
extern void HandleSendSignal       PROTO_XT_ACTIONS_ARGS;
extern void HandleSetPopOnBell     PROTO_XT_ACTIONS_ARGS;
extern void HandleKeepSelection    PROTO_XT_ACTIONS_ARGS;
extern void HandleSetSelect        PROTO_XT_ACTIONS_ARGS;
extern void HandleSetTekText       PROTO_XT_ACTIONS_ARGS;
extern void HandleSetTerminalType  PROTO_XT_ACTIONS_ARGS;
extern void HandleSetVisualBell    PROTO_XT_ACTIONS_ARGS;
extern void HandleSoftReset        PROTO_XT_ACTIONS_ARGS;
extern void HandleSunFunctionKeys  PROTO_XT_ACTIONS_ARGS;
extern void HandleSunKeyboard      PROTO_XT_ACTIONS_ARGS;
extern void HandleTekCopy          PROTO_XT_ACTIONS_ARGS;
extern void HandleTekPage          PROTO_XT_ACTIONS_ARGS;
extern void HandleTekReset         PROTO_XT_ACTIONS_ARGS;
extern void HandleTiteInhibit      PROTO_XT_ACTIONS_ARGS;
extern void HandleToolbar          PROTO_XT_ACTIONS_ARGS;
extern void HandleUTF8Mode         PROTO_XT_ACTIONS_ARGS;
extern void HandleUTF8Title        PROTO_XT_ACTIONS_ARGS;
extern void HandleVisibility       PROTO_XT_ACTIONS_ARGS;

extern void SetupMenus (Widget /*shell*/, Widget */*forms*/, Widget */*menus*/, Dimension * /*menu_high*/);

#if OPT_TOOLBAR
extern void ShowToolbar(Bool);
#endif

/*
 * The following definitions MUST match the order of entries given in
 * the mainMenuEntries, vtMenuEntries, and tekMenuEntries arrays in menu.c.
 */

/*
 * items in primary menu
 */
typedef enum {
#if OPT_TOOLBAR
    mainMenu_toolbar,
#endif
    mainMenu_securekbd,
    mainMenu_allowsends,
    mainMenu_redraw,
    mainMenu_line1,
#ifdef ALLOWLOGGING
    mainMenu_logging,
#endif
    mainMenu_print,
    mainMenu_print_redir,
    mainMenu_line2,
    mainMenu_8bit_ctrl,
    mainMenu_backarrow,
#if OPT_NUM_LOCK
    mainMenu_num_lock,
    mainMenu_alt_esc,
    mainMenu_meta_esc,
#endif
    mainMenu_delete_del,
    mainMenu_old_fkeys,
#if OPT_TCAP_FKEYS
    mainMenu_tcap_fkeys,
#endif
#if OPT_HP_FUNC_KEYS
    mainMenu_hp_fkeys,
#endif
#if OPT_SCO_FUNC_KEYS
    mainMenu_sco_fkeys,
#endif
#if OPT_SUN_FUNC_KEYS
    mainMenu_sun_fkeys,
#endif
#if OPT_SUNPC_KBD
    mainMenu_sun_kbd,
#endif
    mainMenu_line3,
    mainMenu_suspend,
    mainMenu_continue,
    mainMenu_interrupt,
    mainMenu_hangup,
    mainMenu_terminate,
    mainMenu_kill,
    mainMenu_line4,
    mainMenu_quit,
    mainMenu_LAST
} mainMenuIndices;


/*
 * items in vt100 mode menu
 */
typedef enum {
    vtMenu_scrollbar,
    vtMenu_jumpscroll,
    vtMenu_reversevideo,
    vtMenu_autowrap,
    vtMenu_reversewrap,
    vtMenu_autolinefeed,
    vtMenu_appcursor,
    vtMenu_appkeypad,
    vtMenu_scrollkey,
    vtMenu_scrollttyoutput,
    vtMenu_allow132,
    vtMenu_keepSelection,
    vtMenu_selectToClipboard,
    vtMenu_visualbell,
    vtMenu_bellIsUrgent,
    vtMenu_poponbell,
#if OPT_BLINK_CURS
    vtMenu_cursorblink,
#endif
    vtMenu_titeInhibit,
#ifndef NO_ACTIVE_ICON
    vtMenu_activeicon,
#endif /* NO_ACTIVE_ICON */
    vtMenu_line1,
    vtMenu_softreset,
    vtMenu_hardreset,
    vtMenu_clearsavedlines,
    vtMenu_line2,
#if OPT_TEK4014
    vtMenu_tekshow,
    vtMenu_tekmode,
    vtMenu_vthide,
#endif
    vtMenu_altscreen,
    vtMenu_LAST
} vtMenuIndices;

/*
 * items in vt100 font menu
 */
typedef enum {
    fontMenu_default,
    fontMenu_font1,
    fontMenu_font2,
    fontMenu_font3,
    fontMenu_font4,
    fontMenu_font5,
    fontMenu_font6,
#define fontMenu_lastBuiltin fontMenu_font6
    fontMenu_fontescape,
    fontMenu_fontsel,
/* number of non-line items down to here should match NMENUFONTS in ptyx.h */

#if OPT_DEC_CHRSET || OPT_BOX_CHARS || OPT_DEC_SOFTFONT
    fontMenu_line1,
#if OPT_BOX_CHARS
    fontMenu_font_boxchars,
#endif
#if OPT_DEC_CHRSET
    fontMenu_font_doublesize,
#endif
#if OPT_DEC_SOFTFONT
    fontMenu_font_loadable,
#endif
#endif

#if OPT_RENDERFONT || OPT_WIDE_CHARS
    fontMenu_line2,
#if OPT_RENDERFONT
    fontMenu_render_font,
#endif
#if OPT_WIDE_CHARS
    fontMenu_wide_chars,
    fontMenu_wide_title,
#endif
#endif

    fontMenu_LAST
} fontMenuIndices;


/*
 * items in tek4014 mode menu
 */
#if OPT_TEK4014
typedef enum {
    tekMenu_tektextlarge,
    tekMenu_tektext2,
    tekMenu_tektext3,
    tekMenu_tektextsmall,
    tekMenu_line1,
    tekMenu_tekpage,
    tekMenu_tekreset,
    tekMenu_tekcopy,
    tekMenu_line2,
    tekMenu_vtshow,
    tekMenu_vtmode,
    tekMenu_tekhide,
    tekMenu_LAST
} tekMenuIndices;
#endif


/*
 * functions for updating menus
 */

extern void SetItemSensitivity(Widget mi, XtArgVal val);

/*
 * there should be one of each of the following for each checkable item
 */
#if OPT_TOOLBAR
extern void update_toolbar(void);
#else
#define update_toolbar() /* nothing */
#endif

extern void update_securekbd(void);
extern void update_allowsends(void);

#ifdef ALLOWLOGGING
extern void update_logging(void);
#else
#define update_logging() /*nothing*/
#endif

extern void update_print_redir(void);
extern void update_8bit_control(void);
extern void update_decbkm(void);

#if OPT_NUM_LOCK
extern void update_num_lock(void);
extern void update_alt_esc(void);
extern void update_meta_esc(void);
#else
#define update_num_lock() /*nothing*/
#define update_alt_esc()  /*nothing*/
#define update_meta_esc() /*nothing*/
#endif

extern void update_old_fkeys(void);
extern void update_delete_del(void);

#if OPT_SUNPC_KBD
extern void update_sun_kbd(void);
#endif

#if OPT_HP_FUNC_KEYS
extern void update_hp_fkeys(void);
#else
#define update_hp_fkeys() /*nothing*/
#endif

#if OPT_SCO_FUNC_KEYS
extern void update_sco_fkeys(void);
#else
#define update_sco_fkeys() /*nothing*/
#endif

#if OPT_SUN_FUNC_KEYS
extern void update_sun_fkeys(void);
#else
#define update_sun_fkeys() /*nothing*/
#endif

#if OPT_TCAP_FKEYS
extern void update_tcap_fkeys(void);
#else
#define update_tcap_fkeys() /*nothing*/
#endif

extern void update_scrollbar(void);
extern void update_jumpscroll(void);
extern void update_reversevideo(void);
extern void update_autowrap(void);
extern void update_reversewrap(void);
extern void update_autolinefeed(void);
extern void update_appcursor(void);
extern void update_appkeypad(void);
extern void update_scrollkey(void);
extern void update_keepSelection(void);
extern void update_selectToClipboard(void);
extern void update_scrollttyoutput(void);
extern void update_allow132(void);
extern void update_cursesemul(void);
extern void update_visualbell(void);
extern void update_bellIsUrgent(void);
extern void update_poponbell(void);

#define update_marginbell() /* nothing */

#if OPT_BLINK_CURS
extern void update_cursorblink(void);
#else
#define update_cursorblink() /* nothing */
#endif

extern void update_altscreen(void);
extern void update_titeInhibit(void);

#ifndef NO_ACTIVE_ICON
extern void update_activeicon(void);
#endif /* NO_ACTIVE_ICON */

#if OPT_DEC_CHRSET
extern void update_font_doublesize(void);
#else
#define update_font_doublesize() /* nothing */
#endif

#if OPT_BOX_CHARS
extern void update_font_boxchars(void);
#else
#define update_font_boxchars() /* nothing */
#endif

#if OPT_DEC_SOFTFONT
extern void update_font_loadable(void);
#else
#define update_font_loadable() /* nothing */
#endif

#if OPT_RENDERFONT
extern void update_font_renderfont(void);
#else
#define update_font_renderfont() /* nothing */
#endif

#if OPT_WIDE_CHARS
extern void update_font_utf8_mode(void);
extern void update_font_utf8_title(void);
#else
#define update_font_utf8_mode() /* nothing */
#define update_font_utf8_title() /* nothing */
#endif

#if OPT_TEK4014
extern void update_tekshow(void);
extern void update_vttekmode(void);
extern void update_vtshow(void);
extern void set_vthide_sensitivity(void);
extern void set_tekhide_sensitivity(void);
#else
#define update_tekshow() /*nothing*/
#define update_vttekmode() /*nothing*/
#define update_vtshow() /*nothing*/
#define set_vthide_sensitivity() /*nothing*/
#define set_tekhide_sensitivity() /*nothing*/
#endif

/*
 * macros for mapping font size to tekMenu placement
 */
#define FS2MI(n) (n)			/* font_size_to_menu_item */
#define MI2FS(n) (n)			/* menu_item_to_font_size */

#if OPT_TEK4014
extern void set_tekfont_menu_item(int n,int val);
#else
#define set_tekfont_menu_item(n,val) /*nothing*/
#endif

extern void set_menu_font(int val);

#endif	/*included_menu_h*/
