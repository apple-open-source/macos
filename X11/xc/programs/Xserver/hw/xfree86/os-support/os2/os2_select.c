/* $XConsortium: os2_select.c /main/6 1996/10/27 11:48:55 kaleb $ */




/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_select.c,v 3.10 2004/02/14 00:10:18 dawes Exp $ */

/*
 * (c) Copyright 1996 by Sebastien Marineau
 *			<marineau@genie.uottawa.ca>
 *     Modified 1999 by Holger.Veit@gmd.de
 *     Modified 2004 by Frank Giessler
 *			<giessler@biomag.uni-jena.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * HOLGER VEIT  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Sebastien Marineau shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Sebastien Marineau.
 *
 */

/* os2_select.c: reimplementation of the xserver select(), optimized for speed */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <io.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <emx/io.h>

#define I_NEED_OS2_H
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROFILE
#define INCL_DOSPROCESS
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#define INCL_DOSMODULEMGR


#include "Xpoll.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

#include "os2_select.h"

int os2MouseQueueQuery();
int os2KbdQueueQuery();
void os2RecoverFromPopup();
void os2CheckPopupPending();
void os2SocketMonitorThread();
extern BOOL os2PopupErrorPending;

extern HEV hKbdSem;
extern HEV hMouseSem;
extern HEV hevServerHasFocus;
HEV hPipeSem;
HEV hSocketSem;
HEV hActivateSocketSem;
HEV hSwitchToSem;
static HMUX hSelectWait;
SEMRECORD SelectMuxRecord[5];
HMODULE hmod_so32dll;
static struct select_data sd;

static int (*os2_tcp_select)(int *,int,int,int,long);
static int (*os2_so_cancel)(int);
static int (*os2_sock_errno)();
int os2_set_error(ULONG);
extern int _files[];



/* This is a new implementation of select, for improved efficiency */
/* This function performs select() on sockets  */
/* but uses OS/2 internal fncts to check mouse */
/* and keyboard. S. Marineau, 27/4/96          */

/* This is still VERY messy */

/* A few note on optimizations: this select has been tuned for maximum
* performance, and thus has a different approach than a general-purpose
* select. It should not be used in another app without modifications. Further,
* it may need modifications if the Xserver os code is modified
* Assumptions: this is never called with anything in exceptfds. This is
* silently ignored. Further, if any pipes are specified in the write mask, it is
* because they have just been stuffed full by the xserver. There is not much
* in immediately returning with those bits set. Instead, we block on the
* semaphore for at least one tick, which will let the client at least start
* to flush the pipe. */

int os2PseudoSelect(nfds,readfds,writefds,exceptfds,timeout)
	int nfds;
	fd_set *readfds,*writefds,*exceptfds;
	struct timeval *timeout;
{

	static BOOL FirstTime=TRUE;

	int n,ns,np;
	int ready_handles;
	ULONG timeout_ms;
	BOOL any_ready;
	ULONG semKey,postCount;
	APIRET rc;
	char faildata[16];
	static int Socket_Tid;

	sd.have_read=FALSE; sd.have_write=FALSE;
	sd.socket_nread=0; sd.socket_nwrite=0; sd.socket_ntotal=0;
	sd.max_fds=31; ready_handles=0; any_ready=FALSE;
	sd.pipe_ntotal=0; sd.pipe_have_write=FALSE;

	/* Stuff we have to do the first time this is called to set up various parameters */

