/* From: Header: xc/programs/mkcfm/RCS/mkcfm.c,v 1.3 1996/05/08 21:38:21 ib Exp $ */
/* Copyright (c) 1994-1999 Silicon Graphics, Inc. All Rights Reserved.
 *
 * The contents of this file are subject to the CID Font Code Public Licence
 * Version 1.0 (the "License"). You may not use this file except in compliance
 * with the Licence. You may obtain a copy of the License at Silicon Graphics,
 * Inc., attn: Legal Services, 2011 N. Shoreline Blvd., Mountain View, CA
 * 94043 or at http://www.sgi.com/software/opensource/cid/license.html.
 *
 * Software distributed under the License is distributed on an "AS IS" basis.
 * ALL WARRANTIES ARE DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF FITNESS FOR A PARTICULAR PURPOSE OR OF
 * NON-INFRINGEMENT. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Software is CID font code that was developed by Silicon
 * Graphics, Inc.
 */
/* $XFree86: xc/programs/mkcfm/mkcfm.c,v 1.13 2003/05/27 22:26:56 tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>

#include "os.h"

#include "fntfilst.h"
#include "FSproto.h"
#include "FS.h"
#include "font.h"
#include "fontenc.h"
#include "fontstruct.h"
#include "fsmasks.h"
#include "range.h"
#include "objects.h"
#include "spaces.h"

#include "fontutil.h"
#include "util.h"
#include "fontfcn.h"
#include "blues.h"
#include "t1intf.h"
#include "t1unicode.h"
 
static void CIDFillVals(FontScalablePtr);
static Bool DoDirectory(char *dirName);
 
#define DEFAULTCID "/usr/X11R6/lib/X11/fonts/CID"
#define DECIPOINTSPERINCH 722.7
#define DEFAULTRES 75
#define DEFAULTPOINTSIZE 120

FontScalableRec vals;
FontEntryRec entry;

int 
main(int argc, char **argv)
{
       CIDFillVals(&vals);
       CIDRegisterFontFileFunctions();

       if (argc == 1) {
           if (!DoDirectory(DEFAULTCID)) {
               fprintf(stderr, "Default CID directory %s not found.\n",
                   DEFAULTCID);
               fprintf(stderr, "CFM files not created.\n");
               fprintf(stderr, 
                   "Specify CID directory: mkcfm directory-name.\n");
               exit(1);
           }
       } else if(argc == 2) {
           if (strstr(argv[1], "CID") == NULL || !DoDirectory(argv[1])) {
               fprintf(stderr, 
               "Specified CID directory not found. CFM files not created.\n");
               exit(1);
           }
       }
       exit(0);
}

static Bool 
DoDirectory(char *dirName) {
    DIR *dir, *dir1, *dir2, *dir3;
    struct dirent *dp, *dp1, *dp2;
    char fname[CID_NAME_MAX];
    char dirName1[CID_NAME_MAX], dirName2[CID_NAME_MAX];
    char dirName3[CID_NAME_MAX];
    FontPtr fontptr = NULL;

    entry.name.name = 
        "-foundry-familyname-bold-r-normal--0-0-0-0-p-0-jisx0201.1976-0";

    if ((dir = opendir(dirName))) {
        while((dp = readdir(dir))) {
            if (dp->d_name[0] == '.' || !strncmp(dp->d_name,"fonts.dir",9) ||
                !strncmp(dp->d_name, "fonts.scale", 11))
                continue;
            strcpy(dirName1, dirName);
            strcat(dirName1, "/");
            strcat(dirName1, dp->d_name);
            if ((dir1 = opendir(dirName1))) {
                strcpy(dirName2, dirName1);
                strcat(dirName2, "/CIDFont"); 
                if ((dir2 = opendir(dirName2))) {
                    while((dp1 = readdir(dir2))) {
                        if (dp1->d_name[0] == '.')
                            continue;
                        strcpy(dirName3, dirName1);
                        strcat(dirName3, "/CMap");
                        if ((dir3 = opendir(dirName3))) {
                            while((dp2 = readdir(dir3))) {
                                if (dp2->d_name[0] == '.')
                                    continue;
                                strcpy(fname, dirName1);
                                strcat(fname, "/");
                                strcat(fname, dp1->d_name);
                                strcat(fname, "--");
                                strcat(fname, dp2->d_name);
                                strcat(fname, ".cid");
                                (void)CIDOpenScalable(NULL, &fontptr, 0, &entry, fname, &vals, 0, 0, NULL);
                            }
                            closedir(dir3);
                        }
                    }
                    closedir(dir2);
                }
                closedir(dir1);
            }
        }
        closedir(dir);
        return TRUE;
    } return FALSE;
}

static void 
CIDFillVals(FontScalablePtr vals)
{
    FontResolutionPtr res;
    int         x_res = DEFAULTRES;
    int         y_res = DEFAULTRES;
    int         pointsize = DEFAULTPOINTSIZE;  /* decipoints */
    int         num_res;
 
    /* Must have x, y, and pixel */
    if (!vals->x || !vals->y || !vals->pixel) {
        res = (FontResolutionPtr) GetClientResolutions(&num_res);
        if (num_res) {
            if (res->x_resolution)
                x_res = res->x_resolution;
            if (res->y_resolution)
                y_res = res->y_resolution;
            if (res->point_size)
                pointsize = res->point_size;
        }
        if (!vals->x)
            vals->x = x_res;
        if (!vals->y)
            vals->y = y_res;
        if (!vals->point) {
            if (!vals->pixel) vals->point = pointsize;
            else vals->point = (vals->pixel * DECIPOINTSPERINCH) / vals->y;
            vals->point_matrix[0] = vals->point_matrix[3] = vals->point;
            vals->point_matrix[1] = vals->point_matrix[2] = 0;
        }
        if (!vals->pixel) {
            vals->pixel = (vals->point * vals->y) / DECIPOINTSPERINCH;
            vals->pixel_matrix[0] = vals->pixel_matrix[3] = vals->pixel;
            vals->pixel_matrix[1] = vals->pixel_matrix[2] = 0;
        } 
        /* Make sure above arithmetic is normally in range and will
           round properly. +++ */
    }
}
 
