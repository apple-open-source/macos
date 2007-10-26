/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <signal.h>

#include <curses.h>
#include <panel.h>

#include <string.h>

#include "top.h"

/* Default signal and signal name. */
#define DISP_SIG		SIGTERM
#define DISP_SIGNAME		"TERM"

/* Value to ungetch() when a signal should cause shutdown. */
#define DISP_KEY_EXIT		(KEY_MAX + 1)

/*
 * Buffer large enough to hold a full line of text, plus a '\0'.  This is
 * dynamically resized as necessary.
 */
char	*disp_lbuf;

/*
 * Buffer for interactive command line large enough to hold a full line of text,
 * plus a '\0'. This is dynamically resized as necessary (same as disp_lbuf).
 */
char	*disp_sbuf;

/* Size of disp_lbuf and disp_sbuf. */
int	disp_bufsize;

/*
 * Buffer large enough to hold any of the following, plus a '\0':
 *
 *   + mode (1)
 *   + number of processes (13)
 *   + sort key (6)
 *   + signal name (6)
 *   + signal number (13)
 *   + pid (9)
 *   + update interval (13)
 *   + username (8)
 *   + uid (13)
 */
static char	disp_ibuf[17];

/* Default signal number and name. */
static int	disp_sig;
static const char *disp_signame;

/* Main window and panel. */
static WINDOW	*disp_dwin;
static PANEL	*disp_dpan;

/*
 * Line on which to print next.  If the line is outside the range displayable,
 * output is ignored.  This is done in order to keep the sampling code from
 * having to understand the constraints of the terminal size.
 *
 * Trickery is necessary when printing to the interactive command line and when
 * printing the help screen, since these operations involve non-linear printing.
 */
static unsigned disp_curline;

/*
 * Minimum number of seconds to leave the contents of the interaction line
 * displayed before clearing it.  This is only a lower bound; if the sample
 * update delay is longer, then the text will not be cleared until the next
 * sample update.
 */
#define DISP_ILINE_CLEAR_DELAY	1

/* Line on which interactive prompts and messages are displayed. */
static int		disp_iline;
static struct timeval	disp_ilinetime;
static boolean_t	disp_ilineclear;
static boolean_t        disp_pending_resize = false;

static boolean_t
disp_p_skipl(void);
static boolean_t
disp_p_printl(const char *a_format, ...);
static boolean_t
disp_p_println(const char *a_format, ...);
static boolean_t
disp_p_vprintln(boolean_t a_newline, const char *a_format, va_list a_p);
static boolean_t
disp_p_wprintl(WINDOW *a_window, const char *a_format, ...);
static boolean_t
disp_p_vwprintln(WINDOW *a_window, boolean_t a_newline, const char *a_format,
    va_list a_p);
static boolean_t
disp_p_mvwprintl(WINDOW *a_window, int a_y, int a_x, const char *a_format, ...);
static boolean_t
disp_p_mvwprintln(WINDOW *a_window, int a_y, int a_x, const char *a_format,
    ...);
static boolean_t
disp_p_vmvwprintln(WINDOW *a_window, boolean_t a_newline, int a_y, int a_x,
    const char *a_format, va_list a_p);
static const char *
disp_p_iline_prompt(const char *a_format, ...);
static boolean_t
disp_p_iline_set(const char *a_format, ...);
static boolean_t
disp_p_iline_eset(boolean_t a_newline, const char *a_format, va_list a_p);
static boolean_t
disp_p_iline_vset(const char *a_format, va_list a_p);
static boolean_t
disp_p_init(void);
static boolean_t
disp_p_fini(void);
static void
disp_p_shutdown(int a_signal);
static boolean_t
disp_p_help(void);
static void
disp_p_sigwinch(int a_signal);
static void
disp_p_resize(int a_signal);
static boolean_t
disp_p_interp_c(void);
static boolean_t
disp_p_interp_ns(const char *a_name, unsigned int *r_int);
static boolean_t
disp_p_interp_Oo(const char *a_name, top_sort_key_t *r_key,
    boolean_t *r_ascend);
static boolean_t
disp_p_interp_S(void);
static boolean_t
disp_p_interp_U(void);

