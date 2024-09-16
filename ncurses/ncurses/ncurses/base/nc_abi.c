/* This file is @generated */
/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * libncurses builds with NCURSES_WANT_BASEABI defined by default so that we
 * don't accidentally use new symbol versions in intermediate functions.  This
 * file, however, intentionally wants all of the newer prototypes.
 */
#undef NCURSES_WANT_BASEABI
#include <curses.priv.h>

unsigned int __thread _nc_abiver = NCURSES_DEFAULT_ABI;

/* Generated symbols with macros */
#undef addch
#undef addchnstr
#undef addchstr
#undef addnstr
#undef addstr
#undef attroff
#undef attron
#undef attrset
#undef attr_get
#undef attr_off
#undef attr_on
#undef attr_set
#undef bkgd
#undef bkgdset
#undef border
#undef box
#undef chgat
#undef color_set
#undef deleteln
#undef echochar
#undef getbkgd
#undef getch
#undef getnstr
#undef getstr
#undef hline
#undef inch
#undef inchnstr
#undef inchstr
#undef innstr
#undef insch
#undef insdelln
#undef insertln
#undef insnstr
#undef insstr
#undef instr
#undef mvaddch
#undef mvaddchnstr
#undef mvaddchstr
#undef mvaddnstr
#undef mvaddstr
#undef mvchgat
#undef mvgetch
#undef mvgetnstr
#undef mvgetstr
#undef mvhline
#undef mvinch
#undef mvinchnstr
#undef mvinchstr
#undef mvinnstr
#undef mvinsch
#undef mvinsnstr
#undef mvinsstr
#undef mvinstr
#undef mvvline
#undef mvwaddch
#undef mvwaddchnstr
#undef mvwaddchstr
#undef mvwaddnstr
#undef mvwaddstr
#undef mvwchgat
#undef mvwgetch
#undef mvwgetnstr
#undef mvwgetstr
#undef mvwhline
#undef mvwinch
#undef mvwinchnstr
#undef mvwinchstr
#undef mvwinnstr
#undef mvwinsch
#undef mvwinsnstr
#undef mvwinsstr
#undef mvwinstr
#undef mvwvline
#undef redrawwin
#undef refresh
#undef scrl
#undef scroll
#undef slk_attr_off
#undef slk_attr_on
#undef standout
#undef standend
#undef vline
#undef vw_printw
#undef vw_scanw
#undef waddchstr
#undef waddstr
#undef wattron
#undef wattroff
#undef wattrset
#undef wattr_get
#undef wattr_set
#undef wdeleteln
#undef wgetstr
#undef winchstr
#undef winsertln
#undef winsstr
#undef winstr
#undef wstandout
#undef wstandend
#undef add_wch
#undef add_wchnstr
#undef add_wchstr
#undef addnwstr
#undef addwstr
#undef bkgrnd
#undef bkgrndset
#undef border_set
#undef box_set
#undef echo_wchar
#undef get_wch
#undef get_wstr
#undef getbkgrnd
#undef getn_wstr
#undef hline_set
#undef in_wch
#undef in_wchnstr
#undef in_wchstr
#undef innwstr
#undef ins_nwstr
#undef ins_wch
#undef ins_wstr
#undef inwstr
#undef mvadd_wch
#undef mvadd_wchnstr
#undef mvadd_wchstr
#undef mvaddnwstr
#undef mvaddwstr
#undef mvget_wch
#undef mvget_wstr
#undef mvgetn_wstr
#undef mvhline_set
#undef mvin_wch
#undef mvin_wchnstr
#undef mvin_wchstr
#undef mvinnwstr
#undef mvins_nwstr
#undef mvins_wch
#undef mvins_wstr
#undef mvinwstr
#undef mvvline_set
#undef mvwadd_wch
#undef mvwadd_wchnstr
#undef mvwadd_wchstr
#undef mvwaddnwstr
#undef mvwaddwstr
#undef mvwget_wch
#undef mvwget_wstr
#undef mvwgetn_wstr
#undef mvwhline_set
#undef mvwin_wch
#undef mvwin_wchnstr
#undef mvwin_wchstr
#undef mvwinnwstr
#undef mvwins_nwstr
#undef mvwins_wch
#undef mvwins_wstr
#undef mvwinwstr
#undef mvwvline_set
#undef vline_set
#undef wadd_wchstr
#undef waddwstr
#undef wget_wstr
#undef wgetbkgrnd
#undef win_wchstr
#undef wins_wstr

