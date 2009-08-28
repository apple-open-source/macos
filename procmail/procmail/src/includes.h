/*$Id: includes.h,v 1.77 2001/08/25 04:38:37 guenther Exp $*/

#include "../autoconf.h"
#ifdef NO_const
#ifdef const
#undef const
#endif
#define const
#endif
#include "../config.h"
	/* not all the "library identifiers" specified here need to be
	   available for all programs in this package; some have substitutes
	   as well (see autoconf); this is just an informal list */

#ifndef _HPUX_SOURCE
#define _HPUX_SOURCE	      /* sad, but needed on HP-UX when compiling -Aa */
#endif
#ifndef NO_FIX_MALLOC
#define NO_FIX_MALLOC		   /* we don't need a `fixed' malloc(0) call */
#endif				/* saves a few bytes in some implementations */

#include <sys/types.h>		/* pid_t mode_t uid_t gid_t off_t */
#ifndef LIMITS_H_MISSING
#include <limits.h>		/* absolutely nothing, just for fun */
#undef LIMITS_H_MISSING
#endif
#ifndef UNISTD_H_MISSING
#include <unistd.h>		/* open() read() write() close() dup() pipe()
				/* fork() getuid() geteuid() getgid() getegid()
				   getpid() execv() execvp() sleep() setuid()
				   setgid() setruid() setrgid() setegid()
				   chown() nice() ftruncate() truncate() */
#undef EX_OK
#else
#undef UNISTD_H_MISSING
#endif
#include <stdio.h>		/* setbuf() fclose() stdin stdout stderr
				/* fopen() fread() fwrite() fgetc() getc()
				   getchar() fdopen() putc() fputs() printf()
				   sprintf() fprintf() sscanf() FILE EOF
				   fileno() */
#ifndef STDDEF_H_MISSING
#include <stddef.h>		/* ptrdiff_t size_t */
#else
#undef STDDEF_H_MISSING
#endif
#ifndef STDLIB_H_MISSING
#include <stdlib.h>		/* getenv() malloc() realloc() free()
				/* strtol() strtod() exit() EXIT_SUCCESS */
#endif
#include <time.h>		/* time() ctime() time_t */
#include <fcntl.h>		/* fcntl() struct flock O_RDONLY O_WRONLY
				/* O_APPEND O_CREAT O_EXCL */
#include <grp.h>		/* getgrgid() struct group */
#include <pwd.h>		/* getpwuid() getpwnam() struct passwd */
#ifndef DIRENT_H_MISSING
#include <dirent.h>		/* opendir() readdir() closedir() DIR
				/* struct dirent */
#endif
#ifndef SYS_WAIT_H_MISSING
#include <sys/wait.h>		/* wait() waitpid() WIFEXITED() WIFSTOPPED()
				/* WEXITSTATUS() WTERMSIG() WNOHANG */
#else
#undef SYS_WAIT_H_MISSING
#endif
#ifndef SYS_UTSNAME_H_MISSING
#include <sys/utsname.h>	/* uname() utsname */
#endif
#include <sys/stat.h>		/* stat() S_ISDIR() S_ISREG() struct stat
				/* chmod() mkdir() */
#include <signal.h>		/* signal() kill() alarm() SIG_IGN SIGHUP
				/* SIGINT SIGQUIT SIGALRM SIGTERM */
#ifndef STRING_H_MISSING
#include <string.h>		/* strcpy() strcat() strlen()
				/* strspn() strcspn() strchr() strcmp()
				   strncmp() strpbrk() strstr() memmove()
				   strncasecmp() memset() */
#endif
#ifndef MATH_H_MISSING
#include <math.h>		/* pow() */
#endif
#ifndef SYSLOG_H_MISSING
#include <syslog.h>		/* openlog() syslog() closelog() LOG_EMERG
				/* LOG_ALERT LOG_CRIT LOG_ERR LOG_NOTICE
				   LOG_PID LOG_MAIL */
#endif
#include <errno.h>		/* EINTR EEXIST ENFILE EACCES EAGAIN EXDEV */
				/* EDQUOT ENOSPC strerror() */
#ifndef SYSEXITS_H_MISSING
#include <sysexits.h>		/* EX_USAGE EX_DATAERR EX_NOINPUT EX_NOUSER
				   EX_UNAVAILABLE EX_OSERR EX_OSFILE
				   EX_CANTCREAT EX_IOERR EX_TEMPFAIL
				   EX_NOPERM EX__BASE */
#endif