/* Main entry point for the interactive display. */
boolean_t
disp_run(void)
{
	boolean_t	retval;
	int		c, i, y, x, ysize, xsize;
	struct timeval	curtime;

	if (disp_p_init()
	    || samp_init(disp_p_skipl, disp_p_printl, disp_p_println,
	    disp_p_vprintln, disp_p_iline_eset)) {
		retval = TRUE;
		goto RETURN;
	}

	/*
	 * Get the current time for the first redisplay, since gettimeofday() is
	 * called immediatly after wgetch(), so that it is up to date for the
	 * input processing code.
	 */
	gettimeofday(&curtime, NULL);

	for (;;) {
		/* If detached, quit */
		if (!isatty(0)) exit(1);

		if (disp_pending_resize) {
		  disp_p_resize(0);
		  disp_pending_resize = false;
		}

		/* Take a sample and print it. */
		disp_curline = 0;
		if (wmove(disp_dwin, 0, 0) == ERR
		    || samp_run()) {
			retval = TRUE;
			goto RETURN;
		}

		/*
		 * Clear lines between the current position and the end of the
		 * screen, excluding the interactive command line.
		 */
		getmaxyx(disp_dwin, ysize, xsize);
		getyx(disp_dwin, y, x);
		if (y < disp_iline) {
			/* Before interactive command line. */
			for (i = y + 1; i <= disp_iline && i < ysize; i++) {
				if (wclrtoeol(disp_dwin) == ERR
				    || wmove(disp_dwin, i, 0) == ERR) {
					retval = TRUE;
					goto RETURN;
				}
			}
			getyx(disp_dwin, y, x);
		}
		if (y == disp_iline && y + 1 < ysize) {
			/* Skip interactive command line. */
			if (wmove(disp_dwin, disp_iline + 1, 0) == ERR) {
				retval = TRUE;
				goto RETURN;
			}
			getyx(disp_dwin, y, x);
		}
		if (y > disp_iline) {
			if (wclrtobot(disp_dwin) == ERR) {
				retval = TRUE;
				goto RETURN;
			}
		}

		/* Redisplay. */
		update_panels();
		if (doupdate() == ERR) {
			retval = TRUE;
			goto RETURN;
		}
	GET_CHAR:
		/* Read a character. */
		c = wgetch(disp_dwin);

		/* Get the current time. */
		gettimeofday(&curtime, NULL);

		/*
		 * Clear the interaction line if necessary.  This must be done
		 * after the wgetch() call above, since that call implicitly
		 * does a wrefresh().
		 */
		if (disp_ilineclear
		    && disp_ilinetime.tv_sec + DISP_ILINE_CLEAR_DELAY
		    < curtime.tv_sec) {
			if (disp_p_iline_set("")) {
				retval = TRUE;
				goto RETURN;
			}
			disp_ilineclear = FALSE;
		}

		/* Interpret the typed character. */
		switch (c) {
		case 'c':
			/* Set the output mode. */
			if (disp_p_interp_c()) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 'f':
			/* Toggle shared library reporting. */
			top_opt_f = !top_opt_f;

			if (disp_p_iline_set(top_opt_f
			    ? "Report shared library statistics"
			    : "Do not report shared library statistics"
			    )) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 'n':
			/* Set the update interval. */
			if (disp_p_interp_ns("number of processes",
			    &top_opt_n)) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 'O':
			/* Set the secondary sort key. */
			if (disp_p_interp_Oo("secondary", &top_opt_O,
			    &top_opt_O_ascend)) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 'o':
			/* Set the primary sort key. */
			if (disp_p_interp_Oo("primary", &top_opt_o,
			    &top_opt_o_ascend)) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case DISP_KEY_EXIT: /* SIGINT or SIGQUIT. */
		case 'q':
			/* Quit. */
			samp_fini();
			retval = FALSE;
			goto RETURN;
		case 'r':
			/* Toggle memory object map reporting. */
			top_opt_r = !top_opt_r;

			if (disp_p_iline_set(top_opt_r
			    ? "Report process memory object maps"
			    : "Do not report process memory object maps"
			    )) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 'S':
			/* Send a signal. */
			if (disp_p_interp_S()) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 's':
			/* Set the update interval. */
			if (disp_p_interp_ns("update interval", &top_opt_s)) {
				retval = TRUE;
				goto RETURN;
			}

			wtimeout(disp_dwin, top_opt_s * 1000);

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 't':
			/* Toggle memory object map reporting for pid 0. */
			top_opt_t = !top_opt_t;

			if (disp_p_iline_set(top_opt_t
			    ? "Translate uid numbers to usernames"
			    : "Do not translate uid numbers to usernames")) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 'U':
			/* Only display processes owned by a particular user. */
			if (disp_p_interp_U()) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 'w':
			/* Toggle wide/narrow delta mode. */
			top_opt_w = !top_opt_w;

			if (disp_p_iline_set(top_opt_w
			    ? "Display wide deltas"
			    : "Display narrow deltas")) {
				retval = TRUE;
				goto RETURN;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case 'x':
			/* Toggle display format. */
			top_opt_x = !top_opt_x;

			if (disp_p_iline_set(top_opt_x
			    ? "Normal display"
			    : "legacy display")) {
				retval = TRUE;
				goto RETURN;
			}

			if (top_opt_x) {
				top_opt_f = TRUE;
				top_opt_r = TRUE;
			}

			gettimeofday(&disp_ilinetime, NULL);
			disp_ilineclear = TRUE;
			break;
		case '\x0c': /* C-l */
		case ' ':    /* space */
		case '\r':   /* return */
			/* Redraw. */
			if (disp_ilineclear) {
				if (disp_p_iline_set("")) {
					retval = TRUE;
					goto RETURN;
				}
				disp_ilineclear = FALSE;
			}

			disp_p_resize(0);
			if (redrawwin(disp_dwin) == ERR) {
				retval = TRUE;
				goto RETURN;
			}
			break;
		case '?':
			/* Display the help panel. */
			if (disp_p_help()) {
				retval = TRUE;
				goto RETURN;
			}
			/* Fall through. */
		case ERR: /* Timeout. */
			break;
		default:
			/* Ignore this keypress.  */
		  goto GET_CHAR;
		}
	}
	assert(0); /* Not reached. */

	RETURN:
	if (disp_p_fini()) {
		retval = TRUE;
		goto RETURN;
	}

	return retval;
}

/*
 * The following print functions take pains to assure that any text that falls
 * outside the dimensions of the window are silently cropped.  Additionally, the
 * bottom right cell of the window is never printed to.
 */

/* Skip the interactive prompt line, if not on the last line. */
static boolean_t
disp_p_skipl(void)
{
	boolean_t	retval;
	int		y, x, ysize, xsize;

	getmaxyx(disp_dwin, ysize, xsize);
	getyx(disp_dwin, y, x);

	if (y + 1 < ysize) {
		if (disp_iline != y) {
			/* The interactive command line is moving. */
			disp_iline = y;
			if (disp_p_iline_set(disp_sbuf)) {
				retval = TRUE;
				goto RETURN;
			}
		}
		if (wmove(disp_dwin, y + 1, 0) == ERR) {
			retval = TRUE;
			goto RETURN;
		}
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/* Print a formatted string to a_window at the current position. */
static boolean_t
disp_p_printl(const char *a_format, ...)
{
	boolean_t	retval;
	va_list		ap;

	va_start(ap, a_format);
	retval = disp_p_vwprintln(disp_dwin, FALSE, a_format, ap);
	va_end(ap);

	return retval;
}

/*
 * Print a formatted string to a_window at the current position, and move to
 * the beginning of the next line, if not already on the last line.
 */
static boolean_t
disp_p_println(const char *a_format, ...)
{
	boolean_t	retval;
	va_list		ap;

	va_start(ap, a_format);
	retval = disp_p_vwprintln(disp_dwin, TRUE, a_format, ap);
	va_end(ap);

	return retval;
}

/*
 * Print a formatted string to a_window at the current position, and move to
 * the beginning of the next line, if a_newline is TRUE and not already on the
 * last line.
 */
static boolean_t
disp_p_vprintln(boolean_t a_newline, const char *a_format, va_list a_p)
{
	return disp_p_vwprintln(disp_dwin, a_newline, a_format, a_p);
}

/* Print a formatted string to a_window at the current position. */
static boolean_t
disp_p_wprintl(WINDOW *a_window, const char *a_format, ...)
{
	boolean_t	retval;
	va_list		ap;

	va_start(ap, a_format);
	retval = disp_p_vwprintln(a_window, FALSE, a_format, ap);
	va_end(ap);

	return retval;
}

/*
 * Print a formatted string to a_window at the current position, and move to
 * the beginning of the next line, if a_newline is TRUE and not already on the
 * last line.
 */
static boolean_t
disp_p_vwprintln(WINDOW *a_window, boolean_t a_newline, const char *a_format,
    va_list a_p)
{
	boolean_t	retval;
	int		y, x, ysize, xsize, maxlen;

	getmaxyx(a_window, ysize, xsize);
	getyx(a_window, y, x);
	if (disp_curline + 1 >= ysize) {
		/* Past the bottom of the screen. */
		maxlen = 0;
		assert(maxlen + 1 <= disp_bufsize);
	} else if (y + 1 == ysize) {
		/* Bottom line.  Avoid bottom right corner. */
		maxlen = xsize - x - 1;
		assert(maxlen + 1 <= disp_bufsize);
	} else
#ifdef TOP_DBG
	if (y < ysize)
#endif
	{
		/* Above bottom line. */
		maxlen = xsize - x;
		assert(maxlen + 1 <= disp_bufsize);
	}
#ifdef TOP_DBG
	else assert(0);
#endif

#ifdef TOP_DBG
	assert(maxlen + 1 <= disp_bufsize);
#else
	if (maxlen > disp_bufsize - 1) maxlen = disp_bufsize - 1;
#endif

	if (maxlen > 0) {
		/* Clear to end of line. */
		if (wclrtoeol(a_window) == ERR) {
			retval = TRUE;
			goto RETURN;
		}

		/* Print. */
		if (vsnprintf(disp_lbuf, disp_bufsize, a_format, a_p) == -1
		    || waddnstr(a_window, disp_lbuf, maxlen) == ERR) {
			retval = TRUE;
			goto RETURN;
		}
	}

	/*
	 * Move to the beginning of the next line if requested, and not on the
	 * last line.
	 */
	if (a_newline) {
		disp_curline++;
		if (y + 1 < ysize) {
			if (wmove(a_window, y + 1, 0) == ERR) {
				retval = TRUE;
				goto RETURN;
			}
		}
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/* Print a formatted string to a_window at (a_y, a_x). */
static boolean_t
disp_p_mvwprintl(WINDOW *a_window, int a_y, int a_x, const char *a_format, ...)
{
	boolean_t	retval;
	va_list		ap;

	va_start(ap, a_format);
	retval = disp_p_vmvwprintln(a_window, FALSE, a_y, a_x, a_format, ap);
	va_end(ap);

	return retval;
}

/*
 * Print a formatted string to a_window at (a_y, a_x), and move to the beginning
 * of the next line, if not already on the last line.
 */
static boolean_t
disp_p_mvwprintln(WINDOW *a_window, int a_y, int a_x, const char *a_format, ...)
{
	boolean_t	retval;
	va_list		ap;

	va_start(ap, a_format);
	retval = disp_p_vmvwprintln(a_window, TRUE, a_y, a_x, a_format, ap);
	va_end(ap);

	return retval;
}

/*
 * Print a formatted string to a_window at (a_y, a_x), and move to the beginning
 * of the next line, if a_newline is TRUE and not already on the last line.
 */
static boolean_t
disp_p_vmvwprintln(WINDOW *a_window, boolean_t a_newline, int a_y, int a_x,
    const char *a_format, va_list a_p)
{
	boolean_t	retval;
	int		oldy, oldx, ysize, xsize;

	getmaxyx(a_window, ysize, xsize);

	if (a_y < ysize && a_x < xsize) {
		getyx(a_window, oldy, oldx);

		if (wmove(a_window, a_y, a_x) == ERR) {
			retval = TRUE;
			goto RETURN;
		}

		if (disp_p_vwprintln(a_window, a_newline, a_format, a_p)) {
			wmove(a_window, oldy, oldx);
			retval = TRUE;
			goto RETURN;
		}
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/*
 * Print a formatted string on the interactive command line and get the user
 * input.  Return a pointer the result string, which is '\0'-terminated, or
 * return NULL if there is an error.
 *
 * The maximum size of the input is constrained by disp_ibuf.
 *
 * The allowable input characters are limited to those that make sense for the
 * various inputs top needs to understand.
 */
static const char *
disp_p_iline_prompt(const char *a_format, ...)
{
	const char *	retval;
	va_list		ap;
	boolean_t	done, exited = FALSE;
	int		c, y, x, ysize, xsize, ilen, tcurline;

	getmaxyx(disp_dwin, ysize, xsize);

	tcurline = disp_curline;

	/* Display the prompt. */
	va_start(ap, a_format);
	if (disp_p_iline_vset(a_format, ap)) {
		retval = NULL;
		goto RETURN;
	}
	va_end(ap);

	/* Get the starting position for displaying user input. */
	getyx(disp_dwin, y, x);

	/* Initialize the input string. */
	ilen = 0;
	disp_ibuf[ilen] = '\0';

	for (done = FALSE; done == FALSE;) {
		/* Render the input string if it is visible. */
		if (y == disp_iline && x < xsize - 1) {
			disp_curline = 0;
			if (wmove(disp_dwin, y, x) == ERR
			    || wclrtoeol(disp_dwin) == ERR
			    || disp_p_wprintl(disp_dwin, "%s", disp_ibuf)) {
				wmove(disp_dwin, y, x);
				retval = NULL;
				goto RETURN;
			}
		}

		/* Redisplay. */
		update_panels();
		if (doupdate() == ERR) {
			retval = NULL;
			goto RETURN;
		}

		if (disp_pending_resize) {
		        disp_p_resize(0);

			/* Display the prompt. */
			va_start(ap, a_format);
			if (disp_p_iline_vset(a_format, ap)) {
				retval = NULL;
				goto RETURN;
			}
			va_end(ap);

			/*
			 * Get the starting position for displaying user input.
			 */
			getyx(disp_dwin, y, x);
		}
		  
		/* Read a character. */
		c = wgetch(disp_dwin);

		/* Interpret the typed character. */
		switch (c) {
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
		case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
		case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
		case 'Y': case 'Z':

		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
		case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
		case 's': case 't': case 'u': case 'v': case 'w': case 'x':
		case 'y': case 'z':

		case '0': case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':

		case '_':

		case '+': case '-':
			if (ilen < sizeof(disp_ibuf) - 1) {
				disp_ibuf[ilen] = c;
				ilen++;
				disp_ibuf[ilen] = '\0';
			}
			break;
		case '\x08': /* Backspace. */
		case '\x7f': /* Delete. */
		case KEY_BACKSPACE:
		case KEY_DC:
			if (ilen > 0) {
				ilen--;
				disp_ibuf[ilen] = '\0';
			}
			break;
		case '\x0a': /* Line feed. */
		case '\x0d': /* Carriage return. */
		case KEY_ENTER:
			done = TRUE;
			break;
		case '\x07': /* C-g. */
			ilen = 0;
			disp_ibuf[ilen] = '\0';
			if (disp_p_iline_set("")) {
				retval = NULL;
				goto RETURN;
			}
			done = TRUE;
			break;
		case DISP_KEY_EXIT: /* SIGINT or SIGQUIT. */
			done = TRUE;
			exited = TRUE;
			break;
		case '?':
			/* Display the help panel. */
			if (disp_p_help()) {
				retval = NULL;
				goto RETURN;
			}
			/* Fall through. */
		case ERR: /* Timeout. */
		default:
			break;
		}
	}

	/*
	 * Append the input string to the interactive command line buffer, in
	 * case the interactive command line has to be redrawn on a different
	 * line.
	 */
	strncat(disp_sbuf, disp_ibuf, disp_bufsize - strlen(disp_sbuf) - 1);

	retval = disp_ibuf;
	RETURN:
	disp_curline = tcurline;
	/*
	 * Unget DISP_KEY_EXIT if we processed an exit event, so that the main
	 * event loop knows to exit.
	 */
	if (exited) {
		if (ungetch(DISP_KEY_EXIT) == ERR) {
			retval = NULL;
		}
	}

	return retval;
}

/* Set the contents of the interactive command line. */
static boolean_t
disp_p_iline_set(const char *a_format, ...)
{
	boolean_t	retval;
	va_list		ap;

	va_start(ap, a_format);
	retval = disp_p_iline_vset(a_format, ap);
	va_end(ap);

	return retval;
}

/*
 * Set the contents of the interactive command line, but preserve the current
 * position.
 */
static boolean_t
disp_p_iline_eset(boolean_t a_newline, const char *a_format, va_list a_p)
{
	boolean_t	retval;
	int		y, x;

	getyx(disp_dwin, y, x);

	if (disp_p_iline_vset(a_format, a_p)) {
		retval = TRUE;
		goto RETURN;
	}

	if (wmove(disp_dwin, y, x) == ERR) {
		retval = TRUE;
		goto RETURN;
	}

	gettimeofday(&disp_ilinetime, NULL);
	disp_ilineclear = TRUE;

	retval = FALSE;
	RETURN:
	return retval;
}

/* Set the contents of the interactive command line. */
static boolean_t
disp_p_iline_vset(const char *a_format, va_list a_p)
{
	boolean_t	retval;
	int		ysize, xsize, tcurline;

	getmaxyx(disp_dwin, ysize, xsize);

	tcurline = disp_curline;

	/* Render to the buffer that stores the interactive command line. */
	if (vsnprintf(disp_sbuf, disp_bufsize, a_format, a_p) == -1) {
		retval = TRUE;
		goto RETURN;
	}

	/* If the interactive command line is visible, update it. */
	if (disp_iline < ysize) {
		disp_curline = 0;
		if (disp_p_mvwprintl(disp_dwin, disp_iline, 0, "%s",
		    disp_sbuf)) {
			retval = TRUE;
			goto RETURN;
		}

		update_panels();
		if (doupdate() == ERR) {
			retval = TRUE;
			goto RETURN;
		}
	}

	retval = FALSE;
	RETURN:
	disp_curline = tcurline;
	return retval;
}

/* Initializer. */
static boolean_t
disp_p_init(void)
{
	boolean_t	retval;

	/* Do generic curses initialization. */
	if (initscr() == NULL
	    || cbreak() == ERR
	    || noecho() == ERR
	    || nonl() == ERR
	    || intrflush(stdscr, FALSE) == ERR
	    || meta(stdscr, TRUE) == ERR
	    || keypad(stdscr, TRUE) == ERR) {
		retval = TRUE;
		goto RETURN;
	}

	/*
	 * Initialize signal handlers.  SIGWINCH is taken care of by ncurses,
	 * via disp_pending_resize.
	 */
	signal(SIGINT, disp_p_shutdown);
	signal(SIGQUIT, disp_p_shutdown);
	signal(SIGWINCH, disp_p_sigwinch);
	signal(SIGCONT, disp_p_resize);

	/* Initialize windows and panels. */
	if ((disp_dwin = newwin(0, 0, 0, 0)) == NULL) {
		retval = TRUE;
		goto RETURN;
	}
	wtimeout(disp_dwin, top_opt_s * 1000);
	if ((disp_dpan = new_panel(disp_dwin)) == NULL) {
		retval = TRUE;
		goto RETURN;
	}

	/* Allocate lbuf. */
	disp_lbuf = (char *)malloc(COLS + 1);
	if (disp_lbuf == NULL) {
		retval = TRUE;
		goto RETURN;
	}

	/* Allocate sbuf. */
	disp_sbuf = (char *)malloc(COLS + 1);
	if (disp_sbuf == NULL) {
		retval = TRUE;
		goto RETURN;
	}

	disp_sbuf[0]='\0'; disp_lbuf[0]='\0';
	disp_bufsize = COLS + 1;

	/* Initialize the default signal. */
	disp_sig = DISP_SIG;
	disp_signame = DISP_SIGNAME;

	disp_iline = 0;
	disp_ilineclear = FALSE;

	retval = FALSE;
	RETURN:
	return retval;
}

/* Clean up routine. */
static boolean_t
disp_p_fini(void)
{
	boolean_t	retval;

	/* If no longer attached to tty, just exit */
	if (!isatty(0))
		exit(1);

	if (del_panel(disp_dpan) == ERR
	    || delwin(disp_dwin) == ERR
	    || endwin() == ERR) {
		retval = TRUE;
		goto RETURN;
	}

	/* Free sbuf. */
	free(disp_sbuf);

	/* Free lbuf. */
	free(disp_lbuf);

	retval = FALSE;
	RETURN:
	return retval;
}

/* Set a flag to shut down. */
static void
disp_p_shutdown(int a_signal)
{

	ungetch(DISP_KEY_EXIT);
}

/* Display the help screen. */
static boolean_t
disp_p_help(void)
{
	boolean_t	retval, again, exited = FALSE;
	int		c, y, tcurline;
	WINDOW		*hwin;
	PANEL		*hpan;
	char		opt_U_str[11];

	tcurline = disp_curline;
	disp_curline = 0;

	if ((hwin = newwin(0, 0, 0, 0)) == NULL) {
		retval = TRUE;
		goto RETURN;
	}
	/* Set a timeout so that signals cause a "timeout". */
	wtimeout(hwin, 1000);
	if ((hpan = new_panel(hwin)) == NULL) {
		retval = TRUE;
		goto RETURN;
	}

	/* Loop in the case of a resize or a timeout. */
	for (again = TRUE; again;) {
		again = FALSE;

		/* Initialize the contents of the help window. */
		y = 0;
		if (wattron(hwin, A_UNDERLINE) == ERR
		    || disp_p_mvwprintl(hwin, y, 5, "State")
		    || disp_p_mvwprintl(hwin, y, 12, "Command")
		    || disp_p_mvwprintln(hwin, y, 27, "Description")
		    || wattroff(hwin, A_UNDERLINE) == ERR) {
			retval = TRUE;
			goto RETURN;
		}
		y++;

		if (top_opt_U == FALSE) {
			opt_U_str[0] = '\0';
		} else if (top_opt_t) {
			struct passwd	*pwd;

			pwd = getpwuid((uid_t)top_opt_U_uid);
			if (pwd == NULL) {
				retval = TRUE;
				goto RETURN;
			}
			snprintf(opt_U_str, sizeof(opt_U_str), "%s",
			    pwd->pw_name);
			endpwent();
		} else {
			snprintf(opt_U_str, sizeof(opt_U_str), "%u",
			    top_opt_U_uid);
		}

		if (disp_p_mvwprintln(hwin, y++, 0, "\
            ?              Display this help screen, regardless of context.")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
            ^L             Redraw the screen.")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10c  c<mode>        Set event counting mode to {a|d|e|n}.", top_opt_c)
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10s  f              Toggle shared library reporting.",
		    top_opt_f ? "on" : "off")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10u  n<nprocs>      Only display <nprocs> processes.", top_opt_n)
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10s  O<skey>        Set secondary sort key to <skey> (see o<key>).",
		    top_sort_key_str(top_opt_O, top_opt_O_ascend))
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10s  o<key>         Set primary sort key to <key>: [+-]{command|cpu|pid",
		    top_sort_key_str(top_opt_o, top_opt_o_ascend))
		    || disp_p_mvwprintln(hwin, y++, 0, "\
                           |prt|reg|rprvt|rshrd|rsize|th|time|uid|username|vprvt")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
                           |vsize}.")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
            q              Quit.")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10s  r              Toggle process memory object map reporting.",
		    top_opt_r ? "on" : "off")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
            S<sig>\\n<pid>  Send signal <sig> to pid <pid>.")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10u  s<delay>       Set the delay between updates to <delay> seconds.",
		    top_opt_s)
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10s  t              Toggle uid to username translation.",
		    top_opt_t ? "on" : "off")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10s  U<user>        Only display processes owned by <user>, or all.",
		    opt_U_str)
		    || disp_p_mvwprintln(hwin, y++, 0, "\
%10s  w              Toggle wide/narrow delta mode.",
		    top_opt_w ? "wide" : "narrow")
		    || disp_p_mvwprintln(hwin, y++, 0, "\
")
		    || disp_p_mvwprintl(hwin, y++, 0, "\
Press any key to continue...")) {
			retval = TRUE;
			goto  RETURN;
		}

		/* Refresh the display. */
		update_panels();
		if (doupdate() == ERR) {
			retval = TRUE;
			goto RETURN;
		}

		if (disp_pending_resize) {
		  /*
		   * Resize, but preserve the resize event for the main
		   * event loop so that the main window can be redone.
		   */
		  again = TRUE;
		  
		  disp_p_resize(0);
		  if (wresize(hwin, LINES, COLS) == ERR
		      || replace_panel(hpan, hwin) == ERR) {
		    retval = TRUE;
		    goto RETURN;
		  }
		}

		/* Get a keypress. */
		c = wgetch(hwin);
		switch (c) {
		case DISP_KEY_EXIT: /* SIGINT or SIGQUIT. */
			exited = TRUE;
			break;
		case ERR: /* Timeout. */
			again = TRUE;
			break;
		default:
			/* Do nothing. */
			break;
		}

	}

	/* Clean up. */
	if (del_panel(hpan) == ERR
	    || delwin(hwin) == ERR) {
		retval = TRUE;
		goto RETURN;
	}

	/* Refresh the display. */
	update_panels();
	if (doupdate() == ERR) {
		retval = TRUE;
		goto RETURN;
	}

	retval = FALSE;
	RETURN:
	disp_curline = tcurline;
	/*
	 * Unget DISP_KEY_EXIT if we processed an exit event, so that the main
	 * event loop knows to exit.
	 */
	if (exited && (ungetch(DISP_KEY_EXIT) == ERR)) return FALSE;

	return retval;
}

