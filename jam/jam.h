/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * jam.h - includes and globals for jam
 *
 * 04/08/94 (seiwald) - Coherent/386 support added.
 * 04/21/94 (seiwald) - DGUX is __DGUX__, not just __DGUX.
 * 05/04/94 (seiwald) - new globs.jobs (-j jobs)
 * 11/01/94 (wingerd) - let us define path of Jambase at compile time.
 * 12/30/94 (wingerd) - changed command buffer size for NT (MS-DOS shell).
 * 02/22/95 (seiwald) - Jambase now in /usr/local/lib.
 * 04/30/95 (seiwald) - FreeBSD added.  Live Free or Die.
 * 05/10/95 (seiwald) - SPLITPATH character set up here.
 * 08/20/95 (seiwald) - added LINUX.
 * 08/21/95 (seiwald) - added NCR.
 * 10/23/95 (seiwald) - added SCO.
 * 01/03/96 (seiwald) - SINIX (nixdorf) added.
 * 03/13/96 (seiwald) - Jambase now compiled in; remove JAMBASE variable.
 * 04/29/96 (seiwald) - AIX now has 31 and 42 OSVERs.
 * 11/21/96 (peterk)  - added BeOS with MW CW mwcc
 * 12/21/96 (seiwald) - OSPLAT now defined for NT.
 */

# ifdef VMS

int unlink( char *f ); 	/* In filevms.c */

# include <types.h>
# include <file.h>
# include <stat.h>
# include <stdio.h>
# include <ctype.h>
# include <stdlib.h>
# include <signal.h>
# include <string.h>
# include <time.h>
# include <unixlib.h>

# ifdef __DECC
# define OSSYMS "VMS=true","OS=OPENVMS"
# else
# define OSSYMS "VMS=true","OS=VMS"
# endif 

# define MAXLINE 1024 /* longest 'together' actions */
# define SPLITPATH ','
# define EXITOK 1
# define EXITBAD 0

# else

# ifdef NT

# include <fcntl.h>
# include <stdlib.h>
# include <stdio.h>
# include <ctype.h>
# include <malloc.h>
# include <memory.h>
# include <signal.h>
# include <string.h>
# include <time.h>

# define OSSYMS "NT=true","OS=NT"
# define SPLITPATH ';'
# define MAXLINE 996	/* longest 'together' actions */
# define EXITOK 0
# define EXITBAD 1

# else

# ifdef __OS2__

# include <fcntl.h>
# include <stdlib.h>
# include <stdio.h>
# include <ctype.h>
# include <malloc.h>
# include <signal.h>
# include <string.h>
# include <time.h>

# define OSSYMS "OS2=true","OS=OS2"
# define SPLITPATH ';'
# define MAXLINE 996	/* longest 'together' actions */
# define EXITOK 0
# define EXITBAD 1

# else

# ifdef __QNX__

# define unix

# include <fcntl.h>
# include <stdlib.h>
# include <stdio.h>
# include <ctype.h>
# include <malloc.h>
# include <signal.h>
# include <string.h>
# include <time.h>

# define OSSYMS "UNIX=true","OS=QNX"
# define SPLITPATH ':'
# define MAXLINE 996	/* longest 'together' actions */
# define EXITOK 0
# define EXITBAD 1

# else /* QNX */

# ifdef macintosh
# include <time.h>
# include <stdlib.h>
# include <string.h>
# include <stdio.h>

# define OSSYMS "MAC=true","OS=MAC"
# define SPLITPATH ','
# define MAXLINE 1024	/* longest 'together' actions */
# define EXITOK 0
# define EXITBAD 1

# else /* not MAC */

# include <sys/types.h>
# include <sys/file.h>
# include <sys/stat.h>
# include <fcntl.h>
# ifndef ultrix
# include <stdlib.h>
# endif
# include <stdio.h>
# include <ctype.h>
# if !defined(__bsdi__)&&!defined(__FreeBSD__)
# if !defined(NeXT)&&!defined(__APPLE__)&&!defined(__MACHTEN__)
# if !defined(MVS)
# include <malloc.h>
# endif
# endif
# endif
# include <memory.h>
# include <signal.h>
# include <string.h>
# include <time.h>
# if defined(NeXT) || defined(__APPLE__)
# include <unistd.h>
# endif
# ifdef _AIX
# define unix
# ifdef _AIX41
# define OSSYMS "UNIX=true","OS=AIX","OSVER=41"
# else
# define OSSYMS "UNIX=true","OS=AIX","OSVER=32"
# endif
# endif

