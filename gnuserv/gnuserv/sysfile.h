/*
   Copyright (C) 1995 Free Software Foundation, Inc.

This file is part of XEmacs.

XEmacs is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

XEmacs is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with XEmacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Synched up with: Not really in FSF. */

#ifndef INCLUDED_sysfile_h_
#define INCLUDED_sysfile_h_

#include <errno.h>

#ifndef WIN32_NATIVE
#include <sys/errno.h>          /* <errno.h> does not always imply this */
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef INCLUDED_FCNTL
# define INCLUDED_FCNTL
# include <fcntl.h>
#endif /* INCLUDED_FCNTL */

/* Load sys/types.h if not already loaded.
   In some systems loading it twice is suicidal.  */
#ifndef makedev
#include <sys/types.h>		/* some typedefs are used in sys/file.h */
#endif

#ifndef WIN32_NATIVE
#include <sys/file.h>
#endif

#include <sys/stat.h>

#ifndef WIN32_NATIVE
/* Some configuration files' definitions for the LOAD_AVE_CVT macro
   (like sparc.h's) use macros like FSCALE, defined here. */
#include <sys/param.h>
#endif

#if defined (NeXT) || defined(CYGWIN)
/* what is needed from here?  Do others need it too?
 O_BINARY is in here under cygwin. */
# include <sys/fcntl.h>
#endif /* NeXT */

#ifdef WIN32_NATIVE
#include <io.h>
#include <direct.h>
#endif

#ifndef	STDERR_FILENO
#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#ifndef O_WRONLY
#define O_WRONLY 1
#endif

#ifndef O_RDWR
#define O_RDWR 2
#endif

/* file opening defaults */
#ifndef OPEN_BINARY
#ifdef O_BINARY
#define OPEN_BINARY	O_BINARY
#else
#define OPEN_BINARY	(0)
#endif
#endif

#ifndef OPEN_TEXT
#ifdef O_TEXT
#define OPEN_TEXT	O_TEXT
#else
#define OPEN_TEXT	(0)
#endif
#endif

#ifndef CREAT_MODE
#ifdef WIN32_NATIVE
#define CREAT_MODE	(S_IREAD | S_IWRITE)
#else
#define CREAT_MODE	(0666)
#endif
#endif

#ifndef READ_TEXT
#ifdef O_TEXT
#define READ_TEXT "rt"
#else
#define READ_TEXT "r"
#endif
#endif

#ifndef READ_BINARY
#ifdef O_BINARY
#define READ_BINARY "rb"
#else
#define READ_BINARY "r"
#endif
#endif

#ifndef READ_PLUS_TEXT
#ifdef O_TEXT
#define READ_PLUS_TEXT "r+t"
#else
#define READ_PLUS_TEXT "r+"
#endif
#endif

#ifndef READ_PLUS_BINARY
#ifdef O_BINARY
#define READ_PLUS_BINARY "r+b"
#else
#define READ_PLUS_BINARY "r+"
#endif
#endif

#ifndef WRITE_TEXT
#ifdef O_TEXT
#define WRITE_TEXT "wt"
#else
#define WRITE_TEXT "w"
#endif
#endif

#ifndef WRITE_BINARY
#ifdef O_BINARY
#define WRITE_BINARY "wb"
#else
#define WRITE_BINARY "w"
#endif
#endif

#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define O_NONBLOCK O_NDELAY
#else
#define O_NONBLOCK 04000
#endif
#endif

/* if system does not have symbolic links, it does not have lstat.
   In that case, use ordinary stat instead.  */

#ifndef S_IFLNK
#define lstat xemacs_stat
#endif

#if !S_IRUSR
# if S_IREAD
#  define S_IRUSR S_IREAD
# else
#  define S_IRUSR 00400
# endif
#endif

#if !S_IWUSR
# if S_IWRITE
#  define S_IWUSR S_IWRITE
# else
#  define S_IWUSR 00200
# endif
#endif

#if !S_IXUSR
# if S_IEXEC
#  define S_IXUSR S_IEXEC
# else
#  define S_IXUSR 00100
# endif
#endif

#ifdef STAT_MACROS_BROKEN
#undef S_ISBLK
#undef S_ISCHR
#undef S_ISDIR
#undef S_ISFIFO
#undef S_ISLNK
#undef S_ISMPB
#undef S_ISMPC
#undef S_ISNWK
#undef S_ISREG
#undef S_ISSOCK
#endif /* STAT_MACROS_BROKEN.  */

#if !defined(S_ISBLK) && defined(S_IFBLK)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#endif
#if !defined(S_ISCHR) && defined(S_IFCHR)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#endif
#if !defined(S_ISDIR) && defined(S_IFDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISFIFO) && defined(S_IFIFO)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#if !defined(S_ISSOCK) && defined(S_IFSOCK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#endif
#if !defined(S_ISMPB) && defined(S_IFMPB) /* V7 */
#define S_ISMPB(m) (((m) & S_IFMT) == S_IFMPB)
#define S_ISMPC(m) (((m) & S_IFMT) == S_IFMPC)
#endif
#if !defined(S_ISNWK) && defined(S_IFNWK) /* HP/UX */
#define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
#endif

/* Client .c files should simply use `PATH_MAX'. */
#ifndef PATH_MAX
# if defined (_POSIX_PATH_MAX)
#  define PATH_MAX _POSIX_PATH_MAX
# elif defined (MAXPATHLEN)
#  define PATH_MAX MAXPATHLEN
# else
#  define PATH_MAX 1024
# endif
#endif

/* MAXPATHLEN is deprecated, but, as of this writing, still used. */
#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif

/* The following definitions are needed under Windows, at least */
#ifndef X_OK
# define X_OK 1
#endif

#ifndef R_OK
# define R_OK 4
#endif

#ifndef W_OK
# define W_OK 2
#endif

#ifndef F_OK
# define F_OK 0
#endif

#ifndef FD_CLOEXEC
# define FD_CLOEXEC 1
#endif

/* Emacs needs to use its own definitions of certain system calls on
   some systems (like SunOS 4.1 and USG systems, where the read system
   call is interruptible but Emacs expects it not to be; and under
   MULE, where all filenames need to be converted to external format).
   To do this, we #define read to be sys_read, which is defined in
   sysdep.c.  We first #undef read, in case some system file defines
   read as a macro.  sysdep.c doesn't encapsulate read, so the call to
   read inside of sys_read will do the right thing.

   DONT_ENCAPSULATE is used in files such as sysdep.c that want to
   call the actual system calls rather than the encapsulated versions.
   Those files can call sys_read to get the (possibly) encapsulated
   versions.

   IMPORTANT: the redefinition of the system call must occur *after* the
   inclusion of any header files that declare or define the system call;
   otherwise lots of unfriendly things can happen.  This goes for all
   encapsulated system calls.

   We encapsulate the most common system calls here; we assume their
   declarations are in one of the standard header files included above.
   Other encapsulations are declared in the appropriate sys*.h file. */

#ifdef ENCAPSULATE_READ
ssize_t sys_read (int, void *, size_t);
#endif
#if defined (ENCAPSULATE_READ) && !defined (DONT_ENCAPSULATE)
# undef read
# define read sys_read
#endif
#if !defined (ENCAPSULATE_READ) && defined (DONT_ENCAPSULATE)
# define sys_read read
#endif

#ifdef ENCAPSULATE_WRITE
ssize_t sys_write (int, const void *, size_t);
#endif
#if defined (ENCAPSULATE_WRITE) && !defined (DONT_ENCAPSULATE)
# undef write
# define write sys_write
#endif
#if !defined (ENCAPSULATE_WRITE) && defined (DONT_ENCAPSULATE)
# define sys_write write
#endif

#ifdef ENCAPSULATE_OPEN
int sys_open (const char *, int, ...);
#endif
#if defined (ENCAPSULATE_OPEN) && !defined (DONT_ENCAPSULATE)
# undef open
# define open sys_open
#endif
#if !defined (ENCAPSULATE_OPEN) && defined (DONT_ENCAPSULATE)
# define sys_open open
#endif

#ifdef ENCAPSULATE_CLOSE
int sys_close (int);
#endif
#if defined (ENCAPSULATE_CLOSE) && !defined (DONT_ENCAPSULATE)
# undef close
# define close sys_close
#endif
#if !defined (ENCAPSULATE_CLOSE) && defined (DONT_ENCAPSULATE)
# define sys_close close
#endif

/* Now the stdio versions ... */

#ifdef ENCAPSULATE_FREAD
size_t sys_fread (void *, size_t, size_t, FILE *);
#endif
#if defined (ENCAPSULATE_FREAD) && !defined (DONT_ENCAPSULATE)
# undef fread
# define fread sys_fread
#endif
#if !defined (ENCAPSULATE_FREAD) && defined (DONT_ENCAPSULATE)
# define sys_fread fread
#endif

#ifdef ENCAPSULATE_FWRITE
size_t sys_fwrite (const void *, size_t, size_t, FILE *);
#endif
#if defined (ENCAPSULATE_FWRITE) && !defined (DONT_ENCAPSULATE)
# undef fwrite
# define fwrite sys_fwrite
#endif
#if !defined (ENCAPSULATE_FWRITE) && defined (DONT_ENCAPSULATE)
# define sys_fwrite fwrite
#endif

#ifdef ENCAPSULATE_FOPEN
FILE *sys_fopen (const char *, const char *);
#endif
#if defined (ENCAPSULATE_FOPEN) && !defined (DONT_ENCAPSULATE)
# undef fopen
# define fopen sys_fopen
#endif
#if !defined (ENCAPSULATE_FOPEN) && defined (DONT_ENCAPSULATE)
# define sys_fopen fopen
#endif

#ifdef ENCAPSULATE_FCLOSE
int sys_fclose (FILE *);
#endif
#if defined (ENCAPSULATE_FCLOSE) && !defined (DONT_ENCAPSULATE)
# undef fclose
# define fclose sys_fclose
#endif
#if !defined (ENCAPSULATE_FCLOSE) && defined (DONT_ENCAPSULATE)
# define sys_fclose fclose
#endif


