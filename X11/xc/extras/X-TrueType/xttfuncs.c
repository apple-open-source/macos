/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Go Watanabe, All rights reserved.
   Copyright (c) 1998 Kazushi (Jam) Marukawa, All rights reserved.
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
/* $XFree86: xc/extras/X-TrueType/xttfuncs.c,v 1.22 2003/10/28 18:01:47 tsi Exp $ */

#include "xttversion.h"

#if 0
static char const * const releaseID =
    _XTT_RELEASE_NAME;
#endif

/*
  X-TrueType Server -- invented by Go Watanabe.

  This Version includes the follow feautures:
    JAMPATCHs:
      written by Kazushi (Jam) Marukawa.
    CodeConv:
    Moduled CodeConv:
    TTCap:
      written by Takuya SHIOZAKI.
*/

#ifndef FONTMODULE
#include <X11/Xos.h>
#endif
#include <X11/X.h>
#include "fntfilst.h"
#include "xttcommon.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttstruct.h"

/*
 * Any bit blitters are not included in FreeType 1.x.
 * Because bitmap glyph is visible for client aplictions and
 * FreeType is not provide any text renderers.
 */
#include "xttblit.h"

/*
 * macros
 */
#ifndef ABS
#define ABS(a)  (((a)<0)?-(a):(a))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#define FLOOR64(x) ((x) & -64)
#define CEIL64(x) (((x) + 64 - 1) & -64)

#define XTT_PROPORTIONAL_SPACING  1
#define XTT_MONOSPACED            0
#define XTT_CONSTANT_SPACING     -1

#define XTT_DOUBLE_STRIKE                     0x01
#define XTT_DOUBLE_STRIKE_MKBOLD_EDGE_LEFT    0x02
#define XTT_DOUBLE_STRIKE_CORRECT_B_BOX_WIDTH 0x10

#define XTT_FORCE_CONSTANT_SPACING 0x01
#define XTT_FORCE_CONSTANT_SPACING_CACHE_KEY_OFFSET 0x00010000

#define IS_TT_Matrix_Unit(matrix) \
(((matrix).xx == 65536) && ((matrix).yx == 0) && \
 ((matrix).xy == 0    ) && ((matrix).yy == 65536))

/*
 * prototypes
 */

/* from lib/font/util/format.c */
extern int
CheckFSFormat(fsBitmapFormat format, fsBitmapFormatMask fmask,
              int *bit_order, int *byte_order, int *scan,
              int *glyph, int *image);
/* from lib/font/util/fontaccel.c */
extern void
FontComputeInfoAccelerators(FontInfoPtr pFontInfo);

static int FreeType_InitCount = 0;
static TT_Engine engine;

struct xtt_char_width {
    int pixel;   /* pixel */
    int raw;     /* 1000 pixel */
};

static int
FreeType_Init(void)
{
    dprintf((stderr, "FreeTypeInit\n"));
    if (FreeType_InitCount == 0) {
        if (TT_Init_FreeType(&engine)) {
            fprintf(stderr, "freetype: Could not create engine instance\n");
            return -1;
        }
        if (TT_Init_SBit_Extension(engine))
            fprintf(stderr, "freetype: This engine is not provided sbit extension\n");
    }
    FreeType_InitCount++;
    return 0;
}

static int
FreeType_Deinit(void)
{
    dprintf((stderr, "FreeTypeDeInit\n"));
    if (FreeType_InitCount <= 0) {
        return -1;
    }
    if (--FreeType_InitCount == 0) {
        if (TT_Done_FreeType(engine) < 0) {
            return -1;
        }
    }
    return 0;
}

static FreeTypeFaceInfo *faceTable = NULL;
static int faceTableCount = 0;

static int
FreeType_OpenFace(FreeTypeOpenFaceHints const *refHints)
{
    int i, num;
    TT_Face face;
    TT_Face_Properties prop;
    TT_Glyph glyph;
    TT_SBit_Image* sbit;
    FreeTypeFaceInfoPtr ptr;

    dprintf((stderr,
             "FreeType_OpenFace: %s %s %s\n",
             refHints->fontName, refHints->familyName, refHints->ttFontName));

    if (TT_Open_Face(engine, refHints->ttFontName, &face)) {
        fprintf(stderr, "freetype: can't open face: %s\n", refHints->ttFontName);
        return -1;
    }

    if (TT_Get_Face_Properties(face, &prop)) {
        TT_Close_Face(face);
        fprintf(stderr, "freetype: can't get face property.\n");
        return -1;
    }

    if ( refHints->ttcno ) {
        TT_Close_Face(face);
        if ( refHints->ttcno<0 || refHints->ttcno>=prop.num_Faces ) {
            fprintf(stderr, "Bad face collection:%d\n", refHints->ttcno);
            return -1;
        }
        if (TT_Open_Collection(engine, refHints->ttFontName,
                                        refHints->ttcno, &face)) {
            fprintf(stderr, "Can't Open face collection:%d\n",
                    refHints->ttcno);
            return -1;
        }
    }

    dprintf((stderr, "Select Collection %d\n", refHints->ttcno));

    /* check !! */
    for (i = 0; i < faceTableCount; i++) {
        if (strcmp(faceTable[i].fontName, refHints->fontName) == 0 &&
            faceTable[i].ttcno == refHints->ttcno) {
            /* reopen */
            if (faceTable[i].flag) {
                if (TT_Get_Face_Properties(face, &prop)) {
                    TT_Close_Face(face);
                    fprintf(stderr, "freetype: can't get face property.\n");
                    return -1;
                }
                if ((num = TT_Get_CharMap_Count(face)) < 0) {
                    TT_Close_Face(face);
                    fprintf(stderr, "freetype: can't get charmap count.\n");
                    return -1;
                }
                if (TT_New_Glyph(face, &glyph)) {
                    TT_Close_Face(face);
                    fprintf(stderr, "freetype: can't get new glyph.\n");
                    return -1;
                }
                {
                    /* check and initialize support stuffs for sbit extesion */
                    TT_EBLC eblc; /* fake */

                    if (!TT_Get_Face_Bitmaps(face, &eblc)) {
                        if (TT_New_SBit_Image(&sbit)) {
                            TT_Close_Face(face);
                            fprintf(stderr,
                                    "freetype: can't get new sbit image.\n");
                            return -1;
                        }
                    } else
                        sbit = NULL;
                }
                faceTable[i].face   = face;
                faceTable[i].ttcno  = refHints->ttcno;
                faceTable[i].prop   = prop;
                faceTable[i].glyph  = glyph;
                faceTable[i].sbit   = sbit;
                faceTable[i].mapnum = num;
                faceTable[i].flag   = 0;
            } else
                TT_Close_Face(face);
            faceTable[i].refCount++;
            return i;
        }
    }

    dprintf((stderr, "No Face. Make New Face\n"));

    if (!(ptr = (FreeTypeFaceInfoPtr)
          xrealloc(faceTable, (faceTableCount+1) * sizeof(*ptr)))){
        fprintf(stderr, "xrealloc: can't alloc memory for fonttable\n");
        return -1;
    }

    if (TT_Get_Face_Properties(face, &prop)) {
        TT_Close_Face(face);
        fprintf(stderr, "freetype: can't get face property.\n");
        return -1;
    }
    if ((num = TT_Get_CharMap_Count(face)) < 0) {
        TT_Close_Face(face);
        fprintf(stderr, "freetype: can't get charmap count.\n");
        return -1;
    }
    if (TT_New_Glyph(face, &glyph)) {
        TT_Close_Face(face);
        fprintf(stderr, "freetype: can't get new glyph.\n");
        return -1;
    }
    {
        /* check support stuff for sbit extesion */
        TT_EBLC eblc; /* use only for checking the stuff */

        if (!TT_Get_Face_Bitmaps(face, &eblc)) {
            if (TT_New_SBit_Image(&sbit)) {
                TT_Close_Face(face);
                fprintf(stderr, "freetype: can't get new sbit image.\n");
                return -1;
            }
        } else
            sbit = NULL;
    }

    faceTable = ptr;
    faceTable[faceTableCount].fontName = xstrdup(refHints->fontName);
    faceTable[faceTableCount].ttcno    = refHints->ttcno;
    faceTable[faceTableCount].refCount = 1;
    faceTable[faceTableCount].face     = face;
    faceTable[faceTableCount].prop     = prop;
    faceTable[faceTableCount].glyph    = glyph;
    faceTable[faceTableCount].sbit     = sbit;
    faceTable[faceTableCount].mapnum   = num;
    faceTable[faceTableCount].flag     = 0;

    i = faceTableCount++;
    return i;
}

static int
FreeType_CloseFace(int index)
{
    dprintf((stderr, "FreeType_CloseFace: %d\n", index));
    if (index < faceTableCount) {
        if (faceTable[index].refCount <= 0) {
            fprintf(stderr, "FreeType_CloseFace: bad index\n");
            return -1;
        }
        if (--faceTable[index].refCount == 0) {
            TT_Done_Glyph(faceTable[index].glyph);
            if (faceTable[index].sbit)
                TT_Done_SBit_Image(faceTable[index].sbit);
            TT_Close_Face(faceTable[index].face);
            faceTable[index].flag = -1;
        }
        return 0;
    }
    fprintf(stderr, "FreeType_CloseFace: bad index\n");
    return -1;
}

static void
convertNothing(FreeTypeFont *ft, unsigned char *p, int size)
{
}

static void
convertBitOrder(FreeTypeFont *ft, unsigned char *p, int size)
{
    BitOrderInvert(p, size);
}

static void
convertByteOrder(FreeTypeFont *ft, unsigned char *p, int size)
{
    if (ft->pFont->bit != ft->pFont->byte) {
        switch (ft->pFont->scan) {
        case 1:
            break;
        case 2:
            TwoByteSwap(p, size);
        case 4:
            FourByteSwap(p, size);
        }
    }
}

static void
convertBitByteOrder(FreeTypeFont *ft, unsigned char *p, int size)
{
    convertBitOrder(ft, p, size);
    convertByteOrder(ft, p, size);
}


static int
FreeType_OpenFont(FreeTypeFont *ft,
                  FontScalablePtr vals, int glyph,
                  FreeTypeOpenFaceHints const *refHints)
{
    int mapID, fid, result;
    FreeTypeFaceInfo *fi;
    TT_Instance instance;
    double base_size;
    int num_faces = 0, linesize;

    base_size = hypot(vals->point_matrix[2], vals->point_matrix[3]);

    result = Successful;
    fid = -1;

    if (FreeType_Init()< 0) {
        result = AllocError;
        goto abort;
    }

    if ((ft->fid = fid = FreeType_OpenFace(refHints)) < 0) {
        result = BadFontName;
        goto deinitQuit;
    }

    fi = &faceTable[fid];
    if (!codeconv_search_code_converter(refHints->charsetName,
                                        fi->face, fi->mapnum,
                                        refHints->refListPropRecVal,
                                        &ft->codeConverterInfo,
                                        &mapID)) {
        fprintf(stderr,
                "FreeType_OpenFont: don't match charset %s\n",
                refHints->charsetName);
        result = BadFontName;
        goto deinitQuit;
    }

    TT_Get_CharMap(fi->face, mapID, &ft->charmap);

    /* create instance */
    if (TT_New_Instance(fi->face, &instance)) {
        result = BadFontName;
        goto deinitQuit;
    }

    /* set resolution of instance */
    if (TT_Set_Instance_Resolutions(instance, (int)vals->x, (int)vals->y)) {
        result = BadFontName;
        goto doneInstQuit;
    }

    {
        int flRotated = 0, flStretched = 0;

        if (vals->point_matrix[1] != 0 || vals->point_matrix[2] != 0)
            flRotated = flStretched = 1;
        else if (vals->point_matrix[0] != vals->point_matrix[3])
            flStretched = 1;
        else if (ft->scaleWidth != 1.0)
            flStretched = 1;

        TT_Set_Instance_Transform_Flags(instance, flRotated, flStretched);

        /*
         * The size depend on the height, not the width.
         * So use point_matrix[3].
         */
        if (TT_Set_Instance_CharSize(instance, base_size *64)) {
            result = BadFontName;
            goto doneInstQuit;
        }
        /* set matrix */
        ft->matrix.xx =
            ft->scaleWidth *
            vals->point_matrix[0] / base_size * 65536;
        ft->matrix.xy =
            ft->scaleWidth *
            vals->point_matrix[2] / base_size * 65536;
        ft->matrix.yx =
            vals->point_matrix[1] / base_size * 65536;
        ft->matrix.yy =
            vals->point_matrix[3] / base_size * 65536;

        dprintf((stderr, "matrix: %x %x %x %x\n",
                 ft->matrix.xx,
                 ft->matrix.yx,
                 ft->matrix.xy,
                 ft->matrix.yy));
    }

    ft->instance = instance;
    TT_Get_Instance_Metrics(instance, &ft->imetrics);

    num_faces = fi->prop.num_Faces;
    if (num_faces == 0) {
        num_faces = 1;
    }
    if ((fi->prop.num_Glyphs / num_faces) > 256) {
        linesize = 128;
    } else {
        linesize = 16;
    }
    if ((ft->cache = FontCacheOpenCache((void *)(long) linesize)) == NULL) {
        result = AllocError;
        goto doneInstQuit;
    }

    return result;

 doneInstQuit:
    TT_Done_Instance(instance);
 deinitQuit:
    if (fid>=0)
        FreeType_CloseFace(fid);
    FreeType_Deinit();
 abort:
    return result;
}

