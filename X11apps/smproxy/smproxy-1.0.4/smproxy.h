/******************************************************************************

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

Author:  Ralph Mor, X Consortium
******************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <X11/Xosdefs.h>
#include <X11/Xfuncs.h>

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/SM/SMlib.h>

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#include <stdio.h>
#undef _POSIX_SOURCE
#else
#include <stdio.h>
#endif
#include <stdlib.h>

#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
#include <limits.h>
#else
#define _POSIX_SOURCE
#include <limits.h>
#undef _POSIX_SOURCE
#endif
#endif /* X_NOT_POSIX */
#ifndef PATH_MAX
#include <sys/param.h>
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif
#endif /* PATH_MAX */


typedef struct WinInfo {
    Window window;
    SmcConn smc_conn;
    XtInputId input_id;
    char *client_id;
    char **wm_command;
    int wm_command_count;
    XClassHint class;
    char *wm_name;
    XTextProperty wm_client_machine;
    struct WinInfo *next;

    unsigned int tested_for_sm_client_id : 1;
    unsigned int has_save_yourself : 1;
    unsigned int waiting_for_update : 1;
    unsigned int got_first_save_yourself : 1;

} WinInfo;

typedef struct ProxyFileEntry
{
    struct ProxyFileEntry *next;
    int tag;
    char *client_id;
    XClassHint class;
    char *wm_name;
    int wm_command_count;
    char **wm_command;
} ProxyFileEntry;

extern int WriteProxyFileEntry ( FILE *proxyFile, WinInfo *theWindow );
extern int ReadProxyFileEntry ( FILE *proxyFile, ProxyFileEntry **pentry );
extern void ReadProxyFile ( char *filename );
extern char * WriteProxyFile ( void );
extern char * LookupClientID ( WinInfo *theWindow );

extern WinInfo *win_head;

#define SAVEFILE_VERSION 1