# ifdef __BEOS__
# define OSSYMS "UNIX=true","OS=BEOS"
# define unix
# endif

# ifdef __bsdi__
# define OSSYMS "UNIX=true","OS=BSDI"
# endif
# if defined (COHERENT) && defined (_I386)
# define OSSYMS "UNIX=true","OS=COHERENT"
# endif
# ifdef __FreeBSD__
# define OSSYMS "UNIX=true","OS=FREEBSD"
# endif
# ifdef __DGUX__
# define OSSYMS "UNIX=true","OS=DGUX"
# endif
# ifdef __hpux
# define OSSYMS "UNIX=true","OS=HPUX"
# endif
# ifdef __sgi
# define OSSYMS "UNIX=true","OS=IRIX"
# endif
# ifdef __ISC
# define OSSYMS "UNIX=true","OS=ISC"
# endif
# ifdef linux
# define OSSYMS "UNIX=true","OS=LINUX"
# endif
# ifdef __Lynx__
# define OSSYMS "UNIX=true","OS=LYNX"
# define unix
# endif
# ifdef __MACHTEN__
# define OSSYMS "UNIX=true","OS=MACHTEN"
# endif
# ifdef MVS
# define unix
# define OSSYMS "UNIX=true","OS=MVS"
# endif
# ifdef _ATT4
# define OSSYMS "UNIX=true","OS=NCR"
# endif
# ifdef NeXT
# define OSSYMS "UNIX=true","OS=NEXT"
# endif
# ifdef __APPLE__
# define unix
# define OSSYMS "UNIX=true","OS=MACOS"
# endif
# ifdef __osf__
# define OSSYMS "UNIX=true","OS=OSF"
# endif
# ifdef _SEQUENT_
# define OSSYMS "UNIX=true","OS=PTX"
# endif
# ifdef M_XENIX
# define OSSYMS "UNIX=true","OS=SCO"
# endif
# ifdef sinix
# define OSSYMS "UNIX=true","OS=SINIX"
# endif
# ifdef sun
# if defined(__svr4__) || defined(__SVR4)
# define OSSYMS "UNIX=true","OS=SOLARIS"
# else
# define OSSYMS "UNIX=true","OS=SUNOS"
# endif
# endif
# ifdef ultrix
# define OSSYMS "UNIX=true","OS=ULTRIX"
# endif
# if defined(__USLC__) && !defined(M_XENIX)
# define OSSYMS "UNIX=true","OS=UNIXWARE"
# endif
# ifndef OSSYMS
# define OSSYMS "UNIX=true","OS=UNKNOWN"
# endif

# define MAXLINE 10240	/* longest 'together' actions' */
# define SPLITPATH ':'
# define EXITOK 0
# define EXITBAD 1

# endif /* mac */

# endif /* QNX */

# endif /* OS/2 */

# endif /* NT */

# endif /* UNIX */

/* OSPLAT definitions - note the leading , */

# define OSPLATSYM /**/

# ifdef _M_PPC
# undef OSPLATSYM
# define OSPLATSYM ,"OSPLAT=PPC"
# endif

# if defined( _ALPHA_ ) || defined( __alpha__ )
# undef OSPLATSYM
# define OSPLATSYM ,"OSPLAT=AXP"
# endif

# if defined( _i386_ ) || defined( __i386__ ) || defined( _M_IX86 )
# undef OSPLATSYM
# define OSPLATSYM ,"OSPLAT=X86"
# endif 

# ifdef __sparc__
# undef OSPLATSYM
# define OSPLATSYM ,"OSPLAT=SPARC"
# endif

/* You probably don't need to muck with these. */

# define MAXSYM	1024	/* longest symbol in the environment */
# define MAXJPATH 1024	/* longest filename */

# define MAXJOBS 64	/* silently enforce -j limit */
# define MAXARGC 32	/* words in $(JAMSHELL) */

# define CMDBUF 10240	/* size of command blocks */

