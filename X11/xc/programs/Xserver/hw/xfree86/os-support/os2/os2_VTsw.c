/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_VTsw.c,v 3.11 2002/05/31 18:46:01 dawes Exp $ */
/*
 * Copyright 1993 by David Wexelblat <dwex@goblin.org>
 * Modified 1996 by Sebastien Marineau <marineau@genie.uottawa.ca>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of David Wexelblat not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  David Wexelblat makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * DAVID WEXELBLAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL DAVID WEXELBLAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XConsortium: os2_VTsw.c /main/7 1996/05/13 16:37:55 kaleb $ */

#define I_NEED_OS2_H
#define NEED_EVENTS
#include "X.h"
#include "input.h"
#include "scrnintstr.h"

#define INCL_WINSWITCHLIST
#define INCL_VIO
#define INCL_KBD
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#undef RT_FONT
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"
#include "atKeynames.h"

BOOL SwitchedToWPS=FALSE;
BOOL WaitingForAccess=FALSE;
void os2PostKbdEvent();
HEV hevServerHasFocus;
HEV hevSwitchRequested;
HEV hevErrorPopupDetected;
extern HEV hevPopupPending;
BOOL os2PopupErrorPending=FALSE;

/*
 * Added OS/2 code to handle switching back to WPS
 */


Bool xf86VTSwitchPending()
{
	return(xf86Info.vtRequestsPending ? TRUE : FALSE);
}

Bool xf86VTSwitchAway()
{
        APIRET rc;
	ULONG  drive;

        xf86Info.vtRequestsPending=FALSE;
        SwitchedToWPS=TRUE;

	rc = DosQuerySysInfo(5,5,&drive,sizeof(drive));
	rc = DosSuppressPopUps(0x0000L,drive+96);	/* Disable popups */
	DosPostEventSem(hevSwitchRequested);
	usleep(30000);
	return(TRUE);
}

Bool xf86VTSwitchTo()
{
	APIRET rc;
	ULONG drive;

        xf86Info.vtRequestsPending=FALSE;
        SwitchedToWPS=FALSE;
	DosPostEventSem(hevSwitchRequested);
	rc = DosQuerySysInfo(5,5,&drive,sizeof(drive));
	rc = DosSuppressPopUps(0x0001L,drive+96);     /* Disable popups */
	/* We reset the state of the control key */
	os2PostKbdEvent(KEY_LCtrl,1);
	os2PostKbdEvent(KEY_LCtrl,0);
	return(TRUE);
}


/* This function is run as a thread and will notify of switch-to/switch-away events */
void os2VideoNotify(arg)
void * arg;
{
   USHORT Indic;
   USHORT NotifyType;
   APIRET rc;
   ULONG postCount;
   Bool FirstTime=TRUE;
   int timeout_count;

	rc=DosCreateEventSem(NULL,&hevServerHasFocus,0L,FALSE);
	rc=DosPostEventSem(hevServerHasFocus);
	rc=DosCreateEventSem(NULL,&hevSwitchRequested,0L,FALSE);
	rc=DosPostEventSem(hevSwitchRequested);


	while(1) {
	  Indic=0;
	  rc=VioSavRedrawWait(Indic,&NotifyType,(HVIO)0);

/* Here we handle the semaphore used to indicate wether we have screen access */
	  if(NotifyType==0) rc=DosResetEventSem(hevServerHasFocus,&postCount);
          if(FirstTime){
                   FirstTime=FALSE;
                   if(NotifyType==1) NotifyType=65535; /* In case a redraw is requested on first call */
                }
  
/* Sanity check */
	  if(NotifyType==1){
		if (!SwitchedToWPS) {
			xf86Msg(X_ERROR,
				"Abnormal switching back to server detected\n");
		}
	  }

/* Here we set the semaphore used to indicate switching request */

        if((NotifyType!=65535)&&(!WaitingForAccess)) {
                rc=DosResetEventSem(hevSwitchRequested,&postCount);
                xf86Info.vtRequestsPending=TRUE;
/* Then wait for semaphore to be posted once switch is complete. Wait 20 secs, then kill server */
               timeout_count=0;
               rc=DosSetPriority(2,3,0,1);
                     do {
                            rc=DosWaitEventSem(hevSwitchRequested,1000L);
                            if(rc==ERROR_TIMEOUT){
                               timeout_count++;
                               if(timeout_count>25){
                                  xf86Msg(X_ERROR,
					  "Server timeout on VT switch request. Server was killed\n");
                                  DosExit(1L,0);
                                  }
                               if(WaitingForAccess) {  /* The server is resetting */
                                  DosPostEventSem(hevSwitchRequested);
                                  xf86Info.vtRequestsPending=FALSE;
                                  }
                               }
                          } while (rc==ERROR_TIMEOUT);
          rc=DosSetPriority(2,2,0,1);
          }
         if(NotifyType==1) rc=DosPostEventSem(hevServerHasFocus);
         if((NotifyType==0)&&(!SwitchedToWPS))
		xf86Msg(X_ERROR,
			"Abnormal switching away from server!\n");
	} /* endwhile */

/* End of thread */
}