static void
FreeType_CloseFont(FontPtr pFont)
{
    FreeTypeFont *ft = (FreeTypeFont*) pFont->fontPrivate;

    dprintf((stderr, "FreeType_CloseFont: %x\n", pFont));

    TT_Done_Instance(ft->instance);
    FontCacheCloseCache(ft->cache);

    FreeType_CloseFace(ft->fid);
    FreeType_Deinit();
    codeconv_free_code_converter(&ft->codeConverterInfo);

    if ( ft->dummy_bitmap ) xfree(ft->dummy_bitmap);
    xfree(ft);
    xfree(pFont->info.props);
}

static char nochardat;
static CharInfoRec nocharinfo = {/*metrics*/{0,0,0,0,0,0},
                                     /*bits*/&nochardat};
/*
 * Pseudo enbolding similar as Microsoft Windows.
 * It is useful but poor.
 */
static void
make_up_bold_bitmap(TT_Raster_Map *map, int ds_mode)
{
    int x, y;
    unsigned char *p = (unsigned char *)map->bitmap;
    if ( ds_mode & XTT_DOUBLE_STRIKE_MKBOLD_EDGE_LEFT ) {
        for (y=0; y<map->rows; y++) {
            unsigned char rev_pat=0;
            unsigned char lsb = 0;
            for (x=0; x<map->cols; x++) {
                unsigned char tmp = *p<<7;
                if ( (rev_pat & 0x01) && (*p & 0x80) ) p[-1] &= 0xfe;
                rev_pat = ~(*p);
                *p |= (*p>>1) | lsb;
                *p &= ~(rev_pat & (*p << 1));
                lsb = tmp;
                p++;
            }
        }
    }
    else {
        for (y=0; y<map->rows; y++) {
            unsigned char lsb = 0;
            for (x=0; x<map->cols; x++) {
                unsigned char tmp = *p<<7;
                *p |= (*p>>1) | lsb;
                lsb = tmp;
                p++;
            }
        }
    }
}

static void
make_up_italic_bitmap( char *raster, int bpr, int ht, int shift,
                       int h_total, int h_offset, double a_italic)
{
    int x, y;
    unsigned char *p = (unsigned char *)raster;
    if ( a_italic < 0 ) shift = -shift;
    for (y=0; y<ht; y++) {
        unsigned char *tmp_p = p + y*bpr;
        int tmp_shift = shift * (h_total -1 -(y+h_offset)) / h_total;
        int tmp_byte_shift;
        if ( 0 <= tmp_shift ) {
            tmp_byte_shift = tmp_shift/8;
            tmp_shift %= 8;
            if ( tmp_shift ) {
                for (x=bpr-1;0<=x;x--) {
                    if ( x != bpr-1 ) 
                        tmp_p[x+1] |= tmp_p[x]<<(8-tmp_shift);
                    tmp_p[x]>>=tmp_shift;
                }
            }
            if ( tmp_byte_shift ) {
                for (x=bpr-1;0<x;x--) {
                    tmp_p[x] = tmp_p[x-1];
                }
                tmp_p[x]=0;
            }
        }
        else {
            tmp_shift = -tmp_shift;
            tmp_byte_shift = tmp_shift/8;
            tmp_shift %= 8;
            if ( tmp_shift ) {
                for (x=0;x<bpr;x++) {
                    if ( x != 0 ) 
                        tmp_p[x-1] |= tmp_p[x]>>(8-tmp_shift);
                    tmp_p[x]<<=tmp_shift;
                }
            }
            if ( tmp_byte_shift ) {
                for (x=0;x<bpr-1;x++) {
                    tmp_p[x] = tmp_p[x+1];
                }
                tmp_p[x]=0;
            }
        }
    }
}

/* font cache private area */

struct fc_tt_private {
    int shift;
};


static void
fc_tt_private_dispose(void *f_private)
{
    if ( f_private )
        xfree(f_private);
}

static struct fc_entry_vfuncs fc_tt_vfuncs = {
    &fc_tt_private_dispose   /* f_private_dispose */
};

#define FC_TT_INIT_PRIVATE(entry) \
  { \
    (entry)->f_private = xalloc(sizeof(struct fc_tt_private)); \
    memset((entry)->f_private, 0, sizeof(struct fc_tt_private)); \
  }
#define FC_TT_PRIVATE(entry) ((struct fc_tt_private *)((entry)->f_private))
#define FC_TT_SETVFUNC(entry) ((void)((entry)->vfuncs=&fc_tt_vfuncs))


/* calculate glyph metric from glyph index */

static xCharInfo *
get_metrics(FreeTypeFont *ft, int c, struct xtt_char_width char_width, int force_c)
{
    FreeTypeFaceInfo *fi = &faceTable[ft->fid];

    FontCacheEntryPtr entry;
    CharInfoPtr charInfo;
    int width, ascent, descent;
    int lbearing, rbearing;
    int sbit_rsb_shift=0;
    int sbit_lsb_shift=0;
    int cache_key_offset;
    int sbit_ok;
    TT_Glyph_Metrics metrics;

    TT_BBox* bbox = &metrics.bbox;

    if ( force_c & XTT_FORCE_CONSTANT_SPACING )
        cache_key_offset = XTT_FORCE_CONSTANT_SPACING_CACHE_KEY_OFFSET;
    else cache_key_offset=0;

    /*
     * Check invalid char index.
     */
    if ( c < 0 ) {
        charInfo = &nocharinfo;
        goto next;
    }

    if (!FontCacheSearchEntry(ft->cache, cache_key_offset+c, &entry)) {
      if ( force_c & XTT_FORCE_CONSTANT_SPACING ) {
        /* return &(ft->forceConstantMetrics); */
        if ((entry = FontCacheGetEntry()) == NULL) {
            charInfo = &nocharinfo;
            fprintf(stderr, "get_metrics: can't get cache entry\n");
            goto next;
        }
        FC_TT_SETVFUNC(entry);
        FC_TT_INIT_PRIVATE(entry);
        charInfo = &entry->charInfo;
        charInfo->metrics.leftSideBearing = ft->forceConstantMetrics.leftSideBearing;
        charInfo->metrics.rightSideBearing = ft->forceConstantMetrics.rightSideBearing;
        charInfo->metrics.ascent = ft->forceConstantMetrics.ascent;
        charInfo->metrics.descent = ft->forceConstantMetrics.descent;
        charInfo->metrics.characterWidth = ft->forceConstantMetrics.characterWidth;
        charInfo->metrics.attributes = ft->forceConstantMetrics.attributes;
      }
      else {
        if ( !(ft->isVeryLazy && 0.0 < ft->scaleBitmap) && fi->sbit &&
             (IS_TT_Matrix_Unit(ft->matrix) 
              || ft->lsbShiftOfBitmapAutoItalic || ft->rsbShiftOfBitmapAutoItalic) &&
             ft->isEmbeddedBitmap ) {
            if ( TT_Load_Glyph_Bitmap(fi->face, ft->instance, c,
                                      fi->sbit) ) {
                fi->sbit->map.size = 0;
            } else {
                metrics.bbox     = fi->sbit->metrics.bbox;
                metrics.bearingX = fi->sbit->metrics.horiBearingX;
                metrics.bearingY = fi->sbit->metrics.horiBearingY;
                metrics.advance  = fi->sbit->metrics.horiAdvance;
                sbit_rsb_shift = ft->rsbShiftOfBitmapAutoItalic;
                sbit_lsb_shift = ft->lsbShiftOfBitmapAutoItalic;
            }
        } else
            if ( fi->sbit )
                fi->sbit->map.size = 0;
        sbit_ok=0;
        if ( fi->sbit ) {
            if ( fi->sbit->map.size ) sbit_ok=1;
        }
        if ( !sbit_ok ) {
          /* load outline's metrics */
          if (!ft->isVeryLazy) {
              TT_Load_Glyph(ft->instance, fi->glyph, c, ft->flag);
              TT_Get_Glyph_Metrics(fi->glyph, &metrics);
              {
                  TT_Outline outline;
                  TT_Get_Glyph_Outline(fi->glyph, &outline);
                  outline.dropout_mode = 2;
                  TT_Transform_Outline(&outline, &ft->matrix);
                  TT_Get_Outline_BBox(&outline, bbox);
              }
          }
          else {
            /*
             * very lazy method,
             * parse the htmx field in TrueType font.
             */
            TT_Vector p0, p1, p2, p3;

            {
                /* horizontal */
                TT_Short   leftBearing = 0;
                TT_UShort  advance = 0;

                TT_Get_Face_Metrics(fi->face, c, c,
                                    &leftBearing, &advance,
                                    NULL, NULL);

                bbox->xMax = metrics.advance =
                    TT_MulFix( advance, ft->imetrics.x_scale );
                bbox->xMin = metrics.bearingX =
                    TT_MulFix( leftBearing, ft->imetrics.x_scale );
            }
            {
                /* vertical */
                TT_Header *pH = fi->prop.header;
                bbox->yMin = TT_MulFix( pH->yMin,
                                        ft->imetrics.y_scale );
                bbox->yMax = TT_MulFix( pH->yMax,
                                        ft->imetrics.y_scale );
            }
            p0.x = p2.x = bbox->xMin;
            p1.x = p3.x = bbox->xMax;
            p0.y = p1.y = bbox->yMin;
            p2.y = p3.y = bbox->yMax;

            TT_Transform_Vector(&p0.x, &p0.y, &ft->matrix);
            TT_Transform_Vector(&p1.x, &p1.y, &ft->matrix);
            TT_Transform_Vector(&p2.x, &p2.y, &ft->matrix);
            TT_Transform_Vector(&p3.x, &p3.y, &ft->matrix);
#if 0
            fprintf(stderr,
                    "(%.1f %.1f) (%.1f %.1f)"
                    "(%.1f %.1f) (%.1f %.1f)\n",
                    p0.x / 64.0, p0.y / 64.0,
                    p1.x / 64.0, p1.y / 64.0,
                    p2.x / 64.0, p2.y / 64.0,
                    p3.x / 64.0, p3.y / 64.0);
#endif
            bbox->xMin = MIN(p0.x, MIN(p1.x, MIN(p2.x, p3.x)));
            bbox->xMax = MAX(p0.x, MAX(p1.x, MAX(p2.x, p3.x)));
            bbox->yMin = MIN(p0.y, MIN(p1.y, MIN(p2.y, p3.y)));
            bbox->yMax = MAX(p0.y, MAX(p1.y, MAX(p2.y, p3.y)));

            {
                TT_SBit_Strike strike;
                /*
                 * We use TTCap but it is naive.
                 * It has better provide  another calculation way.
                 */
                double sbit_scale = ft->scaleBitmap;

                TT_Pos bbox_width = bbox->xMax - bbox->xMin;
                TT_Pos add_width;

                if ( sbit_scale <= 0.0 ) sbit_scale=1.0;

                /* parse sbit entry */
                if (!TT_Get_SBit_Strike(fi->face, ft->instance, &strike)) {
                    TT_SBit_Range*  range = strike.sbit_ranges;
                    TT_SBit_Range*  limit = range + strike.num_ranges;

                    while((range<limit) &&
                          (range->first_glyph <= c) &&
                          (c <= range->last_glyph ))
                        range++;

                    if (range<limit) {
                        add_width = bbox_width * (sbit_scale - 1);
                        bbox->xMin -= add_width / 2;
                        bbox->xMax += add_width / 2;
                    }
                }
            }
          }
        }

        /*
         * Use floor64 and ceil64 to calulate exectly since values might
         * be minus.
         */
        ascent   = FLOOR64(bbox->yMax + 32) / 64;
        descent  = CEIL64(-bbox->yMin - 32) / 64;
        lbearing = FLOOR64(bbox->xMin + 32) / 64;
        rbearing = FLOOR64(bbox->xMax + 32) / 64;
        if ( ft->doubleStrike ) rbearing++;
        rbearing += sbit_rsb_shift;
        lbearing += sbit_lsb_shift;

        width    = FLOOR64((int)floor(metrics.advance * ft->scaleBBoxWidth
                              * ft->pixel_width_unit_x + .5) + 32) / 64;
        if ( ft->doubleStrike & XTT_DOUBLE_STRIKE_CORRECT_B_BOX_WIDTH ) {
            width++;
        }
        width += ft->adjustBBoxWidthByPixel;
        rbearing += ft->adjustRightSideBearingByPixel;
        lbearing += ft->adjustLeftSideBearingByPixel;

        if ((entry = FontCacheGetEntry()) == NULL) {
            charInfo = &nocharinfo;
            fprintf(stderr, "get_metrics: can't get cache entry\n");
            goto next;
        }
        FC_TT_SETVFUNC(entry);
        FC_TT_INIT_PRIVATE(entry);
        charInfo = &entry->charInfo;

        /* monospaced */
        if ( char_width.pixel && width) {
            int max_rsb,max_lsb;
            /* XXX */
            FC_TT_PRIVATE(entry)->shift = (char_width.pixel - width) / 2;

            /* Shift to "correct" position. */
            lbearing += FC_TT_PRIVATE(entry)->shift;
            rbearing += FC_TT_PRIVATE(entry)->shift;

            /* when char_width < 0 ??? */
            /* It is always lbearing < rbeaing */
            if (rbearing - lbearing <= char_width.pixel) {
                if (lbearing < 0) {
                    rbearing                    -= lbearing;
                    FC_TT_PRIVATE(entry)->shift -= lbearing;
                    lbearing                     = 0;
                }
                if (rbearing > char_width.pixel) {
                    lbearing                    -=
                        (rbearing - char_width.pixel);
                    FC_TT_PRIVATE(entry)->shift -=
                        (rbearing - char_width.pixel);
                    rbearing                     = char_width.pixel;
                }
            }
            width = char_width.pixel;
            if ( ft->isInconsistentMetrics ) {
                /* prevent excess */
                if ( ft->pFont->info.minbounds.rightSideBearing 
                     < ft->pFont->info.maxbounds.rightSideBearing ) 
                    max_rsb=ft->pFont->info.maxbounds.rightSideBearing;
                else
                    max_rsb=ft->pFont->info.minbounds.rightSideBearing;
                if ( ft->pFont->info.maxbounds.leftSideBearing 
                     < ft->pFont->info.minbounds.leftSideBearing ) 
                    max_lsb=ft->pFont->info.maxbounds.leftSideBearing;
                else
                    max_lsb=ft->pFont->info.minbounds.leftSideBearing;
                if ( max_rsb - max_lsb < rbearing-lbearing) {
                    rbearing-=(rbearing-lbearing)-(max_rsb - max_lsb);
                }
                if ( rbearing < lbearing ) rbearing=lbearing;
                if ( ft->pFont->info.maxbounds.ascent < ascent )
                    ascent = ft->pFont->info.maxbounds.ascent;
                if ( ft->pFont->info.maxbounds.descent < descent )
                    descent = ft->pFont->info.maxbounds.descent;
            }
        }
        charInfo->metrics.leftSideBearing  = lbearing;
        charInfo->metrics.rightSideBearing = rbearing;
        charInfo->metrics.ascent           = ascent;
        charInfo->metrics.descent          = descent;
        charInfo->metrics.characterWidth   = width;
        charInfo->metrics.attributes       =
            (char_width.raw ? char_width.raw :
             (unsigned short)(short)(floor(1000 * metrics.advance / 64.
                                           / ft->pixel_size)));
      }
      FontCacheInsertEntry(ft->cache, cache_key_offset+c, entry);
    } else {
        charInfo = &entry->charInfo;
    }
 next:
    return &charInfo->metrics;
}


