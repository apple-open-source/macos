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
 */

#include <X11/Xos.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "dm.h"
#include "dm_error.h"

/* detach */
void
BecomeDaemon (void)
{

    /* If our C library has the daemon() function, just use it. */
#ifdef HAVE_DAEMON
    if (daemon (0, 0) < 0) {
       /* error */
       LogError("daemon() failed, %s\n", _SysErrorMsg (errno));
       exit(1);
    }
#else
    switch (fork()) {
    case -1:
       /* error */
       LogError("daemon fork failed, %s\n", _SysErrorMsg (errno));
       exit(1);
       break;
    case 0:
       /* child */
       break;
    default:
       /* parent */
       exit(0);
    }

    if (setsid() == -1) {
       LogError("setting session id for daemon failed: %s\n",
                  _SysErrorMsg (errno));
       exit(1);
    }

    chdir("/");

    close (0);
    close (1);
    close (2);


    /*
     * Set up the standard file descriptors.
     */
    (void) open ("/dev/null", O_RDWR);
    (void) dup2 (0, 1);
    (void) dup2 (0, 2);
#endif /* HAVE_DAEMON */
}