#ifdef STDLIB_H_MISSING
#undef STDLIB_H_MISSING
void*malloc(),*realloc();
const char*getenv();
#endif
#ifdef DIRENT_H_MISSING
#undef DIRENT_H_MISSING
#ifndef NDIR_H_MISSING
#include <ndir.h>
#define dirent	direct
#else
#undef NDIR_H_MISSING
#ifndef SYS_NDIR_H_MISSING
#include <sys/ndir.h>
#define dirent	direct
#else
#undef SYS_NDIR_H_MISSING
#ifndef SYS_DIR_H_MISSING
#include <sys/dir.h>
#define dirent	direct
#else			  /* due to brain-damaged NeXT sys/dirent.h contents */
#undef SYS_DIR_H_MISSING
#ifndef SYS_DIRENT_H_MISSING	     /* sys/dirent.h must be moved down here */
#include <sys/dirent.h>
#else
/*#undef SYS_DIRENT_H_MISSING			       /* needed by autoconf */
/* I give up, I can only hope that your system defines DIR and struct dirent */
#endif
#endif
#endif
#endif
#endif /* DIRENT_H_MISSING */
#ifdef STRING_H_MISSING
#undef STRING_H_MISSING
#include <strings.h>
#ifndef strchr
char*strchr();
#endif
char*strpbrk();
#endif
#ifdef SYS_UTSNAME_H_MISSING
#undef SYS_UTSNAME_H_MISSING
#define NOuname
#endif
#ifdef MATH_H_MISSING
#undef MATH_H_MISSING
double pow();
#endif
#ifdef SYSEXITS_H_MISSING
#undef SYSEXITS_H_MISSING
		/* Standard exitcodes, original list maintained
		   by Eric Allman (eric@Sendmail.COM) */
#define EX__BASE	64
#define EX_USAGE	64
#define EX_DATAERR	65
#define EX_NOINPUT	66
#define EX_NOUSER	67
#define EX_UNAVAILABLE	69
#define EX_OSERR	71
#define EX_OSFILE	72
#define EX_CANTCREAT	73
#define EX_IOERR	74
#define EX_TEMPFAIL	75
#define EX_NOPERM	77
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS	0
#endif

#ifdef NO_exit
#define _exit(sig)	exit(sig)
#endif

#if O_SYNC
#else
#undef O_SYNC
#define O_SYNC		0
#endif
#ifndef O_RDONLY
#define O_RDONLY	0
#define O_WRONLY	1
#endif
#ifndef SEEK_SET
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#endif
#ifndef tell
#define tell(fd)	lseek(fd,(off_t)0,SEEK_CUR)
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK	EAGAIN
#endif
#ifndef EAGAIN
#define EAGAIN		EINTR
#endif

#ifndef EOF
#define EOF	(-1)
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode)	(((mode)&S_IFMT)==S_IFDIR)
#ifndef S_IFDIR
#define S_IFDIR 0040000
#endif
#endif

#ifndef S_ISREG
#define S_ISREG(mode)	(((mode)&S_IFMT)==S_IFREG)
#ifndef S_IFREG
#define S_IFREG 0100000
#endif
#endif

#ifndef S_ISLNK
#ifndef S_IFLNK
#define lstat(path,stbuf)	stat(path,stbuf)
#define S_ISLNK(mode)	0
#else
#define S_ISLNK(mode)	(((mode)&S_IFMT)==S_IFLNK)
#endif
#endif

#ifndef S_IFMT
#define S_IFMT	0170000
#endif

#ifndef S_IRWXU
#define S_IRWXU 00700
#define S_IRWXG 00070
#define S_IRWXO 00007
#endif
#ifndef S_IWUSR
#ifdef S_IREAD
#define S_IRUSR	 S_IREAD
#define S_IWUSR	 S_IWRITE
#define S_IXUSR	 S_IEXEC
#else
#define S_IRUSR	 0400
#define S_IWUSR	 0200
#define S_IXUSR	 0100
#endif /* S_IREAD */
#define S_IRGRP	 0040
#define S_IWGRP	 0020
#define S_IXGRP	 0010
#define S_IROTH	 0004
#define S_IWOTH	 0002
#define S_IXOTH	 0001
#endif /* S_IWUSR */
#ifndef S_ISGID
#define S_ISUID 04000
#define S_ISGID 02000
#endif
#ifndef S_ISVTX
#define S_ISVTX 01000
#endif