/* unify get_glyph_prop and get_glyph_const */
static CharInfoPtr
get_glyph(FreeTypeFont *ft, int c, int spacing, int force_c)
{
    FreeTypeFaceInfo *fi = &faceTable[ft->fid];
    FontCacheEntryPtr entry;
    CharInfoPtr charInfo;
    TT_Outline outline;

    int width, height, descent, lbearing;
    int glyph = ft->pFont->glyph;
    int bytes;
    int cache_key_offset;
    int sbit_ok;

    if ( force_c & XTT_FORCE_CONSTANT_SPACING )
        cache_key_offset = XTT_FORCE_CONSTANT_SPACING_CACHE_KEY_OFFSET;
    else cache_key_offset=0;

    /*
     * Check invalid char index.
     */
    if ( c < 0 ) {
        charInfo = &nocharinfo;
        goto next;
    }

    if (FontCacheSearchEntry(ft->cache, cache_key_offset+c, &entry)) {
        if (entry->charInfo.bits != NULL) {
            return &entry->charInfo;
        }
    }
    if (entry == NULL) {
        struct xtt_char_width char_width;
        /* Make charInfo */
        if ( spacing == XTT_MONOSPACED ) {
            char_width.pixel = ft->pFont->info.maxbounds.characterWidth;
            char_width.raw   = ft->pFont->info.maxbounds.attributes;
        } else
            char_width.pixel = char_width.raw = 0;
        get_metrics(ft, c, char_width, force_c);
        /* Retry to get it created in get_metrics(). */
        if (!FontCacheSearchEntry(ft->cache, cache_key_offset+c, &entry)) {
            charInfo = &nocharinfo;
            fprintf(stderr, "get_glyph: can't get cache entry\n");
            goto next;
        }
    }
    charInfo = &entry->charInfo;
    {
        if (fi->sbit && 
            ( IS_TT_Matrix_Unit(ft->matrix) 
              || ft->lsbShiftOfBitmapAutoItalic || ft->rsbShiftOfBitmapAutoItalic ) &&
            ft->isEmbeddedBitmap) {
            if (TT_Load_Glyph_Bitmap(fi->face, ft->instance, c, fi->sbit))
                fi->sbit->map.size = 0;
        } else {
            if (fi->sbit)
                fi->sbit->map.size = 0;
        }
        /*
         * load outline if fails now.
         * But it is better that we are able to select
         * another way, i.e. load NULL glyph if fails.
         */
        sbit_ok=0;
        if ( fi->sbit ) {
            if ( fi->sbit->map.size ) sbit_ok=1;
        }
        if ( !sbit_ok ) {
            TT_Load_Glyph(ft->instance, fi->glyph, c,  ft->flag);
            TT_Get_Glyph_Outline(fi->glyph, &outline);
            outline.dropout_mode = 2;
            TT_Transform_Outline(&outline, &ft->matrix);
        }
        /*
         * set glyph metrics for drawing a character.
         */
        /* prepare to shift */
        lbearing = 0;
        switch (spacing) {
        case XTT_CONSTANT_SPACING:
            /* Make maxbounds bitmap and draw character on it */
            lbearing = ft->pFont->info.maxbounds.leftSideBearing;
            descent  = ft->pFont->info.maxbounds.descent;
            width    = ft->pFont->info.maxbounds.rightSideBearing - lbearing;
            height   = ft->pFont->info.maxbounds.ascent + descent;

            /* Write charInfo->metrics as constant charcell */
            charInfo->metrics = ft->pFont->info.maxbounds;
            break;

        case XTT_MONOSPACED:
            /* shift */
            lbearing -= FC_TT_PRIVATE(entry)->shift;
        case XTT_PROPORTIONAL_SPACING:
        default:
            /* Make just fit bitmap and draw character on it */
            lbearing += charInfo->metrics.leftSideBearing;
            descent   = charInfo->metrics.descent;
            width     = charInfo->metrics.rightSideBearing -
                charInfo->metrics.leftSideBearing;
            height   = charInfo->metrics.ascent + descent;
            break;
        }

        ft->map.rows  = height;
        bytes = (width + 7) / 8;
        /* alignment */
        bytes = (bytes + (glyph) - 1) & -glyph;
        ft->map.cols  = bytes;
        /* In the case of XFree86-4, map.cols should not be zero when height exists. */
        /* (The crash of verdana.ttf) */
        if ( ft->map.cols <= 0 ) ft->map.cols = glyph;
        ft->map.width = width;
        ft->map.flow = TT_Flow_Down;
        ft->map.size  = ft->map.rows * ft->map.cols;
        if (!FontCacheGetBitmap(entry, ft->map.size+2*ft->map.cols)) {
            charInfo = &nocharinfo;
            fprintf(stderr, "can't get glyph image area\n");
            goto next;
        }
        ft->map.bitmap = charInfo->bits;
        if ( ft->map.bitmap == NULL ) goto next;
        if ( bytes <= 0 ) goto next;
        if ( height <= 0 ) goto next;

        /*
         * draw a sbit or an outline glyph
         */
        if ( sbit_ok &&
            (IS_TT_Matrix_Unit(ft->matrix) 
             || ft->lsbShiftOfBitmapAutoItalic || ft->rsbShiftOfBitmapAutoItalic) &&
            ft->isEmbeddedBitmap) {
            /*
             * Metrics on sbits are already cropped.
             * And the parameters, ?_offset, on XTT_Get_SBit_Bitmap are
             * different from TT_Get_Glyph_Bitmap's one.
             */
            XTT_Get_SBit_Bitmap(&ft->map, fi->sbit, -lbearing, descent);
            if (ft->rsbShiftOfBitmapAutoItalic - ft->lsbShiftOfBitmapAutoItalic != 0)
                make_up_italic_bitmap(ft->map.bitmap, ft->map.cols, ft->map.rows,
                                      ft->rsbShiftOfBitmapAutoItalic
                                      - ft->lsbShiftOfBitmapAutoItalic,
                                      ft->pFont->info.maxbounds.ascent
                                      + ft->pFont->info.maxbounds.descent,
                                      ft->pFont->info.maxbounds.ascent 
                                      - charInfo->metrics.ascent,
                                      ft->autoItalic);
        }
        else {
            TT_Translate_Outline(&outline, -lbearing * 64, descent * 64);
            TT_Get_Outline_Bitmap(engine, &outline, &ft->map);
        }
        if (ft->doubleStrike)
            make_up_bold_bitmap(&ft->map,ft->doubleStrike);
        (*ft->convert)(ft, ft->map.bitmap, ft->map.size);
    }
 next:
    return charInfo;
}

static char *get_dummy_bitmap(FreeTypeFont *ft)
{
    int width, height, bytes, size;
    int max_rsb, max_lsb;
    int glyph = ft->pFont->glyph;

    if ( ft->dummy_bitmap ) goto next;
    if ( ft->pFont->info.minbounds.rightSideBearing 
         < ft->pFont->info.maxbounds.rightSideBearing ) 
        max_rsb=ft->pFont->info.maxbounds.rightSideBearing;
    else
        max_rsb=ft->pFont->info.minbounds.rightSideBearing;
    if ( ft->pFont->info.maxbounds.leftSideBearing 
         < ft->pFont->info.minbounds.leftSideBearing ) 
        max_lsb=ft->pFont->info.maxbounds.leftSideBearing;
    else
        max_lsb=ft->pFont->info.minbounds.leftSideBearing;
    width  = max_rsb - max_lsb;
    height = ft->pFont->info.maxbounds.ascent 
        + ft->pFont->info.maxbounds.descent;
    bytes = (width + 7) / 8;
    bytes = (bytes + (glyph) - 1) & -glyph;
    if ( bytes <= 0 ) bytes = glyph;
    size  = (height+2) * bytes;

    ft->dummy_bitmap = 
        (char *)xalloc(sizeof(char)*size);
    if( ft->dummy_bitmap )
        memset(ft->dummy_bitmap,0,sizeof(char)*size);
 next:
    return(ft->dummy_bitmap);
}

