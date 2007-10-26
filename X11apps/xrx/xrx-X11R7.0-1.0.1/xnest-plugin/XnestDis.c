/* $Xorg: XnestDis.c,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/
/* $XFree86$ */

#include "RxPlugin.h"
#include <sys/stat.h>

/***********************************************************************
 * Utility functions and global variable to manage display numbers
 ***********************************************************************/

/* maximum numbers of concurrent instances (per machine) */
#define MAX_PLUGINS 128

/* start from 80 to avoid possible conflict with multi-display X server
 * like SunRay,LTSP, etc.*/
#define XNEST_OFFSET 80

/* global allowing to know which display numbers are in use */
static char xnest_display_numbers[MAX_PLUGINS];

void
RxpInitXnestDisplayNumbers()
{
    memset(xnest_display_numbers, 0, sizeof(char) * MAX_PLUGINS);
}

static
Bool IsDisplayNumFree(int id)
{
    char        fnamebuf[256];
    struct stat sbuf;
    int         res;

    /* /tmp/.X%d-lock is more or less the official way... */    
    sprintf(fnamebuf, "/tmp/.X%d-lock", id);
    res = stat(fnamebuf, &sbuf);
    if (res == 0)
        return False;

    /* ... but then we have to test for the old stuff, too... ;-( */
    sprintf(fnamebuf, "/tmp/.X11-pipe/X%d", id);
    res = stat(fnamebuf, &sbuf);
    if (res == 0)
        return False;

    sprintf(fnamebuf, "/tmp/.X11-unix/X%d", id);
    res = stat(fnamebuf, &sbuf);
    if (res == 0)
        return False;
    
    return True;
}

/* function returning first display number available */
int
RxpXnestDisplayNumber()
{
    int i;
    for (i = 0; i < MAX_PLUGINS; i++)
	if ((xnest_display_numbers[i] == 0) &&
            IsDisplayNumFree(i + XNEST_OFFSET)) {
	    xnest_display_numbers[i] = 1;
	    return i + XNEST_OFFSET;
	}
    /* no more available */
    return -1;
}

/* function returning first display number available */
void
RxpFreeXnestDisplayNumber(int i)
{
    xnest_display_numbers[i - XNEST_OFFSET] = 0;
}

/* function returning display name with specified display number */
char *
RxpXnestDisplay(int display_number)
{
    static char name[1024];
    char *display_name, *dpy_name, *display_num, *screen_num;

    display_name = getenv("DISPLAY");
    if (display_name == NULL)
	return NULL;

    /* if of the form x11:display skip scheme part */
    if (strncmp(display_name, "x11:", 4) == 0)
	dpy_name = display_name + 4;
    else
	dpy_name = display_name;

    /* Check for RFC 2732 bracketed IPv6 numeric address */
    if (*dpy_name == '[') {
	while (*dpy_name && (*dpy_name != ']')) {
	    dpy_name++;
	}
    }
    
    /* display number is after next ":" character */
    display_num = strchr(dpy_name, ':');
    if (display_num == NULL)	/* invalid display specification */
	return NULL;

    /* copy display name up to the display number */
    strncpy(name, display_name, display_num - display_name);

    /* override display_number */
    sprintf(name + (display_num - display_name), ":%d", display_number);

    /* append screen number */
    screen_num = strchr(display_num, '.');
    if (screen_num != NULL)
	strcat(name, screen_num);

    return name;
}
    
