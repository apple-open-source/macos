/*                                                                     */
/*           ** This is part of the FreeType/2 project! **             */
/* A small utility to display online memory usage stats for FreeType/2 */
/*                                                                     */
/* The method used to keep the window float on top may be completely   */
/* stupid but I found no other way (except putting WinSetWindowPos()   */
/* in the timer code which looks odd).                                 */
/*                                                                     */
/*                Copyright (C) 1998 by M. Necasek                     */

#define INCL_DOSMISC
#define INCL_WINTIMER
#define INCL_PM
#include <os2.h>
#include <stdio.h>

#define ID_TIMER  42
#define IDM_FLOAT 155


/* name of shared memory used for memory usage reporting  */
#define MEM_NAME  "\\sharemem\\freetype"

typedef struct _INFOSTRUCT {
  ULONG  signature;      /* signature (0x46524545, 'FREE') */
  ULONG  used;           /* bytes actually used */
  ULONG  maxused;        /* maximum amount ever used */
  ULONG  num_err;        /* number of (de)allocation errors */
} INFOSTRUCT, *PINFOSTRUCT;

/* structure (in named shared memory) pointing to the above struct */
typedef struct _INFOPTR {
  PINFOSTRUCT  address;        /* pointer to actual memory info  */
} INFOPTR, *PINFOPTR;

HAB          hab;
HWND         hwndFrame;
PINFOSTRUCT  meminfo;
PINFOPTR     memptr;
ULONG        bFloat = TRUE;
HWND         hwndSysSubmenu;

VOID AddFloat(HWND hwndFrame) {
   MENUITEM  mi;
   HWND      hwndSysMenu;
   SHORT     sMenuID;

   /* add Float option to system menu */
   hwndSysMenu = WinWindowFromID(hwndFrame, FID_SYSMENU);
   sMenuID = (SHORT)WinSendMsg(hwndSysMenu, MM_ITEMIDFROMPOSITION,
                               MPFROMSHORT(0), MPVOID);
   WinSendMsg(hwndSysMenu, MM_QUERYITEM, MPFROMSHORT(sMenuID),
              MPFROMP(&mi));
   hwndSysSubmenu = mi.hwndSubMenu;
   mi.iPosition = MIT_END;
   mi.afStyle = MIS_SEPARATOR;
   mi.afAttribute = 0;
   mi.id = -1;
   mi.hwndSubMenu = 0;
   mi.hItem = 0;
   WinSendMsg(hwndSysSubmenu, MM_INSERTITEM, MPFROMP (&mi), NULL);
   mi.afStyle = MIS_TEXT;
   mi.afAttribute = MIA_CHECKED;
   mi.id = IDM_FLOAT;
   WinSendMsg(hwndSysSubmenu, MM_INSERTITEM, MPFROMP (&mi), "~Float on top");
}

MRESULT EXPENTRY ClientWndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   HPS      hps;
   RECTL    rcl;
   static ULONG    i = 1;
   ULONG    ulMem;
   char     szBuf[200];

   switch (msg) {
      case WM_CREATE:
         /* use smaller text */
         WinSetPresParam(hwnd, PP_FONTNAMESIZE, 7, (PVOID)"8.Helv");
         /* start the timer (ticks each 0.5 sec.) */
         AddFloat(WinQueryWindow(hwnd, QW_PARENT));
         WinStartTimer(hab, hwnd, ID_TIMER, 500);
         break;

      /* make window always stay on top (if desired) */
      case WM_VRNENABLED:
         if (bFloat)
            WinSetWindowPos(hwndFrame, HWND_TOP, 0, 0, 0, 0, SWP_ZORDER);
         break;

      case WM_COMMAND:  /* why doesn't WM_SYSCOMMAND work? */
         if (LOUSHORT(mp1) == IDM_FLOAT) {
            bFloat = !bFloat;
            WinCheckMenuItem(hwndSysSubmenu, IDM_FLOAT, bFloat);
         }
         break;

      case WM_TIMER:
         if (++i > 13)
            i = 1;
         WinInvalidateRect(hwnd, NULL, FALSE);
         return FALSE;

      case WM_PAINT:
         hps = WinBeginPaint(hwnd, NULLHANDLE, &rcl);
         /* necessary to avoid incorrectly repainting window */
         WinQueryWindowRect(hwnd, &rcl);

/*         sprintf(szBuf, " Current use %dK  Maximum ever used %dK  Errors %d",
                 meminfo->used / 1024,
                 meminfo->maxused / 1024, meminfo->num_err);*/
         sprintf(szBuf, " Current use %dB  Maximum ever used %dK  Errors %d",
                 meminfo->used,
                 meminfo->maxused / 1024, meminfo->num_err);
         WinDrawText(hps, -1, szBuf, &rcl, CLR_BLACK, CLR_WHITE,
                     DT_CENTER | DT_VCENTER | DT_ERASERECT);

         WinEndPaint(hps);
         break;
   }

   return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

void main (void)
{
   QMSG  qmsg;
   HMQ   hmq;
   HWND  hwndClient;
   ULONG flFrameFlags;

   WinInitialize(0);
   hmq = WinCreateMsgQueue(hab, 0);

   /* get access to shared memory */
   DosGetNamedSharedMem((PVOID*)&memptr, MEM_NAME, PAG_READ);
   if (!memptr)
      WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                    "  FreeType/2 is not running!",
                    "Error", 0, MB_OK | MB_ERROR);

   else {
      meminfo = memptr->address;
      if (meminfo->signature != 0x46524545)
         WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                       "  FreeType/2 is not running!",
                       "Error", 0, MB_OK | MB_ERROR);
      else {
         flFrameFlags = FCF_TITLEBAR      | FCF_SYSMENU  |
                        FCF_TASKLIST ;

         WinRegisterClass(hab, "MyClass",
                          (PFNWP) ClientWndProc,
                          CS_SIZEREDRAW, 0);

         hwndFrame = WinCreateStdWindow(HWND_DESKTOP,
                                        WS_VISIBLE,
                                        &flFrameFlags,
                                        "MyClass", "FreeType/2 Heap Usage",
                                        0, (HMODULE) NULL,
                                        0, &hwndClient);

         WinSetVisibleRegionNotify(hwndClient, TRUE);

         /* make titlebar text look better */
         WinSetPresParam(WinWindowFromID(hwndFrame, FID_TITLEBAR),
                         PP_FONTNAMESIZE, 9, (PVOID)"8.Helv");

         WinSetWindowPos(hwndFrame, NULLHANDLE, 0, 0, 350, 42,
                         SWP_MOVE | SWP_SIZE | SWP_SHOW);

         while (WinGetMsg(hab, &qmsg, (HWND) NULL, 0, 0))
            WinDispatchMsg(hab, &qmsg);

         WinSetVisibleRegionNotify(hwndClient, FALSE);
      }
   }
   /* free shared memory block */
   DosFreeMem(memptr);

   WinDestroyWindow(hwndFrame);
   WinDestroyMsgQueue(hmq);
   WinTerminate(hab);
}
