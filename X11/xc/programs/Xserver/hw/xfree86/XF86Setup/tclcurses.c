/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tclcurses.c,v 1.2 1999/04/05 08:28:23 dawes Exp $ */
/*
 * Copyright 1997 by Joseph V. Moss <joe@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Joseph Moss not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Joseph Moss makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * JOSEPH MOSS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL JOSEPH MOSS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


/*
  This file contains Tcl bindings to the curses library
 */

#include <tcl.h>
#ifdef CURSES
#ifdef NCURSES
#include <ncurses.h>
#else
#include <curses.h>
#endif
#include <string.h>
#include "tclcurses.h"
#endif /* CURSES */

static int curs_debug = 0;

/*
   Adds the curses command to the Tcl interpreter
*/

int
Curses_Init(interp)
    Tcl_Interp	*interp;
{
	Tcl_CreateCommand(interp, "curses", TCL_Curses, NULL,
		(void (*)()) NULL);

	return TCL_OK;
}

#ifndef CURSES

/* Stub version */
int
TCL_Curses(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tcl_AppendResult(interp,
		"Curses not compiled in to this version of XF86SETUP", NULL);
	return TCL_ERROR;
}

#else

/*
*/

int
TCL_Curses(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	struct windata *winPtr;
	WINDOW *tmpwin;

        if (argc < 2) {
                Tcl_SetResult(interp, "Usage: curses ", TCL_STATIC);
                return TCL_ERROR;
        }

	if (!strcmp(argv[1], "init")) {
		if (stdscr) {
			Tcl_AppendResult(interp, "Already initialized", NULL);
			return TCL_ERROR;
		}
		if (initscr() == NULL) {
			Tcl_AppendResult(interp, "Unable to initialize curses", NULL);
			return TCL_ERROR;
		}
		cbreak();
		noecho();
		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);
		if (has_colors()) {
			start_color();
		}
		init_pair(PAIR_SCREEN);
		init_pair(PAIR_ULBORD);
		init_pair(PAIR_LRBORD);
		init_pair(PAIR_TEXT);
		init_pair(PAIR_TITLE);
		init_pair(PAIR_SELECT);
		winPtr = initwinPtr(stdscr);
		bkgd(ATTR_SCREEN);
		attrset(ATTR_SCREEN);
		Tcl_CreateCommand(interp,"stdscr",TCL_WinProc,winPtr,NULL);
	} else if (argc == 3 && !strcmp(argv[1], "debug")) {
		curs_debug = atoi(argv[2]);
	} else if (!strcmp(argv[1], "end")) {
		if (endwin() != OK) {
			Tcl_AppendResult(interp, "curses end failed", NULL);
			return TCL_ERROR;
		}
	} else if (!strcmp(argv[1], "menubar")) {
		/* TBD */
		Tcl_AppendResult(interp, "Not yet implemented", NULL);
		return TCL_OK;
	} else if (!strcmp(argv[1], "newwin")) {
		if (argc != 7) {
			Tcl_AppendResult(interp,
				"Usage: curses newwin <winname> <nlines>"
				" <ncols> <begin_y> <begin_x>",
				NULL);
			return TCL_ERROR;
		}
		if ((tmpwin = newwin(atoi(argv[3]),atoi(argv[4]),
					atoi(argv[5]),atoi(argv[6]))) == NULL) {
			Tcl_AppendResult(interp, "Unable to create window", NULL);
			ckfree(tmpwin);
			return TCL_ERROR;
		}
		keypad(tmpwin, TRUE);
		winPtr = initwinPtr(tmpwin);
		wbkgd(winPtr->win,ATTR_TEXT);
		Tcl_CreateCommand(interp,argv[2],TCL_WinProc,winPtr,NULL);
	} else if (!strcmp(argv[1], "delwin")) {
		if (argc != 3) {
			Tcl_AppendResult(interp,
				"Usage: curses delwin <winname>", NULL);
			return TCL_ERROR;
		}
		if (Tcl_DeleteCommand(interp, argv[2]) == TCL_ERROR)
			return TCL_ERROR;
	} else {
		Tcl_AppendResult(interp, "Invalid option: ",
			argv[2], NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}

void TCL_WinDelete(clientData)
    ClientData	clientData;
{
	struct windata *winPtr;
	int i;

	winPtr = (struct windata *) clientData;
	delwin(winPtr->win);
	ckfree(winPtr->title);
	ckfree(winPtr->menu.chkd);
	ckfree(winPtr->menu.win);
	ckfree(winPtr->menu.data);
	ckfree(winPtr->text.buffer);
	ckfree(winPtr->text.data);
	for (i = 0; i < MAXBUTTONS; i++)
		ckfree(winPtr->button[i]);
	ckfree(winPtr);
}

/*
*/

int
TCL_WinProc(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	struct windata *winPtr = (struct windata *) clientData;
	char tmpbuf[64];

	if (argc == 4 && !strcmp(argv[1], "move")) {
		wmove(winPtr->win,atoi(argv[2]),atoi(argv[3]));
	} else if (argc == 3 && !strcmp(argv[1], "addstr")) {
		waddstr(winPtr->win,argv[2]);
	} else if (argc == 3 && !strcmp(argv[1], "text")) {
		return settext(winPtr, interp, argv[2]);
	} else if (argc == 2 && !strcmp(argv[1], "activate")) {
		return processinput(winPtr, interp);
	} else if (argc == 2 && !strcmp(argv[1], "clear")) {
		wclear(winPtr->win);
	} else if (argc == 2 && !strcmp(argv[1], "update")) {
		wrefresh(winPtr->win);
	} else if (argc == 2 && !strcmp(argv[1], "info")) {
		sprintf(tmpbuf, "%d %d %d %d", winPtr->width, winPtr->height,
			winPtr->border, winPtr->shadow);
		Tcl_AppendResult(interp, tmpbuf, NULL);
		Tcl_AppendElement(interp, winPtr->title);
	} else if (argc > 2 && !strcmp(argv[1], "menu")) {
		winPtr->menu.type=TYPE_MENU;
		return setmenu(winPtr, interp, argc, argv);
	} else if (argc > 2 && !strcmp(argv[1], "checklist")) {
		winPtr->menu.type=TYPE_CHECK;
		return setmenu(winPtr, interp, argc, argv);
	} else if (argc > 2 && !strcmp(argv[1], "radiolist")) {
		winPtr->menu.type=TYPE_RADIO;
		return setmenu(winPtr, interp, argc, argv);
	} else if (argc > 2 && !strcmp(argv[1], "buttons")) {
		return setbuttons(winPtr, interp, argc, argv);
	} else if (argc > 2 && !strcmp(argv[1], "configure")) {
		return winconfig(winPtr, interp, argc, argv);
	} else {
		Tcl_AppendResult(interp, "Invalid option: ",
			argv[1], NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}

static int
settext(winPtr, interp, text)
	struct windata	*winPtr;
	Tcl_Interp	*interp;
	char		*text;
{
	int i, numcols;
	char savechar, *ptr, *nlptr, *spcptr, *endchar;

	DEBUG4("settext(%p, %p, %s)\n", winPtr, interp, text);

	numcols = winPtr->width - (winPtr->border==BORDER_NONE? 0: 2);
	if (winPtr->text.data) {
		ckfree(winPtr->text.data);
		ckfree(winPtr->text.buffer);
	}
	winPtr->text.items = winPtr->text.topline = 0;
	if (strlen(text)) {
		winPtr->text.buffer = strdup(text);
		ptr = nlptr = winPtr->text.buffer;
		endchar = winPtr->text.buffer + strlen(text);
		while (nlptr < endchar) {
			if ((nlptr = strchr(ptr, '\n')) == NULL)
				nlptr = endchar;
			if (nlptr - ptr > numcols) {
				*nlptr = '\0';
				while (strlen(ptr) > numcols) {
					savechar = ptr[numcols];
					ptr[numcols] = '\0';
					spcptr = strrchr(ptr, ' ');
					ptr[numcols] = savechar;
					if (spcptr == NULL) {
						ptr[numcols-1] = '\n';
						ptr += numcols - 1;
					} else {
						*spcptr = '\n';
						ptr = spcptr + 1;
					}
				}
				if (nlptr != endchar)
					*nlptr = '\n';
			}
			ptr = nlptr + 1;
		}
	} else {
		winPtr->text.data = (char **) 0;
		winPtr->text.buffer = endchar = NULL;
	}

	for (ptr= nlptr= winPtr->text.buffer; ptr<endchar && nlptr; ptr= nlptr+1) {
		nlptr = strchr(ptr, '\n');
		winPtr->text.items++;
	}
	winPtr->text.data = (char **) ckalloc(winPtr->text.items * sizeof(char *));

	for (i=0, ptr= nlptr= winPtr->text.buffer; ptr<endchar && nlptr; ptr= nlptr+1) {
		if ((nlptr = strchr(ptr, '\n')) != NULL)
			*nlptr = '\0';
		winPtr->text.data[i++] = ptr;
	}
	DEBUG3("(text='%s' items=%d)\n", winPtr->text.buffer, winPtr->text.items);
	
	updatetext(winPtr, interp);
	DEBUG1("settext() returning okay\n");
	return TCL_OK;
}

static int
updatetext(winPtr, interp)
	struct windata	*winPtr;
	Tcl_Interp	*interp;
{
	int i, j, row, col, numrows, numcols;

	DEBUG3("updatetext(%p, %p)\n", winPtr, interp);
	wattrset(winPtr->win, ATTR_TEXT);
	if (winPtr->border == BORDER_NONE) {
		row = col = 0;
		numcols = winPtr->width;
	} else {
		row = col = 1;
		numcols = winPtr->width-2;
	}
	numrows = textlines(winPtr);

	for (i = row; i < row+numrows; i++) {
		wmove(winPtr->win, i, col);
		for (j = col; j < col+numcols; j++)
			waddch(winPtr->win, ATTR_TEXT | ' ');
	}
	if (!winPtr->text.items)
		return TCL_OK;

	for (i= 0, j= winPtr->text.topline; i<numrows && j<winPtr->text.items; i++, j++) {
		wmove(winPtr->win, i+row, col);
		waddstr(winPtr->win, winPtr->text.data[j]);
	}
	DEBUG1("updatetext() returning okay\n");
	return TCL_OK;
}

static int
textlines(winPtr)
	struct windata	*winPtr;
{
	int rows;

	if (winPtr->border == BORDER_NONE) {
		rows = winPtr->height;
		if (winPtr->button[0]) rows--;
	} else {
		rows = winPtr->height-2;
		if (winPtr->button[0]) rows-=2;
	}
	if (winPtr->menu.items) rows -= winPtr->menu.items+2;

	return rows;
}

static int
processinput(winPtr, interp)
	struct windata	*winPtr;
	Tcl_Interp	*interp;
{
	int key=0, i, lastbutton;

#define mybeep()	beep(); wrefresh(winPtr->win)

	for (lastbutton=0; lastbutton<MAXBUTTONS; lastbutton++) {
		if (!winPtr->button[lastbutton])
			break;
	}
	while (1) {
	    switch (key = wgetch(winPtr->win)) {
		case KEY_RIGHT:
			if (!winPtr->button[0] || winPtr->butsel == lastbutton-1) {
				mybeep();
			} else {
				winPtr->butsel++;
				updatebuttons(winPtr);
			}
		when KEY_LEFT:
			if (!winPtr->button[0] || winPtr->butsel == 0) {
				mybeep();
			} else {
				winPtr->butsel--;
				updatebuttons(winPtr);
			}
		when KEY_UP:
		case KEY_SR:
			if (updatepos(winPtr, interp, KEY_UP, SCROLL_LINE) == TCL_ERROR)
				return TCL_ERROR;
		when KEY_DOWN:
		case KEY_SF:
			if (updatepos(winPtr, interp, KEY_DOWN, SCROLL_LINE) == TCL_ERROR)
				return TCL_ERROR;
		when KEY_HOME:
		case KEY_BEG:
			if (updatepos(winPtr, interp, KEY_UP, SCROLL_ALL) == TCL_ERROR)
				return TCL_ERROR;
		when KEY_LL:
		case KEY_END:
			if (updatepos(winPtr, interp, KEY_DOWN, SCROLL_ALL) == TCL_ERROR)
				return TCL_ERROR;
		when KEY_PPAGE:
			if (updatepos(winPtr, interp, KEY_UP, SCROLL_PAGE) == TCL_ERROR)
				return TCL_ERROR;
		when KEY_NPAGE:
			if (updatepos(winPtr, interp, KEY_DOWN, SCROLL_PAGE) == TCL_ERROR)
				return TCL_ERROR;
		when KEY_ENTER:
		case '\r':
		case '\n':
			Tcl_AppendElement(interp, winPtr->button[0]?
					winPtr->button[winPtr->butsel]: "");
			if (winPtr->menu.items) {
				if (winPtr->menu.type == TYPE_RADIO) {
					Tcl_AppendElement(interp,
						winPtr->menu.data[winPtr->menu.rsel]);
				} else if (winPtr->menu.type == TYPE_CHECK) {
					for (i = 0; i < winPtr->menu.items; i++) {
						if (winPtr->menu.chkd[i])
							Tcl_AppendElement(interp,
								winPtr->menu.data[i]);
					}
				} else
					Tcl_AppendElement(interp,
						winPtr->menu.data[winPtr->menu.current]);
			}
			return TCL_OK;
		when KEY_SELECT:
		case ' ':
			if (winPtr->menu.items == 0
					|| winPtr->menu.type == TYPE_NONE
					|| winPtr->menu.type == TYPE_MENU) {
				mybeep();
			} else {
				if (winPtr->menu.type == TYPE_RADIO)
					winPtr->menu.rsel = winPtr->menu.current;
				else
					winPtr->menu.chkd[winPtr->menu.current] =
						(winPtr->menu.chkd[winPtr->menu.current]+1)%2;
				if (updatemenu(winPtr) == TCL_ERROR)
					return TCL_ERROR;
				wrefresh(winPtr->menu.win);
			}
		when KEY_REFRESH:
		case 0x0c:  /* ^L */
			wrefresh(curscr);
		when KEY_ESC:
			Tcl_AppendResult(interp, "Escape", NULL);
			return TCL_OK;
			/* break */
		default:
			mybeep();
	    }
	}
}

static int
updatepos(winPtr, interp, dir, amt)
	struct windata	*winPtr;
	Tcl_Interp	*interp;
	int		dir, amt;
{
	int scroll;

	if ((winPtr->menu.type == TYPE_NONE && winPtr->text.data == NULL)
	 || (winPtr->menu.type != TYPE_NONE && winPtr->menu.lines == 0)   ) {
		mybeep();
		return TCL_OK;
	}
	switch (amt) {
		case SCROLL_LINE:
					scroll = 1;
		when SCROLL_PAGE:
					if (winPtr->menu.type == TYPE_NONE) {
						scroll = textlines(winPtr);
					} else {
						scroll = winPtr->menu.lines;
					}
		when SCROLL_ALL:
					scroll = 0;
					break;
		default:
					Tcl_AppendResult(interp,
					    "internal error: updatepos() called with invalid scroll amount (%d)",
					    amt, NULL);
					return TCL_ERROR;
	}
	if (winPtr->menu.type == TYPE_NONE) {
	    int rows = textlines(winPtr);
	    switch (dir) {
		case KEY_UP:
			if (winPtr->text.topline == 0) {
				mybeep();
				return TCL_OK;
			}
			winPtr->text.topline -= scroll;
			if (!scroll || winPtr->text.topline < 0)
				winPtr->text.topline = 0;
			if (updatetext(winPtr, interp) == TCL_ERROR)
				return TCL_ERROR;
			wrefresh(winPtr->win);
		when KEY_DOWN:
			if (winPtr->text.topline >= winPtr->text.items-rows) {
				mybeep();
				return TCL_OK;
			}
			winPtr->text.topline += scroll;
			if (!scroll || winPtr->text.topline > winPtr->text.items-rows) {
				winPtr->text.topline = winPtr->text.items-rows;
				if (winPtr->text.topline < 0)
					winPtr->text.topline = 0;
			}
			if (updatetext(winPtr, interp) == TCL_ERROR)
				return TCL_ERROR;
			wrefresh(winPtr->win);
			break;
		default:
			Tcl_AppendResult(interp,
				"internal error: updatepos() called with invalid dir (%d)",
				dir, NULL);
			return TCL_ERROR;
	    }
	} else {
	    switch (dir) {
		case KEY_UP:
			if (winPtr->menu.current == 0) {
				mybeep();
				return TCL_OK;
			}
			winPtr->menu.current -= scroll;
			if (!scroll || winPtr->menu.current < 0)
				winPtr->menu.current = 0;
			if (scroll != 1) {
				winPtr->menu.topline -= scroll;
				if (!scroll || winPtr->menu.topline < 0)
					winPtr->menu.topline = 0;
			}
			if (updatemenu(winPtr, interp) == TCL_ERROR)
				return TCL_ERROR;
			wrefresh(winPtr->menu.win);
		when KEY_DOWN:
			if (winPtr->menu.current == winPtr->menu.items-1) {
				mybeep();
				return TCL_OK;
			}
			winPtr->menu.current += scroll;
			if (!scroll || winPtr->menu.current > winPtr->menu.items-1)
				winPtr->menu.current = winPtr->menu.items-1;
			if (scroll != 1) {
				winPtr->menu.topline += scroll;
				if (!scroll || winPtr->menu.topline > winPtr->menu.items-winPtr->menu.lines)
					winPtr->menu.topline = winPtr->menu.items-winPtr->menu.lines;
			}
			if (updatemenu(winPtr, interp) == TCL_ERROR)
				return TCL_ERROR;
			wrefresh(winPtr->menu.win);
			break;
		default:
			Tcl_AppendResult(interp,
				"internal error: updatepos() called with invalid dir (%d)",
				dir, NULL);
			return TCL_ERROR;
	    }
	}
	return TCL_OK;
}

static struct windata *
initwinPtr(win)
	WINDOW	*win;
{
	struct windata *winPtr;
	int i;

	if ((winPtr= (struct windata *) ckalloc(sizeof(struct windata))) == NULL)
		return NULL;
	winPtr->win = win;
	getmaxyx(win, winPtr->height, winPtr->width);
	winPtr->title = (char *) 0;
	winPtr->border = BORDER_NONE;
	winPtr->shadow = 0;

	winPtr->menu.type = TYPE_NONE;
	winPtr->menu.lines = 0;
	winPtr->menu.items = 0;
	winPtr->menu.topline = 0;
	winPtr->menu.current = 0;
	winPtr->menu.rsel = 0;
	winPtr->menu.chkd = (int *) 0;
	winPtr->menu.win = (WINDOW *) 0;
	winPtr->menu.data = (char **) 0;
	winPtr->text.items = 0;
	winPtr->text.topline = 0;
	winPtr->text.buffer = (char *) 0;
	winPtr->text.data = (char **) 0;
	winPtr->butsel = 0;
	for (i = 0; i < MAXBUTTONS; i++)
		winPtr->button[i] = (char *) 0;
	return winPtr;
}

static int
winconfig(winPtr, interp, argc, argv)
	struct windata	*winPtr;
	Tcl_Interp	*interp;
	int		argc;
	char		*argv[];
{
	int i;

	DEBUG5("winconfig(%p, %p, %d, %p)\n", winPtr, interp, argc, argv);
	for (i = 2; i < argc-1; i += 2) {
		if (!strcmp(argv[i],"-shadow")) {
			/* TBD */
			Tcl_AppendResult(interp, "Not yet implemented", NULL);
			return TCL_OK;
		} else if (!strcmp(argv[i],"-title")) {
			if (winPtr->border == BORDER_NONE) {
				Tcl_AppendResult(interp,
					"Only bordered windows can have titles",
					NULL);
				return TCL_ERROR;
			}
			if (strlen(argv[i+1]) > winPtr->width-2) {
				Tcl_AppendResult(interp,
					"Title too long", NULL);
				return TCL_ERROR;
			}
			if (winPtr->title) {
				ckfree(winPtr->title);
				drawbox(winPtr, 0, 0, winPtr->height,
					winPtr->width, winPtr->border-1,
					ATTR_ULBORD, ATTR_LRBORD);
			}
			wmove(winPtr->win, 0, (winPtr->width-strlen(argv[i+1]))/2);
			wattrset(winPtr->win, ATTR_TITLE);
			waddstr(winPtr->win, argv[i+1]);
		} else if (!strcmp(argv[i],"-border")) {
			if (!strcmp(argv[i+1], "none")) {
				winPtr->border=BORDER_NONE;
			} else if (!strcmp(argv[i+1], "blank")) {
				winPtr->border=BORDER_BLANK;
				drawbox(winPtr, 0, 0, winPtr->height,
					winPtr->width, 0, ATTR_ULBORD, ATTR_LRBORD);
			} else if (!strcmp(argv[i+1], "line")) {
				winPtr->border=BORDER_LINE;
				drawbox(winPtr, 0, 0, winPtr->height,
					winPtr->width, 1, ATTR_ULBORD, ATTR_LRBORD);
			} else {
				Tcl_AppendResult(interp, "Invalid border type: ",
					argv[i+1], NULL);
				return TCL_ERROR;
			}
		} else {
			Tcl_AppendResult(interp, "Invalid option: ",
				argv[i], NULL);
			return TCL_ERROR;
		}
	}
	DEBUG1("winconfig() returning okay\n");
	return TCL_OK;
}

static int
setmenu(winPtr, interp, argc, argv)
	struct windata	*winPtr;
	Tcl_Interp	*interp;
	int		argc;
	char		*argv[];
{
	int i, subx, suby, subwid, subht, winy, winx;
	static char *defaultbuttons[] = {"menu", "buttons", "Okay", "Cancel"};

	DEBUG5("setmenu(%p, %p, %d, %p)\n", winPtr, interp, argc, argv);
	if (winPtr->menu.data)
		ckfree(winPtr->menu.data);
	if (winPtr->menu.win)
		delwin(winPtr->menu.win);
	if (!(winPtr->menu.items = argc-3)) {
		winPtr->menu.lines = 0;
		winPtr->menu.topline = 0;
		winPtr->menu.current = 0;
		return TCL_OK;
	}
	subht = winPtr->menu.lines = atoi(argv[2]);
	i = winPtr->height - 3 - (winPtr->border? 3: 0);
	if (subht > i) {
		Tcl_AppendResult(interp, "Not enough room for menu window",
			NULL);
		return TCL_ERROR;
	}
	winPtr->menu.data = (char **) ckalloc(sizeof(char *) * (argc-2));
	for (i = 0; i<argc-3; i++) {
		winPtr->menu.data[i] = strdup(argv[i+3]);
	}
	if (winPtr->menu.type == TYPE_CHECK) {
		winPtr->menu.chkd = (int *) ckalloc(sizeof(int) * (argc-3));
		for (i = 0; i<argc-3; i++) {
			winPtr->menu.chkd[i] = 0;
		}
	}
	winPtr->menu.data[i] = NULL;
	if (!winPtr->button[0]) {
		if (setbuttons(winPtr, interp, 4, defaultbuttons) == TCL_ERROR)
			return TCL_ERROR;
	}
	getbegyx(winPtr->win, winy, winx);
	subx = (winPtr->border? 3: 2);
	suby = winPtr->height - 2 - (winPtr->border? 2: 0) - subht;
	subwid = winPtr->width - (winPtr->border? 6: 4);
	if ((winPtr->menu.win = subwin(winPtr->win, subht, subwid,
					suby+winy, subx+winx)) == NULL) {
		Tcl_AppendResult(interp, "Unable to create menu subwindow",
			NULL);
		return TCL_ERROR;
	}
	drawbox(winPtr, suby-1, subx-1, subht+2, subwid+2, 1,
		 ATTR_LRBORD, ATTR_ULBORD);

	if (updatemenu(winPtr, interp) == TCL_ERROR)
		return TCL_ERROR;
	if (updatetext(winPtr, interp) == TCL_ERROR)
		return TCL_ERROR;
	DEBUG1("setmenu() returning okay\n");
	return TCL_OK;
}

static int
updatemenu(winPtr, interp)
	struct windata	*winPtr;
	Tcl_Interp	*interp;
{
	int i, j, tmp, bottom, wid, ht;
	DEBUG3("updatemenu(%p, %p)\n", winPtr, interp);
	bottom = winPtr->menu.topline + winPtr->menu.lines - 1;
	DEBUG2("bottom = %d\n", bottom);
	if (winPtr->menu.current < winPtr->menu.topline) {
		winPtr->menu.topline = winPtr->menu.current;
	}
	if (winPtr->menu.current > bottom) {
		winPtr->menu.topline = winPtr->menu.current - winPtr->menu.lines + 1;
	}
	DEBUG5("menu.current=%d, menu.topline=%d, menu.lines=%d, bottom=%d\n",
		winPtr->menu.current, winPtr->menu.topline, (int) winPtr->menu.lines, bottom);
	getmaxyx(winPtr->menu.win, ht, wid);
	for (i = 0; i < winPtr->menu.lines; i++) {
		tmp = i+winPtr->menu.topline;
		if (tmp == winPtr->menu.current)
			wattrset(winPtr->menu.win, ATTR_SELECT);
		else
			wattrset(winPtr->menu.win, ATTR_TEXT);
		wmove(winPtr->menu.win, i, 0);
		for (j = 0; j < wid; j++)
			waddch(winPtr->menu.win, ' ');
		if (tmp < winPtr->menu.items && winPtr->menu.data[tmp]) {
			wmove(winPtr->menu.win, i, 0);
			if (winPtr->menu.type == TYPE_RADIO) {
				if (winPtr->menu.rsel == tmp)
					waddstr(winPtr->menu.win, "(X) ");
				else
					waddstr(winPtr->menu.win, "( ) ");
			}
			if (winPtr->menu.type == TYPE_CHECK) {
				if (winPtr->menu.chkd[tmp])
					waddstr(winPtr->menu.win, "[X] ");
				else
					waddstr(winPtr->menu.win, "[ ] ");
			}
			waddstr(winPtr->menu.win, winPtr->menu.data[tmp]);
		}
	}
	return TCL_OK;
}

static int
setbuttons(winPtr, interp, argc, argv)
	struct windata	*winPtr;
	Tcl_Interp	*interp;
	int		argc;
	char		*argv[];
{
	int i, needlines=1, needcols=0;

	DEBUG5("setbuttons(%p, %p, %d, %p)\n", winPtr, interp, argc, argv);
	if (winPtr->text.items) needlines++;
	if (winPtr->border) {
		needlines+= winPtr->text.items? 3:2;
		needcols+=2;
	}
	for (i = 2; i < argc; i++) {
		needcols += strlen(argv[i]) + 3;
	}
	if (winPtr->height < needlines || winPtr->width < needcols) {
		Tcl_AppendResult(interp, "Window too small", NULL);
		return TCL_ERROR;
	}
	if (winPtr->border && winPtr->text.items) {
		wattrset(winPtr->win, ATTR_ULBORD);
		wmove(winPtr->win, winPtr->height-3, 0);
		waddch(winPtr->win, ACS_LTEE);
		for (i = 0; i < winPtr->width-2; i++)
			waddch(winPtr->win, ACS_HLINE);
		wattrset(winPtr->win, ATTR_LRBORD);
		waddch(winPtr->win, ACS_RTEE);
	}
	for (i = 0; i < MAXBUTTONS; i++) {
		if (winPtr->button[i])
			ckfree(winPtr->button);
	}
	for (i = 0; argv[i+2]; i++)
		winPtr->button[i] = strdup(argv[i+2]);
	updatebuttons(winPtr);
	DEBUG1("setbuttons() returning okay\n");
	return TCL_OK;
}

static void
updatebuttons(winPtr)
	struct windata	*winPtr;
{
	int i, j, numbuttons, needcols=0, x, y;

	DEBUG1("updatebuttons(winPtr)\n");

	getyx(winPtr->win, y, x);
	for (i = numbuttons = 0; i < MAXBUTTONS; i++) {
		if (winPtr->button[i])
			numbuttons++;
		else
			break;
	}
	if (winPtr->border)
		needcols+=2;
	for (i = 0; i < numbuttons; i++) {
		needcols += strlen(winPtr->button[i]) + 3;
	}
	wmove(winPtr->win, winPtr->height-2, 1);
	for (i = 0; i < winPtr->width-2; i++) {
		waddch(winPtr->win, ' ');
	}
	wmove(winPtr->win, winPtr->height-2, 1);
	wattrset(winPtr->win, ATTR_TEXT);
	for (i = 0; i < numbuttons; i++) {
		for (j = 0; j < (winPtr->width-needcols)/(numbuttons+1); j++)
			waddch(winPtr->win, ' ');
		if (winPtr->butsel == i) {
			wattrset(winPtr->win, ATTR_SELECT);
			getyx(winPtr->win, y, x);
		}
		waddch(winPtr->win, '<');
		waddstr(winPtr->win, winPtr->button[i]);
		waddch(winPtr->win, '>');
		wattrset(winPtr->win, ATTR_TEXT);
	}
	wmove(winPtr->win, y, x+1);
}

static void
drawbox(winPtr, begrow, begcol, rows, cols, lchar, ulattr, lrattr)
	struct windata	*winPtr;
	int		begrow, begcol, rows, cols, lchar;
	chtype		ulattr, lrattr;
{
	int i;

#define bchar(ch)	(lchar? (ch): ' ')

	DEBUG9("drawbox(%p, %d, %d, %d, %d, %d, %lx, %lx)\n",
		winPtr, begrow, begcol, rows, cols, lchar, ulattr, lrattr);
	wattrset(winPtr->win, ulattr);
	wmove(winPtr->win, begrow, begcol);
	waddch(winPtr->win, bchar(ACS_ULCORNER));
	for (i=0; i<cols-2; i++)
		waddch(winPtr->win, bchar(ACS_HLINE));
	wattrset(winPtr->win, lrattr);
	waddch(winPtr->win, bchar(ACS_URCORNER));
	for (i=begrow+1; i<begrow+rows-1; i++) {
		wattrset(winPtr->win, ulattr);
		mvwaddch(winPtr->win,i,begcol,bchar(ACS_VLINE));
		wattrset(winPtr->win, lrattr);
		mvwaddch(winPtr->win,i,begcol+cols-1,bchar(ACS_VLINE));
	}
	wmove(winPtr->win, begrow+rows-1, begcol);
	wattrset(winPtr->win, ulattr);
	waddch(winPtr->win, bchar(ACS_LLCORNER));
	wattrset(winPtr->win, lrattr);
	for (i=0; i<cols-2; i++)
		waddch(winPtr->win, bchar(ACS_HLINE));
	waddch(winPtr->win, bchar(ACS_LRCORNER));
}

#endif  /* CURSES */
