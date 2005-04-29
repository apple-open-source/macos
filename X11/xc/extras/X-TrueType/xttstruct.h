/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Go Watanabe, All rights reserved.
   Copyright (c) 1998 Takuya SHIOZAKI, All rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved.
   Copyright (c) 2003 After X-TT Project, All rights reserved.

===Notice
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   Major Release ID: X-TrueType Server Version 1.4 [Charles's Wain Release 0]

Notice===
*/
/* $XFree86: xc/extras/X-TrueType/xttstruct.h,v 1.5 2003/11/17 22:20:01 dawes Exp $ */

#ifndef _XTTSTRUCT_H_
#define _XTTSTRUCT_H_

#define _FONTCACHE_SERVER_
#include "fontcache.h"


typedef struct FreeTypeFaceOpenHints {
    char const *fontName;
    int ttcno;
    char const *familyName;
    char const *ttFontName;
    char const *realFontName;
    char const *charsetName;
    Bool isProp;
    SRefPropRecValList *refListPropRecVal;
} FreeTypeOpenFaceHints, *FreeTypeOpenFaceHintsPtr;


/*
 * FreeType "Face" Information
 *  - It contains the common datas
 *    between each of "X" fonts having the same face of "FreeType" world.
 */
typedef struct FreeTypeFaceInfo {
    /* search key */
    char *fontName;
    int ttcno;
    /* information records */
    int refCount;
    TT_Face face;
    TT_Face_Properties prop;  /* get by using TT_Get_FaceProperties.
                                 This derived from TTF headers */
    TT_Glyph glyph;           /* handle for instance of glyph */
    TT_Glyph_Metrics metrics;
    TT_SBit_Image* sbit;
    int mapnum;
    int flag;
} FreeTypeFaceInfo, *FreeTypeFaceInfoPtr;

/*
 * X-TrueType Font Information
 */
typedef struct FreeTypeFont {
    FontPtr pFont;      /* font infomation */
    int fid;            /* faceinfo index */

    TT_Instance instance;
    TT_Instance_Metrics imetrics;

    TT_Matrix matrix;   /* transration matrix for the outline vectors */
    TT_Raster_Map map;
    int flag;

    FCCBPtr cache;

    TT_CharMap  charmap;
    CodeConverterInfo codeConverterInfo;

    void (*convert)(
        struct FreeTypeFont*   /* font */,
        unsigned char *        /* p */,
        int size
    );

    int spacing;
    double scaleBBoxWidth;
    int adjustBBoxWidthByPixel;
    int adjustLeftSideBearingByPixel;
    int adjustRightSideBearingByPixel;
    int doubleStrike;
    double autoItalic;
    int lsbShiftOfBitmapAutoItalic;
    int rsbShiftOfBitmapAutoItalic;
    double PixelAdjustmentBBoxWidthCorrectionRatio;
    Bool isVeryLazy;
    Bool isEmbeddedBitmap;
    Bool isInconsistentMetrics;
    double scaleWidth;
    double scaleBitmap;

    double pixel_size;             /* to calc attributes (actual height) */
    double pixel_width_unit_x;     /* to calc width (cosine) */

    int forceConstantSpacingBegin;
    int forceConstantSpacingEnd;
    xCharInfo forceConstantMetrics;

    char *dummy_bitmap;

} FreeTypeFont, *FreeTypeFontPtr;


/* xttinfo.c */
void freetype_make_standard_props(void);
void freetype_compute_props(FontInfoPtr, FontScalablePtr,
                            int raw_width, int raw_ascent, int raw_descent,
                            char *, char *);

#ifdef DUMP
#define dprintf(args) fprintf args;
void DumpFontPathElement(FontPathElementPtr ptr);
void DumpxCharInfo(xCharInfo *ptr);
void DumpFont(FontPtr ptr);
void DumpFontName(FontNamePtr ptr);
void DumpFontEntry(FontEntryPtr ptr);
void DumpfsRange(fsRange *ptr);
void DumpFontScalable(FontScalablePtr ptr);
#else
#define dprintf(args)
#endif


#endif /* _XTTSTRUCT_H_ */

/* end of file */
