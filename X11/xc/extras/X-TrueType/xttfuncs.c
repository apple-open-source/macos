/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Go Watanabe, All rights reserved.
   Copyright (c) 1998 Kazushi (Jam) Marukawa, All rights reserved.
   Copyright (c) 1998 Takuya SHIOZAKI, All rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved.

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

   Major Release ID: X-TrueType Server Version 1.3 [Aoi MATSUBARA Release 3]

Notice===
*/
/* $XFree86: xc/extras/X-TrueType/xttfuncs.c,v 1.17 2003/02/17 03:59:22 dawes Exp $ */

#include "xttversion.h"

static char const * const releaseID =
    _XTT_RELEASE_NAME;

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

#define IS_TT_Matrix_Unit(matrix) \
(((matrix).xx == 65536) && ((matrix).yx == 0) && \
 ((matrix).xy == 0    ) && ((matrix).yy == 65536))

/*
 * prototypes
 */

/* from lib/font/fontfile/defaults.c */
extern void
FontDefaultFormat(int *bit, int *byte, int *glyph, int *scan);
/* from lib/font/fontfile/renderers.c */
extern Bool
FontFileRegisterRenderer(FontRendererPtr renderer);
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
    int i, error, num;
    TT_Face face;
    TT_Face_Properties prop;
    TT_Glyph glyph;
    TT_SBit_Image* sbit;
    FreeTypeFaceInfoPtr ptr;

    dprintf((stderr,
             "FreeType_OpenFace: %s %s %s\n",
             refHints->fontName, refHints->familyName, refHints->ttFontName));

    if ((error = TT_Open_Face(engine, refHints->ttFontName, &face))) {
        fprintf(stderr, "freetype: can't open face: %s\n", refHints->ttFontName);
        return -1;
    }

    if ((error = TT_Get_Face_Properties(face, &prop))) {
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
        if ((error = TT_Open_Collection(engine, refHints->ttFontName,
                                        refHints->ttcno, &face))) {
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
                if ((error = TT_Get_Face_Properties(face, &prop))) {
                    TT_Close_Face(face);
                    fprintf(stderr, "freetype: can't get face property.\n");
                    return -1;
                }
                if ((num = TT_Get_CharMap_Count(face)) < 0) {
                    TT_Close_Face(face);
                    fprintf(stderr, "freetype: can't get charmap count.\n");
                    return -1;
                }
                if ((error = TT_New_Glyph(face, &glyph))) {
                    TT_Close_Face(face);
                    fprintf(stderr, "freetype: can't get new glyph.\n");
                    return -1;
                }
                {
                    /* check and initialize support stuffs for sbit extesion */
                    TT_EBLC eblc; /* fake */

                    if (!TT_Get_Face_Bitmaps(face, &eblc)) {
                        if ((error = TT_New_SBit_Image(&sbit))) {
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

    if ((error = TT_Get_Face_Properties(face, &prop))) {
        TT_Close_Face(face);
        fprintf(stderr, "freetype: can't get face property.\n");
        return -1;
    }
    if ((num = TT_Get_CharMap_Count(face)) < 0) {
        TT_Close_Face(face);
        fprintf(stderr, "freetype: can't get charmap count.\n");
        return -1;
    }
    if ((error = TT_New_Glyph(face, &glyph))) {
        TT_Close_Face(face);
        fprintf(stderr, "freetype: can't get new glyph.\n");
        return -1;
    }
    {
        /* check support stuff for sbit extesion */
        TT_EBLC eblc; /* use only for checking the stuff */

        if (!TT_Get_Face_Bitmaps(face, &eblc)) {
            if ((error = TT_New_SBit_Image(&sbit))) {
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
    extern void
        BitOrderInvert(register unsigned char *buf, register int nbytes);

    BitOrderInvert(p, size);
}

static void
convertByteOrder(FreeTypeFont *ft, unsigned char *p, int size)
{
    extern void TwoByteSwap(register unsigned char *buf, register int nbytes);
    extern void FourByteSwap(register unsigned char *buf, register int nbytes);

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
    int mapID, fid, error, result;
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
    if ((error = TT_New_Instance(fi->face, &instance))) {
        result = BadFontName;
        goto deinitQuit;
    }

    /* set resolution of instance */
    if ((error = TT_Set_Instance_Resolutions(instance,
                                             (int)vals->x, (int)vals->y))) {
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
        if ((error = TT_Set_Instance_CharSize(instance,
                                              base_size *64))) {
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
make_up_bold_bitmap(TT_Raster_Map *map)
{
    int x, y;
    char *p = (char *)map->bitmap;
    for (y=0; y<map->rows; y++) {
        char lsb = 0;
        for (x=0; x<map->cols; x++) {
            char tmp = *p<<7;
            *p |= (*p>>1) | lsb;
            lsb = tmp;
            p++;
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
get_metrics(FreeTypeFont *ft, int c, struct xtt_char_width char_width)
{
    FreeTypeFaceInfo *fi = &faceTable[ft->fid];

    FontCacheEntryPtr entry;
    CharInfoPtr charInfo;
    int width, ascent, descent;
    int lbearing, rbearing;
    TT_Glyph_Metrics metrics;

    TT_BBox* bbox = &metrics.bbox;

    /*
     * Check invalid char index.
     */
    if ( c < 0 ) {
        charInfo = &nocharinfo;
        goto next;
    }

    if (!FontCacheSearchEntry(ft->cache, c, &entry)) {
        if (!ft->isVeryLazy) {
            /*
             * load sbit's metrics.
             * load outline's metrics if fails now.
             * But it is better that we are able to select
             * another way, i.e. load NULL glyph if fails.
             */
            if ( fi->sbit &&
                 IS_TT_Matrix_Unit(ft->matrix) &&
                 ft->isEmbeddedBitmap ) {
                if ( TT_Load_Glyph_Bitmap(fi->face, ft->instance, c,
                                          fi->sbit) ) {
                    fi->sbit->map.size = 0;
                } else {
                    metrics.bbox     = fi->sbit->metrics.bbox;
                    metrics.bearingX = fi->sbit->metrics.horiBearingX;
                    metrics.bearingY = fi->sbit->metrics.horiBearingY;
                    metrics.advance  = fi->sbit->metrics.horiAdvance;
                }
            } else
                if ( fi->sbit )
                    fi->sbit->map.size = 0;
            if ( (!fi->sbit) || (!fi->sbit->map.size) ) {
                /* load outline's metrics */
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
        } else {
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

        /*
         * Use floor64 and ceil64 to calulate exectly since values might
         * be minus.
         */
        ascent   = FLOOR64(bbox->yMax + 32) / 64;
        descent  = CEIL64(-bbox->yMin - 32) / 64;
        lbearing = FLOOR64(bbox->xMin + 32) / 64;
        rbearing = FLOOR64(bbox->xMax + 32) / 64 + (ft->isDoubleStrike?1:0);

        width    = FLOOR64((int)floor(metrics.advance * ft->scaleBBoxWidth
                              * ft->pixel_width_unit_x + .5) + 32) / 64;

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
        FontCacheInsertEntry(ft->cache, c, entry);
    } else {
        charInfo = &entry->charInfo;
    }
 next:
    return &charInfo->metrics;
}


/* unify get_glyph_prop and get_glyph_const */
static CharInfoPtr
get_glyph(FreeTypeFont *ft, int c, int spacing)
{
    FreeTypeFaceInfo *fi = &faceTable[ft->fid];
    FontCacheEntryPtr entry;
    CharInfoPtr charInfo;
    TT_Outline outline;

    int width, height, descent, lbearing;
    int glyph = ft->pFont->glyph;
    int bytes;

    /*
     * Check invalid char index.
     */
    if ( c < 0 ) {
        charInfo = &nocharinfo;
        goto next;
    }

    if (FontCacheSearchEntry(ft->cache, c, &entry)) {
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
        get_metrics(ft, c, char_width);
        /* Retry to get it created in get_metrics(). */
        if (!FontCacheSearchEntry(ft->cache, c, &entry)) {
            charInfo = &nocharinfo;
            fprintf(stderr, "get_glyph: can't get cache entry\n");
            goto next;
        }
    }
    charInfo = &entry->charInfo;
    {
        if (fi->sbit && IS_TT_Matrix_Unit(ft->matrix) &&
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
        if ((!fi->sbit) || (!fi->sbit->map.size)) {
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
        ft->map.width = width;
        ft->map.flow = TT_Flow_Down;
        ft->map.size  = ft->map.rows * ft->map.cols;
        if (!FontCacheGetBitmap(entry, ft->map.size)) {
            charInfo = &nocharinfo;
            fprintf(stderr, "can't get glyph image area\n");
            goto next;
        }
        ft->map.bitmap = charInfo->bits;
        if ( ft->map.bitmap == NULL ) goto next;

        /*
         * draw a sbit or an outline glyph
         */
        if (fi->sbit && fi->sbit->map.size &&
            IS_TT_Matrix_Unit(ft->matrix) && ft->isEmbeddedBitmap)
            /*
             * Metrics on sbits are already cropped.
             * And the parameters, ?_offset, on XTT_Get_SBit_Bitmap are
             * different from TT_Get_Glyph_Bitmap's one.
             */
            XTT_Get_SBit_Bitmap(&ft->map, fi->sbit, -lbearing, descent);
        else {
            TT_Translate_Outline(&outline, -lbearing * 64, descent * 64);
            TT_Get_Outline_Bitmap(engine, &outline, &ft->map);
        }
        if (ft->isDoubleStrike)
            make_up_bold_bitmap(&ft->map);
        (*ft->convert)(ft, ft->map.bitmap, ft->map.size);
    }
 next:
    return charInfo;
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
    int i,nullbits,ncmark;

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

    while (count--) {
        unsigned int c1=0, c2;

        *glyphs = &nocharinfo;
        switch (encoding) {
        case Linear8Bit:
        case TwoD8Bit:
            c1 = *chars++;
            dprintf((stderr, "%04x\n", c1));
            break;
        case Linear16Bit:
        case TwoD16Bit:
            c1 = *chars++;
            c2 = *chars++;
            dprintf((stderr, "code: %02x%02x", c1,c2));
            if (c1 >= pFont->info.firstRow &&
                c1 <= pFont->info.lastRow  &&
                c2 >= pFont->info.firstCol &&
                c2 <= pFont->info.lastCol) {
                c1 = (c1<<8|c2);
            } else {
                dprintf((stderr, ", out of range.  We use nocharinfo.\n"));
                *glyphs = &nocharinfo;
                goto next;
            }
            break;
        default:
            goto next;
        }

        c1 = ft->codeConverterInfo.ptrCodeConverter(c1);
        dprintf((stderr, " ->%04x\n ->", c1));
        c1 = TT_Char_Index(ft->charmap, c1);
        dprintf((stderr, "%d\n", c1));
        *glyphs = get_glyph(ft, c1, spacing);

    next:
#if 1
        /* fallback for XAA */
        if ( *glyphs == &nocharinfo ) {
            dprintf((stderr, "nocharinfo causes a server crash. Instead We use .notdef glyph.\n"));
            *glyphs = get_glyph(ft, 0, spacing);
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
            glyphsBase[i]->metrics.ascent=0;
            glyphsBase[i]->metrics.descent=0;
            nullbits++;
        }
        /*
          The XFree86 sometimes allocates memory with the value of maxbounds.ascent
          and maxbounds.descent. 
          So (*glyphs)->ascent must not become larger than maxbounds.ascent.
          This is the same also about descent.
         */
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
    unsigned int c=0,c2;
    int i;

    /*dprintf((stderr, "FreeTypeGetMetrics: %d\n", count));*/
    if (ft->spacing == 'm' || ft->spacing == 'p') {
        struct xtt_char_width char_width;

        if (ft->spacing == 'm') {
            char_width.pixel = ft->pFont->info.maxbounds.characterWidth;
            char_width.raw = ft->pFont->info.maxbounds.attributes;
        } else
            char_width.pixel = char_width.raw = 0;

        while (count--) {
            *glyphs = &(&nocharinfo)->metrics;
            switch (encoding) {
            case Linear8Bit:
            case TwoD8Bit:
                c = *chars++;
                break;
            case Linear16Bit:
            case TwoD16Bit:
                c  = *chars++;
                c2 = *chars++;
                if (c  >= pFont->info.firstRow &&
                    c  <= pFont->info.lastRow  &&
                    c2 >= pFont->info.firstCol &&
                    c2 <= pFont->info.lastCol) {
                    c  = (c<<8|c2);
                } else {
                    *glyphs = &(&nocharinfo)->metrics;
                    goto next;
                }
                break;
            default:
                goto next;
            }
            /* dprintf((stderr, "code: %04x ->", c));*/
            c = ft->codeConverterInfo.ptrCodeConverter(c);
            /* dprintf((stderr, "%04x\n", c));*/
            *glyphs = get_metrics(ft, TT_Char_Index(ft->charmap, c),
                                  char_width);
    next:
#if 1
            /* fallback */
            if ( *glyphs == &(&nocharinfo)->metrics ) {
                dprintf((stderr, "nocharinfo -> Instead We use .notdef glyph.\n"));
                *glyphs = get_metrics(ft, 0, char_width);
            }
#endif
            glyphs++;
        }
        *pCount = glyphs - glyphsBase;
        /*
          The XFree86 sometimes allocates memory with the value of maxbounds.ascent
          and maxbounds.descent. 
          So (*glyphs)->ascent must not become larger than maxbounds.ascent.
          This is the same also about descent.
         */
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
    short c;
    xCharInfo minchar, maxchar, *tmpchar = NULL;
    int overlap, maxOverlap;
    long swidth      = 0;
    long total_width = 0;
    int num_chars    = 0;

    minchar.ascent = minchar.descent =
    minchar.leftSideBearing = minchar.rightSideBearing =
    minchar.characterWidth = minchar.attributes = 32767;
    maxchar.ascent = maxchar.descent =
    maxchar.leftSideBearing = maxchar.rightSideBearing =
    maxchar.characterWidth = maxchar.attributes = -32767;
    maxOverlap = -32767;

    for (row = pinfo->firstRow; row <= pinfo->lastRow; row++) {
        for (col = pinfo->firstCol; col <= pinfo->lastCol; col++) {
            c = row<<8|col;
#if 0
            fprintf(stderr, "comp_bounds: %x ->", c);
#endif
            c = ft->codeConverterInfo.ptrCodeConverter(c);
#if 0
            fprintf(stderr, "%x\n", c);
#endif
            if (c) {
                tmpchar =
                    get_metrics(ft, TT_Char_Index(ft->charmap, c),
                                char_width);
                }

            if (!tmpchar || !tmpchar->characterWidth)
                continue;

                adjust_min_max(&minchar, &maxchar, tmpchar);
                overlap = tmpchar->rightSideBearing - tmpchar->characterWidth;
                if (maxOverlap < overlap)
                    maxOverlap = overlap;
                num_chars++;
                swidth += ABS(tmpchar->characterWidth);
                total_width += tmpchar->characterWidth;
        }
    }

    if (num_chars > 0) {
        swidth = (swidth * 10.0 + num_chars / 2.0) / num_chars;
        if (total_width < 0)
            swidth = -swidth;
        vals->width = swidth;
    } else
        vals->width = 0;

    if(char_width.pixel) {
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


static void
restrict_code_range_by_str(unsigned short *refFirstCol,
                          unsigned short *refFirstRow,
                          unsigned short *refLastCol,
                          unsigned short *refLastRow,
                          char const *str)
{
    int nRanges = 0;
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

        if (minpoint>maxpoint) {
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
        restrict_code_range(refFirstCol, refFirstRow, refLastCol, refLastRow,
                           ranges, nRanges);
        xfree(ranges);
    }
}


int
FreeTypeOpenScalable (fpe, ppFont, flags, entry, fileName, vals,
                      format, fmask, non_cachable_font)
     FontPathElementPtr fpe;
     FontPtr            *ppFont;
     int                flags;
     FontEntryPtr       entry;
     char               *fileName;
     FontScalablePtr    vals;
     fsBitmapFormat     format;
     fsBitmapFormatMask fmask;
     FontPtr            non_cachable_font;  /* We don't do licensing */
{
    int result = Successful;
    int i;
    FontPtr pFont;
    FontInfoPtr pinfo;
    FreeTypeFont *ft;
    int ret, bit, byte, glyph, scan, image;

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
    char *dynStrTTFileName = NULL;
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

            if (NULL != (p1=strrchr(fileName, '/')))
                if (NULL != (p2=strrchr(p1, ':'))) {
                    /* colon exist in the right side of slash. */
                    int dirLen = p1-fileName+1;
                    int baseLen = fileName+len - p2 -1;

                    dynStrRealFileName = (char *)xalloc(dirLen+baseLen+1);
                    memcpy(dynStrRealFileName, fileName, dirLen);
                    strcpy(dynStrRealFileName+dirLen, p2+1);
                    capHead = p1+1;
                } else
                    dynStrRealFileName = xstrdup(fileName);
            else
                dynStrRealFileName = xstrdup(fileName);
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

    /* slant control */
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "AutoItalic")) {
        vals->pixel_matrix[2] +=
            vals->pixel_matrix[0] * SPropContainer_value_dbl(contRecValue);
        vals->point_matrix[2] +=
            vals->point_matrix[0] * SPropContainer_value_dbl(contRecValue);
        vals->pixel_matrix[3] +=
            vals->pixel_matrix[1] * SPropContainer_value_dbl(contRecValue);
        vals->point_matrix[3] +=
            vals->point_matrix[1] * SPropContainer_value_dbl(contRecValue);
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
        DestroyFontRec(pFont);
        result = AllocError;
        goto quit;
    }

    /* init private font data */
    memset(ft, 0, sizeof(*ft));

    ft->pixel_size   = base_height;
    ft->pixel_width_unit_x = vals->pixel_matrix[0]/base_height;

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
            return -1;
        }
        ft->scaleWidth = scaleWidth;
    }
    if ( setwidth_value ) {
        ft->scaleWidth *= (double)setwidth_value / 500.0;
    }
    ft->scaleBBoxWidth = 1.0;
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "ScaleBBoxWidth")) {
        /* Scaling to Bounding Box Width */
        double scaleBBoxWidth = SPropContainer_value_dbl(contRecValue);

        if (scaleBBoxWidth<=0.0) {
            fprintf(stderr, "ScaleBBoxWitdh needs plus.\n");
            return -1;
        }
        ft->scaleBBoxWidth = scaleBBoxWidth;
    }
    ft->scaleBBoxWidth *= ft->scaleWidth;
    ft->isDoubleStrike = False;
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "DoubleStrike")) {
        /* Set or Reset Auto Bold Flag */
        if (SPropContainer_value_bool(contRecValue))
            ft->isDoubleStrike = True;
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
                xfree(ft);
                DestroyFontRec(pFont);
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
    if (SPropRecValList_search_record(&listPropRecVal,
                                      &contRecValue,
                                      "EmbeddedBitmap"))
        ft->isEmbeddedBitmap = SPropContainer_value_bool(contRecValue);

    if ((ret = FreeType_OpenFont(ft, vals, glyph, &hints))
        != Successful) {
        xfree(ft);
        DestroyFontRec(pFont);
        result = BadFontName;
        goto quit;
    }

    ft->scaleBitmap = 1.0;
    if((ft->isVeryLazy) &&
       SPropRecValList_search_record(&listPropRecVal,
                                     &contRecValue,
                                     "VeryLazyBitmapWidthScale")) {
        /* Scaling to Bitmap Bounding Box Width */
        double scaleBitmapWidth = SPropContainer_value_dbl(contRecValue);

        if (scaleBitmapWidth<=0.0) {
            fprintf(stderr, "ScaleBitmapWitdh needs plus.\n");
            return -1;
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
            restrict_code_range_by_str(&pinfo->firstCol, &pinfo->firstRow,
                                      &pinfo->lastCol, &pinfo->lastRow,
                                      SPropContainer_value_str(contRecValue));
        }
        restrict_code_range(&pinfo->firstCol, &pinfo->firstRow,
                           &pinfo->lastCol, &pinfo->lastRow,
                           vals->ranges, vals->nranges);
    }

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
        double newlsb, newrsb, newdesc, newasc;
        double point[2];
        double scale;
        struct xtt_char_width char_width = {0,0};

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

        /*
         * Apply scaleBBoxWidth.
         */
        lsb    = (int)floor(lsb * ft->scaleBBoxWidth);
        rsb    = (int)floor(rsb * ft->scaleBBoxWidth + 0.5);
        raw_width = raw_width * ft->scaleBBoxWidth;

#define TRANSFORM_POINT(matrix, x, y, dest) \
    ((dest)[0] = (matrix)[0] * (x) + (matrix)[2] * (y), \
     (dest)[1] = (matrix)[1] * (x) + (matrix)[3] * (y))

#define CHECK_EXTENT(lsb, rsb, desc, asc, data) \
    ((lsb) > (data)[0] ? (lsb) = (data)[0] : 0 , \
     (rsb) < (data)[0] ? (rsb) = (data)[0] : 0, \
     (-desc) > (data)[1] ? (desc) = -(data)[1] : 0 , \
     (asc) < (data)[1] ? (asc) = (data)[1] : 0)

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

        /*
         * TrueType font is scaled.  So we made scaling value.
         */
        scale = 1.0 / faceTable[ft->fid].prop.header->Units_Per_EM;
        /* ???: lsb = (int)floor(newlsb * scale); */
        lsb   = (int)floor(newlsb * scale + 0.5);
        rsb   = (int)floor(newrsb * scale + 0.5);
        desc  = (int)ceil(newdesc * scale - 0.5);
        asc   = (int)floor(newasc * scale + 0.5);

        /*
         * raw_* for are differs.
         */
        raw_width   *= base_width * 1000. * scale / base_height;
        raw_ascent  *= 1000. * scale;
        raw_descent *= 1000. * scale;

        /*
         * Get sbit line metrics.
         * This line metrics returns the line metrics of font file,
         * but not X's font as you want.
         */
        if (IS_TT_Matrix_Unit(ft->matrix) &&
            faceTable[ft->fid].sbit && ft->isEmbeddedBitmap)
        {
            TT_SBit_Strike strike;
            TT_UFWord adv_width_max =
                faceTable[ft->fid].prop.horizontal->advance_Width_Max;
            int scaleBBoxWidth = ft->scaleBBoxWidth;
            int sbit_lsb, sbit_rsb, sbit_desc, sbit_asc;

            if (TT_Get_SBit_Strike(faceTable[ft->fid].face, ft->instance,
                                    &strike))
                faceTable[ft->fid].sbit->map.size = 0;
            else {
                sbit_lsb  = (int)floor(strike.hori.min_origin_SB
                                  * scaleBBoxWidth);
                /* XXX: It is not correct value */
                sbit_rsb  = MAX((int)floor(strike.hori.max_width
                                  * scaleBBoxWidth + .5),
                                (int)floor((adv_width_max * scale
                                  - strike.hori.min_advance_SB)
                                  * scaleBBoxWidth + .5));

                sbit_desc = strike.hori.min_after_BL * -1;
                sbit_asc  = strike.hori.max_before_BL;

                /* We set line metrics as bellow */
                lsb  = MIN(lsb,  sbit_lsb);
                rsb  = MAX(rsb,  sbit_rsb);
                desc = MAX(desc, sbit_desc);
                asc  = MAX(asc,  sbit_asc);

                dprintf((stderr, "outline: lsb %d, rsb %d, desc %d, asc %d\n",
                         lsb, rsb, desc, asc));
                dprintf((stderr, "sbit: lsb %d, rsb %d, desc %d, asc %d\n",
                         sbit_lsb, sbit_rsb, sbit_desc, sbit_asc));
            }
        }


        /* ComputeBounds */
        if (ft->spacing == 'c' || ft->spacing == 'm') { /* constant width */

            int width =
                faceTable[ft->fid].prop.horizontal->advance_Width_Max
                    * ft->scaleBBoxWidth;

            /* width: all widths are identical */
            width = (int)floor(width * vals->pixel_matrix[0]  * scale + 0.5);

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
                                            char_width);
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
    if (dynStrTTFileName)
        xfree(dynStrTTFileName);
    if (dynStrRealFileName)
        xfree(dynStrRealFileName);
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
