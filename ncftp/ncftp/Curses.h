/* Curses.h */

#ifdef HAVE_LIBNCURSES
	/* The header file that came with ncurses may be named either
	 * ncurses.h or curses.h unfortunately.
	 */
#	ifdef HAVE_NCURSES_H
#		include <ncurses.h>
#	else
#		include <curses.h>
#	endif
#	define USE_CURSES 2
#else
#	ifdef HAVE_LIBCURSESX
		/* Ultrix has a cursesX library which has functions we need.
		 * It also has a regular curses, but we can't use it.
		 */
#		include <cursesX.h>
#		define USE_CURSES 3
#	else
#		ifdef HAVE_LIBCURSES
#			include <curses.h>
#			define USE_CURSES 1
#			ifdef __osf__
#				ifndef CURSES_SHELL_BUG
#					define CURSES_SHELL_BUG 1
#				endif
#			endif
#		endif
#	endif
#endif	/* HAVE_LIBNCURSES */

#ifndef CURSES_SHELL_BUG
#	define CURSES_SHELL_BUG 0
#endif

#ifndef HAVE_GETMAXYX
#	ifdef HAVE__MAXX
#		ifndef getmaxyx
#			define getmaxyx(w,y,x) y = w->_maxy;  x = w->_maxx;
#		endif
#		ifndef getbegyx
#			define getbegyx(w,y,x) y = w->_begy;  x = w->_begx;
#		endif
#	endif
#	ifdef HAVE_MAXX
#		ifndef getmaxyx
#			define getmaxyx(w,y,x) y = w->maxy;  x = w->maxx;
#		endif
#		ifndef getbegyx
#			define getbegyx(w,y,x) y = w->begy;  x = w->begx;
#		endif
#	endif
#endif

#ifndef HAVE_BEEP
#	define BEEP(a)	beep()
#else
#	define BEEP(a)
#endif
