/*
 * error message handling
 */
/*
Copyright 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 * Copyright 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices, or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES, DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL NETWORK COMPUTING DEVICES,
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#include	"xfs-config.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<X11/Xos.h>

#ifdef USE_SYSLOG
#include	<syslog.h>
#endif

#include	<errno.h>

#include	"misc.h"
#include	"globals.h"
#include	"osdep.h"

Bool        UseSyslog;
Bool        log_open = FALSE;
char        ErrorFile[PATH_MAX];
static char	CurrentErrorFile[PATH_MAX];

static void
abort_server(void)
{
    fflush(stderr);

#ifdef SABER
    saber_stop();
#else
    _exit(1);
#endif
}

void
InitErrors(void)
{
    int         i;

#ifdef USE_SYSLOG
    if (UseSyslog && !log_open) {
	openlog("xfs", LOG_PID, LOG_DAEMON);
	log_open = TRUE;
	return;
    }
#endif

    if (ErrorFile[0] &&
	(!log_open || (strcmp(CurrentErrorFile, ErrorFile) != 0)) ) {
	i = open(ErrorFile, O_WRONLY | O_APPEND | O_CREAT, 0666);
	if (i != -1) {
	    dup2(i, 2);
	    close(i);
	    log_open = TRUE;
	} else {
	    ErrorF("can't open error file \"%s\"\n", ErrorFile);
	}
	strncpy(CurrentErrorFile, ErrorFile, sizeof CurrentErrorFile);
    }
}

void
CloseErrors(void)
{
    int nullfd;

    if (!log_open)
	return;

    log_open = FALSE;

#ifdef USE_SYSLOG
    if (UseSyslog) {
	closelog();
	return;
    }
#endif

    close (2);
    nullfd = open ("/dev/null", O_RDWR);
    if (nullfd != 2) {
	dup2 (nullfd, 2);
	close(nullfd);
    }
}

void
Error(char *str)
{
#ifdef USE_SYSLOG
    if (UseSyslog) {
	syslog(LOG_ERR, "%s: %s", str, strerror(errno));
	return;
    }
#endif
    perror(str);
}

/*
 * used for informational messages
 */
void
NoticeF(char *f, ...)
{
    /* XXX should Notices just be ignored if not using syslog? */
    va_list args;
    va_start(args, f);
#ifdef USE_SYSLOG
    if (UseSyslog) {
	vsyslog(LOG_NOTICE, f, args);
	return;
    }
#else
    fprintf(stderr, "%s notice: ", progname);
    vfprintf(stderr, f, args);
#endif /* USE_SYSLOG */
    va_end(args);
}

/*
 * used for non-fatal error messages
 */
void
ErrorF(char * f, ...)
{
    va_list args;
    va_start(args, f);
#ifdef USE_SYSLOG
    if (UseSyslog) {
	vsyslog(LOG_WARNING, f, args);
	return;
    }
#else
    fprintf(stderr, "%s error: ", progname);
    vfprintf(stderr, f, args);
#endif
    va_end(args);
}

void
FatalError(char * f, ...)
{
    va_list args;
    va_start(args, f);
#ifdef USE_SYSLOG
    if (UseSyslog) {
	vsyslog(LOG_ERR, f, args);
	return;
    }
#else
    fprintf(stderr, "%s fatal error: ", progname);
    vfprintf(stderr, f, args);
#endif
    va_end(args);
    abort_server();
    /* NOTREACHED */
}