	if (FirstTime) {
		/* First load the so32dll.dll module and get a pointer to the SELECT fn */

		if ((rc=DosLoadModule(faildata,sizeof(faildata),"SO32DLL",&hmod_so32dll))!=0) {
			FatalError("Could not load module so32dll.dll, rc = %d. Error note %s\n",rc,faildata);
		}
		if ((rc = DosQueryProcAddr(hmod_so32dll, 0, "SELECT", (PPFN)&os2_tcp_select))!=0) {
			FatalError("Could not query address of SELECT, rc = %d.\n",rc);
		}
		if ((rc = DosQueryProcAddr(hmod_so32dll, 0, "SO_CANCEL", (PPFN)&os2_so_cancel))!=0) {
			FatalError("Could not query address of SO_CANCEL, rc = %d.\n",rc);
		}
		if ((rc = DosQueryProcAddr(hmod_so32dll, 0, "SOCK_ERRNO", (PPFN)&os2_sock_errno))!=0) {
			FatalError("Could not query address of SOCK_ERRNO, rc = %d.\n",rc);
		}

		/* Call these a first time to set the semaphore */
		xf86OsMouseEvents();
		xf86KbdEvents();

		DosCreateEventSem(NULL, &hSocketSem,DC_SEM_SHARED,FALSE);
		DosResetEventSem(hSocketSem,&postCount);

		DosCreateEventSem(NULL, &hActivateSocketSem, DC_SEM_SHARED, FALSE);
		DosResetEventSem(hActivateSocketSem, &postCount);

		DosCreateEventSem(NULL, &hSwitchToSem, DC_SEM_SHARED, FALSE);
		DosResetEventSem(hSwitchToSem, &postCount);

		Socket_Tid = _beginthread(os2SocketMonitorThread, NULL, 0x2000,(void *) NULL);
		xf86Msg(X_INFO,
			"Started Socket monitor thread, TID=%d\n",Socket_Tid);

		SelectMuxRecord[0].hsemCur = (HSEM)hMouseSem;
		SelectMuxRecord[0].ulUser = MOUSE_SEM_KEY;
		SelectMuxRecord[1].hsemCur = (HSEM)hKbdSem;
		SelectMuxRecord[1].ulUser = KBD_SEM_KEY;
		SelectMuxRecord[2].hsemCur = (HSEM)hPipeSem;
		SelectMuxRecord[2].ulUser = PIPE_SEM_KEY;
		SelectMuxRecord[3].hsemCur = (HSEM)hSocketSem;
		SelectMuxRecord[3].ulUser = SOCKET_SEM_KEY;
		SelectMuxRecord[4].hsemCur = (HSEM)hSwitchToSem;
		SelectMuxRecord[4].ulUser = SWITCHTO_SEM_KEY;
		
		rc = DosCreateMuxWaitSem(NULL, &hSelectWait, 5, SelectMuxRecord,
                DC_SEM_SHARED | DCMW_WAIT_ANY);
		if (rc) {
			xf86Msg(X_ERROR,"Could not create MuxWait semaphore, rc=%d\n",rc);
		}
		FirstTime = FALSE;
	}

	rc = DosResetEventSem(hActivateSocketSem, &postCount);
	/* Set up the time delay structs */

	if (timeout!=NULL) {
	timeout_ms=timeout->tv_sec*1000+timeout->tv_usec/1000;
	} else {
		timeout_ms=1000000;  /* This should be large enough... */
	}

	/* Zero our local fd_masks */
	{FD_ZERO(&sd.read_copy);}
	{FD_ZERO(&sd.write_copy);}

	/* Copy the masks for later use */
	if (readfds!=NULL) { XFD_COPYSET(readfds,&sd.read_copy); sd.have_read=TRUE; }
	if (writefds!=NULL) {XFD_COPYSET(writefds,&sd.write_copy); sd.have_write=TRUE; }

	/* And zero the original masks */
	if (sd.have_read){ FD_ZERO(readfds); }
	if (sd.have_write) {FD_ZERO(writefds); }
	if (exceptfds != NULL) {FD_ZERO(exceptfds); }

	/* Now we parse the fd_sets passed to select and separate pipe/sockets */
	n = os2_parse_select(&sd,nfds);

	/* Now check if we have sockets ready! */

	if (sd.socket_ntotal > 0) {
		ns = os2_poll_sockets(&sd,readfds,writefds);
		if (ns>0) {
			ready_handles+=ns;
			any_ready = TRUE;
		} else if (ns == -1) {
			return(-1);
		}
	}

	/* And pipes */

	if (sd.pipe_ntotal > 0) {
           np = os2_check_pipes(&sd,readfds,writefds);
		if (np > 0) {
			ready_handles+=np;
			any_ready = TRUE;
		} else if (np == -1) {
			return(-1);
		}
	}

	/* And finally poll input devices */
	if(!os2MouseQueueQuery() || !os2KbdQueueQuery() ) any_ready = TRUE;

	if (xf86Info.vtRequestsPending) any_ready=TRUE;

	if (os2PopupErrorPending)
		os2RecoverFromPopup();

