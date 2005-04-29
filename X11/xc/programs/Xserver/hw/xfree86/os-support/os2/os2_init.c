/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_init.c,v 3.19 2004/02/14 00:10:17 dawes Exp $ */
/*
 * (c) Copyright 1994 by Holger Veit
 *			<Holger.Veit@gmd.de>
 * Modified 1996 Sebastien Marineau <marineau@genie.uottawa.ca>
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
 * Except as contained in this notice, the name of Holger Veit shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Holger Veit.
 *
 */
/* $XConsortium: os2_init.c /main/9 1996/10/19 18:07:13 kaleb $ */

#define I_NEED_OS2_H
#define INCL_DOSFILEMGR
#define INCL_KBD
#define INCL_VIO
#define INCL_DOSMISC
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMODULEMGR
#define INCL_DOSFILEMGR
#include <float.h>
#include "X.h"
#include "Xmd.h"
#include "input.h"
#include "scrnintstr.h"

#include "compiler.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

VIOMODEINFO OriginalVideoMode;
void os2VideoNotify();
void os2HardErrorNotify();
void os2KbdMonitorThread();
void os2KbdBitBucketThread();
HEV hevPopupPending;
extern HEV hKbdSem;
extern BOOL os2HRTimerFlag;
static unsigned short cw;
extern void os2_checkinstallation(); /* os2_diag.c */