int
FreeTypeGetGlyphs (FontPtr pFont,
                   unsigned long count,
                   unsigned char *chars,
                   FontEncoding encoding,
                   unsigned long *pCount,
                   CharInfoPtr *glyphs)
{
    FreeTypeFont *ft = (FreeTypeFont*) pFont->fontPrivate;
    CharInfoPtr *glyphsBase = glyphs;

    int spacing = 0;
    int i,nullbits,ncmark,force_c,out_of_range;
    int force_c_init,force_c_inside;

    dprintf((stderr, "FreeTypeGetGlyphs: %p %d\n", pFont, count));

    switch(ft->spacing) {
    case 'p':
        spacing = XTT_PROPORTIONAL_SPACING;
        break;
    case 'm':
        spacing = XTT_MONOSPACED;
        break;
    case 'c':
        spacing = XTT_CONSTANT_SPACING;
        break;
    }

    if ( ft->forceConstantSpacingBegin <= ft->forceConstantSpacingEnd ) force_c_inside=1;
    else force_c_inside=0;
    force_c_init=0;
    out_of_range=0;
    force_c=force_c_init;
    while (count--) {
        unsigned int c1,c2,c=0;

        *glyphs = &nocharinfo;
        switch (encoding) {
        case Linear8Bit:
        case TwoD8Bit:
            c = *chars++;
            dprintf((stderr, "%04x\n", c));
            break;
        case Linear16Bit:
        case TwoD16Bit:
            force_c=force_c_init;
            c1 = *chars++;
            c2 = *chars++;
            c  = (c1<<8|c2);
            if ( force_c_inside ) {
                if ( c <= ft->forceConstantSpacingEnd && ft->forceConstantSpacingBegin <= c )
                    force_c|=XTT_FORCE_CONSTANT_SPACING;
            }
            else {      /* for GB18030 proportional */
                if ( c <= ft->forceConstantSpacingEnd || ft->forceConstantSpacingBegin <= c )
                    force_c|=XTT_FORCE_CONSTANT_SPACING;
            }
            dprintf((stderr, "code: %02x%02x", c1,c2));
            if (c1 >= pFont->info.firstRow &&
                c1 <= pFont->info.lastRow  &&
                c2 >= pFont->info.firstCol &&
                c2 <= pFont->info.lastCol) {
                out_of_range=0;
            } else {
                out_of_range=1;
                dprintf((stderr, ", out of range.  We use nocharinfo.\n"));
                *glyphs = &nocharinfo;
            }
            break;
        default:
            goto next;
        }
        if ( out_of_range == 0 ) {
            c = ft->codeConverterInfo.ptrCodeConverter(c);
            dprintf((stderr, " ->%04x\n ->", c));
            c = TT_Char_Index(ft->charmap, c);
            dprintf((stderr, "%d\n", c));
            *glyphs = get_glyph(ft, c, spacing, force_c);
        }
    next:
#if 1
        /* fallback for XAA */
        if ( *glyphs == &nocharinfo ) {
            dprintf((stderr, "nocharinfo causes a server crash. Instead We use .notdef glyph.\n"));
            *glyphs = get_glyph(ft, 0, spacing, force_c);
        }
#endif
        glyphs++;
    }

    *pCount = glyphs - glyphsBase;

    /*
      (pci)->bits == NULL crashes the Server when gHeight is not zero.
      So we must check each value of (pci)->bits.  Since operation of
      cash intervenes,  this "for () loop"  *MUST*  be independent of
      the upper "while () loop".
                                        Dec.26,2002  Chisato Yamauchi
     */
    dprintf((stderr, "AddressCheckBegin *pCount=%d\n",*pCount));
    nullbits=0;
    ncmark=-1;
    for ( i=0 ; i<*pCount ; i++ ) {
      /* Marking nocharinfo */
      if ( glyphsBase[i] == &nocharinfo ) {
        if ( ncmark == -1 ) ncmark=i;
      }
      else {
        dprintf((stderr,"[%d]:%x\n",i,glyphsBase[i]->bits));
        if ( glyphsBase[i]->bits == NULL ) {
            glyphsBase[i]->bits = get_dummy_bitmap(ft);
            if ( glyphsBase[i]->bits == NULL ) {
                glyphsBase[i]->metrics.ascent=0;
                glyphsBase[i]->metrics.descent=0;
            }
            nullbits++;
        }
        /*
          The XFree86 sometimes allocates memory with the value of maxbounds.ascent
          and maxbounds.descent. 
          So (*glyphs)->ascent must not become larger than maxbounds.ascent.
          This is the same also about descent.
         */
        if ( ft->isInconsistentMetrics ) {
          if ( pFont->info.maxbounds.ascent < glyphsBase[i]->metrics.ascent ) {
            dprintf((stderr, " Invalid ascent : maxbounds.ascent=%d metrics.ascent=%d [corrected]\n",
                     pFont->info.maxbounds.ascent,glyphsBase[i]->metrics.ascent));
            glyphsBase[i]->metrics.ascent = pFont->info.maxbounds.ascent;
          }
          if ( pFont->info.maxbounds.descent < glyphsBase[i]->metrics.descent ) {
            dprintf((stderr, " Invalid descent : maxbounds.descent=%d metrics.descent=%d [corrected]\n",
                     pFont->info.maxbounds.descent,glyphsBase[i]->metrics.descent));
            glyphsBase[i]->metrics.descent = pFont->info.maxbounds.descent;
          }
        }
      }
    }
#if 1
    /* Never return an address outside cache(for XAA). */
    if ( ncmark != -1 ) *pCount = ncmark;
#endif
    dprintf((stderr, "AddressCheckEnd i=%d nullbits=%d\n",i,nullbits));

    return Successful;
}

int
FreeTypeGetMetrics (FontPtr pFont,
                    unsigned long count,
                    unsigned char *chars,
                    FontEncoding encoding,
                    unsigned long *pCount,
                    xCharInfo **glyphs)
{
    FreeTypeFont *ft = (FreeTypeFont*) pFont->fontPrivate;
    xCharInfo **glyphsBase = glyphs;
    unsigned int c,c1,c2;
    int i,force_c,force_c_init,force_c_inside;

    if ( ft->forceConstantSpacingBegin <= ft->forceConstantSpacingEnd ) force_c_inside=1;
    else force_c_inside=0;
    force_c_init=0;

    /*dprintf((stderr, "FreeTypeGetMetrics: %d\n", count));*/
    if (ft->spacing == 'm' || ft->spacing == 'p') {
        struct xtt_char_width char_width;

        if (ft->spacing == 'm') {
            char_width.pixel = ft->pFont->info.maxbounds.characterWidth;
            char_width.raw = ft->pFont->info.maxbounds.attributes;
        } else
            char_width.pixel = char_width.raw = 0;

        switch (encoding) {
        case Linear8Bit:
        case TwoD8Bit:
            while (count--) {
                c = *chars++;
                c = ft->codeConverterInfo.ptrCodeConverter(c);
                *glyphs = get_metrics(ft, TT_Char_Index(ft->charmap, c),
                                      char_width, 0);
#if 1
                if ( *glyphs == &(&nocharinfo)->metrics ) {        /* fallback */
                    /* dprintf((stderr, "nocharinfo -> Instead We use .notdef glyph.\n")); */
                    *glyphs = get_metrics(ft, 0, char_width, 0);
                }
#endif
                glyphs++;
            }
            break;
        case Linear16Bit:
        case TwoD16Bit:
            force_c=force_c_init;
            while (count--) {
                /* force_c=force_c_init; */
                c1 = *chars++;
                c2 = *chars++;
                c  = (c1<<8|c2);
                if ( force_c_inside ) {
                    if ( c <= ft->forceConstantSpacingEnd && ft->forceConstantSpacingBegin <= c ) {
                        /* force_c|=XTT_FORCE_CONSTANT_SPACING; */
                        *glyphs = &(ft->forceConstantMetrics);
                        glyphs++;
                        continue;
                    }
                }
                else {          /* for GB18030 proportional  */
                    if ( c <= ft->forceConstantSpacingEnd || ft->forceConstantSpacingBegin <= c ) {
                        /* force_c|=XTT_FORCE_CONSTANT_SPACING; */
                        *glyphs = &(ft->forceConstantMetrics);
                        glyphs++;
                        continue;
                    }
                }
                /* not forceConstant */
                if ( c1 >= pFont->info.firstRow &&
                     c1 <= pFont->info.lastRow  &&
                     c2 >= pFont->info.firstCol &&
                     c2 <= pFont->info.lastCol ) {
                    c = ft->codeConverterInfo.ptrCodeConverter(c);
                    *glyphs = get_metrics(ft, TT_Char_Index(ft->charmap, c),
                                          char_width, force_c);
                } else {
                    *glyphs = &(&nocharinfo)->metrics;
                }
#if 1
                if ( *glyphs == &(&nocharinfo)->metrics ) {        /* fallback */
                    /* dprintf((stderr, "nocharinfo -> Instead We use .notdef glyph.\n")); */
                    *glyphs = get_metrics(ft, 0, char_width, force_c);
                }
#endif
                glyphs++;
            }
            break;
        default:
            while (count--) {
                *glyphs = &(&nocharinfo)->metrics;
#if 1
                if ( *glyphs == &(&nocharinfo)->metrics ) {        /* fallback */
                    /* dprintf((stderr, "nocharinfo -> Instead We use .notdef glyph.\n")); */
                    *glyphs = get_metrics(ft, 0, char_width, 0);
                }
#endif
                glyphs++;
            }
        }

        *pCount = glyphs - glyphsBase;
        /*
          The XFree86 sometimes allocates memory with the value of maxbounds.ascent
          and maxbounds.descent. 
          So (*glyphs)->ascent must not become larger than maxbounds.ascent.
          This is the same also about descent.
         */
        if ( ft->isInconsistentMetrics ) {
          for ( i=0 ; i<*pCount ; i++ ) {
            if ( pFont->info.maxbounds.ascent < glyphsBase[i]->ascent ) {
                dprintf((stderr, " Invalid ascent : maxbounds.ascent=%d metrics.ascent=%d [corrected]\n",
                         pFont->info.maxbounds.ascent,glyphsBase[i]->ascent));
                glyphsBase[i]->ascent = pFont->info.maxbounds.ascent;
            }
            if ( pFont->info.maxbounds.descent < glyphsBase[i]->descent ) {
                dprintf((stderr, " Invalid descent : maxbounds.descent=%d metrics.descent=%d [corrected]\n",
                         pFont->info.maxbounds.descent,glyphsBase[i]->descent));
                glyphsBase[i]->descent = pFont->info.maxbounds.descent;
            }
          }
        }
    } else {                                    /* -c- */
        switch (encoding) {
        case Linear8Bit:
        case TwoD8Bit:
            while (count--) {
                chars++; chars++;
                *glyphs++ = &pFont->info.maxbounds;
            }
            break;
        case Linear16Bit:
        case TwoD16Bit:
            while (count--) {
                chars++; chars++;
                *glyphs++ = &pFont->info.maxbounds;
            }
            break;
        }
        *pCount = glyphs - glyphsBase;
    }

    return Successful;
}

void
FreeTypeUnloadFont(FontPtr pFont)
{
    dprintf((stderr, "FreeTypeUnloadFont: %x\n", pFont));
    FreeType_CloseFont(pFont);
}

#ifdef USE_XLFD_AUTO_CONTROL
struct {
    char *name;
    int sign;
} slantinfo[] = {
    { "o", 1 },
    { "ro", -1 }
};
#endif /* USE_XLFD_AUTO_CONTROL - obsoleted. */

static void
adjust_min_max(minc, maxc, tmp)
     xCharInfo  *minc, *maxc, *tmp;
{
#define MINMAX(field,ci) \
    if (minc->field > (ci)->field) \
    minc->field = (ci)->field; \
    if (maxc->field < (ci)->field) \
    maxc->field = (ci)->field;

    MINMAX(ascent, tmp);
    MINMAX(descent, tmp);
    MINMAX(leftSideBearing, tmp);
    MINMAX(rightSideBearing, tmp);
    MINMAX(characterWidth, tmp);

    if ((INT16)minc->attributes > (INT16)tmp->attributes)
        minc->attributes = tmp->attributes;
    if ((INT16)maxc->attributes < (INT16)tmp->attributes)
        maxc->attributes = tmp->attributes;
#undef  MINMAX
}

void
freetype_compute_bounds(FreeTypeFont *ft,
                        FontInfoPtr pinfo,
                        FontScalablePtr vals,
                        struct xtt_char_width char_width)
{
    int row, col;
    unsigned int c;
    xCharInfo minchar, maxchar, *tmpchar = NULL;
    int overlap, maxOverlap;
    long swidth      = 0;
    long total_width = 0;
    int num_cols, num_chars = 0;
    int force_c, force_c_init, skip_ok = 0;
    int force_c_inside;

    minchar.ascent = minchar.descent =
    minchar.leftSideBearing = minchar.rightSideBearing =
    minchar.characterWidth = minchar.attributes = 32767;
    maxchar.ascent = maxchar.descent =
    maxchar.leftSideBearing = maxchar.rightSideBearing =
    maxchar.characterWidth = maxchar.attributes = -32767;
    maxOverlap = -32767;

    if ( ft->forceConstantSpacingBegin <= ft->forceConstantSpacingEnd ) force_c_inside=1;
    else force_c_inside=0;
    /* Parse all glyphs */
    force_c_init=0;
    num_cols = 1 + pinfo->lastCol - pinfo->firstCol;
    for (row = pinfo->firstRow; row <= pinfo->lastRow; row++) {
      if ( skip_ok && tmpchar ) {
        if ( force_c_inside ) {
          if ( ft->forceConstantSpacingBegin < row<<8 && row<<8 < (ft->forceConstantSpacingEnd & 0x0ff00) ) {
            if (tmpchar->characterWidth) {
              num_chars += num_cols;
              swidth += ABS(tmpchar->characterWidth)*num_cols;
              total_width += tmpchar->characterWidth*num_cols;
              continue;
            }
          }
          else skip_ok=0;
        }
        else {          /* for GB18030 proportional */
          if ( ft->forceConstantSpacingBegin < row<<8 || row<<8 < (ft->forceConstantSpacingEnd & 0x0ff00) ) {
            if (tmpchar->characterWidth) {
              num_chars += num_cols;
              swidth += ABS(tmpchar->characterWidth)*num_cols;
              total_width += tmpchar->characterWidth*num_cols;
              continue;
            }
          }
          else skip_ok=0;
        }
      }
      for (col = pinfo->firstCol; col <= pinfo->lastCol; col++) {
          c = row<<8|col;
          force_c=force_c_init;
          if ( force_c_inside ) {
              if ( c <= ft->forceConstantSpacingEnd && ft->forceConstantSpacingBegin <= c )
                  force_c|=XTT_FORCE_CONSTANT_SPACING;
          }
          else {        /* for GB18030 proportional */
              if ( c <= ft->forceConstantSpacingEnd || ft->forceConstantSpacingBegin <= c )
                  force_c|=XTT_FORCE_CONSTANT_SPACING;
          }
#if 0
          fprintf(stderr, "comp_bounds: %x ->", c);
#endif
          if ( skip_ok == 0 || force_c == force_c_init ){
              tmpchar=NULL;
              c = ft->codeConverterInfo.ptrCodeConverter(c);
#if 0
              fprintf(stderr, "%x\n", c);
#endif
              if (c) {
                  tmpchar =
                      get_metrics(ft, TT_Char_Index(ft->charmap, c),
                                  char_width, force_c);
              }
          }
          if ( !tmpchar || tmpchar == &(&nocharinfo)->metrics )
              continue;
          adjust_min_max(&minchar, &maxchar, tmpchar);
          overlap = tmpchar->rightSideBearing - tmpchar->characterWidth;
          if (maxOverlap < overlap)
              maxOverlap = overlap;
          
          if (!tmpchar->characterWidth)
              continue;
          num_chars++;
          swidth += ABS(tmpchar->characterWidth);
          total_width += tmpchar->characterWidth;
          
          if ( force_c & XTT_FORCE_CONSTANT_SPACING ) skip_ok=1;
      }
    }

