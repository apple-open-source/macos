/*
 * compat.c - compatibiltiy routines for the deprived
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#include "zsh.h"

/* Return pointer to first occurence of string t *
 * in string s.  Return NULL if not present.     */

#ifndef HAVE_STRSTR
char *
strstr(const char *s, const char *t)
{
    char *p1, *p2;
 
    for (; *s; s++) {
        for (p1 = s, p2 = t; *p2; p1++, p2++)
            if (*p1 != *p2)
                break;
        if (!*p2)
            return (char *)s;
    }
    return NULL;
}
#endif


#ifndef HAVE_GETHOSTNAME
int
gethostname(char *name, int namelen)
{
    struct utsname uname_str;

    uname(&uname_str);
    strncpy(hostnam, uname_str.nodename, 256);
    return 0;
}
#endif


#ifndef HAVE_GETTIMEOFDAY
void
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    tv->tv_usec = 0;
    tv->tv_sec = (long)time((time_t) 0);
}
#endif


/* compute the difference between two calendar times */

#ifndef HAVE_DIFFTIME
double
difftime(time_t t2, time_t t1)
{
    return ((double)t2 - (double)t1);
}
#endif


#ifndef HAVE_STRERROR
extern char *sys_errlist[];

/* Get error message string associated with a particular  *
 * error number, and returns a pointer to that string.    *
 * This is not a particularly robust version of strerror. */

char *
strerror(int errnum)
{
    return (sys_errlist[errnum]);
}
#endif


/* This function will be changed to work the same way *
 * as POSIX getcwd.  Then I'll use the system getcwd  *
 * if configure finds one.                            */

/**/
char *
zgetcwd(void)
{
    static char buf0[PATH_MAX];
    char *buf2 = buf0 + 1;
    char buf3[PATH_MAX];
    struct stat sbuf;
    struct dirent *de;
    DIR *dir;
    ino_t ino, pino, rootino = (ino_t) ~ 0;
    dev_t dev, pdev, rootdev = (dev_t) ~ 0;

    holdintr();
    buf2[0] = '\0';
    buf0[0] = '/';

    if (stat(buf0, &sbuf) >= 0) {
	rootino = sbuf.st_ino;
	rootdev = sbuf.st_dev;
    }

    if (stat(".", &sbuf) < 0) {
	noholdintr();
	return ztrdup(".");
    }

    pino = sbuf.st_ino;
    pdev = sbuf.st_dev;

    for (;;) {
	if (stat("..", &sbuf) < 0) {
	    chdir(buf0);
	    noholdintr();
	    return ztrdup(".");
	}

	ino = pino;
	dev = pdev;
	pino = sbuf.st_ino;
	pdev = sbuf.st_dev;

	if ((ino == pino && dev == pdev) ||
	    (ino == rootino && dev == rootdev)) {
	    chdir(buf0);
	    noholdintr();
	    return ztrdup(buf0);
	}
	dir = opendir("..");
	if (!dir) {
	    chdir(buf0);
	    noholdintr();
	    return ztrdup(".");
	}
	chdir("..");
	while ((de = readdir(dir))) {
	    char *fn = de->d_name;
	    /* Ignore `.' and `..'. */
	    if (fn[0] == '.' &&
		(fn[1] == '\0' ||
		 (fn[1] == '.' && fn[2] == '\0')))
		continue;
	    if (dev != pdev || (ino_t) de->d_ino == ino) {
		lstat(fn, &sbuf);
		if (sbuf.st_dev == dev && sbuf.st_ino == ino) {
		    strcpy(buf3, de->d_name);
		    break;
		}
	    }
	}
	closedir(dir);
	if (!de) {
	    noholdintr();
	    return ztrdup(".");
	}
	if (*buf2)
	    strcat(buf3, "/");
	strcat(buf3, buf2);
	strcpy(buf2, buf3);
    }
}

