/* $Xorg: error.c,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xdm/error.c,v 1.6tsi Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * error.c
 *
 * Log display manager errors to a file as
 * we generally do not have a terminal to talk to
 */

# include <stdio.h>
# include <stdarg.h>

# include "dm.h"
# include "dm_error.h"

#define WRITES(fd, buf) write(fd, buf, strlen(buf))

void LogInfo(char * fmt, ...)
{
    char buf[1024];

    snprintf(buf, sizeof buf, "xdm info (pid %ld): ", (long)getpid());
    WRITES(STDERR_FILENO, buf);
    {
	va_list args;
	va_start(args, fmt);
	vsnprintf (buf, sizeof buf, fmt, args);
	va_end(args);
    }
    WRITES(STDERR_FILENO, buf);
}

void LogError (char * fmt, ...)
{
    char buf[1024];

    snprintf (buf, sizeof buf, "xdm error (pid %ld): ", (long)getpid());
    WRITES(STDERR_FILENO, buf);
    {
	va_list args;
	va_start(args, fmt);
	vsnprintf (buf, sizeof buf, fmt, args);
	va_end(args);
    }
    WRITES(STDERR_FILENO, buf);
}

void LogPanic (char * fmt, ...)
{
    char buf[1024];

    snprintf (buf, sizeof buf, "xdm panic (pid %ld): ", (long)getpid());
    WRITES(STDERR_FILENO, buf);
    {
	va_list args;
	va_start(args, fmt);
	vsnprintf (buf, sizeof buf, fmt, args);
	va_end(args);
    }
    WRITES(STDERR_FILENO, buf);
    _exit (1);
}

void LogOutOfMem (char * fmt, ...)
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

void Debug (char * fmt, ...)
{
    char buf[1024];

    if (debugLevel > 0)
    {
	va_list args;
	va_start(args, fmt);
	vsnprintf (buf, sizeof buf, fmt, args);
	va_end(args);
	WRITES(STDOUT_FILENO, buf);
    }
}

void InitErrorLog (void)
{
	int	i;
	if (errorLogFile[0]) {
		i = creat (errorLogFile, 0666);
		if (i != -1) {
			if (i != 2) {
				dup2 (i, 2);
				close (i);
			}
		} else
			LogError ("Cannot open errorLogFile %s\n", errorLogFile);
	}
}
