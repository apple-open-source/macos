/* System dependent definitions for GNU tar.
   Copyright 1994, 1995, 1996, 1997, 1998, 1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* Declare alloca.  AIX requires this to be the first thing in the file.  */

#if __GNUC__
# define alloca __builtin_alloca
#else
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifdef _AIX
 #pragma alloca
#  else
#   ifndef alloca
char *alloca ();
#   endif
#  endif
# endif
#endif

#include <sys/types.h>

/* Declare a generic pointer type.  */
#if defined (__STDC__) || defined(__TURBOC__)
# define voidstar void *
#else
# define voidstar char *
#endif

#include <ctype.h>

/* CTYPE_DOMAIN (C) is nonzero if the unsigned char C can safely be given
   as an argument to <ctype.h> macros like `isspace'.  */
#if STDC_HEADERS
# define CTYPE_DOMAIN(c) 1
#else
# define CTYPE_DOMAIN(c) ((unsigned) (c) <= 0177)
#endif

#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)
#define ISODIGIT(c) ((unsigned) (c) - '0' <= 7)
#define ISPRINT(c) (CTYPE_DOMAIN (c) && isprint (c))
#define ISSPACE(c) (CTYPE_DOMAIN (c) && isspace (c))

/* Declare string and memory handling routines.  Take care that an ANSI
   string.h and pre-ANSI memory.h might conflict, and that memory.h and
   strings.h conflict on some systems.  */

#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
#else
# include <strings.h>
# ifndef strchr
#  define strchr index
# endif
# ifndef strrchr
#  define strrchr rindex
# endif
# ifndef memcpy
#  define memcpy(Dst, Src, Num) bcopy (Src, Dst, Num)
# endif
# ifndef memcmp
#  define memcmp(Src1, Src2, Num) bcmp (Src1, Src2, Num)
# endif
#endif

/* Declare errno.  */

#include <errno.h>
#ifndef errno
extern int errno;
#endif

/* Declare open parameters.  */

#if HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/file.h>
#endif
				/* Pick only one of the next three: */
#ifndef O_RDONLY
# define O_RDONLY	0	/* only allow read */
#endif
#ifndef O_WRONLY
# define O_WRONLY	1	/* only allow write */
#endif
#ifndef O_RDWR
# define O_RDWR		2	/* both are allowed */
#endif
				/* The rest can be OR-ed in to the above: */
#ifndef O_NDELAY
# define O_NDELAY	4	/* don't block on opening devices */
#endif
#ifndef O_CREAT
# define O_CREAT	8	/* create file if needed */
#endif
#ifndef O_EXCL
# define O_EXCL		16	/* file cannot already exist */
#endif
#ifndef O_TRUNC
# define O_TRUNC	32	/* truncate file on open */
#endif
#ifndef O_APPEND
# define O_APPEND	64	/* always write at end of file */
#endif
				/* MS-DOG forever, with my love! */
#ifndef	O_BINARY
# define O_BINARY 0
#endif
				/* Emulate System V 3-argument open call */
#if EMUL_OPEN3
# define open open3
#endif

/* Declare file status routines and bits.  */

#include <sys/stat.h>

#ifndef S_ISLNK
# define lstat stat
#endif

#if STAT_MACROS_BROKEN
# undef S_ISBLK
# undef S_ISCHR
# undef S_ISDIR
# undef S_ISFIFO
# undef S_ISLNK
# undef S_ISMPB
# undef S_ISMPC
# undef S_ISNWK
# undef S_ISREG
# undef S_ISSOCK
#endif

/* On MSDOS, there are missing things from <sys/stat.h>.  */
#if MSDOS
# define S_ISUID 0
# define S_ISGID 0
# define S_ISVTX 0
#endif

