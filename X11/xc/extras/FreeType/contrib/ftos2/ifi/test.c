/* $XFree86: xc/extras/FreeType/contrib/ftos2/ifi/test.c,v 1.2 2003/01/12 03:55:43 tsi Exp $ */

#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "32pmifi.h"

//#define USE_ORIG
#ifdef USE_ORIG
#  pragma import (fdhdr, "FONT_DRIVER_DISPATCH_TABLE", "TRUETYPE", 0)
#else
#  if defined USE_ATM
#    pragma import (fdhdr, "FONT_DRIVER_DISPATCH_TABLE", "PMATM", 0)
#  else
#    pragma import (fdhdr, "FONT_DRIVER_DISPATCH_TABLE", "FREETYPE", 0)
#  endif
#endif

extern FDHEADER fdhdr;

char *fontnames[20] = {
 "G:\\OS2\\MDOS\\WINOS2\\SYSTEM\\SYMBOL.TTF",
 "G:\\OS2\\MDOS\\WINOS2\\SYSTEM\\WINGDING.TTF",
 "G:\\PSFONTS\\ARIALB.ttf",
 "G:\\PSFONTS\\ARIALI.ttf",
 "G:\\PSFONTS\\ARIALZ.ttf",
 "G:\\PSFONTS\\COUR.TTF",
 "G:\\PSFONTS\\COURB.TTF",
 "G:\\PSFONTS\\COURI.TTF",
 "G:\\PSFONTS\\COURZ.TTF",
 "G:\\PSFONTS\\ARIAL.ttf",
 "G:\\PSFONTS\\TIMESB.TTF",
 "G:\\PSFONTS\\TIMESI.ttf",
 "G:\\PSFONTS\\TIMESZ.ttf",
 "G:\\PSFONTS\\TIMES.TTF",
 "G:\\PSFONTS\\ARIBLK.ttf",
 "G:\\CHINESE\\AVSV.TTF",
 "G:\\CHINESE\\MINGLI.TTC",
 "D:\\PSFONTS\\TNRMT30.TTF"
};

#define FNTNAME1 "\\PSFONTS\\TIMES.TTF"
#ifdef USE_ATM
#  define FNTNAME2 "\\PSFONTS\\helv.ofm"
#else
#  define FNTNAME2 "\\PSFONTS\\symbol.tTf"
#endif

#define BUFSIZE 32768

void ShowChar(PCHARATTR pca, PBITMAPMETRICS pbmm) {
   int i, j;
   int bufwidth = ((pbmm->sizlExtent.cx + 31) & -32) / 8;

   for (i =0; i <  pbmm->sizlExtent.cy; i++) {
      for (j = 0; j < bufwidth * 8; j++)
         if (pca->pBuffer[i * bufwidth + j / 8] & (1 << (7-(j % 8))))
            printf("*");
         else
            printf(" ");
      printf("\n");
   }
}