	if (!any_ready && timeout_ms) {
		DosResetEventSem(hSocketSem,&postCount);

		/* Activate the socket thread */
		if (sd.socket_ntotal>0) {
			rc = DosPostEventSem(hActivateSocketSem);
		}

		rc = DosWaitMuxWaitSem(hSelectWait, timeout_ms, &semKey);

		/* If our socket monitor thread is still blocked in os2_tcp_select()
		 * we have to wake it up by calling os2_so_cancel().
		 * After that, call os2_tcp_select() once more to get rid of
		 * error SOCEINTR (10004)
		 */
		if (sd.socket_ntotal>0) {
			rc = DosQueryEventSem(hSocketSem, &postCount);

			if (postCount == 0) {		/* os2_select still blocked */
				int i,f,g;
				struct select_data *sd_ptr=&sd;

				if (sd.socket_nread > 0) {
					for (i=0; i<sd.socket_nread; i++) {
						f = g = sd_ptr->tcp_select_mask[i];
						os2_so_cancel(f);
						os2_tcp_select(&g, 1, 0, 0, 0);  /* get rid of error 10004 */
					}
				}
				if (sd.socket_nwrite > 0) {
					for (i=sd.socket_nread;
					     i<sd.socket_nread+sd.socket_nwrite;
					     i++) {
						f = g = sd_ptr->tcp_select_mask[i];
						os2_so_cancel(f);
						os2_tcp_select(&g, 0, 1, 0, 0); /* get rid of error 10004 */
					}
				}
			} else {		/* not blocked, something must be ready -> get it */
				ns = os2_poll_sockets(&sd,readfds,writefds);
				if (ns>0) {
					ready_handles+=ns;
				} else if (ns == -1) {
					return(-1);
				}
			}
		}
		if (sd.pipe_ntotal > 0) {
			rc = DosQueryEventSem(hPipeSem,&postCount);
			if (postCount > 0) {
				np = os2_check_pipes(&sd,readfds,writefds);
				if (np > 0) {
					ready_handles+=np;
				} else if (np == -1) {
					return(-1);
				}
			}
		}
	}
	/* The polling of sockets/pipe automatically set the proper bits */
	return (ready_handles);
}


int os2_parse_select(sd,nfds)
	struct select_data *sd;
	int nfds;
{
   int i;
	/* First we determine up to which descriptor we need to check.              */
	/* No need to check up to 256 if we don't have to (and usually we dont...)*/
	/* Note: stuff here is hardcoded for fd_sets which are int[8] as in EMX!!!    */

	if (nfds > sd->max_fds) {
		for (i=0;i<((FD_SETSIZE+31)/32);i++) {
			if (sd->read_copy.fds_bits[i] ||
			    sd->write_copy.fds_bits[i])
				sd->max_fds=(i*32) +32;
		}
	} else { sd->max_fds = nfds; }

	/* Check if this is greater than specified in select() call */
	if(sd->max_fds > nfds) sd->max_fds = nfds;

	if (sd->have_read) {
		for (i = 0; i < sd->max_fds; ++i) {
			if (FD_ISSET (i, &sd->read_copy)) {
				if(_files[i] & F_SOCKET) {
					sd->tcp_select_mask[sd->socket_ntotal]=_getsockhandle(i);
					sd->tcp_emx_handles[sd->socket_ntotal]=i;
					sd->socket_ntotal++; sd->socket_nread++;
				} else if (_files[i] & F_PIPE) {
					sd -> pipe_ntotal++;
				}
			}
		}
	}
	if (sd->have_write) {
		for (i = 0; i < sd->max_fds; ++i) {
			if (FD_ISSET (i, &sd->write_copy)) {
				if (_files[i] & F_SOCKET) {
					sd->tcp_select_mask[sd->socket_ntotal]=_getsockhandle(i);
					sd->tcp_emx_handles[sd->socket_ntotal]=i;
					sd->socket_ntotal++; sd->socket_nwrite++;
				} else if (_files[i] & F_PIPE) {
					sd -> pipe_ntotal++;
					sd -> pipe_have_write=TRUE;
				}
			}
		}
	}
	return(sd->socket_ntotal);
}


int os2_poll_sockets(sd,readfds,writefds)
	struct select_data *sd;
	fd_set *readfds,*writefds;
{
	int e,i;
	int j,n;

	memcpy(sd->tcp_select_copy,sd->tcp_select_mask,
	          sd->socket_ntotal*sizeof(int));

	e = os2_tcp_select(sd->tcp_select_copy,sd->socket_nread,
	                      sd->socket_nwrite, 0, 0);

	if (e == 0) return(e);

	/* We have something ready? */
	if (e>0) {
		j = 0; n = 0;
		for (i = 0; i < sd->socket_nread; ++i, ++j)
			if (sd->tcp_select_copy[j] != -1) {
				FD_SET (sd->tcp_emx_handles[j], readfds);
				n ++;
			}
		for (i = 0; i < sd->socket_nwrite; ++i, ++j)
			if (sd->tcp_select_copy[j] != -1) {
				FD_SET (sd->tcp_emx_handles[j], writefds);
				n ++;
			}
		errno = 0;

		return n;
	}
	if (e<0) {
		/*Error -- TODO */
		xf86Msg(X_ERROR,"Error in server select! sock_errno = %d\n",os2_sock_errno());
		errno = EBADF;
		return (-1);
	}
}