    /* Check index 0 */
    tmpchar = get_metrics(ft, 0, char_width, 0);
    if ( tmpchar && tmpchar != &(&nocharinfo)->metrics ) {
        adjust_min_max(&minchar, &maxchar, tmpchar);
        overlap = tmpchar->rightSideBearing - tmpchar->characterWidth;
        if (maxOverlap < overlap)
            maxOverlap = overlap;
    }
    if ( 0 <= ft->forceConstantSpacingEnd ) {
        tmpchar = get_metrics(ft, 0, char_width, 
                              force_c_init|XTT_FORCE_CONSTANT_SPACING);
        if ( tmpchar && tmpchar != &(&nocharinfo)->metrics ) {
            adjust_min_max(&minchar, &maxchar, tmpchar);
            overlap = tmpchar->rightSideBearing - tmpchar->characterWidth;
            if (maxOverlap < overlap)
                maxOverlap = overlap;
        }
    }
    /* AVERAGE_WIDTH ... 1/10 pixel unit */
    if (num_chars > 0) {
        swidth = (swidth * 10.0 + num_chars / 2.0) / num_chars;
        if (total_width < 0)
            swidth = -swidth;
        vals->width = swidth;
    } else
        vals->width = 0;

    if (char_width.pixel) {
        maxchar.characterWidth = char_width.pixel;
        minchar.characterWidth = char_width.pixel;
    }

    pinfo->maxbounds     = maxchar;
    pinfo->minbounds     = minchar;
    pinfo->ink_maxbounds = maxchar;
    pinfo->ink_minbounds = minchar;
    pinfo->maxOverlap    = maxOverlap;
}


/*
 * restrict code range
 *
 * boolean for the numeric zone:
 *   results = results & (ranges[0] | ranges[1] | ... ranges[nranges-1])
 */

static void
restrict_code_range(unsigned short *refFirstCol,
                    unsigned short *refFirstRow,
                    unsigned short *refLastCol,
                    unsigned short *refLastRow,
                    fsRange const *ranges, int nRanges)
{
    if (nRanges) {
        int minCol = 256, minRow = 256, maxCol = -1, maxRow = -1;
        fsRange const *r = ranges;
        int i;

        for (i=0; i<nRanges; i++) {
            if (r->min_char_high != r->max_char_high) {
                minCol = 0x00;
                maxCol = 0xff;
            } else {
                if (minCol > r->min_char_low)
                    minCol = r->min_char_low;
                if (maxCol < r->max_char_low)
                    maxCol = r->max_char_low;
            }
            if (minRow > r->min_char_high)
                minRow = r->min_char_high;
            if (maxRow < r->max_char_high)
                maxRow = r->max_char_high;
            r++;
        }

        if (minCol > *refLastCol)
            *refFirstCol = *refLastCol;
        else if (minCol > *refFirstCol)
            *refFirstCol = minCol;

        if (maxCol < *refFirstCol)
            *refLastCol = *refFirstCol;
        else if (maxCol < *refLastCol)
            *refLastCol = maxCol;

        if (minRow > *refLastRow) {
            *refFirstRow = *refLastRow;
            *refFirstCol = *refLastCol;
        } else if (minRow > *refFirstRow)
            *refFirstRow = minRow;

        if (maxRow < *refFirstRow) {
            *refLastRow = *refFirstRow;
            *refLastCol = *refFirstCol;
        } else if (maxRow < *refLastRow)
            *refLastRow = maxRow;
    }
}


static int
restrict_code_range_by_str(int count,unsigned short *refFirstCol,
                          unsigned short *refFirstRow,
                          unsigned short *refLastCol,
                          unsigned short *refLastRow,
                          char const *str)
{
    int nRanges = 0;
    int result = 0;
    fsRange *ranges = NULL;
    char const *p, *q;

    p = q = str;
    for (;;) {
        int minpoint=0, maxpoint=65535;
        long val;

        /* skip comma and/or space */
        while (',' == *p || isspace(*p))
            p++;

        /* begin point */
        if ('-' != *p) {
            val = strtol(p, (char **)&q, 0);
            if (p == q)
                /* end or illegal */
                break;
            if (val<0 || val>65535) {
                /* out of zone */
                break;
            }
            minpoint = val;
            p=q;
        }

        /* skip space */
        while (isspace(*p))
            p++;

        if (',' != *p && '\0' != *p) {
            /* contiune */
            if ('-' == *p)
                /* hyphon */
                p++;
            else
                /* end or illegal */
                break;

            /* skip space */
            while (isspace(*p))
                p++;

            val = strtol(p, (char **)&q, 0);
            if (p != q) {
                if (val<0 || val>65535)
                    break;
                maxpoint = val;
            } else if (',' != *p && '\0' != *p)
                /* end or illegal */
                break;
            p=q;
        } else
            /* comma - single code */
            maxpoint = minpoint;

        if ( count <= 0 && minpoint>maxpoint ) {
            int tmp;
            tmp = minpoint;
            minpoint = maxpoint;
            maxpoint = tmp;
        }

        /* add range */
#if 0
        fprintf(stderr, "zone: 0x%04X - 0x%04X\n", minpoint, maxpoint);
        fflush(stderr);
#endif
        nRanges++;
        ranges = (fsRange *)xrealloc(ranges, nRanges*sizeof(*ranges));
        if (NULL == ranges)
            break;
        {
            fsRange *r = ranges+nRanges-1;

            r->min_char_low = minpoint & 0xff;
            r->max_char_low = maxpoint & 0xff;
            r->min_char_high = (minpoint>>8) & 0xff;
            r->max_char_high = (maxpoint>>8) & 0xff;
        }
    }

    if (ranges) {
        if ( count <= 0 ) {
            restrict_code_range(refFirstCol, refFirstRow, refLastCol, refLastRow,
                                ranges, nRanges);
        }
        else {
            int i;
            fsRange *r;
            for ( i=0 ; i<nRanges ; i++ ) {
                if ( count <= i ) break;
                r = ranges+i;
                refFirstCol[i] = r->min_char_low;
                refLastCol[i] = r->max_char_low;
                refFirstRow[i] = r->min_char_high;
                refLastRow[i] = r->max_char_high;
            }
            result=i;
        }
        xfree(ranges);
    }
    return result;
}

static int compute_new_extents( FontScalablePtr vals, double scale, int lsb, int rsb, int desc, int asc,
                                int *lsb_result, int *rsb_result, int *desc_result, int *asc_result )
{
#define TRANSFORM_POINT(matrix, x, y, dest) \
    ((dest)[0] = (matrix)[0] * (x) + (matrix)[2] * (y), \
     (dest)[1] = (matrix)[1] * (x) + (matrix)[3] * (y))

#define CHECK_EXTENT(lsb, rsb, desc, asc, data) \
    ((lsb) > (data)[0] ? (lsb) = (data)[0] : 0 , \
     (rsb) < (data)[0] ? (rsb) = (data)[0] : 0, \
     (-desc) > (data)[1] ? (desc) = -(data)[1] : 0 , \
     (asc) < (data)[1] ? (asc) = (data)[1] : 0)
    double newlsb, newrsb, newdesc, newasc;
    double point[2];

    /* Compute new extents for this glyph */
    TRANSFORM_POINT(vals->pixel_matrix, lsb, -desc, point);
    newlsb  = point[0];
    newrsb  = newlsb;
    newdesc = -point[1];
    newasc  = -newdesc;
    TRANSFORM_POINT(vals->pixel_matrix, lsb, asc, point);
    CHECK_EXTENT(newlsb, newrsb, newdesc, newasc, point);
    TRANSFORM_POINT(vals->pixel_matrix, rsb, -desc, point);
    CHECK_EXTENT(newlsb, newrsb, newdesc, newasc, point);
    TRANSFORM_POINT(vals->pixel_matrix, rsb, asc, point);
    CHECK_EXTENT(newlsb, newrsb, newdesc, newasc, point);

    /* ???: lsb = (int)floor(newlsb * scale); */
    *lsb_result   = (int)floor(newlsb * scale + 0.5);
    *rsb_result   = (int)floor(newrsb * scale + 0.5);
    *desc_result  = (int)ceil(newdesc * scale - 0.5);
    *asc_result   = (int)floor(newasc * scale + 0.5);

    return 0;
#undef CHECK_EXTENT
#undef TRANSFORM_POINT
}

static int
is_matrix_unit(FreeTypeFont *ft, FontScalablePtr vals)
{
    double base_size;
    TT_Matrix m;

    base_size = hypot(vals->point_matrix[2], vals->point_matrix[3]);

    m.xx = ft->scaleWidth *
        vals->point_matrix[0] / base_size * 65536;
    m.xy = ft->scaleWidth *
        vals->point_matrix[2] / base_size * 65536;
    m.yx = vals->point_matrix[1] / base_size * 65536;
    m.yy = vals->point_matrix[3] / base_size * 65536;

    return(IS_TT_Matrix_Unit(m));
}

