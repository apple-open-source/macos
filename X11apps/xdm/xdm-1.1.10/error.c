/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * error.c
 *
 * Log display manager errors to a file as
 * we generally do not have a terminal to talk to
 */

#include <stdio.h>
#include <stdarg.h>

#include "dm.h"
#include "dm_error.h"

/* This function does the actual log message writes. */
static inline void
LogVWrite(const char *fmt, va_list args)
{
    char buf[1024];
    int len;

    len = vsnprintf (buf, sizeof(buf), fmt, args);
    if (len >= sizeof(buf)) {
	len = sizeof(buf) - 1;
    }
    write(STDERR_FILENO, buf, len);
}

#define LogVarArgsWrite(fmt)	\
    do {			\
	va_list args;		\
	va_start(args, fmt);	\
	LogVWrite(fmt, args);	\
	va_end(args);		\
    } while(0)

#define LogHeader(type)		\
    LogAppend("xdm %s (pid %ld): ", type, (long)getpid())

/* Append more text to the log without a new header, right after
   having called LogInfo or LogError */
void
LogAppend(const char * fmt, ...)
{
    LogVarArgsWrite(fmt);
}

void
LogInfo(const char * fmt, ...)
{
    LogHeader("info");
    LogVarArgsWrite(fmt);
}

void
LogError (const char * fmt, ...)
{
    LogHeader("error");
    LogVarArgsWrite(fmt);
}

void
LogPanic (const char * fmt, ...)
{
    LogHeader("panic");
    LogVarArgsWrite(fmt);
    _exit (1);
}

void
LogOutOfMem (const char * fmt, ...)
{
    fprintf (stderr, "xdm: out of memory in routine ");
    {
	va_list args;
	va_start(args, fmt);
	vfprintf (stderr, fmt, args);
	va_end(args);
    }
    fflush (stderr);
}

void
Debug (const char * fmt, ...)
{
    char buf[1024];

    if (debugLevel > 0)
    {
	LogVarArgsWrite(fmt);
    }
}

void
InitErrorLog (void)
{
    int	i;
    if (errorLogFile[0]) {
	i = creat (errorLogFile, 0666);
	if (i != -1) {
	    if (i != STDERR_FILENO) {
		dup2 (i, STDERR_FILENO);
		close (i);
	    }
	} else
	    LogError ("Cannot open errorLogFile %s\n", errorLogFile);
    }
}
