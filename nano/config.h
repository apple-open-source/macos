/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define this to enable debug messages and assert warnings. */
/* #undef DEBUG */

/* Define this to disable the built-in file browser. */
/* #undef DISABLE_BROWSER */

/* Define this to disable the help text display. */
/* #undef DISABLE_HELP */

/* Define this to disable the justify routines. */
/* #undef DISABLE_JUSTIFY */

/* Define this to disable the mouse support. */
/* #undef DISABLE_MOUSE */

/* Define this to disable the setting of the operating directory (chroot of
   sorts). */
/* #undef DISABLE_OPERATINGDIR */

/* Define this to disable text wrapping as root by default. */
/* #undef DISABLE_ROOTWRAPPING */

/* Define this to disable the spell checker functions. */
/* #undef DISABLE_SPELLER */

/* Define to disable the tab completion functions for files and search
   strings. */
/* #undef DISABLE_TABCOMP */

/* Define this to disable all text wrapping. */
/* #undef DISABLE_WRAPPING */

/* Define this to have syntax highlighting, requires regex.h and ENABLE_NANORC
   too! */
#define ENABLE_COLOR 1

/* Define this to enable multiple file buffers. */
#define ENABLE_MULTIBUFFER 1

/* Define this to use .nanorc files. */
#define ENABLE_NANORC 1

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* Define this if your system has sufficient UTF-8 support (a wide curses
   library, iswalnum(), iswpunct(), iswblank() or iswspace(), mblen(),
   mbstowcs(), mbtowc(), wctomb(), and wcwidth()). */
#define ENABLE_UTF8 1

/* Define to 1 if you have the <curses.h> header file. */
/* #undef HAVE_CURSES_H */

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the `getdelim' function. */
/* #undef HAVE_GETDELIM */

/* Define to 1 if you have the `getline' function. */
/* #undef HAVE_GETLINE */

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the `getopt_long' function. */
#define HAVE_GETOPT_LONG 1

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define if you have the iconv() function. */
#define HAVE_ICONV 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `isblank' function. */
#define HAVE_ISBLANK 1

/* Define to 1 if you have the `iswalnum' function. */
#define HAVE_ISWALNUM 1

/* Define to 1 if you have the `iswblank' function. */
#define HAVE_ISWBLANK 1

/* Define to 1 if you have the `iswpunct' function. */
#define HAVE_ISWPUNCT 1

/* Define to 1 if you have the `iswspace' function. */
#define HAVE_ISWSPACE 1

/* Define to 1 if you have the <libintl.h> header file. */
/* #undef HAVE_LIBINTL_H */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `mblen' function. */
#define HAVE_MBLEN 1

/* Define to 1 if you have the `mbstowcs' function. */
#define HAVE_MBSTOWCS 1

/* Define to 1 if you have the `mbtowc' function. */
#define HAVE_MBTOWC 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <ncurses.h> header file. */
#define HAVE_NCURSES_H 1

/* Define to 1 if you have the <regex.h> header file. */
#define HAVE_REGEX_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strcasestr' function. */
#define HAVE_STRCASESTR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strncasecmp' function. */
#define HAVE_STRNCASECMP 1

/* Define to 1 if you have the `strnlen' function. */
/* #undef HAVE_STRNLEN */

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define this if your curses library has the use_default_colors() command. */
#define HAVE_USE_DEFAULT_COLORS 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the `wctomb' function. */
#define HAVE_WCTOMB 1

/* Define to 1 if you have the <wctype.h> header file. */
#define HAVE_WCTYPE_H 1

/* Define to 1 if you have the `wcwidth' function. */
#define HAVE_WCWIDTH 1

/* Define this to enable extra stuff. */
#define NANO_EXTRA 1

/* Define this to make the nano executable as small as possible. */
/* #undef NANO_TINY */

/* Shut up assert warnings :-) */
#define NDEBUG 1

/* Name of package */
#define PACKAGE "nano"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "nano-devel@gnu.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "GNU nano"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "GNU nano 2.0.6"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "nano"

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.0.6"

/* Where data are placed to. */
#define PKGDATADIR "/usr/share/nano"

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to use the slang wrappers for curses instead of native curses. */
/* #undef USE_SLANG */

/* Version number of package */
#define VERSION "2.0.6"

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */
