/* unistd.h -- Unix standard function prototypes

   Copyright (C) 2004 Free Software Foundation, Inc.

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
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Conrad T. Pino and Mark D. Baushke */

#ifndef UNISTD_H
#define UNISTD_H

/* Don't include Microsoft's chdir, getcwd here, done in config.h now */
#include <stddef.h>

/* include Microsoft's close, dup */
#include <io.h>

#include <sys/types.h>

/* These functions doesn't exist under Windows NT; we provide stubs */
char * getpass (const char *prompt);
pid_t getpid (void);
int readlink (const char *path, char *buf, size_t buf_size);
unsigned int sleep (unsigned int seconds);
int usleep (useconds_t microseconds);

/*
FIXME:	gethostname prototype for lib/xgethostname.c, no #include <winsock.h>
		Remove when GNULib folks provide a permenant fix.
		Requested by Mark D. Baushke and committed by Conrad T. Pino
*/
int __declspec(dllimport) __stdcall gethostname (char * name, int namelen);

#if 0 /* someday maybe these should be added here as well */

int chdir (const char *pathname);
int mkdir (const char *pathname, mode_t mode);
int rmdir (const char *pathname);
int link (const char *oldpath, const char *newpath);
int unlink (const char *pathname);
int rename (const char *oldpath, const char *newpath);
int stat (const char *file_name, struct stat *buf);
int chmod (const char *path, mode_t mode);
int chown (const char *path, uid_t owner, gid_t group);
int utime (const char *filename, struct utimbuf *buf);
DIR *opendir (const char *name);
struct dirent *readdir(DIR *dir);
int closedir (DIR *dir);
void rewinddir (DIR *dir);
int access (const char *pathname, int mode);
int open (const char *pathname, int flags);
int creat (const char *pathname, mode_t mode);
int close (int fd);
ssize_t read (int fd, void *buf, size_t count);
ssize_t write (int fd, const void *buf, size_t count);
int fcntl (int fd, int cmd);
int fstat (int filedes, struct stat *buf);
off_t lseek (int fildes, off_t offset, int whence);
int dup (int oldfd);
int dup2 (int oldfd, int newfd);
int pipe (int filedes[2]);
mode_t umask (mode_t mask);
FILE *fdopen (int fildes, const char *mode);
int fileno (FILE *stream);
pid_t fork (void);
int execl (const char *path, const char *arg, ...);
int execle (const char *path, const char *arg, ...);
int execlp (const char *file, const char *arg, ...);
int execv (const char *path, char *const argv[]);
int execve (const char *path, char *const argv[],
            char *const envp[]);
int execvp (const char *file, char *const argv[]);
pid_t waitpid (pid_t pid, int *status, int options);
pid_t waitpid (pid_t pid, int *status, int options);
void _exit (int status);
int kill (pid_t pid, int sig);
int pause (void);
unsigned int alarm (unsigned int seconds);
int setuid (uid_t uid);
int setgid (gid_t gid);

#endif /* someday */

#endif /* UNISTD_H */