/* This function is run as a thread and will notify of hard-error events */
void os2HardErrorNotify(arg)
void * arg;
{
   USHORT Indic;
   USHORT NotifyType;
   APIRET rc;
   ULONG postCount;

	rc=DosCreateEventSem(NULL,&hevErrorPopupDetected,0L,FALSE);
	rc=DosPostEventSem(hevErrorPopupDetected);
        os2PopupErrorPending=FALSE;

	while(1) {
	   Indic=0;
	   rc=VioModeWait(Indic,&NotifyType,(HVIO)0);
	   if(NotifyType==0){
                os2PopupErrorPending=TRUE;
                rc=DosResetEventSem(hevErrorPopupDetected,&postCount);
                rc=DosWaitEventSem(hevErrorPopupDetected,20000L);
                if(rc==ERROR_TIMEOUT) GiveUp(0);  /* Shutdown on timeout of semaphore */
	   }
	} /* endwhile */

/* End of thread */
}

static BOOL is_redirected = FALSE;

static void 
redirect_output(void) 
{
	/* hv300996 create redirect file on boot drive, instead 
	 * anywhere you are just standing
	 */
	char buf[20],dr[3];
	ULONG drive;
	APIRET rc;

	if (is_redirected) return;

	if ((rc = DosQuerySysInfo(5,5,&drive,sizeof(drive))) != 0)
		dr[0] = 0;
	else {	
		dr[0] = drive+96;
		dr[1] = ':';
		dr[2] = 0;
	}
	sprintf(buf,"%s\\xf86log.os2",dr);

	ErrorF("\nThis is the XFree86/OS2-4.0 server\n");
	ErrorF("\nAll output from now on will be redirected to %s\n",buf);
	freopen(buf,"w",stderr); 

	is_redirected = TRUE;
}

void 
os2ServerVideoAccess()
{
   APIRET rc;
   ULONG fgSession;
   ULONG length=4;
   CHAR Status;

   /* Redirect output as early as possible */
   redirect_output();
  
/* Wait for screen access. This is called at server reset or at server startup */
/* Here we do some waiting until this session comes in the foreground before *
 * going any further. This is because we may have been started in the bg      */

        if(serverGeneration==1){
                rc=VioScrLock(0, &Status, (HVIO)0);
                while(Status != 0){
                        rc=VioScrLock(0, &Status, (HVIO)0);
                        DosSleep(1000);
                        }
                VioScrUnLock((HVIO)0);
                return;
                }
        WaitingForAccess=TRUE;
        rc=DosWaitEventSem(hevServerHasFocus,SEM_INDEFINITE_WAIT);
        WaitingForAccess=FALSE;
        SwitchedToWPS=FALSE;  /* In case server has reset while we were switched to WPS */
}

/* This next function will attempt to recover from a hard error popup
 * with an EnterLeave call
 */

void os2RecoverFromPopup()
{
	int j;
	if (os2PopupErrorPending) {
#if 0
		for (j = 0; j < screenInfo.numScreens; j++)
			(XF86SCRNINFO(screenInfo.screens[j])->EnterLeaveVT)(LEAVE, j);
		for (j = 0; j < screenInfo.numScreens; j++)
			(XF86SCRNINFO(screenInfo.screens[j])->EnterLeaveVT)(ENTER, j);
#endif
		/* Turn screen saver off when switching back */
		SaveScreens(SCREEN_SAVER_FORCER,ScreenSaverReset);
		os2PopupErrorPending=FALSE;
		DosPostEventSem(hevErrorPopupDetected);
	}
}

/* This checks wether a popup event is waiting. The semaphore would be reset
 * by the XF86VIO.DLL function
 */

void os2CheckPopupPending()
{
	int j;
	ULONG postCount;

	return;  /* For now this is a no-op */

#if 0
	DosQueryEventSem(hevPopupPending,&postCount);
	if (postCount==0) {               /* We have a popup pending */
#if 0
		for (j = 0; j < screenInfo.numScreens; j++)
			(XF86SCRNINFO(screenInfo.screens[j])->EnterLeaveVT)(LEAVE, j);
#endif
		DosPostEventSem(hevPopupPending);
	}
#endif
}