#ifdef WMACROS_NON_POSIX
#undef WMACROS_NON_POSIX
#ifdef WIFEXITED
#undef WIFEXITED
#endif
#ifdef WIFSTOPPED
#undef WIFSTOPPED
#endif
#ifdef WEXITSTATUS
#undef WEXITSTATUS
#endif
#ifdef WTERMSIG
#undef WTERMSIG
#endif
#endif /* WMACROS_NON_POSIX */

#ifndef WIFEXITED
#define WIFEXITED(waitval)	(!((waitval)&255))
#endif
#ifndef WIFSTOPPED
#define WIFSTOPPED(waitval)	(((waitval)&255)==127)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(waitval)	((waitval)>>8&255)
#endif
#ifndef WTERMSIG
#define WTERMSIG(waitval)	((waitval)&255)
#endif

extern /*const*/char**environ;
extern int errno;

#ifndef STDIN_FILENO
#define STDIN	0
#define STDOUT	1
#define STDERR	2
#else
#define STDIN	STDIN_FILENO
#define STDOUT	STDOUT_FILENO
#define STDERR	STDERR_FILENO
#endif

#ifdef NO_fcntl_LOCK
#ifndef NOfcntl_lock
#define NOfcntl_lock
#endif
#endif
#ifdef NO_lockf_LOCK
#ifdef USElockf
#undef USElockf
#endif
#endif
#ifdef NO_flock_LOCK
#ifdef USEflock
#undef USEflock
#endif
#endif

#ifdef SYSLOG_H_MISSING
#undef SYSLOG_H_MISSING
#define Openlog(ident,logopt,facility)	0
#define syslog				(void)
#define closelog()
#define LOG_EMERG			0
#define LOG_CRIT			0
#define LOG_ALERT			0
#define LOG_ERR				0
#define LOG_NOTICE			0
#else
#ifdef LOG_MAIL
#define Openlog(ident,logopt,facility)	openlog(ident,logopt,facility)
#else
#define Openlog(ident,logopt,facility)	openlog(ident,logopt)
#endif
#endif

#ifndef NOuname
#ifndef P		  /* SINIX V5.23 has the wrong prototype for uname() */
extern int uname();					 /* so we fix it :-) */
#define Uname(name)		((int(*)(struct utsname*))uname)(name)
#else
#define Uname(name)		uname(name)		    /* no fix needed */
#endif /* P */
#endif /* NOuname */
				 /* NEWS OS 5.X has the wrong prototype here */
#define Fdopen(fd,type)		((FILE*)fdopen(fd,type))

#ifdef u
#undef u				       /* and the winner is: AIX 3.2 */
#endif

#ifndef strchr		   /* for very old K&R compatible include files with */
#ifdef P						/* new K&R libraries */
#ifdef void
#ifdef NO_const
extern char*strchr();
extern char*strpbrk();
extern char*strstr();
extern void*memmove();
#endif
#endif
#endif
#endif

#define Const			/*const*/     /* Convex cc doesn't grok this */

#ifndef P				      /* no prototypes without const */
#ifdef NO_const
#define P(args) ()
#endif
#endif

#ifdef NOrename
#undef NOrename
#define rename(old,new) (-(link(old,new)||unlink(old)))
#endif

#ifndef NOsetregid
#ifdef NOsetrgid
#define setrgid(gid)	setregid(gid,-1)
#define setruid(uid)	setreuid(uid,-1)
#endif
#ifdef NOsetegid
#define setegid(gid)	setregid(-1,gid)
#endif
#else
#ifndef NOsetresgid
#ifdef NOsetrgid
#define setrgid(gid)	setresgid(gid,-1,-1)
#define setruid(uid)	setresuid(uid,-1,-1)
#endif
#ifdef NOsetegid
#define setegid(gid)	setresgid(-1,gid,-1)
#endif
#else
#ifdef NOsetrgid
#define setrgid(gid)	(-1)
#define setruid(uid)	(-1)
#endif
#ifdef NOsetegid
#define setegid(gid)	setgid(gid)
#endif
#endif
#endif

#ifdef NOsetrgid
#undef NOsetrgid
#endif
#ifdef NOsetegid
#undef NOsetegid
#endif
#ifdef NOsetregid
#undef NOsetregid
#endif
#ifdef NOsetresgid
#undef NOsetresgid
#endif

#ifdef setrgid_BRAIN_DAMAGE
#undef setrgid_BRAIN_DAMAGE
#ifdef setrgid
#undef setrgid
#endif
#ifdef setruid
#undef setruid
#endif
#define setrgid(gid)	(-1)		   /* and you think POSIX is broken? */
#define setruid(uid)	(-1)			   /* BSD 4.4 just topped it */
#endif

