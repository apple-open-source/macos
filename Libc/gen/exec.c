/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <paths.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#if defined(__APPLE__)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char **environ;
#endif

int
#ifdef __STDC__
execl(const char *name, const char *arg, ...)
#else
execl(name, arg, va_alist)
        const char *name;
        const char *arg;
        va_dcl
#endif
{
        va_list ap;
        char **argv;
        int n;

#ifdef __STDC__
        va_start(ap, arg);
#else
        va_start(ap);
#endif
        n = 1;
        while (va_arg(ap, char *) != NULL)
                n++;
        va_end(ap);
        argv = alloca((n + 1) * sizeof(*argv));
        if (argv == NULL) {
                errno = ENOMEM;
                return (-1);
        }
#ifdef __STDC__
        va_start(ap, arg);
#else
        va_start(ap);
#endif
        n = 1;
        argv[0] = (char *)arg;
        while ((argv[n] = va_arg(ap, char *)) != NULL)
                n++;
        va_end(ap);
        return (execve(name, argv, environ));
}

int
#ifdef __STDC__
execle(const char *name, const char *arg, ...)
#else
execle(name, arg, va_alist)
        const char *name;
        const char *arg;
        va_dcl
#endif
{
        va_list ap;
        char **argv, **envp;
        int n;

#ifdef __STDC__
        va_start(ap, arg);
#else
        va_start(ap);
#endif
        n = 1;
        while (va_arg(ap, char *) != NULL)
                n++;
        va_end(ap);
        argv = alloca((n + 1) * sizeof(*argv));
        if (argv == NULL) {
                errno = ENOMEM;
                return (-1);
        }
#ifdef __STDC__
        va_start(ap, arg);
#else
        va_start(ap);
#endif
        n = 1;
        argv[0] = (char *)arg;
        while ((argv[n] = va_arg(ap, char *)) != NULL)
                n++;
        envp = va_arg(ap, char **);
        va_end(ap);
        return (execve(name, argv, envp));
}

int
#ifdef __STDC__
execlp(const char *name, const char *arg, ...)
#else
execlp(name, arg, va_alist)
        const char *name;
        const char *arg;
        va_dcl
#endif
{
        va_list ap;
        char **argv;
        int n;

#ifdef __STDC__
        va_start(ap, arg);
#else
        va_start(ap);
#endif
        n = 1;
        while (va_arg(ap, char *) != NULL)
                n++;
        va_end(ap);
        argv = alloca((n + 1) * sizeof(*argv));
        if (argv == NULL) {
                errno = ENOMEM;
                return (-1);
        }
#ifdef __STDC__
        va_start(ap, arg);
#else
        va_start(ap);
#endif
        n = 1;
        argv[0] = (char *)arg;
        while ((argv[n] = va_arg(ap, char *)) != NULL)
                n++;
        va_end(ap);
        return (execvp(name, argv));
}

int
execv(name, argv)
        const char *name;
        char * const *argv;
{
        (void)execve(name, argv, environ);
        return (-1);
}

int
execvp(name, argv)
        const char *name;
        char * const *argv;
{
        char **memp;
        register int cnt, lp, ln;
        register char *p;
        int eacces = 0, etxtbsy = 0;
        char *bp, *cur, *path, buf[MAXPATHLEN];

        /*
         * Do not allow null name
         */
        if (name == NULL || *name == '\0') {
                errno = ENOENT;
                return (-1);
        }

        /* If it's an absolute or relative path name, it's easy. */
        if (strchr(name, '/')) {
                bp = (char *)name;
                cur = path = NULL;
                goto retry;
        }
        bp = buf;

        /* Get the path we're searching. */
        if (!(path = getenv("PATH")))
                path = _PATH_DEFPATH;
        cur = alloca(strlen(path) + 1);
        if (cur == NULL) {
                errno = ENOMEM;
                return (-1);
        }
        strcpy(cur, path);
        path = cur;
        while ((p = strsep(&cur, ":"))) {
                /*
                 * It's a SHELL path -- double, leading and trailing colons
                 * mean the current directory.
                 */
                if (!*p) {
                        p = ".";
                        lp = 1;
                } else
                        lp = strlen(p);
                ln = strlen(name);

                /*
                 * If the path is too long complain.  This is a possible
                 * security issue; given a way to make the path too long
                 * the user may execute the wrong program.
                 */
                if (lp + ln + 2 > sizeof(buf)) {
                        struct iovec iov[3];

                        iov[0].iov_base = "execvp: ";
                        iov[0].iov_len = 8;
                        iov[1].iov_base = p;
                        iov[1].iov_len = lp;
                        iov[2].iov_base = ": path too long\n";
                        iov[2].iov_len = 16;
                        (void)writev(STDERR_FILENO, iov, 3);
                        continue;
                }
                bcopy(p, buf, lp);
                buf[lp] = '/';
                bcopy(name, buf + lp + 1, ln);
                buf[lp + ln + 1] = '\0';

retry:          (void)execve(bp, argv, environ);
                switch(errno) {
                case E2BIG:
                        goto done;
                case ELOOP:
                case ENAMETOOLONG:
                case ENOENT:
                        break;
                case ENOEXEC:
                        for (cnt = 0; argv[cnt]; ++cnt)
                                ;
                        memp = alloca((cnt + 2) * sizeof(char *));
                        if (memp == NULL)
                                goto done;
                        memp[0] = "sh";
                        memp[1] = bp;
                        bcopy(argv + 1, memp + 2, cnt * sizeof(char *));
                        (void)execve(_PATH_BSHELL, memp, environ);
                        goto done;
                case ENOMEM:
                        goto done;
                case ENOTDIR:
                        break;
                case ETXTBSY:
                        /*
                         * We used to retry here, but sh(1) doesn't.
                         */
                        goto done;
                case EACCES:
                        eacces = 1;
                        break;
                default:
                        goto done;
                }
        }
        if (eacces)
                errno = EACCES;
        else if (!errno)
                errno = ENOENT;
done:
        return (-1);
}
