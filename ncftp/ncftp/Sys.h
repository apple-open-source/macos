/* Sys.h */

#include "Config.h"

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include <sys/types.h>

#include <stdio.h>
#include <sys/time.h>

#ifdef STDC_HEADERS
#	include <stdlib.h>
#endif


#ifdef HAVE_STRING_H
#	include <string.h>		/* They have string.h... */
#	if !defined(STDC_HEADERS) && defined(HAVE_MEMORY_H)
/*
	#		include <memory.h>
*/
#	endif
#	define PTRZERO(p,siz)  memset(p, 0, (size_t) (siz))
#else
#	include <strings.h>		/* Then hope they have strings.h. */
#	define strchr index
#	define strrchr rindex
#	ifdef HAVE_MEMORY_H
#		include <memory.h>
#	endif
#	define memcpy(d,s,n) bcopy((s), (d), (int)(n) )
#	define PTRZERO bzero
#endif

/* Autoconf's AC_TIME_WITH_SYS_TIME defines TIME_WITH_SYS_TIME. */
#ifdef TIME_WITH_SYS_TIME
#	include <sys/time.h>
#	include <time.h>
#else
#	ifdef HAVE_SYS_TIME_H
#		include <sys/time.h>
#	else
#		include <time.h>
#	endif
#endif

#ifdef HAVE_STDARG_H
#	include <stdarg.h>
#else
#	include <varargs.h>
#endif



#ifdef HAVE_FCNTL_H
#	include <fcntl.h>
#endif

/* All of this crap because NeXT doesn't define these symbols. */
#include <sys/stat.h>
#ifndef O_RDONLY
#	define	O_RDONLY	0
#	define	O_WRONLY	1
#	define	O_RDWR		2
#endif

/* These aren't guaranteed to work, as they are defined differently on
 * different systems!
 */
#ifndef O_CREAT
#	define	O_CREAT		0x100
#	define	O_TRUNC		0x200
#	define	O_EXCL		0x400
#endif
#ifndef O_APPEND
#	define O_APPEND		0x08
#endif

/* This group is somewhat standard, though. */
#ifndef S_IRUSR
#	define	S_IRUSR	00400		/* read permission: owner */
#	define	S_IWUSR	00200		/* write permission: owner */
#	define	S_IXUSR	00100		/* execute permission: owner */
#	define	S_IRWXU	00700		/* read, write, execute: owner */
#	define	S_IRWXG	00070		/* read, write, execute: group */
#	define	S_IRGRP	00040		/* read permission: group */
#	define	S_IWGRP	00020		/* write permission: group */
#	define	S_IXGRP	00010		/* execute permission: group */
#	define	S_IRWXO	00007		/* read, write, execute: other */
#	define	S_IROTH	00004		/* read permission: other */
#	define	S_IWOTH	00002		/* write permission: other */
#	define	S_IXOTH	00001		/* execute permission: other */
#endif
#ifndef S_ISDIR
#	define S_ISDIR(mode)	((mode&S_IFMT) == S_IFDIR)
#	define S_ISREG(mode)	((mode&S_IFMT) == S_IFREG) 
#	define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)
#endif



#ifdef HAVE_FLOCK
#		define LOCK_METHOD 2
#else
#	ifdef F_SETLK		/* Def'd if <fcntl.h> has it and fcntl() can lock. */
#		define LOCK_METHOD 1
#	else
#		define LOCK_METHOD 3
#	endif
#endif



#ifdef _POSIX_VERSION
#	define POSIX_SIGNALS 1
#endif

#define PClose pclose

#ifdef SVR4
#	ifndef Gettimeofday
#		define Gettimeofday gettimeofday
#	endif
#endif  /* SVR4 */

#ifndef Gettimeofday
#	define Gettimeofday(a) gettimeofday(a, (struct timezone *)0)
#endif /* Gettimeofday */

/* This malloc stuff is mostly for our own use. */
#define LIBC_MALLOC 0
#define FAST_MALLOC 1
#define DEBUG_MALLOC 2

#ifdef LIBMALLOC
	/* Make sure you use -I to use the malloc.h of choice. */
#	if LIBMALLOC == FAST_MALLOC
#		include "/usr/include/malloc.h"
#	endif
#	if LIBMALLOC == DEBUG_MALLOC
#		include <dbmalloc.h>
#		define MCHK malloc_chain_check(0)
#	endif
#else
#	define LIBMALLOC LIBC_MALLOC
#endif

#if LIBMALLOC != DEBUG_MALLOC
#	define malloc_enter(func)
#	define malloc_leave(func)
#	define malloc_chain_check(a)
#	define malloc_dump(fd)
#	define malloc_list(a,b,c)
#	define malloc_inuse(hist)    (*(hist) = 0, 0)
#	define MCHK
#endif

/* eof */