#ifdef setrgid_RUNTIME_CHECK
#undef setrgid_RUNTIME_CHECK
#define setRgid(gid)	(setrgid(gid)||getgid()!=(gid))
#define setRuid(uid)	(setruid(uid)||getuid()!=(uid))
#else
#define setRgid(gid)	setrgid(gid)
#define setRuid(uid)	setruid(uid)
#endif

#ifdef NOinitgroups
/*#undef NOinitgroups				 need this macro in autoconf */
#define initgroups(n,g)
#endif

#ifdef INEFFICIENTrealloc
#undef INEFFICIENTrealloc
#define EXPBLKSIZ	4		    /* 4+3+2+1 = 10 bits total shift */
#else
#define EXPBLKSIZ	0
#endif

#ifdef NOpow
#define tpow(x,y)	(x)
#else
#define tpow(x,y)	pow(x,y)
#endif

#ifdef NOmkdir
#undef NOmkdir
#define mkdir(dir,mode) (-1)
#endif

#ifdef NOftruncate
#undef NOftruncate
#define ftruncate(fildes,length)	(-1)
#define truncate(file,length)		(-1)
#endif

#ifdef NOfsync
#define fsync(fd)	0
#endif

#ifdef NOfstat
#define fstat(fd,st)	(-1)
#endif

#ifdef NOwaitpid
#undef NOwaitpid
#define waitpid(pid,stat_loc,options)	0
#else
#ifndef WNOHANG
#ifdef waitpid
#undef waitpid
#endif
#define waitpid(pid,stat_loc,options)	0
#endif
#endif

#ifdef NOmemmove
#define memmove(to,from,count) smemmove(to,from,count)
#endif

#ifdef NOstrpbrk
#define strpbrk(s,d) sstrpbrk(s,d)
#endif

#ifdef SLOWstrstr
#ifdef strstr
#undef strstr
#endif
#define strstr(haystack,needle) sstrstr(haystack,needle)
#endif

#ifdef NOmemset
#ifdef NObzero
#define NEEDbbzero
#else
#define bbzero(s,l) bzero(s,l)
#endif
#else
#define bbzero(s,l) memset(s,'\0',l)
#endif

#ifdef NOstrlcat
#define strlcat(d,s,z) sstrlcat(d,s,z)
#define strlcpy(d,s,z) sstrlcpy(d,s,z)
#endif

#ifdef NOstrerror
#define strerror(e) sstrerror(e)
#endif

#ifdef NOstrtol
#define strtol(s,p,b) sstrtol(s,p,b)
#endif

#ifndef P
#define P(args)		args
#endif

/*
 * Until function definitions are ANSI, any function whose argument list
 * includes a size_t, uid_t, gid_t, mode_t, pid_t, or time_t type variable
 * should be declared with Q() instead of P().	This is done to prevent
 * problems caused by one of those types being shorter than int and thereby
 * being passed differently under ANSI rules.
 */
#define Q(args)		()

#ifdef oBRAIN_DAMAGE
#undef oBRAIN_DAMAGE
#undef offsetof
#endif
#ifndef offsetof
#define offsetof(s,m) ((char*)&(((s*)sizeof(s))->m)-(char*)sizeof(s))
#endif

#define SETerrno(v)	(errno=(v))	       /* multi-threading errno hook */

#define PROGID		/*const*/char progid[]="Stephen R. van den Berg"
#define maxindex(x)	(sizeof(x)/sizeof((x)[0])-1)
#define STRLEN(x)	(sizeof(x)-1)
#define ioffsetof(s,m)	((int)offsetof(s,m))
#define numeric(x)	(alphanum((unsigned char)(x))>1)
#define sizeNUM(v)	(8*sizeof(v)*4/10+1+1)
#define charNUM(num,v)	char num[sizeNUM(v)]

#define mx(a,b)		((a)>(b)?(a):(b))

typedef unsigned char uschar;	     /* sometimes uchar is already typedef'd */
#ifdef uchar
#undef uchar
#endif
#define uchar uschar

#if ! (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5))
#define __attribute__(foo)
#endif

#if defined(__OpenBSD__) && defined(DEBUGGING)
#define setuid(x) 0			 /* Under OpenBSD, you can't trace a */
#define setgid(x) 0		      /* program after it calls any of these */
#define setegid(x) 0
#endif
