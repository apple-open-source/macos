/* $XConsortium: os2_select.c /main/6 1996/10/27 11:48:55 kaleb $ */




/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_select.c,v 3.8 2000/04/05 18:13:53 dawes Exp $ */

/*
 * (c) Copyright 1996 by Sebastien Marineau
 *			<marineau@genie.uottawa.ca>
 *     Modified 1999 by Holger.Veit@gmd.de 
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
void os2HighResTimerThread();
extern BOOL os2PopupErrorPending;

extern HEV hKbdSem;
extern HEV hMouseSem;
extern HEV hevServerHasFocus;
HEV hPipeSem;
HEV hHRTSem;
static HMUX hSelectWait;
Bool os2HRTimerFlag=FALSE;
SEMRECORD SelectMuxRecord[5];
HMODULE hmod_so32dll;

static int (*os2_tcp_select)(int *,int,int,int,long);
int os2_set_error(ULONG);
ULONG os2_get_sys_millis();
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

int i,j,n,ns,np;
int ready_handles;
ULONG timeout_ms;
BOOL any_ready;
ULONG semKey,postCount,start_millis,now_millis;
APIRET rc;
char faildata[16];
struct timeval dummy_timeout;
struct select_data sd;
static int HRT_Tid;

sd.have_read=FALSE; sd.have_write=FALSE; 
sd.socket_nread=0; sd.socket_nwrite=0; sd.socket_ntotal=0;
sd.max_fds=31; ready_handles=0; any_ready=FALSE;
sd.pipe_ntotal=0; sd.pipe_have_write=FALSE;

/* Stuff we have to do the first time this is called to set up various parameters */

if(FirstTime){
   /* First load the so32dll.dll module and get a pointer to the SELECT fn */

   if((rc=DosLoadModule(faildata,sizeof(faildata),"SO32DLL",&hmod_so32dll))!=0){
        FatalError("Could not load module so32dll.dll, rc = %d. Error note %s\n",rc,faildata);
        }
   if((rc = DosQueryProcAddr(hmod_so32dll, 0, "SELECT", (PPFN)&os2_tcp_select))!=0){
        FatalError("Could not query address of SELECT, rc = %d.\n",rc);
        }
   /* Call these a first time to set the semaphore */
   xf86OsMouseEvents();
   xf86KbdEvents();

   DosCreateEventSem(NULL, &hHRTSem,DC_SEM_SHARED,FALSE);
   DosResetEventSem(hHRTSem,&postCount);
   if (os2HRTimerFlag) {
	HRT_Tid = _beginthread(os2HighResTimerThread, NULL, 0x2000,(void *) NULL);
	xf86Msg(X_INFO,
		"Started high-resolution timer thread, TID=%d\n",HRT_Tid);
        }
  
   SelectMuxRecord[0].hsemCur = (HSEM)hMouseSem;
   SelectMuxRecord[0].ulUser = MOUSE_SEM_KEY;
   SelectMuxRecord[1].hsemCur = (HSEM)hKbdSem;
   SelectMuxRecord[1].ulUser = KBD_SEM_KEY;
   SelectMuxRecord[2].hsemCur = (HSEM)hPipeSem;
   SelectMuxRecord[2].ulUser = PIPE_SEM_KEY;
   SelectMuxRecord[3].hsemCur = (HSEM)hHRTSem;
   SelectMuxRecord[3].ulUser = HRT_SEM_KEY;
   rc = DosCreateMuxWaitSem(NULL, &hSelectWait, 4, SelectMuxRecord,
                DC_SEM_SHARED | DCMW_WAIT_ANY);
   if(rc){
        xf86Msg(X_ERROR,"Could not create MuxWait semaphore, rc=%d\n",rc);
        }
   FirstTime = FALSE;
}

/* Set up the time delay structs */

    if(timeout!=NULL) {
	timeout_ms=timeout->tv_sec*1000+timeout->tv_usec/1000;
	}
    else { timeout_ms=1000000; }  /* This should be large enough... */
    if(timeout_ms>0) start_millis=os2_get_sys_millis();

/* Zero our local fd_masks */
     {FD_ZERO(&sd.read_copy);}
     {FD_ZERO(&sd.write_copy);}

/* Copy the masks for later use */
	     if(readfds!=NULL){ XFD_COPYSET(readfds,&sd.read_copy); sd.have_read=TRUE;}
	     if(writefds!=NULL) {XFD_COPYSET(writefds,&sd.write_copy);sd.have_write=TRUE;}

