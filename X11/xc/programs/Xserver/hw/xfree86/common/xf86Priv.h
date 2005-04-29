/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86Priv.h,v 3.84 2004/02/13 23:58:38 dawes Exp $ */

/*
 * Copyright (c) 1997-2002 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file contains declarations for private XFree86 functions and variables,
 * and definitions of private macros.
 *
 * "private" means not available to video drivers.
 */

#ifndef _XF86PRIV_H
#define _XF86PRIV_H

#include "xf86Privstr.h"
#include "propertyst.h"

/*
 * Parameters set ONLY from the command line options
 * The global state of these things is held in xf86InfoRec (when appropriate).
 */
extern const char *xf86ConfigFile;
extern Bool xf86AllowMouseOpenFail;
#ifdef XF86VIDMODE
extern Bool xf86VidModeDisabled;
extern Bool xf86VidModeAllowNonLocal; 
#endif 
#ifdef XF86MISC
extern Bool xf86MiscModInDevDisabled;
extern Bool xf86MiscModInDevAllowNonLocal;
#endif 
extern Bool xf86fpFlag;
extern Bool xf86coFlag;
extern Bool xf86sFlag;
extern Bool xf86bsEnableFlag;
extern Bool xf86bsDisableFlag;
extern Bool xf86silkenMouseDisableFlag;
extern char *xf86LayoutName;
extern char *xf86ScreenName;
extern char *xf86PointerName;
extern char *xf86KeyboardName;
#ifdef KEEPBPP
extern int xf86Bpp;
#endif
extern int xf86FbBpp;
extern int xf86Depth;
extern Pix24Flags xf86Pix24;
extern rgb xf86Weight;
extern Bool xf86FlipPixels;
extern Bool xf86BestRefresh;
extern Gamma xf86Gamma;
extern char *xf86ServerName;
extern Bool xf86ShowUnresolved;

/* Other parameters */

extern xf86InfoRec xf86Info;
extern const char *xf86InputDeviceList;
extern const char *xf86ModulePath;
extern MessageType xf86ModPathFrom;
extern const char *xf86LogFile;
extern MessageType xf86LogFileFrom;
extern Bool xf86LogFileWasOpened;
extern serverLayoutRec xf86ConfigLayout;
extern Pix24Flags xf86ConfigPix24;

extern unsigned short xf86MouseCflags[];
extern Bool xf86SupportedMouseTypes[];
extern int xf86NumMouseTypes;

#ifdef XFree86LOADER
extern DriverPtr *xf86DriverList;
extern ModuleInfoPtr *xf86ModuleInfoList;
extern int xf86NumModuleInfos;
#else
extern DriverPtr xf86DriverList[];
#endif
extern int xf86NumDrivers;
extern Bool xf86Resetting;
extern Bool xf86Initialising;
extern Bool xf86ProbeFailed;
extern int xf86NumScreens;
extern pciVideoPtr *xf86PciVideoInfo;
extern xf86CurrentAccessRec xf86CurrentAccess;
extern const char *xf86VisualNames[];
extern int xf86Verbose;                 /* verbosity level */
extern int xf86LogVerbose;		/* log file verbosity level */
extern Bool xf86ProbeOnly;
extern Bool xf86DoProbe;

extern RootWinPropPtr *xf86RegisteredPropertiesTable;

#ifndef DEFAULT_VERBOSE
#define DEFAULT_VERBOSE		0
#endif
#ifndef DEFAULT_LOG_VERBOSE
#define DEFAULT_LOG_VERBOSE	3
#endif
#ifndef DEFAULT_DPI
#define DEFAULT_DPI		75
#endif

#define DEFAULT_UNRESOLVED	TRUE
#define DEFAULT_BEST_REFRESH	FALSE

/* Function Prototypes */
#ifndef _NO_XF86_PROTOTYPES

/* xf86Beta.c */
extern void xf86CheckBeta(int extraDays, char *key);

/* xf86Bus.c */

void xf86BusProbe(void);
void xf86ChangeBusIndex(int oldIndex, int newIndex);
void xf86AccessInit(void);
void xf86AccessEnter(void);
void xf86AccessLeave(void);
void xf86EntityInit(void);
void xf86EntityEnter(void);
void xf86EntityLeave(void);
void xf86AccessLeaveState(void);

void xf86FindPrimaryDevice(void);
/* new RAC */
void xf86ResourceBrokerInit(void);
void xf86PostProbe(void);
void xf86ClearEntityListForScreen(int scrnIndex);
void xf86AddDevToEntity(int entityIndex, GDevPtr dev);
extern void xf86PostPreInit(void);
extern void xf86PostScreenInit(void);
extern memType getValidBIOSBase(PCITAG tag, int num);
extern int pciTestMultiDeviceCard(int bus, int dev, int func, PCITAG** pTag);

/* xf86Config.c */

Bool xf86PathIsAbsolute(const char *path);
Bool xf86PathIsSafe(const char *path);

/* xf86DefaultModes */

extern DisplayModeRec xf86DefaultModes [];

/* xf86DoScanPci.c */

void DoScanPci(int argc, char **argv, int i);

/* xf86DoProbe.c */
void DoProbeArgs(int argc, char **argv, int i);
void DoProbe(void);
void DoConfigure(void);

/* xf86Events.c */

void xf86PostKbdEvent(unsigned key);
void xf86PostMseEvent(DeviceIntPtr device, int buttons, int dx, int dy);
void xf86Wakeup(pointer blockData, int err, pointer pReadmask);
void xf86SigHandler(int signo);
#ifdef MEMDEBUG
void xf86SigMemDebug(int signo);
#endif
void xf86HandlePMEvents(int fd, pointer data);
extern int (*xf86PMGetEventFromOs)(int fd,pmEvent *events,int num);
extern pmWait (*xf86PMConfirmEventToOs)(int fd,pmEvent event);
void xf86GrabServerCallback(CallbackListPtr *, pointer, pointer);

/* xf86Helper.c */
void xf86LogInit(void);
void xf86CloseLog(void);

/* xf86Init.c */
Bool xf86LoadModules(char **list, pointer *optlist);
int xf86SetVerbosity(int verb);
int xf86SetLogVerbosity(int verb);

/* xf86Io.c */

void xf86KbdBell(int percent, DeviceIntPtr pKeyboard, pointer ctrl,
		 int unused);
void xf86KbdLeds(void);
void xf86UpdateKbdLeds(void);
void xf86KbdCtrl(DevicePtr pKeyboard, KeybdCtrl *ctrl); 
void xf86InitKBD(Bool init);  
int xf86KbdProc(DeviceIntPtr pKeyboard, int what);

/* xf86Kbd.c */ 

void xf86KbdGetMapping(KeySymsPtr pKeySyms, CARD8 *pModMap);

/* xf86Lock.c */

#ifdef USE_XF86_SERVERLOCK
void xf86UnlockServer(void);
#endif

/* xf86XKB.c */

void xf86InitXkb(void);

#endif /* _NO_XF86_PROTOTYPES */


#endif /* _XF86PRIV_H */