/* Base symbol declarations */
extern NCURSES_EXPORT(int) (addch_impl) (const chtype) _NCURSES_EXPORT_ABI(addch);
extern NCURSES_EXPORT(int) (addchnstr_impl) (const chtype *, int) _NCURSES_EXPORT_ABI(addchnstr);
extern NCURSES_EXPORT(int) (addchstr_impl) (const chtype *) _NCURSES_EXPORT_ABI(addchstr);
extern NCURSES_EXPORT(int) (addnstr_impl) (const char *, int) _NCURSES_EXPORT_ABI(addnstr);
extern NCURSES_EXPORT(int) (addstr_impl) (const char *) _NCURSES_EXPORT_ABI(addstr);
extern NCURSES_EXPORT(int) (attroff_impl) (NCURSES_ATTR_T) _NCURSES_EXPORT_ABI(attroff);
extern NCURSES_EXPORT(int) (attron_impl) (NCURSES_ATTR_T) _NCURSES_EXPORT_ABI(attron);
extern NCURSES_EXPORT(int) (attrset_impl) (NCURSES_ATTR_T) _NCURSES_EXPORT_ABI(attrset);
extern NCURSES_EXPORT(int) (attr_get_impl) (attr_t *, NCURSES_PAIRS_T *, void *) _NCURSES_EXPORT_ABI(attr_get);
extern NCURSES_EXPORT(int) (attr_off_impl) (attr_t, void *) _NCURSES_EXPORT_ABI(attr_off);
extern NCURSES_EXPORT(int) (attr_on_impl) (attr_t, void *) _NCURSES_EXPORT_ABI(attr_on);
extern NCURSES_EXPORT(int) (attr_set_impl) (attr_t, NCURSES_PAIRS_T, void *) _NCURSES_EXPORT_ABI(attr_set);
extern NCURSES_EXPORT(int) (bkgd_impl) (chtype) _NCURSES_EXPORT_ABI(bkgd);
extern NCURSES_EXPORT(void) (bkgdset_impl) (chtype) _NCURSES_EXPORT_ABI(bkgdset);
extern NCURSES_EXPORT(int) (border_impl) (chtype, chtype, chtype, chtype, chtype, chtype, chtype, chtype) _NCURSES_EXPORT_ABI(border);
extern NCURSES_EXPORT(int) (box_impl) (WINDOW *, chtype, chtype) _NCURSES_EXPORT_ABI(box);
extern NCURSES_EXPORT(int) (chgat_impl) (int, attr_t, NCURSES_PAIRS_T, const void *) _NCURSES_EXPORT_ABI(chgat);
extern NCURSES_EXPORT(int) (color_content_impl) (NCURSES_COLOR_T, NCURSES_COLOR_T*, NCURSES_COLOR_T*, NCURSES_COLOR_T*) _NCURSES_EXPORT_ABI(color_content);
extern NCURSES_EXPORT(int) (color_set_impl) (NCURSES_PAIRS_T, void*) _NCURSES_EXPORT_ABI(color_set);
extern NCURSES_EXPORT(int) (copywin_impl) (const WINDOW*, WINDOW*, int, int, int, int, int, int, int) _NCURSES_EXPORT_ABI(copywin);
extern NCURSES_EXPORT(int) (delay_output_impl) (int) _NCURSES_EXPORT_ABI(delay_output);
extern NCURSES_EXPORT(int) (deleteln_impl) (void) _NCURSES_EXPORT_ABI(deleteln);
extern NCURSES_EXPORT(WINDOW *) (derwin_impl) (WINDOW *, int, int, int, int) _NCURSES_EXPORT_ABI(derwin);
extern NCURSES_EXPORT(int) (doupdate_impl) (void) _NCURSES_EXPORT_ABI(doupdate);
extern NCURSES_EXPORT(WINDOW *) (dupwin_impl) (WINDOW *) _NCURSES_EXPORT_ABI(dupwin);
extern NCURSES_EXPORT(int) (echochar_impl) (const chtype) _NCURSES_EXPORT_ABI(echochar);
extern NCURSES_EXPORT(int) (endwin_impl) (void) _NCURSES_EXPORT_ABI(endwin);
extern NCURSES_EXPORT(int) (flushinp_impl) (void) _NCURSES_EXPORT_ABI(flushinp);
extern NCURSES_EXPORT(chtype) (getbkgd_impl) (WINDOW *) _NCURSES_EXPORT_ABI(getbkgd);
extern NCURSES_EXPORT(int) (getch_impl) (void) _NCURSES_EXPORT_ABI(getch);
extern NCURSES_EXPORT(int) (getnstr_impl) (char *, int) _NCURSES_EXPORT_ABI(getnstr);
extern NCURSES_EXPORT(int) (getstr_impl) (char *) _NCURSES_EXPORT_ABI(getstr);
extern NCURSES_EXPORT(WINDOW *) (getwin_impl) (FILE *) _NCURSES_EXPORT_ABI(getwin);
extern NCURSES_EXPORT(int) (hline_impl) (chtype, int) _NCURSES_EXPORT_ABI(hline);
extern NCURSES_EXPORT(chtype) (inch_impl) (void) _NCURSES_EXPORT_ABI(inch);
extern NCURSES_EXPORT(int) (inchnstr_impl) (chtype *, int) _NCURSES_EXPORT_ABI(inchnstr);
extern NCURSES_EXPORT(int) (inchstr_impl) (chtype *) _NCURSES_EXPORT_ABI(inchstr);
extern NCURSES_EXPORT(WINDOW *) (initscr_impl) (void) _NCURSES_EXPORT_ABI(initscr);
extern NCURSES_EXPORT(int) (init_color_impl) (NCURSES_COLOR_T, NCURSES_COLOR_T, NCURSES_COLOR_T, NCURSES_COLOR_T) _NCURSES_EXPORT_ABI(init_color);
extern NCURSES_EXPORT(int) (init_pair_impl) (NCURSES_PAIRS_T, NCURSES_COLOR_T, NCURSES_COLOR_T) _NCURSES_EXPORT_ABI(init_pair);
extern NCURSES_EXPORT(int) (innstr_impl) (char *, int) _NCURSES_EXPORT_ABI(innstr);
extern NCURSES_EXPORT(int) (insch_impl) (chtype) _NCURSES_EXPORT_ABI(insch);
extern NCURSES_EXPORT(int) (insdelln_impl) (int) _NCURSES_EXPORT_ABI(insdelln);
extern NCURSES_EXPORT(int) (insertln_impl) (void) _NCURSES_EXPORT_ABI(insertln);
extern NCURSES_EXPORT(int) (insnstr_impl) (const char *, int) _NCURSES_EXPORT_ABI(insnstr);
extern NCURSES_EXPORT(int) (insstr_impl) (const char *) _NCURSES_EXPORT_ABI(insstr);
extern NCURSES_EXPORT(int) (instr_impl) (char *) _NCURSES_EXPORT_ABI(instr);
extern NCURSES_EXPORT(int) (intrflush_impl) (WINDOW *, bool) _NCURSES_EXPORT_ABI(intrflush);
extern NCURSES_EXPORT(int) (keypad_impl) (WINDOW *, bool) _NCURSES_EXPORT_ABI(keypad);
extern NCURSES_EXPORT(int) (mvaddch_impl) (int, int, const chtype) _NCURSES_EXPORT_ABI(mvaddch);
extern NCURSES_EXPORT(int) (mvaddchnstr_impl) (int, int, const chtype *, int) _NCURSES_EXPORT_ABI(mvaddchnstr);
extern NCURSES_EXPORT(int) (mvaddchstr_impl) (int, int, const chtype *) _NCURSES_EXPORT_ABI(mvaddchstr);
extern NCURSES_EXPORT(int) (mvaddnstr_impl) (int, int, const char *, int) _NCURSES_EXPORT_ABI(mvaddnstr);
extern NCURSES_EXPORT(int) (mvaddstr_impl) (int, int, const char *) _NCURSES_EXPORT_ABI(mvaddstr);
extern NCURSES_EXPORT(int) (mvchgat_impl) (int, int, int, attr_t, NCURSES_PAIRS_T, const void *) _NCURSES_EXPORT_ABI(mvchgat);
extern NCURSES_EXPORT(int) (mvcur_impl) (int, int, int, int) _NCURSES_EXPORT_ABI(mvcur);
extern NCURSES_EXPORT(int) (mvderwin_impl) (WINDOW *, int, int) _NCURSES_EXPORT_ABI(mvderwin);
extern NCURSES_EXPORT(int) (mvgetch_impl) (int, int) _NCURSES_EXPORT_ABI(mvgetch);
extern NCURSES_EXPORT(int) (mvgetnstr_impl) (int, int, char *, int) _NCURSES_EXPORT_ABI(mvgetnstr);
extern NCURSES_EXPORT(int) (mvgetstr_impl) (int, int, char *) _NCURSES_EXPORT_ABI(mvgetstr);
extern NCURSES_EXPORT(int) (mvhline_impl) (int, int, chtype, int) _NCURSES_EXPORT_ABI(mvhline);
extern NCURSES_EXPORT(chtype) (mvinch_impl) (int, int) _NCURSES_EXPORT_ABI(mvinch);
extern NCURSES_EXPORT(int) (mvinchnstr_impl) (int, int, chtype *, int) _NCURSES_EXPORT_ABI(mvinchnstr);
extern NCURSES_EXPORT(int) (mvinchstr_impl) (int, int, chtype *) _NCURSES_EXPORT_ABI(mvinchstr);
extern NCURSES_EXPORT(int) (mvinnstr_impl) (int, int, char *, int) _NCURSES_EXPORT_ABI(mvinnstr);
extern NCURSES_EXPORT(int) (mvinsch_impl) (int, int, chtype) _NCURSES_EXPORT_ABI(mvinsch);
extern NCURSES_EXPORT(int) (mvinsnstr_impl) (int, int, const char *, int) _NCURSES_EXPORT_ABI(mvinsnstr);
extern NCURSES_EXPORT(int) (mvinsstr_impl) (int, int, const char *) _NCURSES_EXPORT_ABI(mvinsstr);
extern NCURSES_EXPORT(int) (mvinstr_impl) (int, int, char *) _NCURSES_EXPORT_ABI(mvinstr);
extern NCURSES_EXPORT(int) (mvvline_impl) (int, int, chtype, int) _NCURSES_EXPORT_ABI(mvvline);
extern NCURSES_EXPORT(int) (mvwaddch_impl) (WINDOW *, int, int, const chtype) _NCURSES_EXPORT_ABI(mvwaddch);
extern NCURSES_EXPORT(int) (mvwaddchnstr_impl) (WINDOW *, int, int, const chtype *, int) _NCURSES_EXPORT_ABI(mvwaddchnstr);
extern NCURSES_EXPORT(int) (mvwaddchstr_impl) (WINDOW *, int, int, const chtype *) _NCURSES_EXPORT_ABI(mvwaddchstr);
extern NCURSES_EXPORT(int) (mvwaddnstr_impl) (WINDOW *, int, int, const char *, int) _NCURSES_EXPORT_ABI(mvwaddnstr);
extern NCURSES_EXPORT(int) (mvwaddstr_impl) (WINDOW *, int, int, const char *) _NCURSES_EXPORT_ABI(mvwaddstr);
extern NCURSES_EXPORT(int) (mvwchgat_impl) (WINDOW *, int, int, int, attr_t, NCURSES_PAIRS_T, const void *) _NCURSES_EXPORT_ABI(mvwchgat);
extern NCURSES_EXPORT(int) (mvwgetch_impl) (WINDOW *, int, int) _NCURSES_EXPORT_ABI(mvwgetch);
extern NCURSES_EXPORT(int) (mvwgetnstr_impl) (WINDOW *, int, int, char *, int) _NCURSES_EXPORT_ABI(mvwgetnstr);
extern NCURSES_EXPORT(int) (mvwgetstr_impl) (WINDOW *, int, int, char *) _NCURSES_EXPORT_ABI(mvwgetstr);
extern NCURSES_EXPORT(int) (mvwhline_impl) (WINDOW *, int, int, chtype, int) _NCURSES_EXPORT_ABI(mvwhline);
extern NCURSES_EXPORT(int) (mvwin_impl) (WINDOW *, int, int) _NCURSES_EXPORT_ABI(mvwin);
extern NCURSES_EXPORT(chtype) (mvwinch_impl) (WINDOW *, int, int) _NCURSES_EXPORT_ABI(mvwinch);
extern NCURSES_EXPORT(int) (mvwinchnstr_impl) (WINDOW *, int, int, chtype *, int) _NCURSES_EXPORT_ABI(mvwinchnstr);
extern NCURSES_EXPORT(int) (mvwinchstr_impl) (WINDOW *, int, int, chtype *) _NCURSES_EXPORT_ABI(mvwinchstr);
extern NCURSES_EXPORT(int) (mvwinnstr_impl) (WINDOW *, int, int, char *, int) _NCURSES_EXPORT_ABI(mvwinnstr);
extern NCURSES_EXPORT(int) (mvwinsch_impl) (WINDOW *, int, int, chtype) _NCURSES_EXPORT_ABI(mvwinsch);
extern NCURSES_EXPORT(int) (mvwinsnstr_impl) (WINDOW *, int, int, const char *, int) _NCURSES_EXPORT_ABI(mvwinsnstr);
extern NCURSES_EXPORT(int) (mvwinsstr_impl) (WINDOW *, int, int, const char *) _NCURSES_EXPORT_ABI(mvwinsstr);
extern NCURSES_EXPORT(int) (mvwinstr_impl) (WINDOW *, int, int, char *) _NCURSES_EXPORT_ABI(mvwinstr);
extern NCURSES_EXPORT(int) (mvwvline_impl) (WINDOW *, int, int, chtype, int) _NCURSES_EXPORT_ABI(mvwvline);
extern NCURSES_EXPORT(WINDOW *) (newpad_impl) (int, int) _NCURSES_EXPORT_ABI(newpad);
extern NCURSES_EXPORT(SCREEN *) (newterm_impl) (NCURSES_CONST char *, FILE *, FILE *) _NCURSES_EXPORT_ABI(newterm);
extern NCURSES_EXPORT(WINDOW *) (newwin_impl) (int, int, int, int) _NCURSES_EXPORT_ABI(newwin);
extern NCURSES_EXPORT(int) (overlay_impl) (const WINDOW*, WINDOW *) _NCURSES_EXPORT_ABI(overlay);
extern NCURSES_EXPORT(int) (overwrite_impl) (const WINDOW*, WINDOW *) _NCURSES_EXPORT_ABI(overwrite);
extern NCURSES_EXPORT(int) (pair_content_impl) (NCURSES_PAIRS_T, NCURSES_COLOR_T*, NCURSES_COLOR_T*) _NCURSES_EXPORT_ABI(pair_content);
extern NCURSES_EXPORT(int) (pechochar_impl) (WINDOW *, const chtype) _NCURSES_EXPORT_ABI(pechochar);
extern NCURSES_EXPORT(int) (pnoutrefresh_impl) (WINDOW*, int, int, int, int, int, int) _NCURSES_EXPORT_ABI(pnoutrefresh);
extern NCURSES_EXPORT(int) (prefresh_impl) (WINDOW *, int, int, int, int, int, int) _NCURSES_EXPORT_ABI(prefresh);
extern NCURSES_EXPORT(int) (putwin_impl) (WINDOW *, FILE *) _NCURSES_EXPORT_ABI(putwin);
extern NCURSES_EXPORT(int) (redrawwin_impl) (WINDOW *) _NCURSES_EXPORT_ABI(redrawwin);
extern NCURSES_EXPORT(int) (refresh_impl) (void) _NCURSES_EXPORT_ABI(refresh);
extern NCURSES_EXPORT(int) (scr_dump_impl) (const char *) _NCURSES_EXPORT_ABI(scr_dump);
extern NCURSES_EXPORT(int) (scr_init_impl) (const char *) _NCURSES_EXPORT_ABI(scr_init);
extern NCURSES_EXPORT(int) (scrl_impl) (int) _NCURSES_EXPORT_ABI(scrl);
extern NCURSES_EXPORT(int) (scroll_impl) (WINDOW *) _NCURSES_EXPORT_ABI(scroll);
extern NCURSES_EXPORT(int) (scrollok_impl) (WINDOW *, bool) _NCURSES_EXPORT_ABI(scrollok);
extern NCURSES_EXPORT(int) (scr_restore_impl) (const char *) _NCURSES_EXPORT_ABI(scr_restore);
extern NCURSES_EXPORT(int) (scr_set_impl) (const char *) _NCURSES_EXPORT_ABI(scr_set);
extern NCURSES_EXPORT(SCREEN *) (set_term_impl) (SCREEN *) _NCURSES_EXPORT_ABI(set_term);
extern NCURSES_EXPORT(int) (slk_attroff_impl) (const chtype) _NCURSES_EXPORT_ABI(slk_attroff);
extern NCURSES_EXPORT(int) (slk_attr_off_impl) (const attr_t, void *) _NCURSES_EXPORT_ABI(slk_attr_off);
extern NCURSES_EXPORT(int) (slk_attron_impl) (const chtype) _NCURSES_EXPORT_ABI(slk_attron);
extern NCURSES_EXPORT(int) (slk_attr_on_impl) (attr_t, void*) _NCURSES_EXPORT_ABI(slk_attr_on);
extern NCURSES_EXPORT(int) (slk_attrset_impl) (const chtype) _NCURSES_EXPORT_ABI(slk_attrset);
extern NCURSES_EXPORT(attr_t) (slk_attr_impl) (void) _NCURSES_EXPORT_ABI(slk_attr);
extern NCURSES_EXPORT(int) (slk_attr_set_impl) (const attr_t, NCURSES_PAIRS_T, void*) _NCURSES_EXPORT_ABI(slk_attr_set);
extern NCURSES_EXPORT(int) (slk_clear_impl) (void) _NCURSES_EXPORT_ABI(slk_clear);
extern NCURSES_EXPORT(int) (slk_color_impl) (NCURSES_PAIRS_T) _NCURSES_EXPORT_ABI(slk_color);
extern NCURSES_EXPORT(int) (slk_init_impl) (int) _NCURSES_EXPORT_ABI(slk_init);
extern NCURSES_EXPORT(char *) (slk_label_impl) (int) _NCURSES_EXPORT_ABI(slk_label);
extern NCURSES_EXPORT(int) (slk_noutrefresh_impl) (void) _NCURSES_EXPORT_ABI(slk_noutrefresh);
extern NCURSES_EXPORT(int) (slk_refresh_impl) (void) _NCURSES_EXPORT_ABI(slk_refresh);
extern NCURSES_EXPORT(int) (slk_restore_impl) (void) _NCURSES_EXPORT_ABI(slk_restore);
extern NCURSES_EXPORT(int) (slk_set_impl) (int, const char *, int) _NCURSES_EXPORT_ABI(slk_set);
extern NCURSES_EXPORT(int) (slk_touch_impl) (void) _NCURSES_EXPORT_ABI(slk_touch);
extern NCURSES_EXPORT(int) (standout_impl) (void) _NCURSES_EXPORT_ABI(standout);
extern NCURSES_EXPORT(int) (standend_impl) (void) _NCURSES_EXPORT_ABI(standend);
extern NCURSES_EXPORT(int) (start_color_impl) (void) _NCURSES_EXPORT_ABI(start_color);
extern NCURSES_EXPORT(WINDOW *) (subpad_impl) (WINDOW *, int, int, int, int) _NCURSES_EXPORT_ABI(subpad);
extern NCURSES_EXPORT(WINDOW *) (subwin_impl) (WINDOW *, int, int, int, int) _NCURSES_EXPORT_ABI(subwin);
extern NCURSES_EXPORT(int) (ungetch_impl) (int) _NCURSES_EXPORT_ABI(ungetch);
extern NCURSES_EXPORT(int) (vidattr_impl) (chtype) _NCURSES_EXPORT_ABI(vidattr);
extern NCURSES_EXPORT(int) (vidputs_impl) (chtype, NCURSES_OUTC) _NCURSES_EXPORT_ABI(vidputs);
extern NCURSES_EXPORT(int) (vline_impl) (chtype, int) _NCURSES_EXPORT_ABI(vline);
extern NCURSES_EXPORT(int) (vwprintw_impl) (WINDOW *, const char *, va_list) _NCURSES_EXPORT_ABI(vwprintw);
extern NCURSES_EXPORT(int) (vw_printw_impl) (WINDOW *, const char *, va_list) _NCURSES_EXPORT_ABI(vw_printw);
extern NCURSES_EXPORT(int) (vwscanw_impl) (WINDOW *, NCURSES_CONST char *, va_list) _NCURSES_EXPORT_ABI(vwscanw);
extern NCURSES_EXPORT(int) (vw_scanw_impl) (WINDOW *, NCURSES_CONST char *, va_list) _NCURSES_EXPORT_ABI(vw_scanw);
extern NCURSES_EXPORT(int) (waddch_impl) (WINDOW *, const chtype) _NCURSES_EXPORT_ABI(waddch);
extern NCURSES_EXPORT(int) (waddchnstr_impl) (WINDOW *, const chtype *, int) _NCURSES_EXPORT_ABI(waddchnstr);
extern NCURSES_EXPORT(int) (waddchstr_impl) (WINDOW *, const chtype *) _NCURSES_EXPORT_ABI(waddchstr);
extern NCURSES_EXPORT(int) (waddnstr_impl) (WINDOW *, const char *, int) _NCURSES_EXPORT_ABI(waddnstr);
extern NCURSES_EXPORT(int) (waddstr_impl) (WINDOW *, const char *) _NCURSES_EXPORT_ABI(waddstr);
extern NCURSES_EXPORT(int) (wattron_impl) (WINDOW *, int) _NCURSES_EXPORT_ABI(wattron);
extern NCURSES_EXPORT(int) (wattroff_impl) (WINDOW *, int) _NCURSES_EXPORT_ABI(wattroff);
extern NCURSES_EXPORT(int) (wattrset_impl) (WINDOW *, int) _NCURSES_EXPORT_ABI(wattrset);
extern NCURSES_EXPORT(int) (wattr_get_impl) (WINDOW *, attr_t *, NCURSES_PAIRS_T *, void *) _NCURSES_EXPORT_ABI(wattr_get);
extern NCURSES_EXPORT(int) (wattr_on_impl) (WINDOW *, attr_t, void *) _NCURSES_EXPORT_ABI(wattr_on);
extern NCURSES_EXPORT(int) (wattr_off_impl) (WINDOW *, attr_t, void *) _NCURSES_EXPORT_ABI(wattr_off);
extern NCURSES_EXPORT(int) (wattr_set_impl) (WINDOW *, attr_t, NCURSES_PAIRS_T, void *) _NCURSES_EXPORT_ABI(wattr_set);
extern NCURSES_EXPORT(int) (wbkgd_impl) (WINDOW *, chtype) _NCURSES_EXPORT_ABI(wbkgd);
extern NCURSES_EXPORT(void) (wbkgdset_impl) (WINDOW *, chtype) _NCURSES_EXPORT_ABI(wbkgdset);
extern NCURSES_EXPORT(int) (wborder_impl) (WINDOW *, chtype, chtype, chtype, chtype, chtype, chtype, chtype, chtype) _NCURSES_EXPORT_ABI(wborder);
extern NCURSES_EXPORT(int) (wchgat_impl) (WINDOW *, int, attr_t, NCURSES_PAIRS_T, const void *) _NCURSES_EXPORT_ABI(wchgat);
extern NCURSES_EXPORT(int) (wclear_impl) (WINDOW *) _NCURSES_EXPORT_ABI(wclear);
extern NCURSES_EXPORT(int) (wcolor_set_impl) (WINDOW*, NCURSES_PAIRS_T, void*) _NCURSES_EXPORT_ABI(wcolor_set);
extern NCURSES_EXPORT(int) (wdeleteln_impl) (WINDOW *) _NCURSES_EXPORT_ABI(wdeleteln);
extern NCURSES_EXPORT(int) (wechochar_impl) (WINDOW *, const chtype) _NCURSES_EXPORT_ABI(wechochar);
extern NCURSES_EXPORT(int) (wgetch_impl) (WINDOW *) _NCURSES_EXPORT_ABI(wgetch);
extern NCURSES_EXPORT(int) (wgetnstr_impl) (WINDOW *, char *, int) _NCURSES_EXPORT_ABI(wgetnstr);
extern NCURSES_EXPORT(int) (wgetstr_impl) (WINDOW *, char *) _NCURSES_EXPORT_ABI(wgetstr);
extern NCURSES_EXPORT(int) (whline_impl) (WINDOW *, chtype, int) _NCURSES_EXPORT_ABI(whline);
extern NCURSES_EXPORT(chtype) (winch_impl) (WINDOW *) _NCURSES_EXPORT_ABI(winch);
extern NCURSES_EXPORT(int) (winchnstr_impl) (WINDOW *, chtype *, int) _NCURSES_EXPORT_ABI(winchnstr);
extern NCURSES_EXPORT(int) (winchstr_impl) (WINDOW *, chtype *) _NCURSES_EXPORT_ABI(winchstr);
extern NCURSES_EXPORT(int) (winnstr_impl) (WINDOW *, char *, int) _NCURSES_EXPORT_ABI(winnstr);
extern NCURSES_EXPORT(int) (winsch_impl) (WINDOW *, chtype) _NCURSES_EXPORT_ABI(winsch);
extern NCURSES_EXPORT(int) (winsdelln_impl) (WINDOW *, int) _NCURSES_EXPORT_ABI(winsdelln);
extern NCURSES_EXPORT(int) (winsertln_impl) (WINDOW *) _NCURSES_EXPORT_ABI(winsertln);
extern NCURSES_EXPORT(int) (winsnstr_impl) (WINDOW *, const char *, int) _NCURSES_EXPORT_ABI(winsnstr);
extern NCURSES_EXPORT(int) (winsstr_impl) (WINDOW *, const char *) _NCURSES_EXPORT_ABI(winsstr);
extern NCURSES_EXPORT(int) (winstr_impl) (WINDOW *, char *) _NCURSES_EXPORT_ABI(winstr);
extern NCURSES_EXPORT(int) (wnoutrefresh_impl) (WINDOW *) _NCURSES_EXPORT_ABI(wnoutrefresh);
extern NCURSES_EXPORT(int) (wredrawln_impl) (WINDOW *, int, int) _NCURSES_EXPORT_ABI(wredrawln);
extern NCURSES_EXPORT(int) (wrefresh_impl) (WINDOW *) _NCURSES_EXPORT_ABI(wrefresh);
extern NCURSES_EXPORT(int) (wscrl_impl) (WINDOW *, int) _NCURSES_EXPORT_ABI(wscrl);
extern NCURSES_EXPORT(int) (wstandout_impl) (WINDOW *) _NCURSES_EXPORT_ABI(wstandout);
extern NCURSES_EXPORT(int) (wstandend_impl) (WINDOW *) _NCURSES_EXPORT_ABI(wstandend);
extern NCURSES_EXPORT(int) (wvline_impl) (WINDOW *, chtype, int) _NCURSES_EXPORT_ABI(wvline);
extern NCURSES_EXPORT(int) (putp_impl) (const char *) _NCURSES_EXPORT_ABI(putp);
extern NCURSES_EXPORT(bool) (is_term_resized_impl) (int, int) _NCURSES_EXPORT_ABI(is_term_resized);
extern NCURSES_EXPORT(int) (assume_default_colors_impl) (int, int) _NCURSES_EXPORT_ABI(assume_default_colors);
extern NCURSES_EXPORT(int) (resize_term_impl) (int, int) _NCURSES_EXPORT_ABI(resize_term);
extern NCURSES_EXPORT(int) (resizeterm_impl) (int, int) _NCURSES_EXPORT_ABI(resizeterm);
extern NCURSES_EXPORT(int) (use_default_colors_impl) (void) _NCURSES_EXPORT_ABI(use_default_colors);
extern NCURSES_EXPORT(int) (use_screen_impl) (SCREEN *, NCURSES_SCREEN_CB, void *) _NCURSES_EXPORT_ABI(use_screen);
extern NCURSES_EXPORT(int) (use_window_impl) (WINDOW *, NCURSES_WINDOW_CB, void *) _NCURSES_EXPORT_ABI(use_window);
extern NCURSES_EXPORT(int) (wresize_impl) (WINDOW *, int, int) _NCURSES_EXPORT_ABI(wresize);
extern NCURSES_EXPORT(WINDOW *) (wgetparent_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(wgetparent);
extern NCURSES_EXPORT(bool) (is_cleared_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_cleared);
extern NCURSES_EXPORT(bool) (is_idcok_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_idcok);
extern NCURSES_EXPORT(bool) (is_idlok_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_idlok);
extern NCURSES_EXPORT(bool) (is_immedok_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_immedok);
extern NCURSES_EXPORT(bool) (is_keypad_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_keypad);
extern NCURSES_EXPORT(bool) (is_leaveok_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_leaveok);
extern NCURSES_EXPORT(bool) (is_nodelay_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_nodelay);
extern NCURSES_EXPORT(bool) (is_notimeout_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_notimeout);
extern NCURSES_EXPORT(bool) (is_pad_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_pad);
extern NCURSES_EXPORT(bool) (is_scrollok_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_scrollok);
extern NCURSES_EXPORT(bool) (is_subwin_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_subwin);
extern NCURSES_EXPORT(bool) (is_syncok_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(is_syncok);
extern NCURSES_EXPORT(int) (wgetdelay_impl) (const WINDOW *) _NCURSES_EXPORT_ABI(wgetdelay);
extern NCURSES_EXPORT(int) (wgetscrreg_impl) (const WINDOW *, int *, int *) _NCURSES_EXPORT_ABI(wgetscrreg);
extern NCURSES_EXPORT(int) (add_wch_impl) (const cchar_t *) _NCURSES_EXPORT_ABI(add_wch);
extern NCURSES_EXPORT(int) (add_wchnstr_impl) (const cchar_t *, int) _NCURSES_EXPORT_ABI(add_wchnstr);
extern NCURSES_EXPORT(int) (add_wchstr_impl) (const cchar_t *) _NCURSES_EXPORT_ABI(add_wchstr);
extern NCURSES_EXPORT(int) (addnwstr_impl) (const wchar_t *, int) _NCURSES_EXPORT_ABI(addnwstr);
extern NCURSES_EXPORT(int) (addwstr_impl) (const wchar_t *) _NCURSES_EXPORT_ABI(addwstr);
extern NCURSES_EXPORT(int) (bkgrnd_impl) (const cchar_t *) _NCURSES_EXPORT_ABI(bkgrnd);
extern NCURSES_EXPORT(void) (bkgrndset_impl) (const cchar_t *) _NCURSES_EXPORT_ABI(bkgrndset);
extern NCURSES_EXPORT(int) (border_set_impl) (const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*) _NCURSES_EXPORT_ABI(border_set);
extern NCURSES_EXPORT(int) (box_set_impl) (WINDOW *, const cchar_t *, const cchar_t *) _NCURSES_EXPORT_ABI(box_set);
extern NCURSES_EXPORT(int) (echo_wchar_impl) (const cchar_t *) _NCURSES_EXPORT_ABI(echo_wchar);
extern NCURSES_EXPORT(int) (erasewchar_impl) (wchar_t*) _NCURSES_EXPORT_ABI(erasewchar);
extern NCURSES_EXPORT(int) (get_wch_impl) (wint_t *) _NCURSES_EXPORT_ABI(get_wch);
extern NCURSES_EXPORT(int) (get_wstr_impl) (wint_t *) _NCURSES_EXPORT_ABI(get_wstr);
extern NCURSES_EXPORT(int) (getbkgrnd_impl) (cchar_t *) _NCURSES_EXPORT_ABI(getbkgrnd);
extern NCURSES_EXPORT(int) (getcchar_impl) (const cchar_t *, wchar_t*, attr_t*, NCURSES_PAIRS_T*, void*) _NCURSES_EXPORT_ABI(getcchar);
extern NCURSES_EXPORT(int) (getn_wstr_impl) (wint_t *, int) _NCURSES_EXPORT_ABI(getn_wstr);
extern NCURSES_EXPORT(int) (hline_set_impl) (const cchar_t *, int) _NCURSES_EXPORT_ABI(hline_set);
extern NCURSES_EXPORT(int) (in_wch_impl) (cchar_t *) _NCURSES_EXPORT_ABI(in_wch);
extern NCURSES_EXPORT(int) (in_wchnstr_impl) (cchar_t *, int) _NCURSES_EXPORT_ABI(in_wchnstr);
extern NCURSES_EXPORT(int) (in_wchstr_impl) (cchar_t *) _NCURSES_EXPORT_ABI(in_wchstr);
extern NCURSES_EXPORT(int) (innwstr_impl) (wchar_t *, int) _NCURSES_EXPORT_ABI(innwstr);
extern NCURSES_EXPORT(int) (ins_nwstr_impl) (const wchar_t *, int) _NCURSES_EXPORT_ABI(ins_nwstr);
extern NCURSES_EXPORT(int) (ins_wch_impl) (const cchar_t *) _NCURSES_EXPORT_ABI(ins_wch);
extern NCURSES_EXPORT(int) (ins_wstr_impl) (const wchar_t *) _NCURSES_EXPORT_ABI(ins_wstr);
extern NCURSES_EXPORT(int) (inwstr_impl) (wchar_t *) _NCURSES_EXPORT_ABI(inwstr);
extern NCURSES_EXPORT(NCURSES_CONST char*) (key_name_impl) (wchar_t) _NCURSES_EXPORT_ABI(key_name);
extern NCURSES_EXPORT(int) (killwchar_impl) (wchar_t *) _NCURSES_EXPORT_ABI(killwchar);
extern NCURSES_EXPORT(int) (mvadd_wch_impl) (int, int, const cchar_t *) _NCURSES_EXPORT_ABI(mvadd_wch);
extern NCURSES_EXPORT(int) (mvadd_wchnstr_impl) (int, int, const cchar_t *, int) _NCURSES_EXPORT_ABI(mvadd_wchnstr);
extern NCURSES_EXPORT(int) (mvadd_wchstr_impl) (int, int, const cchar_t *) _NCURSES_EXPORT_ABI(mvadd_wchstr);
extern NCURSES_EXPORT(int) (mvaddnwstr_impl) (int, int, const wchar_t *, int) _NCURSES_EXPORT_ABI(mvaddnwstr);
extern NCURSES_EXPORT(int) (mvaddwstr_impl) (int, int, const wchar_t *) _NCURSES_EXPORT_ABI(mvaddwstr);
extern NCURSES_EXPORT(int) (mvget_wch_impl) (int, int, wint_t *) _NCURSES_EXPORT_ABI(mvget_wch);
extern NCURSES_EXPORT(int) (mvget_wstr_impl) (int, int, wint_t *) _NCURSES_EXPORT_ABI(mvget_wstr);
extern NCURSES_EXPORT(int) (mvgetn_wstr_impl) (int, int, wint_t *, int) _NCURSES_EXPORT_ABI(mvgetn_wstr);
extern NCURSES_EXPORT(int) (mvhline_set_impl) (int, int, const cchar_t *, int) _NCURSES_EXPORT_ABI(mvhline_set);
extern NCURSES_EXPORT(int) (mvin_wch_impl) (int, int, cchar_t *) _NCURSES_EXPORT_ABI(mvin_wch);
extern NCURSES_EXPORT(int) (mvin_wchnstr_impl) (int, int, cchar_t *, int) _NCURSES_EXPORT_ABI(mvin_wchnstr);
extern NCURSES_EXPORT(int) (mvin_wchstr_impl) (int, int, cchar_t *) _NCURSES_EXPORT_ABI(mvin_wchstr);
extern NCURSES_EXPORT(int) (mvinnwstr_impl) (int, int, wchar_t *, int) _NCURSES_EXPORT_ABI(mvinnwstr);
extern NCURSES_EXPORT(int) (mvins_nwstr_impl) (int, int, const wchar_t *, int) _NCURSES_EXPORT_ABI(mvins_nwstr);
extern NCURSES_EXPORT(int) (mvins_wch_impl) (int, int, const cchar_t *) _NCURSES_EXPORT_ABI(mvins_wch);
extern NCURSES_EXPORT(int) (mvins_wstr_impl) (int, int, const wchar_t *) _NCURSES_EXPORT_ABI(mvins_wstr);
extern NCURSES_EXPORT(int) (mvinwstr_impl) (int, int, wchar_t *) _NCURSES_EXPORT_ABI(mvinwstr);
extern NCURSES_EXPORT(int) (mvvline_set_impl) (int, int, const cchar_t *, int) _NCURSES_EXPORT_ABI(mvvline_set);
extern NCURSES_EXPORT(int) (mvwadd_wch_impl) (WINDOW *, int, int, const cchar_t *) _NCURSES_EXPORT_ABI(mvwadd_wch);
extern NCURSES_EXPORT(int) (mvwadd_wchnstr_impl) (WINDOW *, int, int, const cchar_t *, int) _NCURSES_EXPORT_ABI(mvwadd_wchnstr);
extern NCURSES_EXPORT(int) (mvwadd_wchstr_impl) (WINDOW *, int, int, const cchar_t *) _NCURSES_EXPORT_ABI(mvwadd_wchstr);
extern NCURSES_EXPORT(int) (mvwaddnwstr_impl) (WINDOW *, int, int, const wchar_t *, int) _NCURSES_EXPORT_ABI(mvwaddnwstr);
extern NCURSES_EXPORT(int) (mvwaddwstr_impl) (WINDOW *, int, int, const wchar_t *) _NCURSES_EXPORT_ABI(mvwaddwstr);
extern NCURSES_EXPORT(int) (mvwget_wch_impl) (WINDOW *, int, int, wint_t *) _NCURSES_EXPORT_ABI(mvwget_wch);
extern NCURSES_EXPORT(int) (mvwget_wstr_impl) (WINDOW *, int, int, wint_t *) _NCURSES_EXPORT_ABI(mvwget_wstr);
extern NCURSES_EXPORT(int) (mvwgetn_wstr_impl) (WINDOW *, int, int, wint_t *, int) _NCURSES_EXPORT_ABI(mvwgetn_wstr);
extern NCURSES_EXPORT(int) (mvwhline_set_impl) (WINDOW *, int, int, const cchar_t *, int) _NCURSES_EXPORT_ABI(mvwhline_set);
extern NCURSES_EXPORT(int) (mvwin_wch_impl) (WINDOW *, int, int, cchar_t *) _NCURSES_EXPORT_ABI(mvwin_wch);
extern NCURSES_EXPORT(int) (mvwin_wchnstr_impl) (WINDOW *, int, int, cchar_t *, int) _NCURSES_EXPORT_ABI(mvwin_wchnstr);
extern NCURSES_EXPORT(int) (mvwin_wchstr_impl) (WINDOW *, int, int, cchar_t *) _NCURSES_EXPORT_ABI(mvwin_wchstr);
extern NCURSES_EXPORT(int) (mvwinnwstr_impl) (WINDOW *, int, int, wchar_t *, int) _NCURSES_EXPORT_ABI(mvwinnwstr);
extern NCURSES_EXPORT(int) (mvwins_nwstr_impl) (WINDOW *, int, int, const wchar_t *, int) _NCURSES_EXPORT_ABI(mvwins_nwstr);
extern NCURSES_EXPORT(int) (mvwins_wch_impl) (WINDOW *, int, int, const cchar_t *) _NCURSES_EXPORT_ABI(mvwins_wch);
extern NCURSES_EXPORT(int) (mvwins_wstr_impl) (WINDOW *, int, int, const wchar_t *) _NCURSES_EXPORT_ABI(mvwins_wstr);
extern NCURSES_EXPORT(int) (mvwinwstr_impl) (WINDOW *, int, int, wchar_t *) _NCURSES_EXPORT_ABI(mvwinwstr);
extern NCURSES_EXPORT(int) (mvwvline_set_impl) (WINDOW *, int, int, const cchar_t *, int) _NCURSES_EXPORT_ABI(mvwvline_set);
extern NCURSES_EXPORT(int) (pecho_wchar_impl) (WINDOW *, const cchar_t *) _NCURSES_EXPORT_ABI(pecho_wchar);
extern NCURSES_EXPORT(int) (setcchar_impl) (cchar_t *, const wchar_t *, const attr_t, NCURSES_PAIRS_T, const void *) _NCURSES_EXPORT_ABI(setcchar);
extern NCURSES_EXPORT(int) (slk_wset_impl) (int, const wchar_t *, int) _NCURSES_EXPORT_ABI(slk_wset);
extern NCURSES_EXPORT(attr_t) (term_attrs_impl) (void) _NCURSES_EXPORT_ABI(term_attrs);
extern NCURSES_EXPORT(int) (unget_wch_impl) (const wchar_t) _NCURSES_EXPORT_ABI(unget_wch);
extern NCURSES_EXPORT(int) (vid_attr_impl) (attr_t, NCURSES_PAIRS_T, void *) _NCURSES_EXPORT_ABI(vid_attr);
extern NCURSES_EXPORT(int) (vid_puts_impl) (attr_t, NCURSES_PAIRS_T, void *, NCURSES_OUTC) _NCURSES_EXPORT_ABI(vid_puts);
extern NCURSES_EXPORT(int) (vline_set_impl) (const cchar_t *, int) _NCURSES_EXPORT_ABI(vline_set);
extern NCURSES_EXPORT(int) (wadd_wch_impl) (WINDOW *, const cchar_t *) _NCURSES_EXPORT_ABI(wadd_wch);
extern NCURSES_EXPORT(int) (wadd_wchnstr_impl) (WINDOW *, const cchar_t *, int) _NCURSES_EXPORT_ABI(wadd_wchnstr);
extern NCURSES_EXPORT(int) (wadd_wchstr_impl) (WINDOW *, const cchar_t *) _NCURSES_EXPORT_ABI(wadd_wchstr);
extern NCURSES_EXPORT(int) (waddnwstr_impl) (WINDOW *, const wchar_t *, int) _NCURSES_EXPORT_ABI(waddnwstr);
extern NCURSES_EXPORT(int) (waddwstr_impl) (WINDOW *, const wchar_t *) _NCURSES_EXPORT_ABI(waddwstr);
extern NCURSES_EXPORT(int) (wbkgrnd_impl) (WINDOW *, const cchar_t *) _NCURSES_EXPORT_ABI(wbkgrnd);
extern NCURSES_EXPORT(void) (wbkgrndset_impl) (WINDOW *, const cchar_t *) _NCURSES_EXPORT_ABI(wbkgrndset);
extern NCURSES_EXPORT(int) (wborder_set_impl) (WINDOW *, const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*, const cchar_t*) _NCURSES_EXPORT_ABI(wborder_set);
extern NCURSES_EXPORT(int) (wecho_wchar_impl) (WINDOW *, const cchar_t *) _NCURSES_EXPORT_ABI(wecho_wchar);
extern NCURSES_EXPORT(int) (wget_wch_impl) (WINDOW *, wint_t *) _NCURSES_EXPORT_ABI(wget_wch);
extern NCURSES_EXPORT(int) (wget_wstr_impl) (WINDOW *, wint_t *) _NCURSES_EXPORT_ABI(wget_wstr);
extern NCURSES_EXPORT(int) (wgetbkgrnd_impl) (WINDOW *, cchar_t *) _NCURSES_EXPORT_ABI(wgetbkgrnd);
extern NCURSES_EXPORT(int) (wgetn_wstr_impl) (WINDOW *, wint_t *, int) _NCURSES_EXPORT_ABI(wgetn_wstr);
extern NCURSES_EXPORT(int) (whline_set_impl) (WINDOW *, const cchar_t *, int) _NCURSES_EXPORT_ABI(whline_set);
extern NCURSES_EXPORT(int) (win_wch_impl) (WINDOW *, cchar_t *) _NCURSES_EXPORT_ABI(win_wch);
extern NCURSES_EXPORT(int) (win_wchnstr_impl) (WINDOW *, cchar_t *, int) _NCURSES_EXPORT_ABI(win_wchnstr);
extern NCURSES_EXPORT(int) (win_wchstr_impl) (WINDOW *, cchar_t *) _NCURSES_EXPORT_ABI(win_wchstr);
extern NCURSES_EXPORT(int) (winnwstr_impl) (WINDOW *, wchar_t *, int) _NCURSES_EXPORT_ABI(winnwstr);
extern NCURSES_EXPORT(int) (wins_nwstr_impl) (WINDOW *, const wchar_t *, int) _NCURSES_EXPORT_ABI(wins_nwstr);
extern NCURSES_EXPORT(int) (wins_wch_impl) (WINDOW *, const cchar_t *) _NCURSES_EXPORT_ABI(wins_wch);
extern NCURSES_EXPORT(int) (wins_wstr_impl) (WINDOW *, const wchar_t *) _NCURSES_EXPORT_ABI(wins_wstr);
extern NCURSES_EXPORT(int) (winwstr_impl) (WINDOW *, wchar_t *) _NCURSES_EXPORT_ABI(winwstr);
extern NCURSES_EXPORT(wchar_t*) (wunctrl_impl) (cchar_t *) _NCURSES_EXPORT_ABI(wunctrl);
extern NCURSES_EXPORT(int) (wvline_set_impl) (WINDOW *, const cchar_t *, int) _NCURSES_EXPORT_ABI(wvline_set);

NCURSES_EXPORT(int) _addch_abi60 (const chtype arg0) _NCURSES_EXPORT_ABI60(addch);

NCURSES_EXPORT(int) _addch_abi60 (const chtype arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = addch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _addchnstr_abi60 (const chtype * arg0, int arg1) _NCURSES_EXPORT_ABI60(addchnstr);

NCURSES_EXPORT(int) _addchnstr_abi60 (const chtype * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = addchnstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _addchstr_abi60 (const chtype * arg0) _NCURSES_EXPORT_ABI60(addchstr);

NCURSES_EXPORT(int) _addchstr_abi60 (const chtype * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = addchstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _addnstr_abi60 (const char * arg0, int arg1) _NCURSES_EXPORT_ABI60(addnstr);

NCURSES_EXPORT(int) _addnstr_abi60 (const char * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = addnstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _addstr_abi60 (const char * arg0) _NCURSES_EXPORT_ABI60(addstr);

NCURSES_EXPORT(int) _addstr_abi60 (const char * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = addstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _attroff_abi60 (NCURSES_ATTR_T arg0) _NCURSES_EXPORT_ABI60(attroff);

NCURSES_EXPORT(int) _attroff_abi60 (NCURSES_ATTR_T arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = attroff_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _attron_abi60 (NCURSES_ATTR_T arg0) _NCURSES_EXPORT_ABI60(attron);

NCURSES_EXPORT(int) _attron_abi60 (NCURSES_ATTR_T arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = attron_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _attrset_abi60 (NCURSES_ATTR_T arg0) _NCURSES_EXPORT_ABI60(attrset);

NCURSES_EXPORT(int) _attrset_abi60 (NCURSES_ATTR_T arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = attrset_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _attr_get_abi60 (attr_t * arg0, NCURSES_PAIRS_T * arg1, void * arg2) _NCURSES_EXPORT_ABI60(attr_get);

NCURSES_EXPORT(int) _attr_get_abi60 (attr_t * arg0, NCURSES_PAIRS_T * arg1, void * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = attr_get_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _attr_off_abi60 (attr_t arg0, void * arg1) _NCURSES_EXPORT_ABI60(attr_off);

NCURSES_EXPORT(int) _attr_off_abi60 (attr_t arg0, void * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = attr_off_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _attr_on_abi60 (attr_t arg0, void * arg1) _NCURSES_EXPORT_ABI60(attr_on);

NCURSES_EXPORT(int) _attr_on_abi60 (attr_t arg0, void * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = attr_on_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _attr_set_abi60 (attr_t arg0, NCURSES_PAIRS_T arg1, void * arg2) _NCURSES_EXPORT_ABI60(attr_set);

NCURSES_EXPORT(int) _attr_set_abi60 (attr_t arg0, NCURSES_PAIRS_T arg1, void * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = attr_set_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _bkgd_abi60 (chtype arg0) _NCURSES_EXPORT_ABI60(bkgd);

NCURSES_EXPORT(int) _bkgd_abi60 (chtype arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = bkgd_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(void) _bkgdset_abi60 (chtype arg0) _NCURSES_EXPORT_ABI60(bkgdset);

NCURSES_EXPORT(void) _bkgdset_abi60 (chtype arg0)
{
	NCURSES_ABI_PUSH(6, 0);
	bkgdset_impl(arg0);
	NCURSES_ABI_POP();
}

NCURSES_EXPORT(int) _border_abi60 (chtype arg0, chtype arg1, chtype arg2, chtype arg3, chtype arg4, chtype arg5, chtype arg6, chtype arg7) _NCURSES_EXPORT_ABI60(border);

NCURSES_EXPORT(int) _border_abi60 (chtype arg0, chtype arg1, chtype arg2, chtype arg3, chtype arg4, chtype arg5, chtype arg6, chtype arg7)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = border_impl(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _box_abi60 (WINDOW * arg0, chtype arg1, chtype arg2) _NCURSES_EXPORT_ABI60(box);

NCURSES_EXPORT(int) _box_abi60 (WINDOW * arg0, chtype arg1, chtype arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = box_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _chgat_abi60 (int arg0, attr_t arg1, NCURSES_PAIRS_T arg2, const void * arg3) _NCURSES_EXPORT_ABI60(chgat);

NCURSES_EXPORT(int) _chgat_abi60 (int arg0, attr_t arg1, NCURSES_PAIRS_T arg2, const void * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = chgat_impl(arg0, arg1, arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _color_content_abi60 (NCURSES_COLOR_T arg0, NCURSES_COLOR_T* arg1, NCURSES_COLOR_T* arg2, NCURSES_COLOR_T* arg3) _NCURSES_EXPORT_ABI60(color_content);

NCURSES_EXPORT(int) _color_content_abi60 (NCURSES_COLOR_T arg0, NCURSES_COLOR_T* arg1, NCURSES_COLOR_T* arg2, NCURSES_COLOR_T* arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = color_content_impl(arg0, arg1, arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _color_set_abi60 (NCURSES_PAIRS_T arg0, void* arg1) _NCURSES_EXPORT_ABI60(color_set);

NCURSES_EXPORT(int) _color_set_abi60 (NCURSES_PAIRS_T arg0, void* arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = color_set_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _copywin_abi60 (const WINDOW* arg0, WINDOW* arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8) _NCURSES_EXPORT_ABI60(copywin);

NCURSES_EXPORT(int) _copywin_abi60 (const WINDOW* arg0, WINDOW* arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = copywin_impl(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _delay_output_abi60 (int arg0) _NCURSES_EXPORT_ABI60(delay_output);

NCURSES_EXPORT(int) _delay_output_abi60 (int arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = delay_output_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _deleteln_abi60 (void) _NCURSES_EXPORT_ABI60(deleteln);

NCURSES_EXPORT(int) _deleteln_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = deleteln_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(WINDOW *) _derwin_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, int arg4) _NCURSES_EXPORT_ABI60(derwin);

NCURSES_EXPORT(WINDOW *) _derwin_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, int arg4)
{
	WINDOW * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = derwin_impl(arg0, arg1, arg2, arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _doupdate_abi60 (void) _NCURSES_EXPORT_ABI60(doupdate);

NCURSES_EXPORT(int) _doupdate_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = doupdate_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(WINDOW *) _dupwin_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(dupwin);

NCURSES_EXPORT(WINDOW *) _dupwin_abi60 (WINDOW * arg0)
{
	WINDOW * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = dupwin_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _echochar_abi60 (const chtype arg0) _NCURSES_EXPORT_ABI60(echochar);

NCURSES_EXPORT(int) _echochar_abi60 (const chtype arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = echochar_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _endwin_abi60 (void) _NCURSES_EXPORT_ABI60(endwin);

NCURSES_EXPORT(int) _endwin_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = endwin_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _flushinp_abi60 (void) _NCURSES_EXPORT_ABI60(flushinp);

NCURSES_EXPORT(int) _flushinp_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = flushinp_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(chtype) _getbkgd_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(getbkgd);

NCURSES_EXPORT(chtype) _getbkgd_abi60 (WINDOW * arg0)
{
	chtype ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = getbkgd_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _getch_abi60 (void) _NCURSES_EXPORT_ABI60(getch);

NCURSES_EXPORT(int) _getch_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = getch_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _getnstr_abi60 (char * arg0, int arg1) _NCURSES_EXPORT_ABI60(getnstr);

NCURSES_EXPORT(int) _getnstr_abi60 (char * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = getnstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _getstr_abi60 (char * arg0) _NCURSES_EXPORT_ABI60(getstr);

NCURSES_EXPORT(int) _getstr_abi60 (char * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = getstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(WINDOW *) _getwin_abi60 (FILE * arg0) _NCURSES_EXPORT_ABI60(getwin);

NCURSES_EXPORT(WINDOW *) _getwin_abi60 (FILE * arg0)
{
	WINDOW * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = getwin_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _hline_abi60 (chtype arg0, int arg1) _NCURSES_EXPORT_ABI60(hline);

NCURSES_EXPORT(int) _hline_abi60 (chtype arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = hline_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(chtype) _inch_abi60 (void) _NCURSES_EXPORT_ABI60(inch);

NCURSES_EXPORT(chtype) _inch_abi60 (void)
{
	chtype ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = inch_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _inchnstr_abi60 (chtype * arg0, int arg1) _NCURSES_EXPORT_ABI60(inchnstr);

NCURSES_EXPORT(int) _inchnstr_abi60 (chtype * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = inchnstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _inchstr_abi60 (chtype * arg0) _NCURSES_EXPORT_ABI60(inchstr);

NCURSES_EXPORT(int) _inchstr_abi60 (chtype * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = inchstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(WINDOW *) _initscr_abi60 (void) _NCURSES_EXPORT_ABI60(initscr);

NCURSES_EXPORT(WINDOW *) _initscr_abi60 (void)
{
	WINDOW * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = initscr_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _init_color_abi60 (NCURSES_COLOR_T arg0, NCURSES_COLOR_T arg1, NCURSES_COLOR_T arg2, NCURSES_COLOR_T arg3) _NCURSES_EXPORT_ABI60(init_color);

NCURSES_EXPORT(int) _init_color_abi60 (NCURSES_COLOR_T arg0, NCURSES_COLOR_T arg1, NCURSES_COLOR_T arg2, NCURSES_COLOR_T arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = init_color_impl(arg0, arg1, arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _init_pair_abi60 (NCURSES_PAIRS_T arg0, NCURSES_COLOR_T arg1, NCURSES_COLOR_T arg2) _NCURSES_EXPORT_ABI60(init_pair);

NCURSES_EXPORT(int) _init_pair_abi60 (NCURSES_PAIRS_T arg0, NCURSES_COLOR_T arg1, NCURSES_COLOR_T arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = init_pair_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _innstr_abi60 (char * arg0, int arg1) _NCURSES_EXPORT_ABI60(innstr);

NCURSES_EXPORT(int) _innstr_abi60 (char * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = innstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _insch_abi60 (chtype arg0) _NCURSES_EXPORT_ABI60(insch);

NCURSES_EXPORT(int) _insch_abi60 (chtype arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = insch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _insdelln_abi60 (int arg0) _NCURSES_EXPORT_ABI60(insdelln);

NCURSES_EXPORT(int) _insdelln_abi60 (int arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = insdelln_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _insertln_abi60 (void) _NCURSES_EXPORT_ABI60(insertln);

NCURSES_EXPORT(int) _insertln_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = insertln_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _insnstr_abi60 (const char * arg0, int arg1) _NCURSES_EXPORT_ABI60(insnstr);

NCURSES_EXPORT(int) _insnstr_abi60 (const char * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = insnstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _insstr_abi60 (const char * arg0) _NCURSES_EXPORT_ABI60(insstr);

NCURSES_EXPORT(int) _insstr_abi60 (const char * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = insstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _instr_abi60 (char * arg0) _NCURSES_EXPORT_ABI60(instr);

NCURSES_EXPORT(int) _instr_abi60 (char * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = instr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _intrflush_abi60 (WINDOW * arg0, bool arg1) _NCURSES_EXPORT_ABI60(intrflush);

NCURSES_EXPORT(int) _intrflush_abi60 (WINDOW * arg0, bool arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = intrflush_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _keypad_abi60 (WINDOW * arg0, bool arg1) _NCURSES_EXPORT_ABI60(keypad);

NCURSES_EXPORT(int) _keypad_abi60 (WINDOW * arg0, bool arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = keypad_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvaddch_abi60 (int arg0, int arg1, const chtype arg2) _NCURSES_EXPORT_ABI60(mvaddch);

NCURSES_EXPORT(int) _mvaddch_abi60 (int arg0, int arg1, const chtype arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addch_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvaddchnstr_abi60 (int arg0, int arg1, const chtype * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvaddchnstr);

NCURSES_EXPORT(int) _mvaddchnstr_abi60 (int arg0, int arg1, const chtype * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addchnstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvaddchstr_abi60 (int arg0, int arg1, const chtype * arg2) _NCURSES_EXPORT_ABI60(mvaddchstr);

NCURSES_EXPORT(int) _mvaddchstr_abi60 (int arg0, int arg1, const chtype * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addchstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvaddnstr_abi60 (int arg0, int arg1, const char * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvaddnstr);

NCURSES_EXPORT(int) _mvaddnstr_abi60 (int arg0, int arg1, const char * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addnstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvaddstr_abi60 (int arg0, int arg1, const char * arg2) _NCURSES_EXPORT_ABI60(mvaddstr);

NCURSES_EXPORT(int) _mvaddstr_abi60 (int arg0, int arg1, const char * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvchgat_abi60 (int arg0, int arg1, int arg2, attr_t arg3, NCURSES_PAIRS_T arg4, const void * arg5) _NCURSES_EXPORT_ABI60(mvchgat);

NCURSES_EXPORT(int) _mvchgat_abi60 (int arg0, int arg1, int arg2, attr_t arg3, NCURSES_PAIRS_T arg4, const void * arg5)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = chgat_impl(arg2, arg3, arg4, arg5);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvcur_abi60 (int arg0, int arg1, int arg2, int arg3) _NCURSES_EXPORT_ABI60(mvcur);

NCURSES_EXPORT(int) _mvcur_abi60 (int arg0, int arg1, int arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = mvcur_impl(arg0, arg1, arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvderwin_abi60 (WINDOW * arg0, int arg1, int arg2) _NCURSES_EXPORT_ABI60(mvderwin);

NCURSES_EXPORT(int) _mvderwin_abi60 (WINDOW * arg0, int arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = mvderwin_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvgetch_abi60 (int arg0, int arg1) _NCURSES_EXPORT_ABI60(mvgetch);

NCURSES_EXPORT(int) _mvgetch_abi60 (int arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = getch_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvgetnstr_abi60 (int arg0, int arg1, char * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvgetnstr);

NCURSES_EXPORT(int) _mvgetnstr_abi60 (int arg0, int arg1, char * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = getnstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvgetstr_abi60 (int arg0, int arg1, char * arg2) _NCURSES_EXPORT_ABI60(mvgetstr);

NCURSES_EXPORT(int) _mvgetstr_abi60 (int arg0, int arg1, char * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = getstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvhline_abi60 (int arg0, int arg1, chtype arg2, int arg3) _NCURSES_EXPORT_ABI60(mvhline);

NCURSES_EXPORT(int) _mvhline_abi60 (int arg0, int arg1, chtype arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = hline_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(chtype) _mvinch_abi60 (int arg0, int arg1) _NCURSES_EXPORT_ABI60(mvinch);

NCURSES_EXPORT(chtype) _mvinch_abi60 (int arg0, int arg1)
{
	chtype ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = inch_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvinchnstr_abi60 (int arg0, int arg1, chtype * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvinchnstr);

NCURSES_EXPORT(int) _mvinchnstr_abi60 (int arg0, int arg1, chtype * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = inchnstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvinchstr_abi60 (int arg0, int arg1, chtype * arg2) _NCURSES_EXPORT_ABI60(mvinchstr);

NCURSES_EXPORT(int) _mvinchstr_abi60 (int arg0, int arg1, chtype * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = inchstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvinnstr_abi60 (int arg0, int arg1, char * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvinnstr);

NCURSES_EXPORT(int) _mvinnstr_abi60 (int arg0, int arg1, char * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = innstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvinsch_abi60 (int arg0, int arg1, chtype arg2) _NCURSES_EXPORT_ABI60(mvinsch);

NCURSES_EXPORT(int) _mvinsch_abi60 (int arg0, int arg1, chtype arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = insch_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvinsnstr_abi60 (int arg0, int arg1, const char * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvinsnstr);

NCURSES_EXPORT(int) _mvinsnstr_abi60 (int arg0, int arg1, const char * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = insnstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvinsstr_abi60 (int arg0, int arg1, const char * arg2) _NCURSES_EXPORT_ABI60(mvinsstr);

NCURSES_EXPORT(int) _mvinsstr_abi60 (int arg0, int arg1, const char * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = insstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvinstr_abi60 (int arg0, int arg1, char * arg2) _NCURSES_EXPORT_ABI60(mvinstr);

NCURSES_EXPORT(int) _mvinstr_abi60 (int arg0, int arg1, char * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = instr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvprintw_abi60 (int arg0, int arg1, const char * arg2, ...) _NCURSES_EXPORT_ABI60(mvprintw);

NCURSES_EXPORT(int) _mvprintw_abi60 (int arg0, int arg1, const char * arg2, ...)
{
	va_list argp;
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	va_start(argp, arg2);
	ret = vwprintw_impl(stdscr, arg2, argp);
	va_end(argp);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvscanw_abi60 (int arg0, int arg1, NCURSES_CONST char * arg2, ...) _NCURSES_EXPORT_ABI60(mvscanw);

NCURSES_EXPORT(int) _mvscanw_abi60 (int arg0, int arg1, NCURSES_CONST char * arg2, ...)
{
	va_list argp;
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	va_start(argp, arg2);
	ret = vwscanw_impl(stdscr, arg2, argp);
	va_end(argp);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvvline_abi60 (int arg0, int arg1, chtype arg2, int arg3) _NCURSES_EXPORT_ABI60(mvvline);

NCURSES_EXPORT(int) _mvvline_abi60 (int arg0, int arg1, chtype arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = vline_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwaddch_abi60 (WINDOW * arg0, int arg1, int arg2, const chtype arg3) _NCURSES_EXPORT_ABI60(mvwaddch);

NCURSES_EXPORT(int) _mvwaddch_abi60 (WINDOW * arg0, int arg1, int arg2, const chtype arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addch_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwaddchnstr_abi60 (WINDOW * arg0, int arg1, int arg2, const chtype * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwaddchnstr);

NCURSES_EXPORT(int) _mvwaddchnstr_abi60 (WINDOW * arg0, int arg1, int arg2, const chtype * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addchnstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwaddchstr_abi60 (WINDOW * arg0, int arg1, int arg2, const chtype * arg3) _NCURSES_EXPORT_ABI60(mvwaddchstr);

NCURSES_EXPORT(int) _mvwaddchstr_abi60 (WINDOW * arg0, int arg1, int arg2, const chtype * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addchstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwaddnstr_abi60 (WINDOW * arg0, int arg1, int arg2, const char * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwaddnstr);

NCURSES_EXPORT(int) _mvwaddnstr_abi60 (WINDOW * arg0, int arg1, int arg2, const char * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addnstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwaddstr_abi60 (WINDOW * arg0, int arg1, int arg2, const char * arg3) _NCURSES_EXPORT_ABI60(mvwaddstr);

NCURSES_EXPORT(int) _mvwaddstr_abi60 (WINDOW * arg0, int arg1, int arg2, const char * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwchgat_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, attr_t arg4, NCURSES_PAIRS_T arg5, const void * arg6) _NCURSES_EXPORT_ABI60(mvwchgat);

NCURSES_EXPORT(int) _mvwchgat_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, attr_t arg4, NCURSES_PAIRS_T arg5, const void * arg6)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = chgat_impl(arg3, arg4, arg5, arg6);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwgetch_abi60 (WINDOW * arg0, int arg1, int arg2) _NCURSES_EXPORT_ABI60(mvwgetch);

NCURSES_EXPORT(int) _mvwgetch_abi60 (WINDOW * arg0, int arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = getch_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwgetnstr_abi60 (WINDOW * arg0, int arg1, int arg2, char * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwgetnstr);

NCURSES_EXPORT(int) _mvwgetnstr_abi60 (WINDOW * arg0, int arg1, int arg2, char * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = getnstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwgetstr_abi60 (WINDOW * arg0, int arg1, int arg2, char * arg3) _NCURSES_EXPORT_ABI60(mvwgetstr);

NCURSES_EXPORT(int) _mvwgetstr_abi60 (WINDOW * arg0, int arg1, int arg2, char * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = getstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwhline_abi60 (WINDOW * arg0, int arg1, int arg2, chtype arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwhline);

NCURSES_EXPORT(int) _mvwhline_abi60 (WINDOW * arg0, int arg1, int arg2, chtype arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = hline_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwin_abi60 (WINDOW * arg0, int arg1, int arg2) _NCURSES_EXPORT_ABI60(mvwin);

NCURSES_EXPORT(int) _mvwin_abi60 (WINDOW * arg0, int arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = mvwin_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(chtype) _mvwinch_abi60 (WINDOW * arg0, int arg1, int arg2) _NCURSES_EXPORT_ABI60(mvwinch);

NCURSES_EXPORT(chtype) _mvwinch_abi60 (WINDOW * arg0, int arg1, int arg2)
{
	chtype ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = inch_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwinchnstr_abi60 (WINDOW * arg0, int arg1, int arg2, chtype * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwinchnstr);

NCURSES_EXPORT(int) _mvwinchnstr_abi60 (WINDOW * arg0, int arg1, int arg2, chtype * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = inchnstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwinchstr_abi60 (WINDOW * arg0, int arg1, int arg2, chtype * arg3) _NCURSES_EXPORT_ABI60(mvwinchstr);

NCURSES_EXPORT(int) _mvwinchstr_abi60 (WINDOW * arg0, int arg1, int arg2, chtype * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = inchstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwinnstr_abi60 (WINDOW * arg0, int arg1, int arg2, char * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwinnstr);

NCURSES_EXPORT(int) _mvwinnstr_abi60 (WINDOW * arg0, int arg1, int arg2, char * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = innstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwinsch_abi60 (WINDOW * arg0, int arg1, int arg2, chtype arg3) _NCURSES_EXPORT_ABI60(mvwinsch);

NCURSES_EXPORT(int) _mvwinsch_abi60 (WINDOW * arg0, int arg1, int arg2, chtype arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = insch_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwinsnstr_abi60 (WINDOW * arg0, int arg1, int arg2, const char * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwinsnstr);

NCURSES_EXPORT(int) _mvwinsnstr_abi60 (WINDOW * arg0, int arg1, int arg2, const char * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = insnstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwinsstr_abi60 (WINDOW * arg0, int arg1, int arg2, const char * arg3) _NCURSES_EXPORT_ABI60(mvwinsstr);

NCURSES_EXPORT(int) _mvwinsstr_abi60 (WINDOW * arg0, int arg1, int arg2, const char * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = insstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwinstr_abi60 (WINDOW * arg0, int arg1, int arg2, char * arg3) _NCURSES_EXPORT_ABI60(mvwinstr);

NCURSES_EXPORT(int) _mvwinstr_abi60 (WINDOW * arg0, int arg1, int arg2, char * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = instr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwprintw_abi60 (WINDOW* arg0, int arg1, int arg2, const char * arg3, ...) _NCURSES_EXPORT_ABI60(mvwprintw);

NCURSES_EXPORT(int) _mvwprintw_abi60 (WINDOW* arg0, int arg1, int arg2, const char * arg3, ...)
{
	va_list argp;
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	va_start(argp, arg3);
	ret = vwprintw_impl(arg0, arg3, argp);
	va_end(argp);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwscanw_abi60 (WINDOW * arg0, int arg1, int arg2, NCURSES_CONST char * arg3, ...) _NCURSES_EXPORT_ABI60(mvwscanw);

NCURSES_EXPORT(int) _mvwscanw_abi60 (WINDOW * arg0, int arg1, int arg2, NCURSES_CONST char * arg3, ...)
{
	va_list argp;
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	va_start(argp, arg3);
	ret = vwscanw_impl(arg0, arg3, argp);
	va_end(argp);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwvline_abi60 (WINDOW * arg0, int arg1, int arg2, chtype arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwvline);

NCURSES_EXPORT(int) _mvwvline_abi60 (WINDOW * arg0, int arg1, int arg2, chtype arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = vline_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(WINDOW *) _newpad_abi60 (int arg0, int arg1) _NCURSES_EXPORT_ABI60(newpad);

NCURSES_EXPORT(WINDOW *) _newpad_abi60 (int arg0, int arg1)
{
	WINDOW * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = newpad_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(SCREEN *) _newterm_abi60 (NCURSES_CONST char * arg0, FILE * arg1, FILE * arg2) _NCURSES_EXPORT_ABI60(newterm);

NCURSES_EXPORT(SCREEN *) _newterm_abi60 (NCURSES_CONST char * arg0, FILE * arg1, FILE * arg2)
{
	SCREEN * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = newterm_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(WINDOW *) _newwin_abi60 (int arg0, int arg1, int arg2, int arg3) _NCURSES_EXPORT_ABI60(newwin);

NCURSES_EXPORT(WINDOW *) _newwin_abi60 (int arg0, int arg1, int arg2, int arg3)
{
	WINDOW * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = newwin_impl(arg0, arg1, arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _overlay_abi60 (const WINDOW* arg0, WINDOW * arg1) _NCURSES_EXPORT_ABI60(overlay);

NCURSES_EXPORT(int) _overlay_abi60 (const WINDOW* arg0, WINDOW * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = overlay_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _overwrite_abi60 (const WINDOW* arg0, WINDOW * arg1) _NCURSES_EXPORT_ABI60(overwrite);

NCURSES_EXPORT(int) _overwrite_abi60 (const WINDOW* arg0, WINDOW * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = overwrite_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _pair_content_abi60 (NCURSES_PAIRS_T arg0, NCURSES_COLOR_T* arg1, NCURSES_COLOR_T* arg2) _NCURSES_EXPORT_ABI60(pair_content);

NCURSES_EXPORT(int) _pair_content_abi60 (NCURSES_PAIRS_T arg0, NCURSES_COLOR_T* arg1, NCURSES_COLOR_T* arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = pair_content_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _pechochar_abi60 (WINDOW * arg0, const chtype arg1) _NCURSES_EXPORT_ABI60(pechochar);

NCURSES_EXPORT(int) _pechochar_abi60 (WINDOW * arg0, const chtype arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = pechochar_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _pnoutrefresh_abi60 (WINDOW* arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) _NCURSES_EXPORT_ABI60(pnoutrefresh);

NCURSES_EXPORT(int) _pnoutrefresh_abi60 (WINDOW* arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = pnoutrefresh_impl(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _prefresh_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) _NCURSES_EXPORT_ABI60(prefresh);

NCURSES_EXPORT(int) _prefresh_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = prefresh_impl(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _printw_abi60 (const char * arg0, ...) _NCURSES_EXPORT_ABI60(printw);

NCURSES_EXPORT(int) _printw_abi60 (const char * arg0, ...)
{
	va_list argp;
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	va_start(argp, arg0);
	ret = vwprintw_impl(stdscr, arg0, argp);
	va_end(argp);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _putwin_abi60 (WINDOW * arg0, FILE * arg1) _NCURSES_EXPORT_ABI60(putwin);

NCURSES_EXPORT(int) _putwin_abi60 (WINDOW * arg0, FILE * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = putwin_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _redrawwin_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(redrawwin);

NCURSES_EXPORT(int) _redrawwin_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = redrawwin_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _refresh_abi60 (void) _NCURSES_EXPORT_ABI60(refresh);

NCURSES_EXPORT(int) _refresh_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = refresh_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _scanw_abi60 (NCURSES_CONST char * arg0, ...) _NCURSES_EXPORT_ABI60(scanw);

NCURSES_EXPORT(int) _scanw_abi60 (NCURSES_CONST char * arg0, ...)
{
	va_list argp;
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	va_start(argp, arg0);
	ret = vwscanw_impl(stdscr, arg0, argp);
	va_end(argp);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _scr_dump_abi60 (const char * arg0) _NCURSES_EXPORT_ABI60(scr_dump);

NCURSES_EXPORT(int) _scr_dump_abi60 (const char * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = scr_dump_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _scr_init_abi60 (const char * arg0) _NCURSES_EXPORT_ABI60(scr_init);

NCURSES_EXPORT(int) _scr_init_abi60 (const char * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = scr_init_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _scrl_abi60 (int arg0) _NCURSES_EXPORT_ABI60(scrl);

NCURSES_EXPORT(int) _scrl_abi60 (int arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = scrl_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _scroll_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(scroll);

NCURSES_EXPORT(int) _scroll_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = scroll_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _scrollok_abi60 (WINDOW * arg0, bool arg1) _NCURSES_EXPORT_ABI60(scrollok);

NCURSES_EXPORT(int) _scrollok_abi60 (WINDOW * arg0, bool arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = scrollok_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _scr_restore_abi60 (const char * arg0) _NCURSES_EXPORT_ABI60(scr_restore);

NCURSES_EXPORT(int) _scr_restore_abi60 (const char * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = scr_restore_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _scr_set_abi60 (const char * arg0) _NCURSES_EXPORT_ABI60(scr_set);

NCURSES_EXPORT(int) _scr_set_abi60 (const char * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = scr_set_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(SCREEN *) _set_term_abi60 (SCREEN * arg0) _NCURSES_EXPORT_ABI60(set_term);

NCURSES_EXPORT(SCREEN *) _set_term_abi60 (SCREEN * arg0)
{
	SCREEN * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = set_term_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_attroff_abi60 (const chtype arg0) _NCURSES_EXPORT_ABI60(slk_attroff);

NCURSES_EXPORT(int) _slk_attroff_abi60 (const chtype arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_attroff_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_attr_off_abi60 (const attr_t arg0, void * arg1) _NCURSES_EXPORT_ABI60(slk_attr_off);

NCURSES_EXPORT(int) _slk_attr_off_abi60 (const attr_t arg0, void * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_attr_off_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_attron_abi60 (const chtype arg0) _NCURSES_EXPORT_ABI60(slk_attron);

NCURSES_EXPORT(int) _slk_attron_abi60 (const chtype arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_attron_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_attr_on_abi60 (attr_t arg0, void* arg1) _NCURSES_EXPORT_ABI60(slk_attr_on);

NCURSES_EXPORT(int) _slk_attr_on_abi60 (attr_t arg0, void* arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_attr_on_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_attrset_abi60 (const chtype arg0) _NCURSES_EXPORT_ABI60(slk_attrset);

NCURSES_EXPORT(int) _slk_attrset_abi60 (const chtype arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_attrset_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(attr_t) _slk_attr_abi60 (void) _NCURSES_EXPORT_ABI60(slk_attr);

NCURSES_EXPORT(attr_t) _slk_attr_abi60 (void)
{
	attr_t ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_attr_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_attr_set_abi60 (const attr_t arg0, NCURSES_PAIRS_T arg1, void* arg2) _NCURSES_EXPORT_ABI60(slk_attr_set);

NCURSES_EXPORT(int) _slk_attr_set_abi60 (const attr_t arg0, NCURSES_PAIRS_T arg1, void* arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_attr_set_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_clear_abi60 (void) _NCURSES_EXPORT_ABI60(slk_clear);

NCURSES_EXPORT(int) _slk_clear_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_clear_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_color_abi60 (NCURSES_PAIRS_T arg0) _NCURSES_EXPORT_ABI60(slk_color);

NCURSES_EXPORT(int) _slk_color_abi60 (NCURSES_PAIRS_T arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_color_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_init_abi60 (int arg0) _NCURSES_EXPORT_ABI60(slk_init);

NCURSES_EXPORT(int) _slk_init_abi60 (int arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_init_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(char *) _slk_label_abi60 (int arg0) _NCURSES_EXPORT_ABI60(slk_label);

NCURSES_EXPORT(char *) _slk_label_abi60 (int arg0)
{
	char * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_label_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_noutrefresh_abi60 (void) _NCURSES_EXPORT_ABI60(slk_noutrefresh);

NCURSES_EXPORT(int) _slk_noutrefresh_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_noutrefresh_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_refresh_abi60 (void) _NCURSES_EXPORT_ABI60(slk_refresh);

NCURSES_EXPORT(int) _slk_refresh_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_refresh_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_restore_abi60 (void) _NCURSES_EXPORT_ABI60(slk_restore);

NCURSES_EXPORT(int) _slk_restore_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_restore_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_set_abi60 (int arg0, const char * arg1, int arg2) _NCURSES_EXPORT_ABI60(slk_set);

NCURSES_EXPORT(int) _slk_set_abi60 (int arg0, const char * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_set_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_touch_abi60 (void) _NCURSES_EXPORT_ABI60(slk_touch);

NCURSES_EXPORT(int) _slk_touch_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_touch_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _standout_abi60 (void) _NCURSES_EXPORT_ABI60(standout);

NCURSES_EXPORT(int) _standout_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = standout_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _standend_abi60 (void) _NCURSES_EXPORT_ABI60(standend);

NCURSES_EXPORT(int) _standend_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = standend_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _start_color_abi60 (void) _NCURSES_EXPORT_ABI60(start_color);

NCURSES_EXPORT(int) _start_color_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = start_color_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(WINDOW *) _subpad_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, int arg4) _NCURSES_EXPORT_ABI60(subpad);

NCURSES_EXPORT(WINDOW *) _subpad_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, int arg4)
{
	WINDOW * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = subpad_impl(arg0, arg1, arg2, arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(WINDOW *) _subwin_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, int arg4) _NCURSES_EXPORT_ABI60(subwin);

NCURSES_EXPORT(WINDOW *) _subwin_abi60 (WINDOW * arg0, int arg1, int arg2, int arg3, int arg4)
{
	WINDOW * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = subwin_impl(arg0, arg1, arg2, arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _ungetch_abi60 (int arg0) _NCURSES_EXPORT_ABI60(ungetch);

NCURSES_EXPORT(int) _ungetch_abi60 (int arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = ungetch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vidattr_abi60 (chtype arg0) _NCURSES_EXPORT_ABI60(vidattr);

NCURSES_EXPORT(int) _vidattr_abi60 (chtype arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vidattr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vidputs_abi60 (chtype arg0, NCURSES_OUTC arg1) _NCURSES_EXPORT_ABI60(vidputs);

NCURSES_EXPORT(int) _vidputs_abi60 (chtype arg0, NCURSES_OUTC arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vidputs_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vline_abi60 (chtype arg0, int arg1) _NCURSES_EXPORT_ABI60(vline);

NCURSES_EXPORT(int) _vline_abi60 (chtype arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vline_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vwprintw_abi60 (WINDOW * arg0, const char * arg1, va_list arg2) _NCURSES_EXPORT_ABI60(vwprintw);

NCURSES_EXPORT(int) _vwprintw_abi60 (WINDOW * arg0, const char * arg1, va_list arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vwprintw_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vw_printw_abi60 (WINDOW * arg0, const char * arg1, va_list arg2) _NCURSES_EXPORT_ABI60(vw_printw);

NCURSES_EXPORT(int) _vw_printw_abi60 (WINDOW * arg0, const char * arg1, va_list arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vw_printw_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vwscanw_abi60 (WINDOW * arg0, NCURSES_CONST char * arg1, va_list arg2) _NCURSES_EXPORT_ABI60(vwscanw);

NCURSES_EXPORT(int) _vwscanw_abi60 (WINDOW * arg0, NCURSES_CONST char * arg1, va_list arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vwscanw_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vw_scanw_abi60 (WINDOW * arg0, NCURSES_CONST char * arg1, va_list arg2) _NCURSES_EXPORT_ABI60(vw_scanw);

NCURSES_EXPORT(int) _vw_scanw_abi60 (WINDOW * arg0, NCURSES_CONST char * arg1, va_list arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vw_scanw_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _waddch_abi60 (WINDOW * arg0, const chtype arg1) _NCURSES_EXPORT_ABI60(waddch);

NCURSES_EXPORT(int) _waddch_abi60 (WINDOW * arg0, const chtype arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = waddch_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _waddchnstr_abi60 (WINDOW * arg0, const chtype * arg1, int arg2) _NCURSES_EXPORT_ABI60(waddchnstr);

NCURSES_EXPORT(int) _waddchnstr_abi60 (WINDOW * arg0, const chtype * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = waddchnstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _waddchstr_abi60 (WINDOW * arg0, const chtype * arg1) _NCURSES_EXPORT_ABI60(waddchstr);

NCURSES_EXPORT(int) _waddchstr_abi60 (WINDOW * arg0, const chtype * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = waddchstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _waddnstr_abi60 (WINDOW * arg0, const char * arg1, int arg2) _NCURSES_EXPORT_ABI60(waddnstr);

NCURSES_EXPORT(int) _waddnstr_abi60 (WINDOW * arg0, const char * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = waddnstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _waddstr_abi60 (WINDOW * arg0, const char * arg1) _NCURSES_EXPORT_ABI60(waddstr);

NCURSES_EXPORT(int) _waddstr_abi60 (WINDOW * arg0, const char * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = waddstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wattron_abi60 (WINDOW * arg0, int arg1) _NCURSES_EXPORT_ABI60(wattron);

NCURSES_EXPORT(int) _wattron_abi60 (WINDOW * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wattron_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wattroff_abi60 (WINDOW * arg0, int arg1) _NCURSES_EXPORT_ABI60(wattroff);

NCURSES_EXPORT(int) _wattroff_abi60 (WINDOW * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wattroff_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wattrset_abi60 (WINDOW * arg0, int arg1) _NCURSES_EXPORT_ABI60(wattrset);

NCURSES_EXPORT(int) _wattrset_abi60 (WINDOW * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wattrset_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wattr_get_abi60 (WINDOW * arg0, attr_t * arg1, NCURSES_PAIRS_T * arg2, void * arg3) _NCURSES_EXPORT_ABI60(wattr_get);

NCURSES_EXPORT(int) _wattr_get_abi60 (WINDOW * arg0, attr_t * arg1, NCURSES_PAIRS_T * arg2, void * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wattr_get_impl(arg0, arg1, arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wattr_on_abi60 (WINDOW * arg0, attr_t arg1, void * arg2) _NCURSES_EXPORT_ABI60(wattr_on);

NCURSES_EXPORT(int) _wattr_on_abi60 (WINDOW * arg0, attr_t arg1, void * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wattr_on_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wattr_off_abi60 (WINDOW * arg0, attr_t arg1, void * arg2) _NCURSES_EXPORT_ABI60(wattr_off);

NCURSES_EXPORT(int) _wattr_off_abi60 (WINDOW * arg0, attr_t arg1, void * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wattr_off_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wattr_set_abi60 (WINDOW * arg0, attr_t arg1, NCURSES_PAIRS_T arg2, void * arg3) _NCURSES_EXPORT_ABI60(wattr_set);

NCURSES_EXPORT(int) _wattr_set_abi60 (WINDOW * arg0, attr_t arg1, NCURSES_PAIRS_T arg2, void * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wattr_set_impl(arg0, arg1, arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wbkgd_abi60 (WINDOW * arg0, chtype arg1) _NCURSES_EXPORT_ABI60(wbkgd);

NCURSES_EXPORT(int) _wbkgd_abi60 (WINDOW * arg0, chtype arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wbkgd_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(void) _wbkgdset_abi60 (WINDOW * arg0, chtype arg1) _NCURSES_EXPORT_ABI60(wbkgdset);

NCURSES_EXPORT(void) _wbkgdset_abi60 (WINDOW * arg0, chtype arg1)
{
	NCURSES_ABI_PUSH(6, 0);
	wbkgdset_impl(arg0, arg1);
	NCURSES_ABI_POP();
}

NCURSES_EXPORT(int) _wborder_abi60 (WINDOW * arg0, chtype arg1, chtype arg2, chtype arg3, chtype arg4, chtype arg5, chtype arg6, chtype arg7, chtype arg8) _NCURSES_EXPORT_ABI60(wborder);

NCURSES_EXPORT(int) _wborder_abi60 (WINDOW * arg0, chtype arg1, chtype arg2, chtype arg3, chtype arg4, chtype arg5, chtype arg6, chtype arg7, chtype arg8)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wborder_impl(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wchgat_abi60 (WINDOW * arg0, int arg1, attr_t arg2, NCURSES_PAIRS_T arg3, const void * arg4) _NCURSES_EXPORT_ABI60(wchgat);

NCURSES_EXPORT(int) _wchgat_abi60 (WINDOW * arg0, int arg1, attr_t arg2, NCURSES_PAIRS_T arg3, const void * arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wchgat_impl(arg0, arg1, arg2, arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wclear_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(wclear);

NCURSES_EXPORT(int) _wclear_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wclear_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wcolor_set_abi60 (WINDOW* arg0, NCURSES_PAIRS_T arg1, void* arg2) _NCURSES_EXPORT_ABI60(wcolor_set);

NCURSES_EXPORT(int) _wcolor_set_abi60 (WINDOW* arg0, NCURSES_PAIRS_T arg1, void* arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wcolor_set_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wdeleteln_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(wdeleteln);

NCURSES_EXPORT(int) _wdeleteln_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wdeleteln_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wechochar_abi60 (WINDOW * arg0, const chtype arg1) _NCURSES_EXPORT_ABI60(wechochar);

NCURSES_EXPORT(int) _wechochar_abi60 (WINDOW * arg0, const chtype arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wechochar_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wgetch_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(wgetch);

NCURSES_EXPORT(int) _wgetch_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wgetch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wgetnstr_abi60 (WINDOW * arg0, char * arg1, int arg2) _NCURSES_EXPORT_ABI60(wgetnstr);

NCURSES_EXPORT(int) _wgetnstr_abi60 (WINDOW * arg0, char * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wgetnstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wgetstr_abi60 (WINDOW * arg0, char * arg1) _NCURSES_EXPORT_ABI60(wgetstr);

NCURSES_EXPORT(int) _wgetstr_abi60 (WINDOW * arg0, char * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wgetstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _whline_abi60 (WINDOW * arg0, chtype arg1, int arg2) _NCURSES_EXPORT_ABI60(whline);

NCURSES_EXPORT(int) _whline_abi60 (WINDOW * arg0, chtype arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = whline_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(chtype) _winch_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(winch);

NCURSES_EXPORT(chtype) _winch_abi60 (WINDOW * arg0)
{
	chtype ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winchnstr_abi60 (WINDOW * arg0, chtype * arg1, int arg2) _NCURSES_EXPORT_ABI60(winchnstr);

NCURSES_EXPORT(int) _winchnstr_abi60 (WINDOW * arg0, chtype * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winchnstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winchstr_abi60 (WINDOW * arg0, chtype * arg1) _NCURSES_EXPORT_ABI60(winchstr);

NCURSES_EXPORT(int) _winchstr_abi60 (WINDOW * arg0, chtype * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winchstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winnstr_abi60 (WINDOW * arg0, char * arg1, int arg2) _NCURSES_EXPORT_ABI60(winnstr);

NCURSES_EXPORT(int) _winnstr_abi60 (WINDOW * arg0, char * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winnstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winsch_abi60 (WINDOW * arg0, chtype arg1) _NCURSES_EXPORT_ABI60(winsch);

NCURSES_EXPORT(int) _winsch_abi60 (WINDOW * arg0, chtype arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winsch_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winsdelln_abi60 (WINDOW * arg0, int arg1) _NCURSES_EXPORT_ABI60(winsdelln);

NCURSES_EXPORT(int) _winsdelln_abi60 (WINDOW * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winsdelln_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winsertln_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(winsertln);

NCURSES_EXPORT(int) _winsertln_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winsertln_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winsnstr_abi60 (WINDOW * arg0, const char * arg1, int arg2) _NCURSES_EXPORT_ABI60(winsnstr);

NCURSES_EXPORT(int) _winsnstr_abi60 (WINDOW * arg0, const char * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winsnstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winsstr_abi60 (WINDOW * arg0, const char * arg1) _NCURSES_EXPORT_ABI60(winsstr);

NCURSES_EXPORT(int) _winsstr_abi60 (WINDOW * arg0, const char * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winsstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winstr_abi60 (WINDOW * arg0, char * arg1) _NCURSES_EXPORT_ABI60(winstr);

NCURSES_EXPORT(int) _winstr_abi60 (WINDOW * arg0, char * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wnoutrefresh_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(wnoutrefresh);

NCURSES_EXPORT(int) _wnoutrefresh_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wnoutrefresh_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wprintw_abi60 (WINDOW * arg0, const char * arg1, ...) _NCURSES_EXPORT_ABI60(wprintw);

NCURSES_EXPORT(int) _wprintw_abi60 (WINDOW * arg0, const char * arg1, ...)
{
	va_list argp;
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	va_start(argp, arg1);
	ret = vwprintw_impl(arg0, arg1, argp);
	va_end(argp);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wredrawln_abi60 (WINDOW * arg0, int arg1, int arg2) _NCURSES_EXPORT_ABI60(wredrawln);

NCURSES_EXPORT(int) _wredrawln_abi60 (WINDOW * arg0, int arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wredrawln_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wrefresh_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(wrefresh);

NCURSES_EXPORT(int) _wrefresh_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wrefresh_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wscanw_abi60 (WINDOW * arg0, NCURSES_CONST char * arg1, ...) _NCURSES_EXPORT_ABI60(wscanw);

NCURSES_EXPORT(int) _wscanw_abi60 (WINDOW * arg0, NCURSES_CONST char * arg1, ...)
{
	va_list argp;
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	va_start(argp, arg1);
	ret = vwscanw_impl(arg0, arg1, argp);
	va_end(argp);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wscrl_abi60 (WINDOW * arg0, int arg1) _NCURSES_EXPORT_ABI60(wscrl);

NCURSES_EXPORT(int) _wscrl_abi60 (WINDOW * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wscrl_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wstandout_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(wstandout);

NCURSES_EXPORT(int) _wstandout_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wstandout_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wstandend_abi60 (WINDOW * arg0) _NCURSES_EXPORT_ABI60(wstandend);

NCURSES_EXPORT(int) _wstandend_abi60 (WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wstandend_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wvline_abi60 (WINDOW * arg0, chtype arg1, int arg2) _NCURSES_EXPORT_ABI60(wvline);

NCURSES_EXPORT(int) _wvline_abi60 (WINDOW * arg0, chtype arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wvline_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _putp_abi60 (const char * arg0) _NCURSES_EXPORT_ABI60(putp);

NCURSES_EXPORT(int) _putp_abi60 (const char * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = putp_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_term_resized_abi60 (int arg0, int arg1) _NCURSES_EXPORT_ABI60(is_term_resized);

NCURSES_EXPORT(bool) _is_term_resized_abi60 (int arg0, int arg1)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_term_resized_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _assume_default_colors_abi60 (int arg0, int arg1) _NCURSES_EXPORT_ABI60(assume_default_colors);

NCURSES_EXPORT(int) _assume_default_colors_abi60 (int arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = assume_default_colors_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _resize_term_abi60 (int arg0, int arg1) _NCURSES_EXPORT_ABI60(resize_term);

NCURSES_EXPORT(int) _resize_term_abi60 (int arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = resize_term_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _resizeterm_abi60 (int arg0, int arg1) _NCURSES_EXPORT_ABI60(resizeterm);

NCURSES_EXPORT(int) _resizeterm_abi60 (int arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = resizeterm_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _use_default_colors_abi60 (void) _NCURSES_EXPORT_ABI60(use_default_colors);

NCURSES_EXPORT(int) _use_default_colors_abi60 (void)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = use_default_colors_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _use_screen_abi60 (SCREEN * arg0, NCURSES_SCREEN_CB arg1, void * arg2) _NCURSES_EXPORT_ABI60(use_screen);

NCURSES_EXPORT(int) _use_screen_abi60 (SCREEN * arg0, NCURSES_SCREEN_CB arg1, void * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = use_screen_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _use_window_abi60 (WINDOW * arg0, NCURSES_WINDOW_CB arg1, void * arg2) _NCURSES_EXPORT_ABI60(use_window);

NCURSES_EXPORT(int) _use_window_abi60 (WINDOW * arg0, NCURSES_WINDOW_CB arg1, void * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = use_window_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wresize_abi60 (WINDOW * arg0, int arg1, int arg2) _NCURSES_EXPORT_ABI60(wresize);

NCURSES_EXPORT(int) _wresize_abi60 (WINDOW * arg0, int arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wresize_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(WINDOW *) _wgetparent_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(wgetparent);

NCURSES_EXPORT(WINDOW *) _wgetparent_abi60 (const WINDOW * arg0)
{
	WINDOW * ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wgetparent_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_cleared_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_cleared);

NCURSES_EXPORT(bool) _is_cleared_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_cleared_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_idcok_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_idcok);

NCURSES_EXPORT(bool) _is_idcok_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_idcok_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_idlok_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_idlok);

NCURSES_EXPORT(bool) _is_idlok_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_idlok_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_immedok_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_immedok);

NCURSES_EXPORT(bool) _is_immedok_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_immedok_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_keypad_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_keypad);

NCURSES_EXPORT(bool) _is_keypad_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_keypad_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_leaveok_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_leaveok);

NCURSES_EXPORT(bool) _is_leaveok_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_leaveok_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_nodelay_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_nodelay);

NCURSES_EXPORT(bool) _is_nodelay_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_nodelay_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_notimeout_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_notimeout);

NCURSES_EXPORT(bool) _is_notimeout_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_notimeout_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_pad_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_pad);

NCURSES_EXPORT(bool) _is_pad_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_pad_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_scrollok_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_scrollok);

NCURSES_EXPORT(bool) _is_scrollok_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_scrollok_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_subwin_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_subwin);

NCURSES_EXPORT(bool) _is_subwin_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_subwin_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(bool) _is_syncok_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(is_syncok);

NCURSES_EXPORT(bool) _is_syncok_abi60 (const WINDOW * arg0)
{
	bool ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = is_syncok_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wgetdelay_abi60 (const WINDOW * arg0) _NCURSES_EXPORT_ABI60(wgetdelay);

NCURSES_EXPORT(int) _wgetdelay_abi60 (const WINDOW * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wgetdelay_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wgetscrreg_abi60 (const WINDOW * arg0, int * arg1, int * arg2) _NCURSES_EXPORT_ABI60(wgetscrreg);

NCURSES_EXPORT(int) _wgetscrreg_abi60 (const WINDOW * arg0, int * arg1, int * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wgetscrreg_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _add_wch_abi60 (const cchar_t * arg0) _NCURSES_EXPORT_ABI60(add_wch);

NCURSES_EXPORT(int) _add_wch_abi60 (const cchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = add_wch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _add_wchnstr_abi60 (const cchar_t * arg0, int arg1) _NCURSES_EXPORT_ABI60(add_wchnstr);

NCURSES_EXPORT(int) _add_wchnstr_abi60 (const cchar_t * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = add_wchnstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _add_wchstr_abi60 (const cchar_t * arg0) _NCURSES_EXPORT_ABI60(add_wchstr);

NCURSES_EXPORT(int) _add_wchstr_abi60 (const cchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = add_wchstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _addnwstr_abi60 (const wchar_t * arg0, int arg1) _NCURSES_EXPORT_ABI60(addnwstr);

NCURSES_EXPORT(int) _addnwstr_abi60 (const wchar_t * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = addnwstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _addwstr_abi60 (const wchar_t * arg0) _NCURSES_EXPORT_ABI60(addwstr);

NCURSES_EXPORT(int) _addwstr_abi60 (const wchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = addwstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _bkgrnd_abi60 (const cchar_t * arg0) _NCURSES_EXPORT_ABI60(bkgrnd);

NCURSES_EXPORT(int) _bkgrnd_abi60 (const cchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = bkgrnd_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(void) _bkgrndset_abi60 (const cchar_t * arg0) _NCURSES_EXPORT_ABI60(bkgrndset);

NCURSES_EXPORT(void) _bkgrndset_abi60 (const cchar_t * arg0)
{
	NCURSES_ABI_PUSH(6, 0);
	bkgrndset_impl(arg0);
	NCURSES_ABI_POP();
}

NCURSES_EXPORT(int) _border_set_abi60 (const cchar_t* arg0, const cchar_t* arg1, const cchar_t* arg2, const cchar_t* arg3, const cchar_t* arg4, const cchar_t* arg5, const cchar_t* arg6, const cchar_t* arg7) _NCURSES_EXPORT_ABI60(border_set);

NCURSES_EXPORT(int) _border_set_abi60 (const cchar_t* arg0, const cchar_t* arg1, const cchar_t* arg2, const cchar_t* arg3, const cchar_t* arg4, const cchar_t* arg5, const cchar_t* arg6, const cchar_t* arg7)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = border_set_impl(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _box_set_abi60 (WINDOW * arg0, const cchar_t * arg1, const cchar_t * arg2) _NCURSES_EXPORT_ABI60(box_set);

NCURSES_EXPORT(int) _box_set_abi60 (WINDOW * arg0, const cchar_t * arg1, const cchar_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = box_set_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _echo_wchar_abi60 (const cchar_t * arg0) _NCURSES_EXPORT_ABI60(echo_wchar);

NCURSES_EXPORT(int) _echo_wchar_abi60 (const cchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = echo_wchar_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _erasewchar_abi60 (wchar_t* arg0) _NCURSES_EXPORT_ABI60(erasewchar);

NCURSES_EXPORT(int) _erasewchar_abi60 (wchar_t* arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = erasewchar_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _get_wch_abi60 (wint_t * arg0) _NCURSES_EXPORT_ABI60(get_wch);

NCURSES_EXPORT(int) _get_wch_abi60 (wint_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = get_wch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _get_wstr_abi60 (wint_t * arg0) _NCURSES_EXPORT_ABI60(get_wstr);

NCURSES_EXPORT(int) _get_wstr_abi60 (wint_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = get_wstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _getbkgrnd_abi60 (cchar_t * arg0) _NCURSES_EXPORT_ABI60(getbkgrnd);

NCURSES_EXPORT(int) _getbkgrnd_abi60 (cchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = getbkgrnd_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _getcchar_abi60 (const cchar_t * arg0, wchar_t* arg1, attr_t* arg2, NCURSES_PAIRS_T* arg3, void* arg4) _NCURSES_EXPORT_ABI60(getcchar);

NCURSES_EXPORT(int) _getcchar_abi60 (const cchar_t * arg0, wchar_t* arg1, attr_t* arg2, NCURSES_PAIRS_T* arg3, void* arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = getcchar_impl(arg0, arg1, arg2, arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _getn_wstr_abi60 (wint_t * arg0, int arg1) _NCURSES_EXPORT_ABI60(getn_wstr);

NCURSES_EXPORT(int) _getn_wstr_abi60 (wint_t * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = getn_wstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _hline_set_abi60 (const cchar_t * arg0, int arg1) _NCURSES_EXPORT_ABI60(hline_set);

NCURSES_EXPORT(int) _hline_set_abi60 (const cchar_t * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = hline_set_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _in_wch_abi60 (cchar_t * arg0) _NCURSES_EXPORT_ABI60(in_wch);

NCURSES_EXPORT(int) _in_wch_abi60 (cchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = in_wch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _in_wchnstr_abi60 (cchar_t * arg0, int arg1) _NCURSES_EXPORT_ABI60(in_wchnstr);

NCURSES_EXPORT(int) _in_wchnstr_abi60 (cchar_t * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = in_wchnstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _in_wchstr_abi60 (cchar_t * arg0) _NCURSES_EXPORT_ABI60(in_wchstr);

NCURSES_EXPORT(int) _in_wchstr_abi60 (cchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = in_wchstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _innwstr_abi60 (wchar_t * arg0, int arg1) _NCURSES_EXPORT_ABI60(innwstr);

NCURSES_EXPORT(int) _innwstr_abi60 (wchar_t * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = innwstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _ins_nwstr_abi60 (const wchar_t * arg0, int arg1) _NCURSES_EXPORT_ABI60(ins_nwstr);

NCURSES_EXPORT(int) _ins_nwstr_abi60 (const wchar_t * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = ins_nwstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _ins_wch_abi60 (const cchar_t * arg0) _NCURSES_EXPORT_ABI60(ins_wch);

NCURSES_EXPORT(int) _ins_wch_abi60 (const cchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = ins_wch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _ins_wstr_abi60 (const wchar_t * arg0) _NCURSES_EXPORT_ABI60(ins_wstr);

NCURSES_EXPORT(int) _ins_wstr_abi60 (const wchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = ins_wstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _inwstr_abi60 (wchar_t * arg0) _NCURSES_EXPORT_ABI60(inwstr);

NCURSES_EXPORT(int) _inwstr_abi60 (wchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = inwstr_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(NCURSES_CONST char*) _key_name_abi60 (wchar_t arg0) _NCURSES_EXPORT_ABI60(key_name);

NCURSES_EXPORT(NCURSES_CONST char*) _key_name_abi60 (wchar_t arg0)
{
	NCURSES_CONST char* ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = key_name_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _killwchar_abi60 (wchar_t * arg0) _NCURSES_EXPORT_ABI60(killwchar);

NCURSES_EXPORT(int) _killwchar_abi60 (wchar_t * arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = killwchar_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvadd_wch_abi60 (int arg0, int arg1, const cchar_t * arg2) _NCURSES_EXPORT_ABI60(mvadd_wch);

NCURSES_EXPORT(int) _mvadd_wch_abi60 (int arg0, int arg1, const cchar_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = add_wch_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvadd_wchnstr_abi60 (int arg0, int arg1, const cchar_t * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvadd_wchnstr);

NCURSES_EXPORT(int) _mvadd_wchnstr_abi60 (int arg0, int arg1, const cchar_t * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = add_wchnstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvadd_wchstr_abi60 (int arg0, int arg1, const cchar_t * arg2) _NCURSES_EXPORT_ABI60(mvadd_wchstr);

NCURSES_EXPORT(int) _mvadd_wchstr_abi60 (int arg0, int arg1, const cchar_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = add_wchstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvaddnwstr_abi60 (int arg0, int arg1, const wchar_t * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvaddnwstr);

NCURSES_EXPORT(int) _mvaddnwstr_abi60 (int arg0, int arg1, const wchar_t * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addnwstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvaddwstr_abi60 (int arg0, int arg1, const wchar_t * arg2) _NCURSES_EXPORT_ABI60(mvaddwstr);

NCURSES_EXPORT(int) _mvaddwstr_abi60 (int arg0, int arg1, const wchar_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addwstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvget_wch_abi60 (int arg0, int arg1, wint_t * arg2) _NCURSES_EXPORT_ABI60(mvget_wch);

NCURSES_EXPORT(int) _mvget_wch_abi60 (int arg0, int arg1, wint_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = get_wch_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvget_wstr_abi60 (int arg0, int arg1, wint_t * arg2) _NCURSES_EXPORT_ABI60(mvget_wstr);

NCURSES_EXPORT(int) _mvget_wstr_abi60 (int arg0, int arg1, wint_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = get_wstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvgetn_wstr_abi60 (int arg0, int arg1, wint_t * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvgetn_wstr);

NCURSES_EXPORT(int) _mvgetn_wstr_abi60 (int arg0, int arg1, wint_t * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = getn_wstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvhline_set_abi60 (int arg0, int arg1, const cchar_t * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvhline_set);

NCURSES_EXPORT(int) _mvhline_set_abi60 (int arg0, int arg1, const cchar_t * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = hline_set_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvin_wch_abi60 (int arg0, int arg1, cchar_t * arg2) _NCURSES_EXPORT_ABI60(mvin_wch);

NCURSES_EXPORT(int) _mvin_wch_abi60 (int arg0, int arg1, cchar_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = in_wch_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvin_wchnstr_abi60 (int arg0, int arg1, cchar_t * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvin_wchnstr);

NCURSES_EXPORT(int) _mvin_wchnstr_abi60 (int arg0, int arg1, cchar_t * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = in_wchnstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvin_wchstr_abi60 (int arg0, int arg1, cchar_t * arg2) _NCURSES_EXPORT_ABI60(mvin_wchstr);

NCURSES_EXPORT(int) _mvin_wchstr_abi60 (int arg0, int arg1, cchar_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = in_wchstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvinnwstr_abi60 (int arg0, int arg1, wchar_t * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvinnwstr);

NCURSES_EXPORT(int) _mvinnwstr_abi60 (int arg0, int arg1, wchar_t * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = innwstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvins_nwstr_abi60 (int arg0, int arg1, const wchar_t * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvins_nwstr);

NCURSES_EXPORT(int) _mvins_nwstr_abi60 (int arg0, int arg1, const wchar_t * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = ins_nwstr_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvins_wch_abi60 (int arg0, int arg1, const cchar_t * arg2) _NCURSES_EXPORT_ABI60(mvins_wch);

NCURSES_EXPORT(int) _mvins_wch_abi60 (int arg0, int arg1, const cchar_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = ins_wch_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvins_wstr_abi60 (int arg0, int arg1, const wchar_t * arg2) _NCURSES_EXPORT_ABI60(mvins_wstr);

NCURSES_EXPORT(int) _mvins_wstr_abi60 (int arg0, int arg1, const wchar_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = ins_wstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvinwstr_abi60 (int arg0, int arg1, wchar_t * arg2) _NCURSES_EXPORT_ABI60(mvinwstr);

NCURSES_EXPORT(int) _mvinwstr_abi60 (int arg0, int arg1, wchar_t * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = inwstr_impl(arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvvline_set_abi60 (int arg0, int arg1, const cchar_t * arg2, int arg3) _NCURSES_EXPORT_ABI60(mvvline_set);

NCURSES_EXPORT(int) _mvvline_set_abi60 (int arg0, int arg1, const cchar_t * arg2, int arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(stdscr, arg0, arg1)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = vline_set_impl(arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwadd_wch_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3) _NCURSES_EXPORT_ABI60(mvwadd_wch);

NCURSES_EXPORT(int) _mvwadd_wch_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = add_wch_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwadd_wchnstr_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwadd_wchnstr);

NCURSES_EXPORT(int) _mvwadd_wchnstr_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = add_wchnstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwadd_wchstr_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3) _NCURSES_EXPORT_ABI60(mvwadd_wchstr);

NCURSES_EXPORT(int) _mvwadd_wchstr_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = add_wchstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwaddnwstr_abi60 (WINDOW * arg0, int arg1, int arg2, const wchar_t * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwaddnwstr);

NCURSES_EXPORT(int) _mvwaddnwstr_abi60 (WINDOW * arg0, int arg1, int arg2, const wchar_t * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addnwstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwaddwstr_abi60 (WINDOW * arg0, int arg1, int arg2, const wchar_t * arg3) _NCURSES_EXPORT_ABI60(mvwaddwstr);

NCURSES_EXPORT(int) _mvwaddwstr_abi60 (WINDOW * arg0, int arg1, int arg2, const wchar_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = addwstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwget_wch_abi60 (WINDOW * arg0, int arg1, int arg2, wint_t * arg3) _NCURSES_EXPORT_ABI60(mvwget_wch);

NCURSES_EXPORT(int) _mvwget_wch_abi60 (WINDOW * arg0, int arg1, int arg2, wint_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = get_wch_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwget_wstr_abi60 (WINDOW * arg0, int arg1, int arg2, wint_t * arg3) _NCURSES_EXPORT_ABI60(mvwget_wstr);

NCURSES_EXPORT(int) _mvwget_wstr_abi60 (WINDOW * arg0, int arg1, int arg2, wint_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = get_wstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwgetn_wstr_abi60 (WINDOW * arg0, int arg1, int arg2, wint_t * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwgetn_wstr);

NCURSES_EXPORT(int) _mvwgetn_wstr_abi60 (WINDOW * arg0, int arg1, int arg2, wint_t * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = getn_wstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwhline_set_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwhline_set);

NCURSES_EXPORT(int) _mvwhline_set_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = hline_set_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwin_wch_abi60 (WINDOW * arg0, int arg1, int arg2, cchar_t * arg3) _NCURSES_EXPORT_ABI60(mvwin_wch);

NCURSES_EXPORT(int) _mvwin_wch_abi60 (WINDOW * arg0, int arg1, int arg2, cchar_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = in_wch_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwin_wchnstr_abi60 (WINDOW * arg0, int arg1, int arg2, cchar_t * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwin_wchnstr);

NCURSES_EXPORT(int) _mvwin_wchnstr_abi60 (WINDOW * arg0, int arg1, int arg2, cchar_t * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = in_wchnstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwin_wchstr_abi60 (WINDOW * arg0, int arg1, int arg2, cchar_t * arg3) _NCURSES_EXPORT_ABI60(mvwin_wchstr);

NCURSES_EXPORT(int) _mvwin_wchstr_abi60 (WINDOW * arg0, int arg1, int arg2, cchar_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = in_wchstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwinnwstr_abi60 (WINDOW * arg0, int arg1, int arg2, wchar_t * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwinnwstr);

NCURSES_EXPORT(int) _mvwinnwstr_abi60 (WINDOW * arg0, int arg1, int arg2, wchar_t * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = innwstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwins_nwstr_abi60 (WINDOW * arg0, int arg1, int arg2, const wchar_t * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwins_nwstr);

NCURSES_EXPORT(int) _mvwins_nwstr_abi60 (WINDOW * arg0, int arg1, int arg2, const wchar_t * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = ins_nwstr_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwins_wch_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3) _NCURSES_EXPORT_ABI60(mvwins_wch);

NCURSES_EXPORT(int) _mvwins_wch_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = ins_wch_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwins_wstr_abi60 (WINDOW * arg0, int arg1, int arg2, const wchar_t * arg3) _NCURSES_EXPORT_ABI60(mvwins_wstr);

NCURSES_EXPORT(int) _mvwins_wstr_abi60 (WINDOW * arg0, int arg1, int arg2, const wchar_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = ins_wstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwinwstr_abi60 (WINDOW * arg0, int arg1, int arg2, wchar_t * arg3) _NCURSES_EXPORT_ABI60(mvwinwstr);

NCURSES_EXPORT(int) _mvwinwstr_abi60 (WINDOW * arg0, int arg1, int arg2, wchar_t * arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = inwstr_impl(arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _mvwvline_set_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3, int arg4) _NCURSES_EXPORT_ABI60(mvwvline_set);

NCURSES_EXPORT(int) _mvwvline_set_abi60 (WINDOW * arg0, int arg1, int arg2, const cchar_t * arg3, int arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	if ((ret = wmove(arg0, arg1, arg2)) == ERR) {
		NCURSES_ABI_POP();
		return (ret);
	}
	ret = vline_set_impl(arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _pecho_wchar_abi60 (WINDOW * arg0, const cchar_t * arg1) _NCURSES_EXPORT_ABI60(pecho_wchar);

NCURSES_EXPORT(int) _pecho_wchar_abi60 (WINDOW * arg0, const cchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = pecho_wchar_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _setcchar_abi60 (cchar_t * arg0, const wchar_t * arg1, const attr_t arg2, NCURSES_PAIRS_T arg3, const void * arg4) _NCURSES_EXPORT_ABI60(setcchar);

NCURSES_EXPORT(int) _setcchar_abi60 (cchar_t * arg0, const wchar_t * arg1, const attr_t arg2, NCURSES_PAIRS_T arg3, const void * arg4)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = setcchar_impl(arg0, arg1, arg2, arg3, arg4);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _slk_wset_abi60 (int arg0, const wchar_t * arg1, int arg2) _NCURSES_EXPORT_ABI60(slk_wset);

NCURSES_EXPORT(int) _slk_wset_abi60 (int arg0, const wchar_t * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = slk_wset_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(attr_t) _term_attrs_abi60 (void) _NCURSES_EXPORT_ABI60(term_attrs);

NCURSES_EXPORT(attr_t) _term_attrs_abi60 (void)
{
	attr_t ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = term_attrs_impl();
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _unget_wch_abi60 (const wchar_t arg0) _NCURSES_EXPORT_ABI60(unget_wch);

NCURSES_EXPORT(int) _unget_wch_abi60 (const wchar_t arg0)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = unget_wch_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vid_attr_abi60 (attr_t arg0, NCURSES_PAIRS_T arg1, void * arg2) _NCURSES_EXPORT_ABI60(vid_attr);

NCURSES_EXPORT(int) _vid_attr_abi60 (attr_t arg0, NCURSES_PAIRS_T arg1, void * arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vid_attr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vid_puts_abi60 (attr_t arg0, NCURSES_PAIRS_T arg1, void * arg2, NCURSES_OUTC arg3) _NCURSES_EXPORT_ABI60(vid_puts);

NCURSES_EXPORT(int) _vid_puts_abi60 (attr_t arg0, NCURSES_PAIRS_T arg1, void * arg2, NCURSES_OUTC arg3)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vid_puts_impl(arg0, arg1, arg2, arg3);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _vline_set_abi60 (const cchar_t * arg0, int arg1) _NCURSES_EXPORT_ABI60(vline_set);

NCURSES_EXPORT(int) _vline_set_abi60 (const cchar_t * arg0, int arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = vline_set_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wadd_wch_abi60 (WINDOW * arg0, const cchar_t * arg1) _NCURSES_EXPORT_ABI60(wadd_wch);

NCURSES_EXPORT(int) _wadd_wch_abi60 (WINDOW * arg0, const cchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wadd_wch_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wadd_wchnstr_abi60 (WINDOW * arg0, const cchar_t * arg1, int arg2) _NCURSES_EXPORT_ABI60(wadd_wchnstr);

NCURSES_EXPORT(int) _wadd_wchnstr_abi60 (WINDOW * arg0, const cchar_t * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wadd_wchnstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wadd_wchstr_abi60 (WINDOW * arg0, const cchar_t * arg1) _NCURSES_EXPORT_ABI60(wadd_wchstr);

NCURSES_EXPORT(int) _wadd_wchstr_abi60 (WINDOW * arg0, const cchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wadd_wchstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _waddnwstr_abi60 (WINDOW * arg0, const wchar_t * arg1, int arg2) _NCURSES_EXPORT_ABI60(waddnwstr);

NCURSES_EXPORT(int) _waddnwstr_abi60 (WINDOW * arg0, const wchar_t * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = waddnwstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _waddwstr_abi60 (WINDOW * arg0, const wchar_t * arg1) _NCURSES_EXPORT_ABI60(waddwstr);

NCURSES_EXPORT(int) _waddwstr_abi60 (WINDOW * arg0, const wchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = waddwstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wbkgrnd_abi60 (WINDOW * arg0, const cchar_t * arg1) _NCURSES_EXPORT_ABI60(wbkgrnd);

NCURSES_EXPORT(int) _wbkgrnd_abi60 (WINDOW * arg0, const cchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wbkgrnd_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(void) _wbkgrndset_abi60 (WINDOW * arg0, const cchar_t * arg1) _NCURSES_EXPORT_ABI60(wbkgrndset);

NCURSES_EXPORT(void) _wbkgrndset_abi60 (WINDOW * arg0, const cchar_t * arg1)
{
	NCURSES_ABI_PUSH(6, 0);
	wbkgrndset_impl(arg0, arg1);
	NCURSES_ABI_POP();
}

NCURSES_EXPORT(int) _wborder_set_abi60 (WINDOW * arg0, const cchar_t* arg1, const cchar_t* arg2, const cchar_t* arg3, const cchar_t* arg4, const cchar_t* arg5, const cchar_t* arg6, const cchar_t* arg7, const cchar_t* arg8) _NCURSES_EXPORT_ABI60(wborder_set);

NCURSES_EXPORT(int) _wborder_set_abi60 (WINDOW * arg0, const cchar_t* arg1, const cchar_t* arg2, const cchar_t* arg3, const cchar_t* arg4, const cchar_t* arg5, const cchar_t* arg6, const cchar_t* arg7, const cchar_t* arg8)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wborder_set_impl(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wecho_wchar_abi60 (WINDOW * arg0, const cchar_t * arg1) _NCURSES_EXPORT_ABI60(wecho_wchar);

NCURSES_EXPORT(int) _wecho_wchar_abi60 (WINDOW * arg0, const cchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wecho_wchar_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wget_wch_abi60 (WINDOW * arg0, wint_t * arg1) _NCURSES_EXPORT_ABI60(wget_wch);

NCURSES_EXPORT(int) _wget_wch_abi60 (WINDOW * arg0, wint_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wget_wch_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wget_wstr_abi60 (WINDOW * arg0, wint_t * arg1) _NCURSES_EXPORT_ABI60(wget_wstr);

NCURSES_EXPORT(int) _wget_wstr_abi60 (WINDOW * arg0, wint_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wget_wstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wgetbkgrnd_abi60 (WINDOW * arg0, cchar_t * arg1) _NCURSES_EXPORT_ABI60(wgetbkgrnd);

NCURSES_EXPORT(int) _wgetbkgrnd_abi60 (WINDOW * arg0, cchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wgetbkgrnd_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wgetn_wstr_abi60 (WINDOW * arg0, wint_t * arg1, int arg2) _NCURSES_EXPORT_ABI60(wgetn_wstr);

NCURSES_EXPORT(int) _wgetn_wstr_abi60 (WINDOW * arg0, wint_t * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wgetn_wstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _whline_set_abi60 (WINDOW * arg0, const cchar_t * arg1, int arg2) _NCURSES_EXPORT_ABI60(whline_set);

NCURSES_EXPORT(int) _whline_set_abi60 (WINDOW * arg0, const cchar_t * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = whline_set_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _win_wch_abi60 (WINDOW * arg0, cchar_t * arg1) _NCURSES_EXPORT_ABI60(win_wch);

NCURSES_EXPORT(int) _win_wch_abi60 (WINDOW * arg0, cchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = win_wch_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _win_wchnstr_abi60 (WINDOW * arg0, cchar_t * arg1, int arg2) _NCURSES_EXPORT_ABI60(win_wchnstr);

NCURSES_EXPORT(int) _win_wchnstr_abi60 (WINDOW * arg0, cchar_t * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = win_wchnstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _win_wchstr_abi60 (WINDOW * arg0, cchar_t * arg1) _NCURSES_EXPORT_ABI60(win_wchstr);

NCURSES_EXPORT(int) _win_wchstr_abi60 (WINDOW * arg0, cchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = win_wchstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winnwstr_abi60 (WINDOW * arg0, wchar_t * arg1, int arg2) _NCURSES_EXPORT_ABI60(winnwstr);

NCURSES_EXPORT(int) _winnwstr_abi60 (WINDOW * arg0, wchar_t * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winnwstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wins_nwstr_abi60 (WINDOW * arg0, const wchar_t * arg1, int arg2) _NCURSES_EXPORT_ABI60(wins_nwstr);

NCURSES_EXPORT(int) _wins_nwstr_abi60 (WINDOW * arg0, const wchar_t * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wins_nwstr_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wins_wch_abi60 (WINDOW * arg0, const cchar_t * arg1) _NCURSES_EXPORT_ABI60(wins_wch);

NCURSES_EXPORT(int) _wins_wch_abi60 (WINDOW * arg0, const cchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wins_wch_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wins_wstr_abi60 (WINDOW * arg0, const wchar_t * arg1) _NCURSES_EXPORT_ABI60(wins_wstr);

NCURSES_EXPORT(int) _wins_wstr_abi60 (WINDOW * arg0, const wchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wins_wstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _winwstr_abi60 (WINDOW * arg0, wchar_t * arg1) _NCURSES_EXPORT_ABI60(winwstr);

NCURSES_EXPORT(int) _winwstr_abi60 (WINDOW * arg0, wchar_t * arg1)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = winwstr_impl(arg0, arg1);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(wchar_t*) _wunctrl_abi60 (cchar_t * arg0) _NCURSES_EXPORT_ABI60(wunctrl);

NCURSES_EXPORT(wchar_t*) _wunctrl_abi60 (cchar_t * arg0)
{
	wchar_t* ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wunctrl_impl(arg0);
	NCURSES_ABI_POP();

	return (ret);
}

NCURSES_EXPORT(int) _wvline_set_abi60 (WINDOW * arg0, const cchar_t * arg1, int arg2) _NCURSES_EXPORT_ABI60(wvline_set);

NCURSES_EXPORT(int) _wvline_set_abi60 (WINDOW * arg0, const cchar_t * arg1, int arg2)
{
	int ret;

	NCURSES_ABI_PUSH(6, 0);
	ret = wvline_set_impl(arg0, arg1, arg2);
	NCURSES_ABI_POP();

	return (ret);
}