/* And zero the original masks */
	     if(sd.have_read){ FD_ZERO(readfds);}
	     if(sd.have_write) {FD_ZERO(writefds);}
	     if(exceptfds != NULL) {FD_ZERO(exceptfds);}

/* Now we parse the fd_sets passed to select and separate pipe/sockets */
        n = os2_parse_select(&sd,nfds);

/* Now check if we have sockets ready! */

        if (sd.socket_ntotal > 0){
           ns = os2_poll_sockets(&sd,readfds,writefds);
           if(ns>0){
               ready_handles+=ns;
               any_ready = TRUE;
               }
           else if (ns == -1) {return(-1);}
           }
   
/* And pipes */

        if(sd.pipe_ntotal > 0){
           np = os2_check_pipes(&sd,readfds,writefds);
           if(np > 0){
              ready_handles+=np;
              any_ready = TRUE;
              }
           else if (np == -1) { return(-1); }
          }

/* ... */

/* And finally poll input devices */
         if(!os2MouseQueueQuery() || !os2KbdQueueQuery() ) any_ready = TRUE;

        while(!any_ready && timeout_ms){
                DosResetEventSem(hHRTSem,&postCount);
                rc = DosWaitMuxWaitSem(hSelectWait, 5, &semKey);
                if ((rc == 0) && (semKey != HRT_SEM_KEY) && (semKey != PIPE_SEM_KEY)){
                   any_ready = TRUE;
                   }
                if (os2PopupErrorPending) {
                        os2RecoverFromPopup();
                        any_ready=TRUE;
                        }
                if(xf86Info.vtRequestsPending) any_ready=TRUE;
                if (sd.socket_ntotal > 0){
                   ns = os2_poll_sockets(&sd,readfds,writefds,exceptfds);
                   if(ns>0){
                        ready_handles+=ns;
                        any_ready = TRUE;
                       }
                   else if (ns == -1) {return(-1);}
                  }

                rc = DosQueryEventSem(hPipeSem,&postCount);
                if(postCount && (sd.pipe_ntotal > 0)){
                        np = os2_check_pipes(&sd,readfds,writefds);
                        if(np > 0){
                        ready_handles+=np;
                        any_ready = TRUE;
                        }
                        else if (np == -1) { 
                                return(-1); }
                      }


                  if (i%8 == 0) { 
                    now_millis = os2_get_sys_millis();
                    if((now_millis-start_millis) > timeout_ms) timeout_ms = 0;
                    }
                  i++;
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


  if(nfds > sd->max_fds){
     for(i=0;i<((FD_SETSIZE+31)/32);i++){
        if(sd->read_copy.fds_bits[i] ||
            sd->write_copy.fds_bits[i])
                        sd->max_fds=(i*32) +32;
        }
     }
   else { sd->max_fds = nfds; }

/* Check if this is greater than specified in select() call */
  if(sd->max_fds > nfds) sd->max_fds = nfds;

  if (sd->have_read)
    {
      for (i = 0; i < sd->max_fds; ++i) {
        if (FD_ISSET (i, &sd->read_copy)){
         if(_files[i] & F_SOCKET)
           {
            sd->tcp_select_mask[sd->socket_ntotal]=_getsockhandle(i);
            sd->tcp_emx_handles[sd->socket_ntotal]=i;
            sd->socket_ntotal++; sd->socket_nread++;
           }
         else if (_files[i] & F_PIPE)
          {
            sd -> pipe_ntotal++;
          }
        }
      }
    }

  if (sd->have_write)
    {
      for (i = 0; i < sd->max_fds; ++i) {
        if (FD_ISSET (i, &sd->write_copy)){
         if(_files[i] & F_SOCKET)
         {
            sd->tcp_select_mask[sd->socket_ntotal]=_getsockhandle(i);
            sd->tcp_emx_handles[sd->socket_ntotal]=i;
            sd->socket_ntotal++; sd->socket_nwrite++;
         }
         else if (_files[i] & F_PIPE)
          {
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

        if(e == 0) return(e);
	/* We have something ready? */
        if(e>0){
            j = 0; n = 0;
            for (i = 0; i < sd->socket_nread; ++i, ++j)
                 if (sd->tcp_select_copy[j] != -1)
                    {
                    FD_SET (sd->tcp_emx_handles[j], readfds);
                    n ++;
                    }
             for (i = 0; i < sd->socket_nwrite; ++i, ++j)
                  if (sd->tcp_select_copy[j] != -1)
                     {
                     FD_SET (sd->tcp_emx_handles[j], writefds);
                     n ++;
                     }
               errno = 0;
               
               return n;
              }
        if(e<0){
           /*Error -- TODO */
           xf86Msg(X_ERROR,"Error in server select! e=%d\n",e);
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
           if((pipeSemState[i].fStatus == 1) &&
                    (FD_ISSET(pipeSemState[i].usKey,&sd->read_copy))){
                FD_SET(pipeSemState[i].usKey,readfds);
                e++;
                }
           else if((pipeSemState[i].fStatus == 2)  &&
                    (FD_ISSET(pipeSemState[i].usKey,&sd->write_copy))){
                FD_SET(pipeSemState[i].usKey,writefds);
                e++;
                }
            else if( (pipeSemState[i].fStatus == 3) &&
                ( (FD_ISSET(pipeSemState[i].usKey,&sd->read_copy)) ||
                  (FD_ISSET(pipeSemState[i].usKey,&sd->write_copy)) )){
                errno = EBADF;
                /* xf86Msg(X_ERROR,"Pipe has closed down, fd=%d\n",pipeSemState[i].usKey); */
                return (-1);
                }
            i++;
            } /* endwhile */

	errno = 0;
	return(e);
}


/* This thread runs with the high-res timer, timer0.sys */
/* The semaphore hHRTSem gets posted every HRT_DELAY ms */
/* Temptation may be strong to decrease this delay, but it already */
/* consumes proportionally quite a bit of cpu and 12 ms seems quite good*/

#define HRT_DELAY 12

void os2HighResTimerThread(void* arg)
{
	HFILE hTimer;
	ULONG ulDelay,ulAction,ulSize,ulPostCount;
	APIRET rc;
	char *fmt;

	ulDelay = HRT_DELAY;
	ulSize=sizeof(ulDelay);

	rc = DosOpen("TIMER0$",&hTimer,&ulAction,
		0,0,OPEN_ACTION_OPEN_IF_EXISTS,
		OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE, 
		NULL);
	if (rc) {
		fmt = "Open TIMER0.SYS failed, rc=%d. No High-resolution available\n";
		goto errexit2;
	}
	hTimer = _imphandle(hTimer);
	if (hTimer<0) {
		fmt = "Could not import handle from TIMER0.SYS, rc=%d.\n";
		goto errexit2;
	}

	/* Make the thread time critical */
	DosSetPriority(2L,3L,0L,0L);
	while (1) {
		rc = DosDevIOCtl(hTimer,0x80,5,
				 &ulDelay,ulSize,&ulSize,
				 NULL,0,NULL);
		if (rc != 0) {
			fmt = "Bad return code from timer0.sys, rc=%d\n";
			goto errexit1;
		}

		rc = DosQueryEventSem(hHRTSem,&ulPostCount);
		if (rc != 0) {
			fmt = "Bad return code from QueryEventSem, rc=%d\n";
			goto errexit1;
		}

		if (ulPostCount == 0) rc = DosPostEventSem(hHRTSem);
		if (rc != 0 && rc != 299) {
			fmt = "Bad return code from PostEventSem, rc=%d\n";
			goto errexit1;
		}

		rc = DosQueryEventSem(hevServerHasFocus,&ulPostCount);
		if (rc != 0) {
			fmt = "Bad return code from QueryEventSem for server focus, rc=%d\n";
			goto errexit1;
		}

		/* Disable the HRT timer thread while switched away. */
		if (ulPostCount == 0) 
			DosWaitEventSem(hevServerHasFocus, SEM_INDEFINITE_WAIT);

	}

	/* XXX reached? */
	DosClose(hTimer);
	return;

	/* error catch blocks */
errexit1:
	DosClose(hTimer);
errexit2:
	xf86Msg(X_ERROR,fmt,rc);
	DosExitList(0l,0l);
}

ULONG os2_get_sys_millis() 
{
	APIRET rc;
	ULONG milli;

	rc = DosQuerySysInfo(14, 14, &milli, sizeof(milli));
	if (rc) {
	        xf86Msg(X_ERROR,
			"Bad return code querying the millisecond counter! rc=%d\n",
			rc);
		return 0;
	}
	return milli;
}
