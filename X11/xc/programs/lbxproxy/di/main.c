/* $Xorg: main.c,v 1.5 2001/02/09 02:05:31 xorgcvs Exp $ */
/*

Copyright 1998  The Open Group

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

*/
/*
 * Copyright 1992 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of NCD. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCD. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NCD.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/lbxproxy/di/main.c,v 1.9 2001/12/14 20:00:51 dawes Exp $ */

#include <stdlib.h>
#include "lbx.h"
#include "wire.h"
#include "atomcache.h"
#include "colormap.h"
#include "tags.h"
#include "lbxext.h"
#include "os.h"
#include "resource.h"
#include "pm.h"
#include "misc.h"

int LbxWhoAmI = 0;		/*
				 * for lbx zlib library to know who we are
				 * server = 1
				 * proxy = 0
				 */

#define DEFAULT_MAXSERVERS 20

XServerPtr  	*servers;

char 		*display;

int		lbxMaxServers = DEFAULT_MAXSERVERS;

static void InitializeGlobals ();

int
main (argc, argv)
    int	    argc;
    char    **argv;
{
    display = "63";

    ProcessCommandLine (argc, argv);

    InitConnectionLimits();

    AdjustProcVector();

    InitializeGlobals ();

    proxyMngr = CheckForProxyManager ();
    CreateWellKnownSockets ();
    if (proxyMngr)
    {
	ConnectToProxyManager ();
	ListenToProxyManager ();
    }

    while (1)
    {
	OsInit ();
	InitColors ();
        InitDeleteFuncs();

	if (!proxyMngr && !ConnectToServer (display_name))
	{
	    /*
	     * If no display name was given on the command line, the
	     * DISPLAY variable is used.
	     */
	    if(!display_name)
		display_name = getenv("DISPLAY");

	    if(display_name) {
		FatalError("could not connect to display '%s'", display_name);
	    } else {
		FatalError("no display specified");
	    }
	}

        if (!InitClientResources(clients[0]))
            FatalError("couldn't init server resources");
	FinishInitClientResources(clients[0], 0, 0x3ffff);

	if (Dispatch () != 0)
	    break;

        FreeAllResources();
        FreeAtoms();
        FreeColors();
        FreeTags();
    }
    CloseServerSockets();
    exit (0);
}

/*
 * Initalize those global variables that are needed to connect
 * to a display server.
 */
static void
InitializeGlobals ()
{
    int		i;
    ClientPtr	tmp;

    clients = (ClientPtr *)xalloc(MAXCLIENTS * sizeof(ClientPtr));
	if (!clients)
	    FatalError("couldn't create client array");
    for (i=1; i < MAXCLIENTS; i++) 
	clients[i] = NullClient;

    servers = (XServerPtr *)xalloc(lbxMaxServers * sizeof (XServerPtr));
	if (!servers)
	    FatalError("couldn't create servers array");
    for (i=0; i < lbxMaxServers; i++)
	servers[i] = (XServerPtr) NULL;

    tmp = (ClientPtr)xcalloc(sizeof(ClientRec));
    if (!tmp)
	FatalError("couldn't create server client");

    tmp->sequence = 0;
    tmp->closeDownMode = RetainPermanent;
    tmp->clientGone = FALSE;
    tmp->server = servers[0];
    tmp->index = 0;
    tmp->noClientException = Success;
    tmp->awaitingSetup = FALSE;
    tmp->swapped = FALSE;
    tmp->big_requests = FALSE;

    clients[0] = tmp;

    currentMaxClients = 1;
}


/* ARGSUSED */
void
MarkClientException (client)
    ClientPtr	client;
{
}