int
FreeTypeOpenScalable (
     FontPathElementPtr fpe,
     FontPtr            *ppFont,
     int                flags,
     FontEntryPtr       entry,
     char               *fileName,
     FontScalablePtr    vals,
     fsBitmapFormat     format,
     fsBitmapFormatMask fmask,
     FontPtr            non_cachable_font   /* We don't do licensing */
)
{
    int result = Successful;
    int i;
    FontPtr pFont    = NULL;
    FreeTypeFont *ft = NULL;
    FontInfoPtr pinfo;
    int ret, bit, byte, glyph, scan, image;
    int maxbounds_character_width, force_c_lsb_flag=0, force_c_rsb_flag=0;
    int always_embedded_bitmap;
    int force_c_adjust_width_by_pixel=0;
    int force_c_adjust_lsb_by_pixel=0, force_c_adjust_rsb_by_pixel=0;
    int force_c_representative_metrics_char_code;
    double force_c_scale_b_box_width;
    double force_c_scale_lsb=0.0, force_c_scale_rsb=1.0;

    char xlfdName[MAXFONTNAMELEN];
    char familyname[MAXFONTNAMELEN];
    char charset[MAXFONTNAMELEN], slant[MAXFONTNAMELEN];
    int spacing = 'r'; /* avoid 'uninitialized variable using' warning */

    /* XXX: To avoid gcc to embed memset function implicitly, we need to
            store strings to fixed-size array indirectly with using strcpy. */
    char const *copyright_string_default = 
        "Copyright Notice is not available or not able to read yet.";
#define MAXCOPYRIGHTLEN 256
    char  copyright_string[MAXCOPYRIGHTLEN + 1];

    SDynPropRecValList listPropRecVal;
    FreeTypeOpenFaceHints hints;
    char *dynStrTTFileName   = NULL;
    char *dynStrRealFileName = NULL;
    SPropRecValContainer contRecValue;
    int setwidth_value = 0;

    double base_width, base_height;

    dprintf((stderr,
             "\n+FreeTypeOpenScalable(%x, %x, %x, %x, %s, %x, %x, %x, %x)\n",
             fpe, ppFont, flags, entry, fileName, vals,
             format, fmask, non_cachable_font));

    strcpy(copyright_string, copyright_string_default);

#ifdef DUMP
    DumpFontPathElement(fpe);
    DumpFontEntry(entry);
    DumpFontScalable(vals);
#endif

    hints.isProp = False;
    hints.fontName = fileName;
    hints.charsetName = charset;
    hints.familyName = familyname;
    hints.refListPropRecVal = &listPropRecVal;
    hints.ttcno = 0;

    if (SPropRecValList_new(&listPropRecVal)) {
        result = AllocError;
        goto quit;
    }
    {
        int len = strlen(fileName);
        char *capHead = NULL;
        {
            /* font cap */
            char *p1=NULL, *p2=NULL;

            p1=strrchr(fileName, '/');
            if ( p1 == NULL ) p1 = fileName;
            else p1++;
            if (NULL != (p2=strrchr(p1, ':'))) {
                /* colon exist in the right side of slash. */
                int dirLen = p1-fileName;
                int baseLen = fileName+len - p2 -1;

                dynStrRealFileName = (char *)xalloc(dirLen+baseLen+1);
                if ( dynStrRealFileName == NULL ) {
                    result = AllocError;
                    goto quit;
                }
                if ( 0 < dirLen )
                    memcpy(dynStrRealFileName, fileName, dirLen);
                strcpy(dynStrRealFileName+dirLen, p2+1);
                capHead = p1;
            } else {
                dynStrRealFileName = xstrdup(fileName);
                if ( dynStrRealFileName == NULL ) {
                    result = AllocError;
                    goto quit;
                }
            }
        } /* font cap */

        hints.ttFontName = dynStrRealFileName;

        {
            /* font cap */
            if (capHead)
                if (SPropRecValList_add_by_font_cap(&listPropRecVal,
                                                    capHead)) {
                    result = BadFontPath;
                    goto quit;
                }
        }
    }

    {
        char *p = entry->name.name, *p1=NULL;
        for (i=0; i<13; i++) {
            if (*p) {
                p1 = p + 1;
                if (!(p = strchr(p1,'-'))) p = strchr(p1, '\0');
            }
            switch (i) {
            case 0: /* foundry */
                break;
            case 1: /* family  */
                strncpy(familyname, p1, p-p1);
                familyname[p-p1] = '\0';
                break;
            case 2: /* weight  */
                break;
            case 3: /* slant   */
                strncpy(slant, p1, p-p1);
                slant[p-p1] = '\0';
                break;
            case 4: /* setwidth */
                setwidth_value = 0;
#if 0
                /* Oops, polymorphic XLFD parser is not implemented! */
                if ( '-' != *p1 ) {
                    /* polymorphic setwidth */
                    /* 500 = normal width */
                    /* 1000 = double width */
                    char *endptr;
                    setwidth_value = strtol(p1, &endptr, 10);
                    if ( '-' != *endptr )
                        setwidth_value = 0;
                    if ( setwidth_value>1000 )
                        setwidth_value=1000;
                }
                break;
#endif
            case 5: /* additional style */
            case 6: /* pixel size */
            case 7: /* point size */
            case 8: /* res x */
            case 9: /* res y */
                break;
            case 10: /* spacing */
                spacing = tolower(*p1);
                break;
            case 11: /* avarage width */
                break;
            case 12: /* charset */
                strcpy(charset, p1);
                /* eliminate zone description */
                {
                    char *p2;
                    if (NULL != (p2 = strchr(charset, '[')))
                        *p2 = '\0';
                }
            }
        }
    }
    dprintf((stderr, "charset: %s spacing: %d slant: %s\n",
             charset, spacing, slant));


    /* get face number from Property File directly */
    if ( SPropRecValList_search_record(&listPropRecVal,
                                       &contRecValue,
                                       "FaceNumber"))
        hints.ttcno = SPropContainer_value_int(contRecValue);

    /* set up default values */
    FontDefaultFormat(&bit, &byte, &glyph, &scan);
    /* get any changes made from above */
    ret = CheckFSFormat(format, fmask, &bit, &byte, &scan, &glyph, &image);
    if (ret != Successful) {
        result = ret;
        goto quit;
    }

    /* allocate font struct */
    if (!(pFont = CreateFontRec())) {
        result = AllocError;
        goto quit;
    }

    /* allocate private font data */
    if ((ft = (FreeTypeFont *)xalloc(sizeof(*ft))) == NULL) {
        result = AllocError;
        goto quit;
    }

    /* init private font data */
    memset(ft, 0, sizeof(*ft));

    /* hinting control */
    ft->flag = TTLOAD_DEFAULT;
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "Hinting"))
        /* if not true, turn off hinting
         * some broken truetype font cannot get bitmaps when
         * hinting is applied */
        if (!SPropContainer_value_bool(contRecValue))
            ft->flag = TTLOAD_SCALE_GLYPH;

    /* scaling */
    ft->scaleWidth = 1.0;
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "ScaleWidth")) {
        /* Scaling to Width */
        double scaleWidth = SPropContainer_value_dbl(contRecValue);

        if (scaleWidth<=0.0) {
            fprintf(stderr, "ScaleWitdh needs plus.\n");
            result = BadFontName;
            goto quit;
        }
        ft->scaleWidth = scaleWidth;
    }
    if ( setwidth_value ) {
        ft->scaleWidth *= (double)setwidth_value / 500.0;
    }
    ft->scaleBBoxWidth = 1.0;
    ft->adjustBBoxWidthByPixel = 0;
    ft->adjustLeftSideBearingByPixel = 0;
    ft->adjustRightSideBearingByPixel = 0;
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "ScaleBBoxWidth")) {
        /* Scaling to Bounding Box Width */
        int lv;
        char *endptr,*beginptr;
        double v,scaleBBoxWidth=1.0;
        beginptr = SPropContainer_value_str(contRecValue);
        do {
            if ( strlen(beginptr) < 1 ) break;
            v=strtod(beginptr, &endptr);
            if ( endptr!=beginptr ) {
                scaleBBoxWidth = v;
            }
            if ( *endptr != ';' && *endptr != ',' ) break;
            beginptr=endptr+1;
            lv=strtol(beginptr, &endptr, 10);
            if ( endptr!=beginptr ) {
                ft->adjustBBoxWidthByPixel = lv;
            }
            if ( *endptr != ',' ) break;
            beginptr=endptr+1;
            lv=strtol(beginptr, &endptr, 10);
            if ( endptr!=beginptr ) {
                ft->adjustLeftSideBearingByPixel = lv;
            }
            if ( *endptr != ',' ) break;
            beginptr=endptr+1;
            lv=strtol(beginptr, &endptr, 10);
            if ( endptr!=beginptr ) {
                ft->adjustRightSideBearingByPixel = lv;
            }
        } while ( 0 );

        if (scaleBBoxWidth<=0.0) {
            fprintf(stderr, "ScaleBBoxWitdh needs plus.\n");
            result = BadFontName;
            goto quit;
        }
        ft->scaleBBoxWidth = scaleBBoxWidth;
    }

    ft->spacing = spacing;
#if True /* obsoleted ->->-> */
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "ForceProportional"))
        ft->spacing = SPropContainer_value_bool(contRecValue)?'p':'c';
    else
#endif /* <-<-<- obsoleted */
        if (SPropRecValList_search_record(&listPropRecVal,
                                           &contRecValue,
                                           "ForceSpacing")) {
            char *strSpace = SPropContainer_value_str(contRecValue);
            Bool err = False;

            if (1 != strlen(strSpace))
                err = True;
            else
                switch (strSpace[0]) {
                case 'p':
                case 'm':
                case 'c':
                    ft->spacing = strSpace[0];
                    break;
                default:
                    err = True;
                }
            if (err) {
                result = BadFontName;
                goto quit;
            }
        }
    /* */
    ft->doubleStrike = 0;
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "DoubleStrike")) {
        /* Set or Reset Auto Bold Flag */
        char *strDoubleStrike = SPropContainer_value_str(contRecValue);
        Bool err = False;
        if ( 0 < strlen(strDoubleStrike) ) {
            switch (strDoubleStrike[0]) {
            case 'm':
            case 'M':
            case 'l':
            case 'L':
                ft->doubleStrike |= XTT_DOUBLE_STRIKE;
                ft->doubleStrike |= XTT_DOUBLE_STRIKE_MKBOLD_EDGE_LEFT;
                break;
            case 'y':
            case 'Y':
                ft->doubleStrike |= XTT_DOUBLE_STRIKE;
                break;
            case 'n':
            case 'N':
                ft->doubleStrike = 0;
                break;
            default:
                err = True;
            }
            if ( err != True ) {
                if ( strDoubleStrike[1] ) {
                    switch (strDoubleStrike[1]) {
                    case 'b':
                    case 'B':
                    case 'p':
                    case 'P':
                    case 'y':
                    case 'Y':
                        ft->doubleStrike |= XTT_DOUBLE_STRIKE_CORRECT_B_BOX_WIDTH;
                        break;
                    default:
                        break;
                    }
                }
                if ( ft->doubleStrike & XTT_DOUBLE_STRIKE_MKBOLD_EDGE_LEFT ) {
                    char *comma_ptr=strchr(strDoubleStrike,';');
                    if ( !comma_ptr ) comma_ptr=strchr(strDoubleStrike,',');
                    if ( comma_ptr ) {
                        if ( comma_ptr[1] ) {
                            char *endptr;
                            int mkboldMaxPixel;
                            mkboldMaxPixel=strtol(comma_ptr+1, &endptr, 10);
                            if ( *endptr == '\0' && mkboldMaxPixel <= vals->pixel ) {
                              ft->doubleStrike &= ~XTT_DOUBLE_STRIKE_MKBOLD_EDGE_LEFT;
                            }
                        }
                    }
                }
            }
        }
        else
            err = True;
        if (err) {
            result = BadFontName;
            goto quit;
        }
    }
    /* very lazy metrics */
    ft->isVeryLazy = False;
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "VeryLazyMetrics"))
#if 0
        if (NULL == getenv("NOVERYLAZY"))
            /* If NOVERYLAZY is defined, the vl option is ignored. */
#endif
            ft->isVeryLazy = SPropContainer_value_bool(contRecValue);

    /* embedded bitmap */
    ft->isEmbeddedBitmap = True;
    always_embedded_bitmap=0;
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "EmbeddedBitmap")) {
        char *strEmbeddedBitmap = SPropContainer_value_str(contRecValue);
        Bool err = False;
        if ( 1 == strlen(strEmbeddedBitmap) ) {
            switch (strEmbeddedBitmap[0]) {
            case 'y':
            case 'Y':
                ft->isEmbeddedBitmap = True;
                always_embedded_bitmap=1;
                break;
            case 'u':
            case 'U':
                ft->isEmbeddedBitmap = True;
                always_embedded_bitmap=0;
                break;
            case 'n':
            case 'N':
                ft->isEmbeddedBitmap = False;
                break;
            default:
                err = True;
            }
        }
        else
            err = True;
        if (err) {
            result = BadFontName;
            goto quit;
        }
    }

    if( !is_matrix_unit(ft,vals) ) {
        /* Turn off EmbeddedBitmap when original matrix is not diagonal. */
        ft->isEmbeddedBitmap = False;
    }

    /* slant control */
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "AutoItalic")) {
        ft->autoItalic = SPropContainer_value_dbl(contRecValue);
    }
    else
        ft->autoItalic=0;

    if ( ft->autoItalic != 0 ) {
        vals->pixel_matrix[2] +=
            vals->pixel_matrix[0] * ft->autoItalic;
        vals->point_matrix[2] +=
            vals->point_matrix[0] * ft->autoItalic;
        vals->pixel_matrix[3] +=
            vals->pixel_matrix[1] * ft->autoItalic;
        vals->point_matrix[3] +=
            vals->point_matrix[1] * ft->autoItalic;
    }
#ifdef USE_XLFD_AUTO_CONTROL
    else
        for (i=0; i<sizeof(slantinfo)/sizeof(slantinfo[0]); i++) {
            if (!mystrcasecmp(slant, slantinfo[i].name)) {
                vals->pixel_matrix[2] +=
                    vals->pixel_matrix[0] * slantinfo[i].sign * 0.5;
                vals->point_matrix[2] +=
                    vals->point_matrix[0] * slantinfo[i].sign * 0.5;
                vals->pixel_matrix[3] +=
                    vals->pixel_matrix[1] * slantinfo[i].sign * 0.5;
                vals->point_matrix[3] +=
                    vals->point_matrix[1] * slantinfo[i].sign * 0.5;
                break;
            }
        }