void xf86OpenConsole()
{
    /* try to catch problems before they become obvious */
    os2_checkinstallation();

    if (serverGeneration == 1) {
	HKBD fd;
	ULONG drive;
	ULONG dummy;
	KBDHWID hwid;
	APIRET rc;
	int VioTid;
        ULONG actual_handles;
        LONG new_handles;

	/* hv 250197 workaround for xkb-Problem: switch to X11ROOT drive */
	char *x11r = getenv("X11ROOT");
        /* Make sure X11ROOT is set before we go further sm280297 */
        if (x11r == NULL){ 
           xf86Msg(X_ERROR,
		   "Environment variable X11ROOT is not set! Aborting...\n");
           exit(1);  
           }
        if (_chdir2(x11r) < 0) {
  		xf86Msg(X_ERROR,"Cannot change to X11ROOT directory!\n");
	}

	xf86Msg(X_INFO,"Console opened\n");
	OriginalVideoMode.cb=sizeof(VIOMODEINFO);
	rc=VioGetMode(&OriginalVideoMode,(HVIO)0);
	if(rc!=0) 
		xf86Msg(X_ERROR,
			"Could not get original video mode. RC=%d\n",rc);
	xf86Info.consoleFd = -1;
        
        /* Set the number of handles to higher than the default 20. Set to 80 which should be plenty */
        new_handles = 0;
        rc = DosSetRelMaxFH(&new_handles,&actual_handles);
        if (actual_handles < 80) {
		new_handles = 80 - actual_handles;
		rc = DosSetRelMaxFH(&new_handles,&actual_handles);
		xf86Msg(X_INFO,"Increased number of available handles to %d\n",
			actual_handles);
	}

	/* grab the keyboard */
	rc = KbdGetFocus(0,0);
	if (rc != 0)
		FatalError("xf86OpenConsole: cannot grab kbd focus, rc=%d\n",rc);

	/* open the keyboard */
	rc = KbdOpen(&fd);
	if (rc != 0)
		FatalError("xf86OpenConsole: cannot open keyboard, rc=%d\n",rc);
	xf86Info.consoleFd = fd;

	xf86Msg(X_INFO,"Keyboard opened\n");

	/* assign logical keyboard */
	KbdFreeFocus(0);
	rc = KbdGetFocus(0,fd);
	if (rc != 0)
		FatalError("xf86OpenConsole: cannot set local kbd focus, rc=%d\n",rc);

/* Create kbd queue semaphore */
 
         rc = DosCreateEventSem(NULL,&hKbdSem,DC_SEM_SHARED,TRUE);
         if (rc != 0)
                  FatalError("xf86OpenConsole: cannot create keyboard queue semaphore, rc=%d\n",rc);
 
/* Create popup semaphore */

	rc = DosCreateEventSem("\\SEM32\\XF86PUP",&hevPopupPending,DC_SEM_SHARED,1);
	if (rc) 
		xf86Msg(X_ERROR,
			"Could not create popup semaphore! RC=%d\n",rc);
#if 0
	rc=VioRegister("xf86vio","XF86POPUP_SUBCLASS",0x20002004L,0L);
	if (rc) {
		FatalError("Could not register XF86VIO.DLL module. Please install in LIBPATH! RC=%d\n",
				rc);
	}
#endif

/* Start up the VIO monitor thread */
	VioTid=_beginthread(os2VideoNotify,NULL,0x4000,(void *)NULL);
	xf86Msg(X_INFO,"Started Vio thread, Tid=%d\n",VioTid);
	rc=DosSetPriority(2,3,0,VioTid);

/* Start up the hard-error VIO monitor thread */
	VioTid=_beginthread(os2HardErrorNotify,NULL,0x4000,(void *)NULL);
	xf86Msg(X_INFO,"Started hard error Vio mode monitor thread, Tid=%d\n",
		VioTid);
	rc=DosSetPriority(2,3,0,VioTid);

/* We have to set the codepage before the keyboard monitor is registered */
	rc = KbdSetCp(0,0,fd);
	if(rc != 0)
		FatalError("xf86OpenConsole: cannot set keyboard codepage, rc=%d\n",rc);

/* Start up the kbd monitor thread */
	VioTid=_beginthread(os2KbdMonitorThread,NULL,0x4000,(void *)NULL);
	xf86Msg(X_INFO,"Started Kbd monitor thread, Tid=%d\n",VioTid);
	rc=DosSetPriority(2,3,0,VioTid);

/* Disable hard-errors through DosError */
	rc = DosQuerySysInfo(5,5,&drive,sizeof(drive));
	rc = DosSuppressPopUps(0x0001L,drive+96);     /* Disable popups */
	
	hwid.cb = sizeof(hwid);	/* fix crash on P9000 */
	rc = KbdGetHWID(&hwid, fd);
	if (rc == 0) {
		switch (hwid.idKbd) {
		default:
		case 0xab54: /* 88/89 key */
		case 0:	/*unknown*/
		case 1: /*real AT 84 key*/
			xf86Info.kbdType = KB_84; break;
		case 0xab85: /* 122 key */
			FatalError("Unsupported extended 122key keyboard found!\n",0);
		case 0xab41: /* 101/102 key */
			xf86Info.kbdType = KB_101; break;
		}				
	} else
		xf86Info.kbdType = KB_84; /*defensive*/

/* Start up the Kbd bit-bucket thread. We don't want to leave the kbd events in the driver queue */
	VioTid=_beginthread(os2KbdBitBucketThread,NULL,0x2000,(void *)NULL);
	xf86Msg(X_INFO,"Started Kbd bit-bucket thread, Tid=%d\n",VioTid);
	
/* fg271103: set control word of FPU to default value to prevent SIGFPE in GLX (and elsewhere?) */

#define DEFAULT_X86_FPU 0x037f
	
	cw = _control87(DEFAULT_X86_FPU, 0xFFFF);
	xf86Msg(X_INFO,"Checking FPCW: %#x\n",cw);

	if (cw != DEFAULT_X86_FPU) {
		cw = _control87(0,0);
		xf86Msg(X_INFO,"Set FPCW to %#x\n",cw);
	}

    }
    return;
}

void xf86CloseConsole()
{
	APIRET rc;
	ULONG drive;

	if (xf86Info.consoleFd != -1) {
		KbdClose(xf86Info.consoleFd);
	}
	VioSetMode(&OriginalVideoMode,(HVIO)0);
	rc = DosQuerySysInfo(5,5,&drive,sizeof(drive));
	rc = DosSuppressPopUps(0x0000L,drive+96);    /* Reenable popups */
	rc = DosCloseEventSem(hevPopupPending);
	rc = VioDeRegister();
	return;
}

/* ARGSUSED */

int xf86ProcessArgument (argc, argv, i)
int argc;
char *argv[];
int i;
{
	return 0;
}

void xf86UseMsg()
{
	return;
}

