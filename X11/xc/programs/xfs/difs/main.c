/* $Xorg: main.c,v 1.5 2001/02/09 02:05:42 xorgcvs Exp $ */
/*
 * Font server main routine
 */
/*
 
Copyright 1990, 1991, 1998  The Open Group

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

 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES,
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */
/* $XFree86: xc/programs/xfs/difs/main.c,v 3.12 2002/10/15 01:45:02 dawes Exp $ */

#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	"FS.h"
#include	"FSproto.h"
#include	"clientstr.h"
#include	"fsresource.h"
#include	"misc.h"
#include	"globals.h"
#include	"servermd.h"
#include	"cache.h"
#include	"site.h"
#include	"dispatch.h"
#include	"extentst.h"
#include	"difs.h"

char       *ConnectionInfo;
int         ConnInfoLen;

Cache       serverCache;

#ifndef DEFAULT_CONFIG_FILE
#define DEFAULT_CONFIG_FILE "/usr/lib/X11/fs/config"
#endif

#define	SERVER_CACHE_SIZE	10000	/* for random server cacheables */

static Bool create_connection_block(void);

char       *configfilename;
extern Bool drone_server;

extern OldListenRec *OldListen;
extern int 	     OldListenCount;

int
main(int argc, char *argv[])
{
    int         i, oldumask;

    argcGlobal = argc;
    argvGlobal = argv;

    configfilename = DEFAULT_CONFIG_FILE;

    /* init stuff */
    ProcessCmdLine(argc, argv);
    InitErrors();

    /*
     * Do this first thing, to get any options that only take effect at
     * startup time.  It is read again each time the server resets.
     */
    if (ReadConfigFile(configfilename) != FSSuccess) {
	FatalError("couldn't read config file\n");
    }

    /* make sure at least world write access is disabled */
    if (((oldumask = umask(022)) & 002) == 002)
	(void)umask(oldumask);

    SetDaemonState();
    SetUserId();

    while (1) {
	serverGeneration++;
	OsInit();
	if (serverGeneration == 1) {
	    /* do first time init */
	    serverCache = CacheInit(SERVER_CACHE_SIZE);
	    CreateSockets(OldListenCount, OldListen);
	    InitProcVectors();
	    clients = (ClientPtr *) fsalloc(MAXCLIENTS * sizeof(ClientPtr));
	    if (!clients)
		FatalError("couldn't create client array\n");
	    for (i = MINCLIENT; i < MAXCLIENTS; i++)
		clients[i] = NullClient;
	    /* make serverClient */
	    serverClient = (ClientPtr) fsalloc(sizeof(ClientRec));
	    if (!serverClient)
		FatalError("couldn't create server client\n");
	}
	ResetSockets();

	/* init per-cycle stuff */
	InitClient(serverClient, SERVER_CLIENT, (pointer) 0);

	clients[SERVER_CLIENT] = serverClient;
	currentMaxClients = MINCLIENT;
	currentClient = serverClient;

	if (!InitClientResources(serverClient))
	    FatalError("couldn't init server resources\n");

	InitExtensions();
	InitAtoms();
	InitFonts();
	SetConfigValues();
	if (!create_connection_block())
	    FatalError("couldn't create connection block\n");

#ifdef DEBUG
	fprintf(stderr, "Entering Dispatch loop\n");
#endif

	Dispatch();

#ifdef DEBUG
	fprintf(stderr, "Leaving Dispatch loop\n");
#endif

	/* clean up per-cycle stuff */
	CacheReset();
	CloseDownExtensions();
	if ((dispatchException & DE_TERMINATE) || drone_server)
	    break;
	fsfree(ConnectionInfo);
	/* note that we're parsing it again, for each time the server resets */
	if (ReadConfigFile(configfilename) != FSSuccess)
	    FatalError("couldn't read config file\n");
    }

    CloseSockets();
    CloseErrors();
    exit(0);
}

int
NotImplemented(void)
{
    NoopDDA();			/* dummy to get difsutils.o to link */
    /* Getting here can become the next xfs exploit... so don't exit */
    ErrorF("not implemented\n");

    return (FSBadImplementation);
}

static Bool
create_connection_block(void)
{
    fsConnSetupAccept setup;
    char       *pBuf;

    setup.release_number = VENDOR_RELEASE;
    setup.vendor_len = strlen(VENDOR_STRING);
    setup.max_request_len = MAX_REQUEST_SIZE;
    setup.length = (SIZEOF(fsConnSetupAccept) + setup.vendor_len + 3) >> 2;

    ConnInfoLen = SIZEOF(fsConnSetupAccept) + ((setup.vendor_len + 3) & ~3);
    ConnectionInfo = (char *) fsalloc(ConnInfoLen);
    if (!ConnectionInfo)
	return FALSE;

    memmove( ConnectionInfo, (char *) &setup, SIZEOF(fsConnSetupAccept));
    pBuf = ConnectionInfo + SIZEOF(fsConnSetupAccept);
    memmove( pBuf, VENDOR_STRING, (int) setup.vendor_len);

    return TRUE;
}
