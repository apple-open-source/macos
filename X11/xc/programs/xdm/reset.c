/* $Xorg: reset.c,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xdm/reset.c,v 1.4 2001/12/14 20:01:23 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * pseudoReset -- pretend to reset the server by killing all clients
 * with windows.  It will reset the server most of the time, unless
 * a client remains connected with no windows.
 */

# include	"dm.h"
# include	"dm_error.h"

# include	<X11/Xlib.h>
# include	<signal.h>

/*ARGSUSED*/
static int
ignoreErrors (Display *dpy, XErrorEvent *event)
{
	Debug ("ignoring error\n");
	return 0;
}

/*
 * this is mostly bogus -- but quite useful.  I wish the protocol
 * had some way of enumerating and identifying clients, that way
 * this code wouldn't have to be this kludgy.
 */

static void
killWindows (Display *dpy, Window window)
{
	Window	root, parent, *children;
	int	child;
	unsigned int nchildren = 0;
	
	while (XQueryTree (dpy, window, &root, &parent, &children, &nchildren)
	       && nchildren > 0)
	{
		for (child = 0; child < nchildren; child++) {
			Debug ("XKillClient 0x%lx\n", (unsigned long)children[child]);
			XKillClient (dpy, children[child]);
		}
		XFree ((char *)children);
	}
}

static Jmp_buf	resetJmp;

/* ARGSUSED */
static SIGVAL
abortReset (int n)
{
	Longjmp (resetJmp, 1);
}

/*
 * this display connection better not have any windows...
 */
 
void
pseudoReset (Display *dpy)
{
	Window	root;
	int	screen;

	if (Setjmp (resetJmp)) {
		LogError ("pseudoReset timeout\n");
	} else {
		(void) Signal (SIGALRM, abortReset);
		(void) alarm (30);
		XSetErrorHandler (ignoreErrors);
		for (screen = 0; screen < ScreenCount (dpy); screen++) {
			Debug ("pseudoReset screen %d\n", screen);
			root = RootWindow (dpy, screen);
			killWindows (dpy, root);
		}
		Debug ("before XSync\n");
		XSync (dpy, False);
		(void) alarm (0);
	}
	Signal (SIGALRM, SIG_DFL);
	XSetErrorHandler ((XErrorHandler)0 );
	Debug ("pseudoReset done\n");
}