#endif /* USE_XLFD_AUTO_CONTROL - obsoleted. */

    /* Reject ridiculously small font sizes that will blow up the math */
    if   ((base_width = hypot(vals->pixel_matrix[0], vals->pixel_matrix[1])) < 1.0 ||
          (base_height = hypot(vals->pixel_matrix[2], vals->pixel_matrix[3])) < 1.0) {
        fprintf(stderr, "too small font\n");
        result = BadFontName;
        goto quit;
    }

    ft->pixel_size   = base_height;
    ft->pixel_width_unit_x = vals->pixel_matrix[0]/base_height;

    if ((ret = FreeType_OpenFont(ft, vals, glyph, &hints))
        != Successful) {
        result = BadFontName;
        goto quit;
    }

    ft->scaleBitmap = 0.0;
    if((ft->isVeryLazy) &&
       SPropRecValList_search_record(&listPropRecVal,
                                     &contRecValue,
                                     "VeryLazyBitmapWidthScale")) {
        /* Scaling to Bitmap Bounding Box Width */
        double scaleBitmapWidth = SPropContainer_value_dbl(contRecValue);

        if (scaleBitmapWidth<=0.0) {
            fprintf(stderr, "ScaleBitmapWitdh needs plus.\n");
            result = BadFontName;
            goto quit;
        }
        ft->scaleBitmap = scaleBitmapWidth;
    }

    ft->pFont = pFont;
    if (bit == MSBFirst) {
        if (byte == LSBFirst) {
            ft->convert = convertNothing;
        } else {
            ft->convert = convertByteOrder;
        }
    } else {
        if (byte == LSBFirst) {
            ft->convert = convertBitOrder;
        } else {
            ft->convert = convertBitByteOrder;
        }
    }

    /* Fill in font record. Data format filled in by reader. */
    pFont->format = format;
    pFont->bit    = bit;
    pFont->byte   = byte;
    pFont->glyph  = glyph;
    pFont->scan   = scan;
    pFont->get_metrics   = FreeTypeGetMetrics;
    pFont->get_glyphs    = FreeTypeGetGlyphs;
    pFont->unload_font   = FreeTypeUnloadFont;
    pFont->unload_glyphs = NULL;
    pFont->refcnt = 0;
    pFont->fontPrivate = (unsigned char *)ft;

    /* set ColInfo */
    pinfo = &pFont->info;
    pinfo->firstCol  = ft->codeConverterInfo.refCharSetInfo->firstCol;
    pinfo->firstRow  = ft->codeConverterInfo.refCharSetInfo->firstRow;
    pinfo->lastCol   = ft->codeConverterInfo.refCharSetInfo->lastCol;
    pinfo->lastRow   = ft->codeConverterInfo.refCharSetInfo->lastRow;
    pinfo->defaultCh = ft->codeConverterInfo.refCharSetInfo->defaultCh;
    /* restriction of the code range */
    {

        if (SPropRecValList_search_record(&listPropRecVal,
                                           &contRecValue,
                                           "CodeRange")) {
            restrict_code_range_by_str(0,&pinfo->firstCol, &pinfo->firstRow,
                                      &pinfo->lastCol, &pinfo->lastRow,
                                      SPropContainer_value_str(contRecValue));
        }
        restrict_code_range(&pinfo->firstCol, &pinfo->firstRow,
                           &pinfo->lastCol, &pinfo->lastRow,
                           vals->ranges, vals->nranges);
    }

    /* forceConstantSpacing{Begin,End} */
    ft->forceConstantSpacingBegin = -1;
    ft->forceConstantSpacingEnd = -1;
    if ( ft->spacing == 'p' ){
        unsigned short first_col=0,last_col=0x00ff;
        unsigned short first_row=0,last_row=0x00ff;
        if (SPropRecValList_search_record(&listPropRecVal,
                                           &contRecValue,
                                           "ForceConstantSpacingCodeRange")) {
            if ( restrict_code_range_by_str(1,&first_col, &first_row,
                                            &last_col, &last_row,
                                            SPropContainer_value_str(contRecValue)) == 1 ) {
              ft->forceConstantSpacingBegin = (int)( first_row<<8 | first_col );
              ft->forceConstantSpacingEnd = (int)( last_row<<8 | last_col );
              /* fprintf(stderr,"fc=%x-%x\n",ft->forceConstantSpacingBegin,ft->forceConstantSpacingEnd); */
            }
        }
    }

    force_c_representative_metrics_char_code = -2;
    force_c_scale_b_box_width = 1.0;
    if ( ft->spacing == 'p' ){
        unsigned short first_col=pinfo->firstCol,last_col=pinfo->lastCol;
        unsigned short first_row=pinfo->firstRow,last_row=pinfo->lastRow;
        if ( SPropRecValList_search_record(&listPropRecVal,
                                           &contRecValue,
                                           "ForceConstantSpacingMetrics")) {
            char *strMetrics;
            strMetrics = SPropContainer_value_str(contRecValue);
            if ( strMetrics ) {
                char *comma_ptr,*period_ptr,*semic_ptr;
                semic_ptr=strchr(strMetrics,';');
                comma_ptr=strchr(strMetrics,',');
                period_ptr=strchr(strMetrics,'.');
                if ( semic_ptr && comma_ptr ) 
                    if ( semic_ptr < comma_ptr ) comma_ptr=NULL;
                if ( semic_ptr && period_ptr ) 
                    if ( semic_ptr < period_ptr ) period_ptr=NULL;
                if ( !comma_ptr && !period_ptr && strMetrics != semic_ptr ) {
                    if ( restrict_code_range_by_str(1,&first_col, &first_row,
                                                    &last_col, &last_row,
                                                    SPropContainer_value_str(contRecValue)) == 1 ) {
                      force_c_representative_metrics_char_code = 
                          (int)( first_row<<8 | first_col );
                      /* fprintf(stderr,"fm=%x\n",force_c_representative_metrics_char_code); */
                    }
                }
                else {
                    double v;
                    char *endptr,*beginptr=strMetrics;
                    do {
                        v=strtod(beginptr, &endptr);
                        if ( endptr!=beginptr ) {
                            force_c_scale_b_box_width = v;
                        }
                        if ( *endptr != ',' ) break;
                        beginptr=endptr+1;
                        v=strtod(beginptr, &endptr);
                        if ( endptr!=beginptr ) {
                            force_c_scale_lsb = v;
                            force_c_lsb_flag=1;
                        }
                        if ( *endptr != ',' ) break;
                        beginptr=endptr+1;
                        v=strtod(beginptr, &endptr);
                        if ( endptr!=beginptr ) {
                            force_c_scale_rsb = v;
                            force_c_rsb_flag=1;
                        }
                    } while (0);
                }
                if ( semic_ptr ) {
                    int lv;
                    char *endptr,*beginptr=semic_ptr+1;
                    do {
                        lv=strtol(beginptr, &endptr, 10);
                        if ( endptr!=beginptr ) {
                            force_c_adjust_width_by_pixel=lv;
                        }
                        if ( *endptr != ',' ) break;
                        beginptr=endptr+1;
                        lv=strtol(beginptr, &endptr, 10);
                        if ( endptr!=beginptr ) {
                            force_c_adjust_lsb_by_pixel=lv;
                        }
                        if ( *endptr != ',' ) break;
                        beginptr=endptr+1;
                        lv=strtol(beginptr, &endptr, 10);
                        if ( endptr!=beginptr ) {
                            force_c_adjust_rsb_by_pixel=lv;
                        }
                    } while (0);
                }
            }
        }
    }

    force_c_scale_b_box_width *= ft->scaleBBoxWidth;
    force_c_scale_b_box_width *= ft->scaleWidth;
    ft->scaleBBoxWidth *= ft->scaleWidth;

    ft->isInconsistentMetrics = False;

    pinfo->allExist = 0;
    pinfo->drawDirection = LeftToRight;
    pinfo->cachable = 1;
    pinfo->anamorphic = False;  /* XXX ? */

    pFont->info.inkMetrics = 0;
    pFont->info.allExist = 0;
    pFont->info.maxOverlap = 0;
    pFont->info.pad = (short)0xf0f0; /* 0, 0xf0f0 ??? */

    {
        /*
         * calculate exact bounding box
         */
        int raw_ascent, raw_descent, raw_width;     /* RAW */
        int lsb, rsb, desc, asc;
        int force_c_lsb, force_c_rsb, force_c_raw_width;
        int newlsb, newrsb, newdesc, newasc;
        int force_c_newlsb, force_c_newrsb, force_c_newdesc, force_c_newasc;
        double scale;
        struct xtt_char_width char_width = {0,0};
        /* Should we change ?? */
        double lsb_rsb_scale = ft->scaleWidth /* ft->scaleBBoxWidth */ ;
        double force_c_lsb_rsb_scale = ft->scaleWidth /* force_c_scale_b_box_width */ ;

        /*
         * X11's values are not same as TrueType values.
         *  lsb is same.
         *  rsb is not same.  X's rsb is xMax.
         *  asc is same.
         *  desc is negative.
         *
         * NOTE THAT:
         *   `prop' is instance of TT_Face_Properties, and
         *   get by using TT_Get_Face_Properties().
         *   Thus, `prop' applied no transformation.
         */

        asc   = faceTable[ft->fid].prop.horizontal->Ascender;
        desc  = -(faceTable[ft->fid].prop.horizontal->Descender);
        lsb   = faceTable[ft->fid].prop.horizontal->min_Left_Side_Bearing;
        rsb   = faceTable[ft->fid].prop.horizontal->xMax_Extent;
        if (rsb == 0)
            rsb = faceTable[ft->fid].prop.horizontal->advance_Width_Max;

        raw_width   = faceTable[ft->fid].prop.horizontal->advance_Width_Max;
        raw_ascent  = faceTable[ft->fid].prop.horizontal->Ascender;
        raw_descent = -(faceTable[ft->fid].prop.horizontal->Descender);

        ft->PixelAdjustmentBBoxWidthCorrectionRatio=1.0;
        scale = 1.0 / faceTable[ft->fid].prop.header->Units_Per_EM;
        maxbounds_character_width = 
            faceTable[ft->fid].prop.horizontal->advance_Width_Max
            * ft->scaleBBoxWidth;
        /* width: all widths are identical */
        maxbounds_character_width = 
            (int)floor(maxbounds_character_width * vals->pixel_matrix[0]  * scale + 0.5); 
        if ( ft->doubleStrike & XTT_DOUBLE_STRIKE_CORRECT_B_BOX_WIDTH ) {
            ft->PixelAdjustmentBBoxWidthCorrectionRatio = 
                (double)(maxbounds_character_width+ft->adjustBBoxWidthByPixel+1)/maxbounds_character_width;
            maxbounds_character_width += ft->adjustBBoxWidthByPixel+1;
        }
        else {
            ft->PixelAdjustmentBBoxWidthCorrectionRatio = 
                (double)(maxbounds_character_width+ft->adjustBBoxWidthByPixel)/maxbounds_character_width;
            maxbounds_character_width += ft->adjustBBoxWidthByPixel;
        }
        /*
         * Apply scaleBBoxWidth.
         */
        if ( force_c_rsb_flag )
            force_c_rsb = (int)floor(raw_width * force_c_scale_rsb
                                     * force_c_lsb_rsb_scale + 0.5);
        else
            force_c_rsb = (int)floor(rsb * force_c_lsb_rsb_scale + 0.5);
        if ( force_c_lsb_flag )
            force_c_lsb = (int)floor(raw_width * force_c_scale_lsb
                                     * force_c_lsb_rsb_scale);
        else
            force_c_lsb = (int)floor(lsb * force_c_lsb_rsb_scale);
        force_c_raw_width = raw_width * force_c_scale_b_box_width
                            * ft->PixelAdjustmentBBoxWidthCorrectionRatio;
        lsb    = (int)floor(lsb * lsb_rsb_scale);
        rsb    = (int)floor(rsb * lsb_rsb_scale + 0.5);
        raw_width = raw_width * ft->scaleBBoxWidth * ft->PixelAdjustmentBBoxWidthCorrectionRatio;

        compute_new_extents( vals, scale, lsb, rsb, desc, asc,
                             &newlsb, &newrsb, &newdesc, &newasc );
        compute_new_extents( vals, scale, force_c_lsb, force_c_rsb, desc, asc,
                             &force_c_newlsb, &force_c_newrsb, &force_c_newdesc, &force_c_newasc );

        /* 
        fprintf(stderr,"rsb %d -> %d\n",(int)floor(vals->pixel_matrix[0] * rsb * scale + 0.5),newrsb);
        fprintf(stderr,"lsb %d -> %d\n",(int)floor(vals->pixel_matrix[0] * lsb * scale),newlsb);
        */
        if ( !IS_TT_Matrix_Unit(ft->matrix) && ft->isEmbeddedBitmap && 
             always_embedded_bitmap && ft->autoItalic != 0 ) {
            ft->rsbShiftOfBitmapAutoItalic=
                newrsb - (int)floor(vals->pixel_matrix[0] * rsb * scale + 0.5);
            if ( ft->rsbShiftOfBitmapAutoItalic < 0 )
                ft->rsbShiftOfBitmapAutoItalic=0;
            ft->lsbShiftOfBitmapAutoItalic=
                newlsb - (int)floor(vals->pixel_matrix[0] * lsb * scale);
            if ( 0 < ft->lsbShiftOfBitmapAutoItalic )
                ft->lsbShiftOfBitmapAutoItalic=0;
        }
        else {
            ft->rsbShiftOfBitmapAutoItalic=0;
            ft->lsbShiftOfBitmapAutoItalic=0;
        }
        lsb = newlsb;
        rsb = newrsb;
        desc = newdesc;
        asc = newasc;
        force_c_lsb = force_c_newlsb;
        force_c_rsb = force_c_newrsb;

        /*
         * raw_* for are differs.
         */
        force_c_raw_width *= base_width * 1000. * scale / base_height;
        raw_width   *= base_width * 1000. * scale / base_height;
        raw_ascent  *= 1000. * scale;
        raw_descent *= 1000. * scale;

        /*
         * Get sbit line metrics.
         * This line metrics returns the line metrics of font file,
         * but not X's font as you want.
         */
        if ( (IS_TT_Matrix_Unit(ft->matrix) 
              || ft->lsbShiftOfBitmapAutoItalic || ft->rsbShiftOfBitmapAutoItalic ) &&
            faceTable[ft->fid].sbit && ft->isEmbeddedBitmap)
        {
            TT_SBit_Strike strike;
            TT_UFWord adv_width_max =
                faceTable[ft->fid].prop.horizontal->advance_Width_Max;
            int sbit_lsb, sbit_rsb, sbit_desc, sbit_asc;
            int force_c_sbit_lsb, force_c_sbit_rsb;

            if (TT_Get_SBit_Strike(faceTable[ft->fid].face, ft->instance,
                                    &strike))
                faceTable[ft->fid].sbit->map.size = 0;
            else {
                sbit_lsb  = (int)floor(strike.hori.min_origin_SB
                                  * lsb_rsb_scale);
                force_c_sbit_lsb  = (int)floor(strike.hori.min_origin_SB
                                    * force_c_lsb_rsb_scale);
                /* XXX: It is not correct value */
                sbit_rsb  = MAX((int)floor(strike.hori.max_width
                                  * lsb_rsb_scale + .5),
                                (int)floor((adv_width_max * scale
                                  - strike.hori.min_advance_SB)
                                  * lsb_rsb_scale + .5));
                force_c_sbit_rsb  = MAX((int)floor(strike.hori.max_width
                                  * force_c_lsb_rsb_scale + .5),
                                (int)floor((adv_width_max * scale
                                  - strike.hori.min_advance_SB)
                                  * force_c_lsb_rsb_scale + .5));

                sbit_desc = strike.hori.min_after_BL * -1;
                sbit_asc  = strike.hori.max_before_BL;

                sbit_rsb += ft->rsbShiftOfBitmapAutoItalic;
                force_c_sbit_rsb += ft->rsbShiftOfBitmapAutoItalic;

                sbit_lsb += ft->lsbShiftOfBitmapAutoItalic;
                force_c_sbit_lsb += ft->lsbShiftOfBitmapAutoItalic;

                /* We set line metrics as bellow */
                lsb  = MIN(lsb,  sbit_lsb);
                rsb  = MAX(rsb,  sbit_rsb);
                desc = MAX(desc, sbit_desc);
                asc  = MAX(asc,  sbit_asc);
                if ( !force_c_rsb_flag ) force_c_rsb = MAX(force_c_rsb,force_c_sbit_rsb);
                if ( !force_c_lsb_flag ) force_c_lsb = MIN(force_c_lsb,force_c_sbit_lsb);

                dprintf((stderr, "outline: lsb %d, rsb %d, desc %d, asc %d\n",
                         lsb, rsb, desc, asc));
                dprintf((stderr, "sbit: lsb %d, rsb %d, desc %d, asc %d\n",
                         sbit_lsb, sbit_rsb, sbit_desc, sbit_asc));
            }
        }

        /* Pixel adjustment */
        if ( ft->doubleStrike /* & XTT_DOUBLE_STRIKE_CORRECT_B_BOX_WIDTH */ ) {
             rsb++;
             force_c_rsb++;
        }
        lsb         += ft->adjustLeftSideBearingByPixel;
        force_c_lsb += ft->adjustLeftSideBearingByPixel;
        rsb         += ft->adjustRightSideBearingByPixel;
        force_c_rsb += ft->adjustRightSideBearingByPixel;

        /* Metrics for forceConstantSpacing */
        {
            xCharInfo *tmpchar = NULL;
            unsigned int c;
            /* Get Representative Metrics */
            if ( force_c_representative_metrics_char_code == -1 ) {
                tmpchar = get_metrics(ft, 0, char_width, 0);
            }
            else if ( 0 <= force_c_representative_metrics_char_code ) {
                c = force_c_representative_metrics_char_code;
                c = ft->codeConverterInfo.ptrCodeConverter(c);
                tmpchar = get_metrics(ft, TT_Char_Index(ft->charmap, c),
                                      char_width, 0);
                if ( tmpchar == &(&nocharinfo)->metrics ) {
                    tmpchar = get_metrics(ft, 0, char_width, 0);
                }
            }
            if ( tmpchar == &(&nocharinfo)->metrics ) tmpchar=NULL;
            if ( tmpchar ) {
                if ( 0 < tmpchar->characterWidth ) {
                    ft->forceConstantMetrics.leftSideBearing  = tmpchar->leftSideBearing;
                    ft->forceConstantMetrics.rightSideBearing = tmpchar->rightSideBearing;
                    ft->forceConstantMetrics.characterWidth   = tmpchar->characterWidth;
                    ft->forceConstantMetrics.ascent           = tmpchar->ascent;
                    ft->forceConstantMetrics.descent          = tmpchar->descent;
                    ft->forceConstantMetrics.attributes       = tmpchar->attributes;
                }
            }
            else {
                int width = 
                    faceTable[ft->fid].prop.horizontal->advance_Width_Max 
                    * force_c_scale_b_box_width;
                /* width: all widths are identical */
                width = (int)floor(width * vals->pixel_matrix[0]  * scale + 0.5);
                if ( ft->doubleStrike & XTT_DOUBLE_STRIKE_CORRECT_B_BOX_WIDTH ) width++;
                width += ft->adjustBBoxWidthByPixel;
                
                ft->forceConstantMetrics.leftSideBearing  = force_c_lsb;
                ft->forceConstantMetrics.rightSideBearing = force_c_rsb;
                ft->forceConstantMetrics.characterWidth   = width;
                ft->forceConstantMetrics.ascent           = asc;
                ft->forceConstantMetrics.descent          = desc;
                ft->forceConstantMetrics.attributes       = force_c_raw_width;
                
            }
            ft->forceConstantMetrics.leftSideBearing  += force_c_adjust_lsb_by_pixel;
            ft->forceConstantMetrics.rightSideBearing += force_c_adjust_rsb_by_pixel;
            ft->forceConstantMetrics.characterWidth   += force_c_adjust_width_by_pixel;
        }

        /* ComputeBounds */
        if (ft->spacing == 'c' || ft->spacing == 'm') { /* constant width */

            int width =
                maxbounds_character_width;

            /* AVERAGE_WIDTH ... 1/10 pixel unit */
            vals->width = width * 10;

            if (ft->spacing == 'c') { /* constant charcell */
                /* Use same maxbounds and minbounds to make fast. */
                pFont->info.maxbounds.leftSideBearing  = lsb;
                pFont->info.maxbounds.rightSideBearing = rsb;
                pFont->info.maxbounds.characterWidth   = width;
                pFont->info.maxbounds.ascent           = asc;
                pFont->info.maxbounds.descent          = desc;
                pFont->info.maxbounds.attributes       = raw_width;

                pFont->info.minbounds = pFont->info.maxbounds;
            } else { /* monospaced */
                static const struct em_index {
                    char* registry;
                    unsigned index;
                } em_indexarray[] = {
                    {"ascii", 0x4d},
                    {"iso8859", 0x4d},
                    {"iso646", 0x4d},
                    {"jisx0201", 0x4d},
                    {"koi8", 0x4d}
                };
                unsigned i=0;

#define CodeConv(x) ft->codeConverterInfo.ptrCodeConverter(x)
#define NUM_EM_ARRAY   sizeof(em_indexarray)/sizeof(struct em_index)

                /* calculate pedantic way */
                for(;i<NUM_EM_ARRAY;i++) {
                    /* Caution! Only charsets on em_indexarray[] are parsed.     */
                    /* In the case of '-m-', we should check each ascent/descent */
                    /* in FreeTypeGetGlyphs() and FreeTypeGetMetrics().          */
                    if (!strncasecmp(em_indexarray[i].registry,
                                     charset,
                                     strlen(em_indexarray[i].registry))) {
                        short c;
                        xCharInfo *em_char;
                        /* get em-square */
                        c = CodeConv(em_indexarray[i].index);
                        if (c) {
                            em_char =
                                get_metrics(ft,
                                            TT_Char_Index(ft->charmap, c),
                                            char_width, 0);
                            char_width.pixel = em_char->characterWidth;
                            char_width.raw   = em_char->attributes;
                            freetype_compute_bounds(ft, pinfo, vals,
                                                    char_width);
                            goto OK;
                        }
                    }
                }
                /* Use different maxbounds and minbounds
                   to let X check metrics. */
                pFont->info.maxbounds.leftSideBearing  = 0;
                pFont->info.maxbounds.rightSideBearing = rsb;
                pFont->info.maxbounds.characterWidth   = width;
                pFont->info.maxbounds.ascent           = asc;
                pFont->info.maxbounds.descent          = desc;
                pFont->info.maxbounds.attributes       = raw_width;

                pFont->info.minbounds.leftSideBearing  = lsb;
                pFont->info.minbounds.rightSideBearing = 0;
                pFont->info.minbounds.characterWidth   = width;
                pFont->info.minbounds.ascent           = asc;
                pFont->info.minbounds.descent          = desc;
                pFont->info.minbounds.attributes       = 0;

                ft->isInconsistentMetrics = True;
            }

            pFont->info.ink_maxbounds = pFont->info.maxbounds;
            pFont->info.ink_minbounds = pFont->info.minbounds;
            pFont->info.maxOverlap    = rsb - width;

        } else { /* proportional */
            freetype_compute_bounds(ft, pinfo, vals, char_width);
        }
        OK:
        /* set ascent/descent */
        pFont->info.fontAscent  = asc;
        pFont->info.fontDescent = desc;

        /* set name for property */
        strncpy(xlfdName, vals->xlfdName, sizeof(xlfdName));
        FontParseXLFDName(xlfdName, vals, FONT_XLFD_REPLACE_VALUE);
        dprintf((stderr, "name: %s\n", xlfdName));

        {
            /* set copyright notice */
            char* name_string;
            unsigned short name_len, copyright_len;

            int i, n;
            unsigned short  platform, encoding, language, id;

            for (n=0;n<faceTable[ft->fid].prop.num_Names;n++) {
                if (TT_Get_Name_ID(faceTable[ft->fid].face,
                                   n, &platform, &encoding, &language, &id) ||
                    TT_Get_Name_String(faceTable[ft->fid].face,
                                       n, &name_string, &name_len))
                    continue;

                if (id != 0) /* not copyright */
                    continue;

                if ((platform == 1) && /* Macintosh script */
                    (encoding == 0) && /* Roman */
                    ((language == 0) || /* English */
                     (language == 1041))) { /* broken (Canon FontGallay) */
                    copyright_len = MIN(name_len, MAXCOPYRIGHTLEN);
                    memcpy(copyright_string, name_string, copyright_len);
                    /* name_string may NOT be null terminated */
                    copyright_string[copyright_len] = '\0';
                    break;
                }

                if (((platform == 3) && /* Microsoft */
                     ((encoding == 0) || (encoding == 1)) &&
                     ((language & 0x3FF) == 0x009)) /* English */ ||
                    ((platform == 0) && /* Apple Unicode */
                     (encoding == 0))) { /* Default semantics */
                    /* It is 2-byte aligned */
                    name_len = MIN(name_len, MAXCOPYRIGHTLEN * 2);
                    copyright_len = 0;

                    for (i=1;i<name_len;i+=2) {
                        copyright_string[copyright_len++] = name_string[i];
                    }
                    copyright_string[copyright_len] = '\0';
                    break;
                }
            }
        }
        /* set properties */
        freetype_compute_props(&pFont->info, vals,
                               raw_width, raw_ascent, raw_descent,
                               xlfdName,
                               copyright_string);
    }

    /* Set the pInfo flags */
    /* Properties set by FontComputeInfoAccelerators:
       pInfo->noOverlap;
       pInfo->terminalFont;
       pInfo->constantMetrics;
       pInfo->constantWidth;
       pInfo->inkInside;
    */
    FontComputeInfoAccelerators(pinfo);