/* Check to see if anything is ready on pipes */

int os2_check_pipes(sd,readfds,writefds)
	struct select_data *sd;
	fd_set *readfds,*writefds;
{
	int i,e;
	ULONG ulPostCount;
	PIPESEMSTATE pipeSemState[128];
	APIRET rc;

	e = 0;
	rc = DosResetEventSem(hPipeSem,&ulPostCount);
	rc = DosQueryNPipeSemState((HSEM) hPipeSem, (PPIPESEMSTATE)&pipeSemState,
	                              sizeof(pipeSemState));
	if(rc) xf86Msg(X_ERROR,"SELECT: rc from QueryNPipeSem: %d\n",rc);
	i=0;
	while (pipeSemState[i].fStatus != 0) {
/*           xf86Msg(X_INFO,"SELECT: sem entry, stat=%d, flag=%d, key=%d,avail=%d\n",
                pipeSemState[i].fStatus,pipeSemState[i].fFlag,pipeSemState[i].usKey,
                pipeSemState[i].usAvail);  */
		if ((pipeSemState[i].fStatus == 1) &&
		    (FD_ISSET(pipeSemState[i].usKey,&sd->read_copy))) {
			FD_SET(pipeSemState[i].usKey,readfds);
			e++;
		} else if ((pipeSemState[i].fStatus == 2)  &&
		           (FD_ISSET(pipeSemState[i].usKey,&sd->write_copy))) {
			FD_SET(pipeSemState[i].usKey,writefds);
			e++;
		} else if ((pipeSemState[i].fStatus == 3) &&
		           ((FD_ISSET(pipeSemState[i].usKey,&sd->read_copy)) ||
		            (FD_ISSET(pipeSemState[i].usKey,&sd->write_copy)) )) {
			errno = EBADF;
			/* xf86Msg(X_ERROR,"Pipe has closed down, fd=%d\n",pipeSemState[i].usKey); */
			return (-1);
		}
		i++;
	} /* endwhile */

	errno = 0;
	return(e);
}


void os2SocketMonitorThread(void *arg)
{
	struct select_data *sd_ptr = &sd;
	ULONG ulPostCount;
	int e,rc;

	/* Make thread time critical */
	DosSetPriority(2L,3L,0L,0L);

	while (1) {
		rc = DosWaitEventSem(hActivateSocketSem, SEM_INDEFINITE_WAIT);
		if (rc != 0 )
			xf86Msg(X_ERROR,"Socket monitor: DosWaitEventSem(hActivateSocketSem..) returned %d\n",rc);

		rc = DosResetEventSem(hActivateSocketSem,&ulPostCount);
		if (rc != 0 )
			xf86Msg(X_ERROR,"Socket monitor: DosResetEventSem(&hActivateSocketSem..) returned %d\n",rc);

		/* fg300104:
		 * The next line shouldn't be here, but the DosPostEventSem()
		 * below will return 299 from time to time under heavy load
		 */
/*		DosResetEventSem(hSocketSem,&ulPostCount);*/

		memcpy(sd_ptr->tcp_select_monitor,sd_ptr->tcp_select_mask,
		        sd_ptr->socket_ntotal*sizeof(int));

		/* call os2_select(), return only if either something is ready or
		 * os2_so_cancel() was called
		 */
		e = os2_tcp_select(sd_ptr->tcp_select_monitor, sd_ptr->socket_nread,
		                    sd_ptr->socket_nwrite, 0, -1);

		if (e>0) {
			rc = DosPostEventSem(hSocketSem);
			if (rc != 0 )
				xf86Msg(X_ERROR,"Socket monitor: DosPostEventSem(hSocketSem..) returned %d\n",rc);
		} else if (e<0) {
			rc = os2_sock_errno();
			if (rc != 10004)
				xf86Msg(X_ERROR,"Socket monitor: os2_select: sock_errno = %d\n",rc);
		}

		rc = DosQueryEventSem(hevServerHasFocus, &ulPostCount);

		/* no need to rush while switched away */
		if ((rc==0) && (ulPostCount==0))
			rc == DosWaitEventSem(hevServerHasFocus,31L);
	}
}