/* Make a note to resize the terminal. */
static void
disp_p_sigwinch(int a_signal)
{
  disp_pending_resize = true;
}

/*
 * Resize the terminal.  This is done in response to disp_pending_resize, which
 * happens when a SIGWINCH signal was received by the process.
 */
static void
disp_p_resize(int a_signal)
{
	struct winsize	size;
	char		*p, *q;

	if (ioctl(1, TIOCGWINSZ, &size) == -1
	    || resizeterm(size.ws_row, size.ws_col) == ERR
	    || (p = (char *)realloc(disp_lbuf, size.ws_col + 1)) == NULL
	    || (q = (char *)realloc(disp_sbuf, size.ws_col + 1)) == NULL) return;

	disp_lbuf = p;
	disp_sbuf = q;
	disp_bufsize = size.ws_col + 1;

	if (wresize(disp_dwin, size.ws_row, size.ws_col) == ERR
	    || replace_panel(disp_dpan, disp_dwin) == ERR) return;

	redrawwin(disp_dwin);
}

/* Interpret the 'm' (mode) command. */
static boolean_t
disp_p_interp_c(void)
{
	boolean_t	retval;
	const char	*s;

	if ((s = disp_p_iline_prompt("mode [%c]: ", top_opt_c)) == NULL) {
		retval = TRUE;
		goto RETURN;
	}

	if (strlen(s) == 1 && (*s == 'a' || *s == 'd'
	    || *s == 'e' || *s == 'n')) {
		top_opt_c = s[0];
	} else if (strlen(s) != 0) {
		if (disp_p_iline_set( "Invalid mode: %s", s)) {
			retval = TRUE;
			goto RETURN;
		}
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/*
 * Interpret the 'n' (max number of processes) or 's' (update interval) command.
 */
static boolean_t
disp_p_interp_ns(const char *a_name, unsigned int *r_int)
{
	boolean_t	retval;
	const char	*s;
	char		*p;
	unsigned	n;

	if ((s = disp_p_iline_prompt("%s [%u]: ", a_name, *r_int)) == NULL) {
		retval = TRUE;
		goto RETURN;
	}

	if (strlen(s) > 0) {
		errno = 0;
		n = strtoul(s, &p, 0);
		if ((errno == EINVAL && n == 0)
		    || (errno == ERANGE && n == ULONG_MAX)
		    || *p != '\0'
		    || n > TOP_MAX_NPROCS) {
			if (disp_p_iline_set("Invalid %s: %s", a_name, s)) {
				retval = TRUE;
				goto RETURN;
			}
		} else {
			*r_int = n;
		}
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/* Interpret the 'O' (secondary sort key) or 'o' (primary sort key) command. */
static boolean_t
disp_p_interp_Oo(const char *a_name, top_sort_key_t *r_key, boolean_t *r_ascend)
{
	 boolean_t	retval;
	 const char	*s, *p;
	 boolean_t	ascend;

	 if ((s = p = disp_p_iline_prompt("%s key [%s]: ", a_name,
	     top_sort_key_str(*r_key, *r_ascend))) == NULL) {
		 retval = TRUE;
		 goto RETURN;
	 }

	 if (strlen(p) > 0) {
		 if (*p == '+') {
			 ascend = TRUE;
			 p++;
		 } else if (*p == '-') {
			 ascend = FALSE;
			 p++;
		 } else {
			 ascend = FALSE;
		 }
		 *r_ascend = ascend;

		 if (strcmp(p, "command") == 0) {
			 *r_key = TOP_SORT_command;
		 } else if (strcmp(p, "cpu") == 0) {
			*r_key = TOP_SORT_cpu;
		 } else if (strcmp(p, "pid") == 0) {
			*r_key = TOP_SORT_pid;
		 } else if (strcmp(p, "prt") == 0) {
			*r_key = TOP_SORT_prt;
		 } else if (strcmp(p, "reg") == 0) {
			*r_key = TOP_SORT_reg;
		 } else if (strcmp(p, "rprvt") == 0) {
			*r_key = TOP_SORT_rprvt;
		 } else if (strcmp(p, "rshrd") == 0) {
			*r_key = TOP_SORT_rshrd;
		 } else if (strcmp(p, "rsize") == 0) {
			*r_key = TOP_SORT_rsize;
		 } else if (strcmp(p, "th") == 0) {
			*r_key = TOP_SORT_th;
		 } else if (strcmp(p, "time") == 0) {
			*r_key = TOP_SORT_time;
		 } else if (strcmp(p, "uid") == 0) {
			*r_key = TOP_SORT_uid;
		 } else if (strcmp(p, "username") == 0) {
			*r_key = TOP_SORT_username;
		 } else if (strcmp(p, "vprvt") == 0) {
			*r_key = TOP_SORT_vprvt;
		 } else if (strcmp(p, "vsize") == 0) {
			*r_key = TOP_SORT_vsize;
		 } else {
			 if (disp_p_iline_set("Invalid key: %s", s)) {
				 retval = TRUE;
				 goto RETURN;
			 }
		 }
	 }

	 retval = FALSE;
	 RETURN:
	 return retval;
}

/* Interpret the 'S' (signal) command. */
static boolean_t
disp_p_interp_S(void)
{
	boolean_t	retval;
	const char	*s, *signame = NULL;
	char		*p;
	int		pid, sig, error;
	uid_t		euid;
	gid_t		egid;

	/* Get the signal name or number. */
	if (disp_signame != NULL) {
		if ((s = disp_p_iline_prompt("signal [%s]: ", disp_signame))
		    == NULL) {
			retval = TRUE;
			goto RETURN;
		}
	} else {
		if ((s = disp_p_iline_prompt("signal [%d]: ", disp_sig))
		    == NULL) {
			retval = TRUE;
			goto RETURN;
		}
	}

	if (strlen(s) > 0) {
		/* Try to interpret s as a number. */
		errno = 0;
		sig = strtol(s, &p, 0);
		if ((errno == EINVAL && sig == 0)
		    || (errno == ERANGE && sig == LONG_MIN)
		    || (errno == ERANGE && sig == LONG_MAX)
		    || *p != '\0') {
			/*
			 * s is not a number.  Try to interpret it as a signal
			 * name.
			 */
			if (strcmp(s, "HUP") == 0) {
				sig = SIGHUP;
				signame = "HUP";
			} else if (strcmp(s, "INT") == 0) {
				sig = SIGINT;
				signame = "INT";
			} else if (strcmp(s, "QUIT") == 0) {
				sig = SIGQUIT;
				signame = "QUIT";
			} else if (strcmp(s, "ILL") == 0) {
				sig = SIGILL;
				signame = "ILL";
			} else if (strcmp(s, "TRAP") == 0) {
				sig = SIGTRAP;
				signame = "TRAP";
			} else if (strcmp(s, "ABRT") == 0) {
				sig = SIGABRT;
				signame = "ABRT";
			} else if (strcmp(s, "IOT") == 0) {
				sig = SIGIOT;
				signame = "IOT";
			} else if (strcmp(s, "EMT") == 0) {
				sig = SIGEMT;
				signame = "EMT";
			} else if (strcmp(s, "FPE") == 0) {
				sig = SIGFPE;
				signame = "FPE";
			} else if (strcmp(s, "KILL") == 0) {
				sig = SIGKILL;
				signame = "KILL";
			} else if (strcmp(s, "BUS") == 0) {
				sig = SIGBUS;
				signame = "BUS";
			} else if (strcmp(s, "SEGV") == 0) {
				sig = SIGSEGV;
				signame = "SEGV";
			} else if (strcmp(s, "SYS") == 0) {
				sig = SIGSYS;
				signame = "SYS";
			} else if (strcmp(s, "PIPE") == 0) {
				sig = SIGPIPE;
				signame = "PIPE";
			} else if (strcmp(s, "ALRM") == 0) {
				sig = SIGALRM;
				signame = "ALRM";
			} else if (strcmp(s, "TERM") == 0) {
				sig = SIGTERM;
				signame = "TERM";
			} else if (strcmp(s, "URG") == 0) {
				sig = SIGURG;
				signame = "URG";
			} else if (strcmp(s, "STOP") == 0) {
				sig = SIGSTOP;
				signame = "STOP";
			} else if (strcmp(s, "TSTP") == 0) {
				sig = SIGTSTP;
				signame = "TSTP";
			} else if (strcmp(s, "CONT") == 0) {
				sig = SIGCONT;
				signame = "CONT";
			} else if (strcmp(s, "CHLD") == 0) {
				sig = SIGCHLD;
				signame = "CHLD";
			} else if (strcmp(s, "TTIN") == 0) {
				sig = SIGTTIN;
				signame = "TTIN";
			} else if (strcmp(s, "TTOU") == 0) {
				sig = SIGTTOU;
				signame = "TTOU";
			} else if (strcmp(s, "IO") == 0) {
				sig = SIGIO;
				signame = "IO";
			} else if (strcmp(s, "XCPU") == 0) {
				sig = SIGXCPU;
				signame = "XCPU";
			} else if (strcmp(s, "XFSZ") == 0) {
				sig = SIGXFSZ;
				signame = "XFSZ";
			} else if (strcmp(s, "VTALRM") == 0) {
				sig = SIGVTALRM;
				signame = "VTALRM";
			} else if (strcmp(s, "PROF") == 0) {
				sig = SIGPROF;
				signame = "PROF";
			} else if (strcmp(s, "WINCH") == 0) {
				sig = SIGWINCH;
				signame = "WINCH";
			} else if (strcmp(s, "INFO") == 0) {
				sig = SIGINFO;
				signame = "INFO";
			} else if (strcmp(s, "USR1") == 0) {
				sig = SIGUSR1;
				signame = "USR1";
			} else if (strcmp(s, "USR2") == 0) {
				sig = SIGUSR2;
				signame = "USR2";
			} else {
				if (disp_p_iline_set("Invalid signal: %s",
				    s)) {
					retval = TRUE;
					goto RETURN;
				}
				retval = FALSE;
				goto RETURN;
			}
		}
	} else {
		sig = disp_sig;
		signame = disp_signame;
	}

	/* Get the pid. */
	if (signame != NULL) {
		if ((s = disp_p_iline_prompt("Send signal %s to pid: ",
		    signame)) == NULL) {
			retval = TRUE;
			goto RETURN;
		}
	} else {
		if ((s = disp_p_iline_prompt("Send signal %u to pid: ", sig))
		    == NULL) {
			retval = TRUE;
			goto RETURN;
		}
	}

	if (strlen(s) > 0) {
		errno = 0;
		pid = strtol(s, &p, 0);
		if ((errno == EINVAL && pid == 0)
		    || (errno == ERANGE && pid == LONG_MIN)
		    || (errno == ERANGE && pid == LONG_MAX)
		    || *p != '\0') {
			if (disp_p_iline_set("Invalid pid: %s", s)) {
				retval = TRUE;
				goto RETURN;
			}
			retval = FALSE;
			goto RETURN;
		}

		/* Temporarily drop permissions. */
		euid = geteuid();
		egid = getegid();
		if (seteuid(getuid()) == -1
		    || setegid(getgid()) == -1) {
			if (disp_p_iline_set("Missing setuid bit", s)) {
				retval = TRUE;
				goto RETURN;
			}
			retval = FALSE;
			goto RETURN;
		}

		/* Actually send the signal. */
		error = kill((pid_t)pid, sig);

		/* Regain permissions. */
		if (seteuid(euid) == -1
		    || setegid(egid) == -1) {
			if (disp_p_iline_set(
			    "Error restoring setuid bit", s)) {
				retval = TRUE;
				goto RETURN;
			}
			retval = FALSE;
			goto RETURN;
		}

		/* Process errors from kill(). */
		if (error == -1) {
			switch (errno) {
			case EINVAL:
				if (disp_p_iline_set("Invalid signal: %d",
				    sig)) {
					retval = TRUE;
					goto RETURN;
				}
				break;
			case ESRCH:
				if (disp_p_iline_set("Invalid pid: %d", pid)) {
					retval = TRUE;
					goto RETURN;
				}
				break;
			case EPERM:
				if (disp_p_iline_set(
				    "Permission error signaling pid: %d",
				    pid)) {
					retval = TRUE;
					goto RETURN;
				}
				break;
			default:
				assert(0);
				break;
			}

			retval = FALSE;
			goto RETURN;
		} else {
			if (signame != NULL) {
				if (disp_p_iline_set(
				    "Send signal %s to pid %d", signame, pid)) {
					retval = TRUE;
					goto RETURN;
				}
			} else {
				if (disp_p_iline_set(
				    "Send signal %d to pid %d", sig, pid)) {
					retval = TRUE;
					goto RETURN;
				}
			}

			/*
			 * Update the default signal, now that the signal was
			 * successfully sent.
			 */
			disp_sig = sig;
			disp_signame = signame;
		}
	} else {
		if (disp_p_iline_set("Signal command canceled")) {
			retval = TRUE;
			goto RETURN;
		}
	}

	retval = FALSE;
	RETURN:
	return retval;
}

/* Interpret the 'U' (user) command. */
static boolean_t
disp_p_interp_U(void)
{
	boolean_t	retval;
	const char	*s;
	char		*p;
	unsigned	uid;
	struct passwd	*pwd;

	/* Get the username or uid. */
	if ((s = disp_p_iline_prompt("user: ")) == NULL) {
		retval = TRUE;
		goto RETURN;
	}

	if (strlen(s) > 0) {
		errno = 0;
		uid = strtoul(s, &p, 0);
		if ((errno == EINVAL && uid == 0)
		    || (errno == ERANGE && uid == ULONG_MAX)
		    || *p != '\0') {
			/* Not a uid.  Try it as a username. */
			pwd = getpwnam(s);
			if (pwd != NULL) {
				top_opt_U = TRUE;
				top_opt_U_uid = pwd->pw_uid;
			} else {
				/* Not a known username. */
				pwd = NULL;
			}
		} else {
			/* A number was specified.  Make sure is a valid uid. */
			pwd = getpwuid((uid_t)uid);

			if (pwd != NULL) {
				top_opt_U = TRUE;
				top_opt_U_uid = (uid_t)uid;
			}
		}
		endpwent();

		if (pwd == NULL) {
			if (disp_p_iline_set("Invalid user: %s", s)) {
				retval = TRUE;
				goto RETURN;
			}
		}
	} else {
		top_opt_U = FALSE;
	}

	retval = FALSE;
	RETURN:
	return retval;
}