#if !defined(S_ISBLK) && defined(S_IFBLK)
# define S_ISBLK(Mode) (((Mode) & S_IFMT) == S_IFBLK)
#endif
#if !defined(S_ISCHR) && defined(S_IFCHR)
# define S_ISCHR(Mode) (((Mode) & S_IFMT) == S_IFCHR)
#endif
#if !defined(S_ISDIR) && defined(S_IFDIR)
# define S_ISDIR(Mode) (((Mode) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG) && defined(S_IFREG)
# define S_ISREG(Mode) (((Mode) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISFIFO) && defined(S_IFIFO)
# define S_ISFIFO(Mode) (((Mode) & S_IFMT) == S_IFIFO)
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
# define S_ISLNK(Mode) (((Mode) & S_IFMT) == S_IFLNK)
#endif
#if !defined(S_ISSOCK) && defined(S_IFSOCK)
# define S_ISSOCK(Mode) (((Mode) & S_IFMT) == S_IFSOCK)
#endif
#if !defined(S_ISMPB) && defined(S_IFMPB) /* V7 */
# define S_ISMPB(Mode) (((Mode) & S_IFMT) == S_IFMPB)
# define S_ISMPC(Mode) (((Mode) & S_IFMT) == S_IFMPC)
#endif
#if !defined(S_ISNWK) && defined(S_IFNWK) /* HP/UX */
# define S_ISNWK(Mode) (((Mode) & S_IFMT) == S_IFNWK)
#endif

#if !HAVE_MKFIFO
# define mkfifo(Path, Mode) (mknod (Path, (Mode) | S_IFIFO, 0))
#endif

#if !defined(S_ISCTG) && defined(S_IFCTG) /* contiguous file */
# define S_ISCTG(Mode) (((Mode) & S_IFMT) == S_IFCTG)
#endif
#ifndef S_ISUID
# define S_ISUID 0004000
#endif
#ifndef S_ISGID
# define S_ISGID 0002000
#endif
#ifndef S_ISVTX
# define S_ISVTX 0001000
#endif
#ifndef S_IRUSR
# define S_IRUSR 0000400
#endif
#ifndef S_IWUSR
# define S_IWUSR 0000200
#endif
#ifndef S_IXUSR
# define S_IXUSR 0000100
#endif
#ifndef S_IRGRP
# define S_IRGRP 0000040
#endif
#ifndef S_IWGRP
# define S_IWGRP 0000020
#endif
#ifndef S_IXGRP
# define S_IXGRP 0000010
#endif
#ifndef S_IROTH
# define S_IROTH 0000004
#endif
#ifndef S_IWOTH
# define S_IWOTH 0000002
#endif
#ifndef S_IXOTH
# define S_IXOTH 0000001
#endif

#define MODE_WXUSR	(S_IWUSR | S_IXUSR)
#define MODE_R		(S_IRUSR | S_IRGRP | S_IROTH)
#define MODE_RW		(S_IWUSR | S_IWGRP | S_IWOTH | MODE_R)
#define MODE_RWX	(S_IXUSR | S_IXGRP | S_IXOTH | MODE_RW)
#define MODE_ALL	(S_ISUID | S_ISGID | S_ISVTX | MODE_RWX)

#ifndef _POSIX_SOURCE
# include <sys/param.h>
#endif

/* Include <unistd.h> before any preprocessor test of _POSIX_VERSION.  */
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef SEEK_SET
# define SEEK_SET 0
#endif
#ifndef SEEK_CUR
# define SEEK_CUR 1
#endif
#ifndef SEEK_END
# define SEEK_END 2
#endif

#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

/* Declare make device, major and minor.  Since major is a function on
   SVR4, we have to resort to GOT_MAJOR instead of just testing if
   major is #define'd.  */

#if MAJOR_IN_MKDEV
# include <sys/mkdev.h>
# define GOT_MAJOR
#endif

#if MAJOR_IN_SYSMACROS
# include <sys/sysmacros.h>
# define GOT_MAJOR
#endif

/* Some <sys/types.h> defines the macros. */
#ifdef major
# define GOT_MAJOR
#endif

#ifndef GOT_MAJOR
# if MSDOS
#  define major(Device)		(Device)
#  define minor(Device)		(Device)
#  define makedev(Major, Minor)	(((Major) << 8) | (Minor))
#  define GOT_MAJOR
# endif
#endif

/* For HP-UX before HP-UX 8, major/minor are not in <sys/sysmacros.h>.  */
#ifndef GOT_MAJOR
# if defined(hpux) || defined(__hpux__) || defined(__hpux)
#  include <sys/mknod.h>
#  define GOT_MAJOR
# endif
#endif

