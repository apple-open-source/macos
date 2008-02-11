/* $Xorg: os.h,v 1.5 2001/02/09 02:05:44 xorgcvs Exp $ */
/*
Copyright 1987, 1998  The Open Group

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
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices,
 * or Digital not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Network Computing Devices, or Digital
 * make no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES, OR DIGITAL BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $NCDXorg: @(#)os.h,v 4.2 1991/05/10 07:59:16 lemke Exp $
 *
 */
/* $XFree86: xc/programs/xfs/include/os.h,v 3.14 2002/10/15 01:45:03 dawes Exp $ */

#ifndef	_OS_H_
#define	_OS_H_

typedef struct _FontPathRec *FontPathPtr;
typedef struct _alt_server *AlternateServerPtr;
typedef struct _auth *AuthPtr;

#include <X11/fonts/FSproto.h>
#include "client.h"
#include "misc.h"

typedef pointer FID;

#define ALLOCATE_LOCAL_FALLBACK(_size) FSalloc((unsigned long)_size)
#define DEALLOCATE_LOCAL_FALLBACK(_ptr) FSfree((pointer)_ptr)

#include "X11/Xalloca.h"

#define	MAX_REQUEST_SIZE	8192

#ifdef SIGNALRETURNSINT
#define SIGVAL int
#else
#define SIGVAL void
#endif

#define	fsalloc(size)		FSalloc((unsigned long)size)
#define	fsrealloc(ptr, size)	FSrealloc((pointer)ptr, (unsigned long)size)
#define	fsfree(ptr)		FSfree((pointer)ptr)

extern int  ListenPort;
extern Bool UseSyslog;
extern Bool CloneSelf;
extern char ErrorFile[];
#ifdef FONTCACHE
#include <X11/extensions/fontcacheP.h>
extern FontCacheSettings cacheSettings;
#endif

struct _osComm;	/* FIXME: osCommPtr */

/* os/config.c */
extern	int	ReadConfigFile(char *filename);

/* os/connection.c */
extern	void	AttendClient(ClientPtr client);
extern	void	CheckConnections(void);
extern	void	CloseDownConnection(ClientPtr client);
extern	void	IgnoreClient(ClientPtr client);
extern	void	MakeNewConnections(void);
extern	void	ReapAnyOldClients(void);
extern	void	ResetSockets(void);
extern	void	CloseSockets(void);
extern	void	StopListening(void);

/* os/daemon.c */
extern	void	BecomeDaemon(void);

/* os/error.c */
extern void	Error(char *str);
extern void	InitErrors(void);
extern void	CloseErrors(void);
extern void	NoticeF(char *f, ...);
extern void	ErrorF(char * f, ...);
extern void	FatalError(char* f, ...);

/* os/io.c */
extern	Bool	InsertFakeRequest(ClientPtr client, char *data, int count);
extern	int	FlushClient(ClientPtr client, struct _osComm *oc, char *extraBuf, int extraCount, int padsize);
extern	int	ReadRequest(ClientPtr client);
extern	void	FlushAllOutput(void);
extern	void	FreeOsBuffers(struct _osComm *oc);
extern	void	ResetCurrentRequest(ClientPtr client);
extern	void	ResetOsBuffers(void);
extern	void	WriteToClient(ClientPtr client, int count, char *buf);
extern	void	WriteToClientUnpadded(ClientPtr client, int count, char *buf);

/* os/osglue.c */
extern int 	ListCatalogues(char *pattern, int patlen, int maxnames, char **catalogues, int *len);
extern int 	ValidateCatalogues(int *num, char *cats);
extern int 	SetAlternateServers(char *list);
extern int 	ListAlternateServers(AlternateServerPtr *svrs);
extern int 	CloneMyself(void);

/* os/osinit.c */
extern	void	OsInit(void);

/* os/utils.c */
extern	SIGVAL	AutoResetServer (int n);
extern	SIGVAL	CleanupChild (int n);
extern	SIGVAL	GiveUp (int n);
extern	SIGVAL	ServerCacheFlush (int n);
extern	SIGVAL	ServerReconfig (int n);
extern	long	GetTimeInMillis (void);
extern	pointer	FSalloc(unsigned long);
extern	pointer	FScalloc (unsigned long amount);
extern	pointer	FSrealloc(pointer, unsigned long);
extern	void	FSfree(pointer);
extern	void	OsInitAllocator (void);
extern	void	ProcessCmdLine (int argc, char **argv);
extern	void	ProcessLSoption (char *str);
extern	void	SetUserId(void);
extern	void	SetDaemonState(void);

/* os/waitfor.c */
extern	int	WaitForSomething(int *pClientsReady);

extern void	SetConfigValues(void);


#endif				/* _OS_H_ */