int 
CheckFSFormat(fsBitmapFormat format, fsBitmapFormatMask fmask, int *bit, 
	      int *byte, int *scan, int *glyph, int *image)
{
       *bit = *byte = 1;
       *glyph = *scan = *image = 1;
       return Successful;
 
}

 
Atom 
MakeAtom(char *p, unsigned len, int makeit)
{
       return *p;
}

FontResolutionPtr 
GetClientResolutions(int *resP)
{
       *resP = 0;
       return NULL;
}
 
pointer 
Xalloc(unsigned long size)
{
       return(malloc(size));
}
 
void 
Xfree(pointer p)
{
       free((char *)p);
}
 
void 
FontDefaultFormat(int *bit, int *byte, int *glyph, int *scan) { ; }
 
Bool 
FontFilePriorityRegisterRenderer(FontRendererPtr renderer, int priority) 
{ return TRUE; }
 
Bool 
FontFileRegisterRenderer(FontRendererPtr renderer) { return TRUE; }
 
Bool 
FontParseXLFDName(char *fname, FontScalablePtr vals, int subst)
{ return TRUE; }

void 
FontComputeInfoAccelerators(FontInfoPtr f) { ; }

void 
FatalError(const char *f, ...) { exit (1); }

void 
ErrorF(const char *f, ...) { ; }

char *
FontEncFromXLFD(const char *name, int n ) { return NULL; }

FontMapPtr
FontEncMapFind(const char *c1, int n1, int n2, int n3, const char *c2) 
{ return NULL; }

char *
unicodetoPSname(unsigned short code) { return NULL; }

unsigned 
FontEncRecode(unsigned i, FontMapPtr m) { return 0; }

char *
FontEncName(unsigned i, FontMapPtr m) { return NULL; }

FontPtr 
CreateFontRec (void)
{
    FontPtr pFont;

    pFont = (FontPtr)xalloc(sizeof(FontRec));
    if(pFont)
        bzero((char*)pFont, sizeof(FontRec));

    return pFont;
}

void DestroyFontRec (FontPtr pFont)
{
   xfree(pFont);
}

