/* Defines that autoheader needs a kick in the rump about.
   Leave the blank lines in.  */


#define PACKAGE cscope
#define VERSION 15.3

/* OS Definitions */
#undef Linux
#undef BSD
#undef Darwin

/* Found some version of curses that we're going to use */
#undef HAS_CURSES
   
/* Use SunOS SysV curses? */
#undef USE_SUNOS_CURSES

/* Use old BSD curses - not used right now */
#undef USE_BSD_CURSES

/* Use SystemV curses? */
#undef USE_SYSV_CURSES

/* Use Ncurses? */
#undef USE_NCURSES

/* If you Curses does not have color define this one */
#undef NO_COLOR_CURSES

/* Define if you want to turn on SCO-specific code */
#undef SCO_FLAVOR

/* Set to reflect version of ncurses *
 *   0 = version 1.*
 *   1 = version 1.9.9g
 *   2 = version 4.0/4.1 */
#undef NCURSES_970530

/* Define this if the lex used is the 'real' AT&T variety. Don't define if
 * it's flex or some other */
#undef USING_LEX


