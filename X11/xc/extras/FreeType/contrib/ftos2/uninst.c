/*                                                                  */
/*                  UNINST - FreeType/2 uninstaller                 */
/*                                                                  */
/*          Copyright 1998 Michal Necasek <mike@mendelu.cz>         */
/*                                                                  */
/* UNINSTALL.CMD rewritten in C - apparently the REXX version fails */
/* when OS/2 is booted to command line. This causes problems if     */
/* FreeType/2 prevents PM from starting successfully.               */
/*                                                                  */
/* Note: This can be compiled as 16-bit app to keep the size down.  */

#define  INCL_NOXLATE_WIN16
#define  INCL_NOXLATE_DOS16
#define  INCL_WINSHELLDATA
#define  INCL_DOSMISC
#include <os2.h>

#include <stdio.h>
#include <string.h>
#include <conio.h>

char szApp[] = "PM_Font_Drivers";
char szKey[] = "TRUETYPE";
char szBuffer[300];

void main(void) {
   ULONG  ulBootDrv;
   USHORT usLen = 0;
   APIRET rc;
   char   c;

   usLen = PrfQueryProfileString(HINI_USERPROFILE, szApp, szKey, NULL,
                                 (PVOID)szBuffer, 260L);

   if (!strcmp("\\OS2\\DLL\\FREETYPE.DLL", szBuffer)) {
      if(!PrfWriteProfileString(HINI_USERPROFILE, szApp, szKey, NULL))
         goto err;

      printf("FreeType/2 successfully removed.\n");
      printf("Do you wish to restore TRUETYPE.DLL (y/n)? ");
      c = getch();
      if (c != 'y' && c != 'Y')
         return;

      if(!PrfWriteProfileString(HINI_USERPROFILE, szApp, szKey,
                                 "\\OS2\\DLL\\TRUETYPE.DLL"))
         goto err;

      printf("\nTRUETYPE.DLL successfully restored");
   }
   else {
      printf("FreeType/2 not installed!");
      return;
   }

   return;

err:
   printf("Uninstallation failed!");
}
