/* Cpp.c */

#include "Sys.h"
#include "Curses.h"
#include "Util.h"
#include "RCmd.h"
#include "Cpp.h"

/* List of CPP symbols that we would like to have show up when we print
 * ther version information.  Some of these are the program's, and others
 * are OS defined symbols.
 *
 * If the compiler chokes in this part, it's probably because the symbol
 * in question was the wrong type (was i when should be s, or vice versa).
 * If you get into trouble, just change the entry for the symbol from
 * using the "i" or "s" macro to use the "b" macro instead.
 *
 * Hopefully that won't happen, since most symbols use the "b" macro
 * below, which doesn't try to use the value of the symbol.  The "i" and
 * "s" macros do use the value, which is nice because then we'll be able
 * to see the value of the symbol when we dump the symbol list.
 */

#define i(a,b) { a, 0, (long) b, NULL },
#define s(a,b) { a, 1, 0L, (char *)b },
#define b(a,b) { a, 0, (long) 1, NULL },

CppSymbol gCppSymbols[] = {
#ifdef __AIX
	b("__AIX", __AIX)
#else
#	ifdef _AIX
		b("_AIX", _AIX)
#	else
#		ifdef AIX
			b("AIX", AIX)
#		endif
#	endif
#endif

#ifdef apollo
	b("apollo", apollo)
#endif
#ifdef aux
	b("aux", aux)
#endif
#ifdef __Besta__
	b("__Besta__", __Besta__)
#endif

#ifdef __bsd__
	b("__bsd__", __bsd__)
#else
#	ifdef __bsd
		b("__bsd", __bsd)
#	else
#		ifdef __BSD
			b("__BSD", __BSD)
#		else
#			ifdef _BSD
				b("_BSD", _BSD)
#			else
#				ifdef BSD
					b("BSD", BSD)
#				else
#					ifdef _SYSTYPE_BSD
						b("_SYSTYPE_BSD", _SYSTYPE_BSD)
#					endif
#				endif
#			endif
#		endif
#	endif
#endif

#ifdef __bsdi__
	b("__bsdi__", __bsdi__)
#endif
#ifdef BULL
	b("BULL", BULL)
#endif
#ifdef USE_CURSES
	i("USE_CURSES", USE_CURSES)
#endif
#ifdef DEBUG
	b("DEBUG", DEBUG)
#endif
#ifdef __dgux
	b("__dgux", __dgux)
#endif
#ifdef DGUX
	b("DGUX", DGUX)
#endif
#ifdef DOMAINNAME
	s("DOMAINNAME", DOMAINNAME)
#endif
#ifdef DYNIX
	b("DYNIX", DYNIX)
#endif
#ifdef DYNIXPTX
	b("DYNIXPTX", DYNIXPTX)
#endif
#ifdef __FreeBSD__
	b("__FreeBSD__", __FreeBSD__)
#endif
#ifdef FTP_PORT
	i("FTP_PORT", FTP_PORT)
#endif
#ifdef __GNUC__
	i("__GNUC__", __GNUC__)
#endif
#ifdef HAVE_LIBCURSES
	b("HAVE_LIBCURSES", HAVE_LIBCURSES)
#endif
#ifdef HAVE_LIBNCURSES
	b("HAVE_LIBNCURSES", HAVE_LIBNCURSES)
#endif
#ifdef HAVE_LIBTERMCAP
	b("HAVE_LIBTERMCAP", HAVE_LIBTERMCAP)
#endif
#ifdef HAVE_LIBREADLINE
	b("HAVE_LIBREADLINE", HAVE_LIBREADLINE)
#endif
#ifdef HAVE_LIBGETLINE
	b("HAVE_LIBGETLINE", HAVE_LIBGETLINE)
#endif
#ifdef HAVE_LIBSOCKS
	b("HAVE_LIBSOCKS", HAVE_LIBSOCKS)
#endif
#ifdef HAVE_UNISTD_H
	b("HAVE_UNISTD_H", HAVE_UNISTD_H)
#endif
#ifdef HOSTNAME
	s("HOSTNAME", HOSTNAME)
#endif

#ifdef __hpux
	b("__hpux", __hpux)
#else
#	ifdef HPUX
		b("HPUX", HPUX)
#	endif
#endif

#ifdef IP_TOS
	b("IP_TOS", IP_TOS)
#endif
#ifdef ISC
	b("ISC", ISC)
#endif
#ifdef LIBMALLOC
	b("LIBMALLOC", LIBMALLOC)
#endif

#ifdef __linux__
	b("__linux__", __linux__)
#else
#	ifdef linux
		b("linux", linux)
#	endif
#endif

#ifdef LOCK_METHOD
	i("LOCK_METHOD", LOCK_METHOD)
#endif
#ifdef NCURSES_VERSION
	s("NCURSES_VERSION", NCURSES_VERSION)
#endif
#ifdef NO_FGTEST
	b("NO_FGTEST", NO_FGTEST)
#endif
#ifdef NeXT
	b("NeXT", NeXT)
#endif
#ifdef __osf__
	b("__osf__", __osf__)
#endif
#ifdef _POSIX_VERSION
	i("_POSIX_VERSION", _POSIX_VERSION)
#endif
#ifdef POSIX_SIGNALS
	b("POSIX_SIGNALS", POSIX_SIGNALS)
#endif
#ifdef pyr
	b("pyr", pyr)
#endif
#ifdef SCO322
	b("SCO322", SCO322)
#endif
#ifdef SCO324
	b("SCO324", SCO324)
#endif
#ifdef SETVBUF_REVERSED
	b("SETVBUF_REVERSED", SETVBUF_REVERSED)
#endif
#ifdef __sgi
	b("__sgi", __sgi)
#endif
#ifdef SINIX
	b("SINIX", SINIX)
#endif
#ifdef __STDC__
	i("__STDC__", __STDC__)
#endif

#ifdef __sun
	b("__sun", __sun)
#else
#	ifdef sun
		b("sun", sun)
#	endif
#endif

#ifdef __svr3__
	b("__svr3__", __svr3__)
#else
#	ifdef SVR3
		b("SVR3", SVR3)
#	endif
#endif

#ifdef __svr4__
	b("__svr4__", __svr4__)
#else
#	ifdef SVR4
		b("SVR4", SVR4)
#	endif
#endif

#ifdef SYSLOG
	b("SYSLOG", SYSLOG)
#endif

#ifdef __sysv__
	b("__sysv__", __sysv__)
#else
#	ifdef __sysv
		b("__sysv", __sysv)
#	else
#		ifdef __SYSV
			b("__SYSV", __SYSV)
#		else
#			ifdef _SYSV
				b("_SYSV", _SYSV)
#			else
#				ifdef SYSV
					b("SYSV", SYSV)
#				else
#					ifdef _SYSTYPE_SYSV
						b("_SYSTYPE_SYSV", _SYSTYPE_SYSV)
#					endif
#				endif
#			endif
#		endif
#	endif
#endif

#ifdef ultrix
	b("ultrix", ultrix)
#endif
#ifdef UNAME
	s("UNAME", UNAME)
#endif
#ifdef USE_GETPWUID
	b("USE_GETPWUID", USE_GETPWUID)
#endif
#ifdef __386BSD__
	b("__386BSD__", __386BSD__)
#endif
	{ NULL, 0, 0 }
};

int gNumCppSymbols = (int) (sizeof(gCppSymbols) / sizeof(CppSymbol)) - 1;

/* eof... */