void main(int argc, char **argv)
{
   char          fname[260];
   PFDDISPATCH   pfdisp;
   LONG          rc;
   HFF           hff, hff2;      /* font file */
   HFC           hfc, hfc2;      /* font context */
   static   IFIMETRICS    ifimet[12];      /* IFI metrics */
   CONTEXTINFO   ci;
   CHARATTR      charattr; /* character attributes */
   BITMAPMETRICS bmm;      /* bit-map metrics */
   PBYTE         buf;
   int           glyph = 0, i, j;
   int           numFaces;
   int           faceIndex = 0;

   switch (argc) {
      case 4:
         strcpy(fname, argv[1]);
         glyph = atoi(argv[2]);
         faceIndex = atoi(argv[3]);
         break;

      case 2:
         glyph = atoi(argv[1]);
      case 1:
         strcpy(fname, FNTNAME1);
         break;

      default:
         strcpy(fname, argv[1]);
         glyph = atoi(argv[2]);
   }

   buf = (PBYTE)malloc(BUFSIZE);
   if (strncmp("OS/2 FONT DRIVER", fdhdr.strId, 16)) {
      printf("Invalid Font Driver\n");
      return;
   }
   printf("Font Driver OK, ");
   printf("Version %d\n", fdhdr.ulVersion);
   printf("Technology: %s\n", fdhdr.szTechnology);

   hff = fdhdr.pfddisp->FdLoadFontFile(fname);
   printf("Loading font... HFF = %X\n", hff);
   if (hff == (HFF)0xFFFFFFFF)
      return;

/*   rc = fdhdr.pfddisp->FdConvertFontFile("G:\\PSFONTS\\TIMES.TTF",
           "G:\\PSFONTS", buf); */

#if 0
   for (i = 0; i < 18; i++) {
      hff = fdhdr.pfddisp->FdLoadFontFile(fontnames[i]);
      if (hff == (HFF)-1) {
         printf("x");
         continue;
      }
      numFaces = fdhdr.pfddisp->FdQueryFaces(hff, NULL, 0, -1, 0);
      if (numFaces < 0) {
         printf("x");
         continue;
      }
      for (j = 0; j < numFaces; j++) {
         rc = fdhdr.pfddisp->FdQueryFaces(hff, &ifimet[0], 238, 1, j);
         if (rc < 0) {
            printf("x");
            continue;
         }
      }

      rc = fdhdr.pfddisp->FdUnloadFontFile(hff);
      if (rc)
         printf("x");
      else
         printf(".");
   }
   printf("\n");
#endif

   hff = fdhdr.pfddisp->FdLoadFontFile(FNTNAME2);
   hff = fdhdr.pfddisp->FdLoadFontFile(fname);
   rc = fdhdr.pfddisp->FdUnloadFontFile(hff);

   hff = fdhdr.pfddisp->FdLoadFontFile(fname);
   printf("Loading font... HFF = %X\n", hff);
   if (hff == (HFF)0xFFFFFFFF)
      return;
   numFaces = fdhdr.pfddisp->FdQueryFaces(hff, NULL, 0, -1, 0);
   printf("Number of faces = %d\n", numFaces);
   rc = fdhdr.pfddisp->FdQueryFaces(hff, &ifimet[0], sizeof(IFIMETRICS), numFaces, 0);
   printf("Querying faces... RC = %X\n", rc);
   hfc = fdhdr.pfddisp->FdOpenFontContext(hff, faceIndex);
   printf("Opening context... HFC = %X\n", hfc);
   if (hfc == (HFC)0xFFFFFFFF) {
      rc = fdhdr.pfddisp->FdUnloadFontFile(hff);
      printf("Unloading font... RC = %X\n", rc);
   }
   ci.cb = sizeof(ci);
   ci.fl = 0;
/*   ci.sizlPPM.cx = 3618;
   ci.sizlPPM.cy = 3622;
   ci.pfxSpot.x = 46340;
   ci.pfxSpot.y = 46340;
   ci.matXform.eM11 = 511;
   ci.matXform.eM12 = 0;
   ci.matXform.eM21 = 0;
   ci.matXform.eM22 = 511; */
   ci.sizlPPM.cx = 3622;
   ci.sizlPPM.cy = 3622;
   ci.pfxSpot.x = 46340;
   ci.pfxSpot.y = 46340;
   ci.matXform.eM11 = 768;
   ci.matXform.eM12 = 0;
   ci.matXform.eM21 = 0;
   ci.matXform.eM22 = 768;

   rc = fdhdr.pfddisp->FdQueryFaceAttr(hfc, FD_QUERY_ABC_WIDTHS, buf,
                                       sizeof(ABC_TRIPLETS), NULL, glyph);
   printf("Querying face attrs... RC = %d\n", rc);
   rc = fdhdr.pfddisp->FdQueryFaceAttr(hfc, FD_QUERY_KERNINGPAIRS, buf,
                                       ifimet[0].cKerningPairs * sizeof(FD_KERNINGPAIRS),
                                       NULL, 0);


   rc = fdhdr.pfddisp->FdSetFontContext(hfc, &ci);
   printf("Setting context... rc = %X\n", rc);

   charattr.cb = sizeof(charattr);
   charattr.iQuery = FD_QUERY_BITMAPMETRICS | FD_QUERY_CHARIMAGE;
//   charattr.iQuery = FD_QUERY_OUTLINE;
   charattr.gi = glyph;
   charattr.pBuffer = buf;
   charattr.cbLen = BUFSIZE;
   if (rc == -1)
      return;
   rc = fdhdr.pfddisp->FdQueryCharAttr(hfc, &charattr, &bmm);
   printf("Querying char attrs... bytes = %d\n", rc);
   ShowChar(&charattr, &bmm);

//   rc = fdhdr.pfddisp->FdQueryCharAttr(hfc, &charattr, &bmm, NULL);
//   printf("Querying char attrs... bytes = %d\n", rc);

   hff2 = fdhdr.pfddisp->FdLoadFontFile(FNTNAME2);
   printf("Loading font... HFF = %X\n", hff2);
   if (hff2 == (HFF)0xFFFFFFFF)
      return;

   charattr.cbLen = 0;
   rc = fdhdr.pfddisp->FdQueryCharAttr(hfc, &charattr, &bmm);
   printf("Querying char attrs... bytes = %d\n", rc);
   rc = fdhdr.pfddisp->FdQueryFaces(hff2, &ifimet[0], sizeof(ifimet), 1, 0);
   printf("Querying faces... RC = %X\n", rc);
   hfc2 = fdhdr.pfddisp->FdOpenFontContext(hff2, 0);
   printf("Opening context... HFC = %X\n", hfc2);
   if (hfc2 == (HFC)0xFFFFFFFF) {
      rc = fdhdr.pfddisp->FdUnloadFontFile(hff2);
      printf("Unloading font... RC = %X\n", rc);
   }
   rc = fdhdr.pfddisp->FdCloseFontContext(hfc);
   printf("Closing context... RC = %X\n", rc);
   rc = fdhdr.pfddisp->FdUnloadFontFile(hff);
   printf("Unloading font... RC = %X\n", rc);
   rc = fdhdr.pfddisp->FdUnloadFontFile(hff);
   printf("Unloading font... RC = %X\n", rc);

   rc = fdhdr.pfddisp->FdCloseFontContext(hfc2);
   printf("Closing context... RC = %X\n", rc);
   rc = fdhdr.pfddisp->FdUnloadFontFile(hff2);
   printf("Unloading font... RC = %X\n", rc);
   rc = fdhdr.pfddisp->FdUnloadFontFile(hff2);
   printf("Unloading font... RC = %X\n", rc);
}