#ifndef GOT_MAJOR
# define major(Device)		(((Device) >> 8) & 0xff)
# define minor(Device)		((Device) & 0xff)
# define makedev(Major, Minor)	(((Major) << 8) | (Minor))
#endif

#undef GOT_MAJOR

/* Declare directory reading routines and structures.  */

#if __TURBOC__
# include "msd_dir.h"
# define NAMLEN(dirent) ((dirent)->d_namlen)
#else
# if HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMLEN(dirent) (strlen((dirent)->d_name))
# else
#  define dirent direct
#  define NAMLEN(dirent) ((dirent)->d_namlen)
#  if HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
# endif
#endif

/* Declare wait status.  */

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#if HAVE_UNION_WAIT
# define WAIT_T union wait
# ifndef WTERMSIG
#  define WTERMSIG(Status)     ((Status).w_termsig)
# endif
# ifndef WCOREDUMP
#  define WCOREDUMP(Status)    ((Status).w_coredump)
# endif
# ifndef WEXITSTATUS
#  define WEXITSTATUS(Status)  ((Status).w_retcode)
# endif
#else
# define WAIT_T int
# ifndef WTERMSIG
#  define WTERMSIG(Status)     ((Status) & 0x7f)
# endif
# ifndef WCOREDUMP
#  define WCOREDUMP(Status)    ((Status) & 0x80)
# endif
# ifndef WEXITSTATUS
#  define WEXITSTATUS(Status)  (((Status) >> 8) & 0xff)
# endif
#endif

#ifndef WIFSTOPPED
# define WIFSTOPPED(Status)    (WTERMSIG(Status) == 0x7f)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(Status)   (WTERMSIG(Status) != 0)
#endif
#ifndef WIFEXITED
# define WIFEXITED(Status)     (WTERMSIG(Status) == 0)
#endif

/* FIXME: It is wrong to use BLOCKSIZE for buffers when the logical block
   size is greater than 512 bytes; so ST_BLKSIZE code below, in preparation
   for some cleanup in this area, later.  */

/* Get or fake the disk device blocksize.  Usually defined by sys/param.h
   (if at all).  */

#if !defined(DEV_BSIZE) && defined(BSIZE)
# define DEV_BSIZE BSIZE
#endif
#if !defined(DEV_BSIZE) && defined(BBSIZE) /* SGI */
# define DEV_BSIZE BBSIZE
#endif
#ifndef DEV_BSIZE
# define DEV_BSIZE 4096
#endif

/* Extract or fake data from a `struct stat'.  ST_BLKSIZE gives the
   optimal I/O blocksize for the file, in bytes.  Some systems, like
   Sequents, return st_blksize of 0 on pipes.  */

#if !HAVE_ST_BLKSIZE
# define ST_BLKSIZE(Statbuf) DEV_BSIZE
#else
# define ST_BLKSIZE(Statbuf) \
    ((Statbuf).st_blksize > 0 ? (Statbuf).st_blksize : DEV_BSIZE)
#endif

/* Extract or fake data from a `struct stat'.  ST_NBLOCKS gives the
   number of ST_NBLOCKSIZE-byte blocks in the file (including indirect blocks).
   HP-UX counts st_blocks in 1024-byte units,
   this loses when mixing HP-UX and BSD filesystems with NFS.  AIX PS/2
   counts st_blocks in 4K units.  */

#if !HAVE_ST_BLOCKS
# if defined(_POSIX_SOURCE) || !defined(BSIZE)
#  define ST_NBLOCKS(Statbuf) ((Statbuf).st_size / ST_NBLOCKSIZE + ((Statbuf).st_size % ST_NBLOCKSIZE != 0))
# else
   off_t st_blocks ();
#  define ST_NBLOCKS(Statbuf) (st_blocks ((Statbuf).st_size))
# endif
#else
# define ST_NBLOCKS(Statbuf) ((Statbuf).st_blocks)
# if defined(hpux) || defined(__hpux__) || defined(__hpux)
#  define ST_NBLOCKSIZE 1024
# else
#  if defined(_AIX) && defined(_I386)
#   define ST_NBLOCKSIZE (4 * 1024)
#  endif
# endif
#endif

#ifndef ST_NBLOCKSIZE
#define ST_NBLOCKSIZE 512
#endif

