/* $XConsortium: vendor.h,v 1.12 94/04/17 20:44:00 rws Exp $ */
/*

Copyright (c) 1991  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
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
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xman/vendor.h,v 1.13 2003/07/29 21:16:56 dawes Exp $ */

/* Vendor-specific definitions */

#ifdef CSRG_BASED
#include <sys/param.h>
#endif

#define SUFFIX "suffix"
#define FOLD "fold"
#define FOLDSUFFIX "foldsuffix"
#define MNULL 0
#define MSUFFIX 1
#define MFOLD 2
#define MFOLDSUFFIX 3

/*
 * The directories to search.  Assume that the manual directories are more
 * complete than the cat directories.
 */

#if ( defined(UTEK) || defined(apollo) || defined(Lynx) )
#  define SEARCHDIR  CAT
#else
#  define SEARCHDIR  MAN
#endif

#if ( defined(sgi) || (defined(i386) && (defined(SYSV) || defined(SVR4))) || (defined(BSD) && (BSD >= 199103)) || defined(linux) )
# define SEARCHOTHER CAT
#endif

/*
 * The default manual page directory.
 *
 * The MANPATH enviornment variable will override this.
 */

#ifndef SYSMANPATH

#if defined(macII)
#  define SYSMANPATH "/usr/catman/u_man:/usr/catman/a_man"
#elif defined(__bsdi__)
#  define SYSMANPATH "/usr/share/man:/usr/contrib/man:/usr/contrib/isode/man:/usr/local/man"
#elif defined(__OpenBSD__) || defined(__DARWIN__)
#  define SYSMANPATH "/usr/share/man:/usr/local/man:/usr/X11R6/man"
#elif defined(SVR4) || defined(__osf__) || (defined(BSD) && (BSD >= 199103))
#  define SYSMANPATH "/usr/share/man"
#elif defined(hcx)
#  define SYSMANPATH "/usr/catman/local_man:/usr/catman/u_man:/usr/catman/a_man:/usr/catman/p_man:/usr/catman/ada_man"
#elif defined(SYSV) && defined(i386) && !defined(SCO)
#  define SYSMANPATH "/usr/catman/u_man:/usr/catman/p_man"
#elif defined(sgi)
#  define SYSMANPATH "/usr/catman/a_man:/usr/catman/g_man:/usr/catman/p_man:/usr/catman/u_man:/usr/man/p_man:/usr/man/u_man:/usr/man"
#endif

#ifndef SYSMANPATH
#  define SYSMANPATH "/usr/man"
#endif

#endif

/*
 * Compression Definitions.
 */

#if defined( macII ) || defined( hcx ) || \
	(defined(SYSV) && defined(i386) && !defined(ISC) && !defined(SCO)) || \
	defined(sgi)
#  define COMPRESSION_EXTENSION   "z"
#  define UNCOMPRESS_FORMAT       "pcat %s > %s"
#  define NO_COMPRESS		/* mac can't handle using pack as a filter and
				   xman needs it to be done that way. */
#elif defined(UTEK)
#  define COMPRESSION_EXTENSION "C"
#  define UNCOMPRESS_FORMAT     "ccat < %s > %s"
#  define COMPRESS              "compact"
#elif defined (ISC) || defined(SCO)
#  define COMPRESSION_EXTENSION   "Z"     /* dummy */
#  ifndef SCO
#    define COMPRESSION_EXTENSIONS  "zZF" /* pack, compress, freeze */
#  else
#    define COMPRESSION_EXTENSIONS  "zZ"  /* pack, compress */
#  endif
#  define UNCOMPRESS_FORMAT       uncompress_format
#  define UNCOMPRESS_FORMAT_1     "pcat %s > %s"
#  define UNCOMPRESS_FORMAT_2     "zcat < %s > %s"
#  define UNCOMPRESS_FORMAT_3     "fcat < %s > %s"
#  define NO_COMPRESS
#else
#  define COMPRESSION_EXTENSION "Z"
#  ifndef HAS_MKSTEMP
#    define UNCOMPRESS_FORMAT     "zcat < %s > %s"
#  else
#    define UNCOMPRESS_FORMAT     "zcat < %s >> %s"
#  endif
#  define COMPRESS              "compress"
#  define GZIP_EXTENSION "gz"
#  ifndef HAS_MKSTEMP
#    define GUNZIP_FORMAT "gzip -c -d < %s > %s"
#  else
#    define GUNZIP_FORMAT "gzip -c -d < %s >> %s"
#  endif
#  define GZIP_COMPRESS "gzip"
#endif