#ifdef DUMP
    DumpFont(pFont);
#endif
    *ppFont = pFont;

    result = Successful;

 quit:
    if (dynStrTTFileName) xfree(dynStrTTFileName);
    if (dynStrRealFileName) xfree(dynStrRealFileName);
    if ( result != Successful ) {
        if (ft) xfree(ft);
        if (pFont) DestroyFontRec(pFont);
    }
    return result;
}

int
FreeTypeGetInfoScalable(fpe, pFontInfo, entry, fontName, fileName, vals)
     FontPathElementPtr fpe;
     FontInfoPtr        pFontInfo;
     FontEntryPtr   entry;
     FontNamePtr        fontName;
     char        *fileName;
     FontScalablePtr    vals;
{
    FontPtr pfont;
    int flags = 0;
    long format = 0;
    long fmask  = 0;
    int ret;

    dprintf((stderr, "FreeTypeGetInfoScalable\n"));

    ret = FreeTypeOpenScalable(fpe, &pfont, flags, entry,
                               fileName, vals, format, fmask, 0);
    if (ret != Successful)
        return ret;
    *pFontInfo  = pfont->info;

    pfont->info.props = NULL;
    pfont->info.isStringProp = NULL;

    FreeType_CloseFont(pfont);
    return Successful;
}

static FontRendererRec renderers[] =
{
    {
        ".ttf", 4,
        (int (*)()) 0, FreeTypeOpenScalable,
        (int (*)()) 0, FreeTypeGetInfoScalable,
        0, CAP_MATRIX | CAP_CHARSUBSETTING
    },
    {
        ".ttc", 4,
        (int (*)()) 0, FreeTypeOpenScalable,
        (int (*)()) 0, FreeTypeGetInfoScalable,
        0, CAP_MATRIX | CAP_CHARSUBSETTING
    },
    {
        ".TTF", 4,
        (int (*)()) 0, FreeTypeOpenScalable,
        (int (*)()) 0, FreeTypeGetInfoScalable,
        0, CAP_MATRIX | CAP_CHARSUBSETTING
    },
    {
        ".TTC", 4,
        (int (*)()) 0, FreeTypeOpenScalable,
        (int (*)()) 0, FreeTypeGetInfoScalable,
        0, CAP_MATRIX | CAP_CHARSUBSETTING
    }
};

int
XTrueTypeRegisterFontFileFunctions()
{
    int i;
    /* make standard prop */
    freetype_make_standard_props();

    /* reset */
    /* register */
    for (i=0;i<sizeof(renderers)/sizeof(renderers[0]);i++) {
        /* If the user has both the FreeType and the X-TT backends
           linked in, he probably wants X-TT to be used for TrueType
           fonts. */
        FontFilePriorityRegisterRenderer(renderers + i, +10);
    }

    return 0;
}


/* end of file */