/* This is a real challenge to properly get MTIO* symbols :-(.  ISC uses
   <sys/gentape.h>.  SCO and BSDi uses <sys/tape.h>; BSDi also requires
   <sys/tprintf.h> and <sys/device.h> for defining tp_dev and tpr_t.  It
   seems that the rest use <sys/mtio.h>, which itself requires other files,
   depending on systems.  Pyramid defines _IOW in <sgtty.h>, for example.  */

#if HAVE_SYS_GENTAPE_H
# include <sys/gentape.h>
#else
# if HAVE_SYS_TAPE_H
#  if HAVE_SYS_DEVICE_H
#   include <sys/device.h>
#  endif
#  if HAVE_SYS_BUF_H
#   include <sys/buf.h>
#  endif
#  if HAVE_SYS_TPRINTF_H
#   include <sys/tprintf.h>
#  endif
#  include <sys/tape.h>
# else
#  if HAVE_SYS_MTIO_H
#   include <sys/ioctl.h>
#   if HAVE_SGTTY_H
#    include <sgtty.h>
#   endif
#   if HAVE_SYS_IO_TRIOCTL_H
#    include <sys/io/trioctl.h>
#   endif
#   include <sys/mtio.h>
#  endif
# endif
#endif

/* Declare standard functions.  */

#if STDC_HEADERS
# include <stdlib.h>
#else
voidstar malloc ();
voidstar realloc ();
# if HAVE_GETCWD
char *getcwd ();
# endif
char *getenv ();
#endif

#include <stdio.h>

#ifndef _POSIX_VERSION
# if MSDOS
#  include <io.h>
# else
off_t lseek ();
# endif
#endif

#include <pathmax.h>

#if WITH_DMALLOC
# undef HAVE_VALLOC
# define DMALLOC_FUNC_CHECK
# include <dmalloc.h>
#endif

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#ifndef CHAR_BIT
# define CHAR_BIT 8
#endif

#ifndef CHAR_MAX
# define CHAR_MAX TYPE_MAXIMUM (char)
#endif

#ifndef LONG_MAX
# define LONG_MAX TYPE_MAXIMUM (long)
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

/* These macros work even on ones'-complement hosts (!).
   The extra casts work around common compiler bugs.  */
#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
#define TYPE_MINIMUM(t) (TYPE_SIGNED (t) \
			 ? ~ (t) 0 << (sizeof (t) * CHAR_BIT - 1) \
			 : (t) 0)
#define TYPE_MAXIMUM(t) (~ (t) 0 - TYPE_MINIMUM (t))

/* Bound on length of the string representing an integer value of type t.
   Subtract one for the sign bit if t is signed;
   302 / 1000 is log10 (2) rounded up;
   add one for integer division truncation;
   add one more for a minus sign if t is signed.  */
#define INT_STRLEN_BOUND(t) \
  ((sizeof (t) * CHAR_BIT - TYPE_SIGNED (t)) * 302 / 1000 \
   + 1 + TYPE_SIGNED (t))

#define UINTMAX_STRSIZE_BOUND (INT_STRLEN_BOUND (uintmax_t) + 1)

/* Prototypes for external functions.  */

#ifndef PARAMS
# if PROTOTYPES
#  define PARAMS(Args) Args
# else
#  define PARAMS(Args) ()
# endif
#endif

#if HAVE_LOCALE_H
# include <locale.h>
#endif
#if !HAVE_SETLOCALE
# define setlocale(Category, Locale) /* empty */
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# undef bindtextdomain
# define bindtextdomain(Domain, Directory) /* empty */
# undef textdomain
# define textdomain(Domain) /* empty */
# define _(Text) Text
#endif
#define N_(Text) Text

/* Library modules.  */

#include "error.h"

#if !HAVE_STRSTR
char *strstr PARAMS ((const char *, const char *));
#endif

#if HAVE_VALLOC
# ifndef valloc
voidstar valloc PARAMS ((size_t));
# endif
#else
# define valloc(Size) malloc (Size)
#endif

voidstar xmalloc PARAMS ((size_t));
voidstar xrealloc PARAMS ((voidstar, size_t));
char *xstrdup PARAMS ((const char *));