/*
 * The command filters for the manual and apropos searches.
 */

#if (defined(hpux) || defined(macII) || defined(CRAY) || defined(ultrix) || \
	defined(hcx) )
#  define NO_MANPATH_SUPPORT
#endif

#ifndef HAS_MKSTEMP
#  ifdef NO_MANPATH_SUPPORT
#    define APROPOS_FORMAT ("man -k %s | pr -h Apropos > %s")
#  else
#    define APROPOS_FORMAT ("man -M %s -k %s | pr -h Apropos > %s")
#  endif
#else
/* with mkstemp the temp output file is already created */
#  ifdef NO_MANPATH_SUPPORT
#    define APROPOS_FORMAT ("man -k %s | pr -h Apropos >> %s")
#  else
#    define APROPOS_FORMAT ("man -M %s -k %s | pr -h Apropos >> %s")
#  endif
#endif

#ifndef HANDLE_ROFFSEQ
# if defined(ultrix)
#  define FORMAT "| nroff -man"             /* The format command. */
# elif defined(CSRG_BASED)
#  define FORMAT "| eqn | tbl | nroff -mandoc"
# elif defined(BSD) && (BSD >= 199103)
#  define FORMAT "| eqn | tbl | nroff -man"
# elif defined(linux)
#  define FORMAT "| eqn | tbl | GROFF_NO_SGR= groff -Tlatin1 -mandoc"
# else
#  define FORMAT "| neqn | nroff -man"      /* The format command. */
# endif
# define TBL "tbl"
#else /* HANDLE_ROFFSEQ */
# if defined(linux)
#  define ZSOELIM	"zsoelim"
# else
#  define ZSOELIM	"soelim"
#endif
# define EQN		"eqn"
# define TBL		"tbl"
# define GRAP		"grap"
# define PIC		"pic"
# define VGRIND		"vgrind"
# define REFER		"refer"
# if defined(CSRG_BASED)
#  define FORMAT	"nroff -mandoc"
# elif defined(linux)
#  define FORMAT	"GROFF_NO_SGR= groff -Tlatin1 -mandoc"
# elif defined(__DARWIN__)
#  define FORMAT	"nroff -man"
# else
#  define FORMAT	"groff -man"
# endif
# define DEFAULT_MANROFFSEQ "et"
#endif /*HANDLE_ROFFSEQ */

/*
 * Names of the man and cat dirs.
 */

#ifdef __bsdi__
#define MAN "cat"
#else
#define MAN "man"
#endif

/*
 * The Apple, Cray,, SYSV386, and HCX folks put the preformatted pages in the
 * "man" directories.
 */
#if (defined(macII) || defined(CRAY) || defined(hcx) || \
	(defined(SYSV) && defined(i386))) && !defined(SCO)
#  define CAT MAN
#elif defined(SCO)
#  define CAT "cat."
#else
#  define CAT "cat"
#endif

typedef struct _SectionList {
  struct _SectionList * next;
  char * label;			/* section label */
  char * directory;		/* section directory */
  int flags;
} SectionList;

extern char * CreateManpageName(char * entry, int section, int flags);
extern void AddStandardSections(SectionList **list, char * path);
extern void AddNewSection(SectionList **list, char * path, char * file, char * label, int flags);