/* encapsulations: file-information calls */

#ifdef ENCAPSULATE_ACCESS
int sys_access (const char *path, int mode);
#endif
#if defined (ENCAPSULATE_ACCESS) && !defined (DONT_ENCAPSULATE)
# undef access
# define access sys_access
#endif
#if !defined (ENCAPSULATE_ACCESS) && defined (DONT_ENCAPSULATE)
# define sys_access access
#endif

#ifdef ENCAPSULATE_EACCESS
int sys_eaccess (const char *path, int mode);
#endif
#if defined (ENCAPSULATE_EACCESS) && !defined (DONT_ENCAPSULATE)
# undef eaccess
# define eaccess sys_eaccess
#endif
#if !defined (ENCAPSULATE_EACCESS) && defined (DONT_ENCAPSULATE)
# define sys_eaccess eaccess
#endif

#ifdef ENCAPSULATE_LSTAT
int sys_lstat (const char *path, struct stat *buf);
#endif
#if defined (ENCAPSULATE_LSTAT) && !defined (DONT_ENCAPSULATE)
# undef lstat
# define lstat sys_lstat
#endif
#if !defined (ENCAPSULATE_LSTAT) && defined (DONT_ENCAPSULATE)
# define sys_lstat lstat
#endif

#ifdef ENCAPSULATE_READLINK
int sys_readlink (const char *path, char *buf, size_t bufsiz);
#endif
#if defined (ENCAPSULATE_READLINK) && !defined (DONT_ENCAPSULATE)
# undef readlink
# define readlink sys_readlink
#endif
#if !defined (ENCAPSULATE_READLINK) && defined (DONT_ENCAPSULATE)
# define sys_readlink readlink
#endif

#ifdef ENCAPSULATE_FSTAT
int sys_fstat (int fd, struct stat *buf);
#endif
#if defined (ENCAPSULATE_FSTAT) && !defined (DONT_ENCAPSULATE)
# undef fstat
# define fstat sys_fstat
#endif
#if !defined (ENCAPSULATE_FSTAT) && defined (DONT_ENCAPSULATE)
# define sys_fstat fstat
#endif

int xemacs_stat (const char *path, struct stat *buf);

/* encapsulations: file-manipulation calls */

#ifdef ENCAPSULATE_CHMOD
int sys_chmod (const char *path, mode_t mode);
#endif
#if defined (ENCAPSULATE_CHMOD) && !defined (DONT_ENCAPSULATE)
# undef chmod
# define chmod sys_chmod
#endif
#if !defined (ENCAPSULATE_CHMOD) && defined (DONT_ENCAPSULATE)
# define sys_chmod chmod
#endif

#ifdef ENCAPSULATE_CREAT
int sys_creat (const char *path, mode_t mode);
#endif
#if defined (ENCAPSULATE_CREAT) && !defined (DONT_ENCAPSULATE)
# undef creat
# define creat sys_creat
#endif
#if !defined (ENCAPSULATE_CREAT) && defined (DONT_ENCAPSULATE)
# define sys_creat creat
#endif

#ifdef ENCAPSULATE_LINK
int sys_link (const char *existing, const char *new);
#endif
#if defined (ENCAPSULATE_LINK) && !defined (DONT_ENCAPSULATE)
# undef link
# define link sys_link
#endif
#if !defined (ENCAPSULATE_LINK) && defined (DONT_ENCAPSULATE)
# define sys_link link
#endif

#ifdef ENCAPSULATE_RENAME
int sys_rename (const char *old, const char *new);
#endif
#if defined (ENCAPSULATE_RENAME) && !defined (DONT_ENCAPSULATE)
# undef rename
# define rename sys_rename
#endif
#if !defined (ENCAPSULATE_RENAME) && defined (DONT_ENCAPSULATE)
# define sys_rename rename
#endif

#ifdef ENCAPSULATE_SYMLINK
int sys_symlink (const char *name1, const char *name2);
#endif
#if defined (ENCAPSULATE_SYMLINK) && !defined (DONT_ENCAPSULATE)
# undef symlink
# define symlink sys_symlink
#endif
#if !defined (ENCAPSULATE_SYMLINK) && defined (DONT_ENCAPSULATE)
# define sys_symlink symlink
#endif

#ifdef ENCAPSULATE_UNLINK
int sys_unlink (const char *path);
#endif
#if defined (ENCAPSULATE_UNLINK) && !defined (DONT_ENCAPSULATE)
# undef unlink
# define unlink sys_unlink
#endif
#if !defined (ENCAPSULATE_UNLINK) && defined (DONT_ENCAPSULATE)
# define sys_unlink unlink
#endif

#ifdef ENCAPSULATE_EXECVP
int sys_execvp (const char *, char * const *);
#endif
#if defined (ENCAPSULATE_EXECVP) && !defined (DONT_ENCAPSULATE)
# undef execvp
# define execvp sys_execvp
#endif
#if !defined (ENCAPSULATE_EXECVP) && defined (DONT_ENCAPSULATE)
# define sys_execvp execvp
#endif

#endif /* INCLUDED_sysfile_h_ */