/* Jam private definitions below. */

# define DEBUG_MAX	10

struct globs {
	int	noexec;
	int	jobs;
	int	ignore;	/* continue after errors */
#ifdef APPLE_EXTENSIONS
	int	parsable_output;
	const char ** cmdline_defines;
#endif
	char	debug[DEBUG_MAX];
} ;

extern struct globs globs;

#ifdef APPLE_EXTENSIONS
# define PARSABLE_OUTPUT ( globs.parsable_output )    /* should we annotate our output to make parsing easier? */
#endif

# define DEBUG_MAKE	( globs.debug[ 1 ] )	/* show actions when executed */
# define DEBUG_MAKEQ	( globs.debug[ 2 ] )	/* show even quiet actions */
# define DEBUG_EXEC	( globs.debug[ 2 ] )	/* show text of actons */
# define DEBUG_MAKEPROG	( globs.debug[ 3 ] )	/* show progress of make0 */
# define DEBUG_BIND	( globs.debug[ 3 ] )	/* show when files bound */

# define DEBUG_EXECCMD	( globs.debug[ 4 ] )	/* show execcmds()'s work */

# define DEBUG_COMPILE	( globs.debug[ 5 ] )	/* show rule invocations */

# define DEBUG_HEADER	( globs.debug[ 6 ] )	/* show result of header scan */
# define DEBUG_BINDSCAN	( globs.debug[ 6 ] )	/* show result of dir scan */
# define DEBUG_SEARCH	( globs.debug[ 6 ] )	/* show attempts at binding */

# define DEBUG_VARSET	( globs.debug[ 7 ] )	/* show variable settings */
# define DEBUG_VARGET	( globs.debug[ 8 ] )	/* show variable fetches */
# define DEBUG_VAREXP	( globs.debug[ 8 ] )	/* show variable expansions */
# define DEBUG_IF	( globs.debug[ 8 ] )	/* show 'if' calculations */
# define DEBUG_LISTS	( globs.debug[ 9 ] )	/* show list manipulation */
# define DEBUG_SCAN	( globs.debug[ 9 ] )	/* show scanner tokens */
# define DEBUG_MEM	( globs.debug[ 9 ] )	/* show memory use */

#ifdef APPLE_EXTENSIONS
int pbx_printf( const char * category_tag, const char * format, ... );
#endif

#ifdef DEBUG_MEMORY_OPERATIONS

#define malloc(n_)  ({ unsigned n__ = (n_); void * p__ = malloc(n__); printf("[malloc(%u) -> 0x%08x..0x%08x] in %s() at %s:%u\n", n__, (unsigned)p__, (unsigned)p__ + n__, __FUNCTION__, __FILE__, __LINE__);fflush(stdout); p__; })
#define calloc(m_,n_)  ({ unsigned m__ = (m_); unsigned n__ = (n_); void * p__ = calloc(m__, n__); printf("[calloc(%u,%u) -> 0x%08x..0x%08x] in %s() at %s:%u\n", m__, n__, (unsigned)p__, (unsigned)p__ + (m__ * n__), __FUNCTION__, __FILE__, __LINE__);fflush(stdout); p__; })
#define realloc(p_,n_)  ({ unsigned n__ = (n_); void * op__ = (p_); void * np__ = realloc(op__, n__); printf("[realloc(0x%08x, %u) -> 0x%08x..0x%08x] in %s() at %s:%u\n", (unsigned)op__, n__, (unsigned)np__, (unsigned)np__ + n__, __FUNCTION__, __FILE__, __LINE__);fflush(stdout); np__; })
#define free(p_)  ({ void * p__ = (p_); printf("[free(0x%08x)] in %s() at %s:%u\n", (unsigned)p__, __FUNCTION__, __FILE__, __LINE__);fflush(stdout); free(p__); })

#define memset(p_,c_,n_)  ({ void * p__ = (p_); unsigned c__ = (c_); unsigned n__ = (n_); printf("[memset(0x%08x,%u,%u) 0x%08x..0x%08x] in %s() at %s:%u\n", (unsigned)p__, c__, n__, (unsigned)p__, (unsigned)p__ + n__, __FUNCTION__, __FILE__, __LINE__);fflush(stdout); memset(p_, c_, n_); })

#endif
