/*
Copyright (c) 1997 by Mark Leisher
Copyright (c) 1998-2002 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/* $XFree86: xc/lib/font/FreeType/ftfuncs.c,v 1.27 2003/02/13 03:01:45 dawes Exp $ */

#include "fontmisc.h"

#ifndef FONTMODULE
#include <string.h>
#include <math.h>
#else
#include "Xmd.h"
#include "Xdefs.h"
#include "xf86_ansic.h"
#endif

#include "fntfilst.h"
#include "fontutil.h"
#include "FSproto.h"
#include "freetype/freetype.h"
#include "freetype/ftsizes.h"
#include "freetype/ttnameid.h"
#include "freetype/tttables.h"
#include "freetype/t1tables.h"
#include "freetype/ftxf86.h"

#include "fontenc.h"
#include "ft.h"
#include "ftfuncs.h"

/* The propery names for all the XLFD properties. */

static char *xlfd_props[] = {
    "FOUNDRY",
    "FAMILY_NAME",
    "WEIGHT_NAME",
    "SLANT",
    "SETWIDTH_NAME",
    "ADD_STYLE_NAME",
    "PIXEL_SIZE",
    "POINT_SIZE",
    "RESOLUTION_X",
    "RESOLUTION_Y",
    "SPACING",
    "AVERAGE_WIDTH",
    "CHARSET_REGISTRY",
    "CHARSET_ENCODING",
};


static int ftypeInitP = 0;      /* is the engine initialised? */
static FT_Library ftypeLibrary;

static FTFacePtr faceTable[NUMFACEBUCKETS];

static unsigned
hash(char *string)
{
    int i;
    unsigned u = 0;
    for(i = 0; string[i] != '\0'; i++)
        u = (u<<2) + (unsigned char)string[i];
    return u;
}

static int
ifloor(int x, int y)
{
    if(x >= 0)
        return x/y;
    else
        return x/y - 1;
}

static int
iceil(int x, int y)
{
    return ifloor(x + y - 1, y);
}
  
static int
FreeTypeOpenFace(FTFacePtr *facep, char *fileName)
{
    FT_Error ftrc;
    int bucket;
    FTFacePtr face, otherFace;
    char *realFileName;
    int faceNumber;

    if (!ftypeInitP) {
        ftrc = FT_Init_FreeType(&ftypeLibrary);
        if (ftrc != 0) {
            ErrorF("FreeType: error initializing ftypeEngine: %d\n", ftrc);
            return AllocError;
        }
        ftypeInitP = 1;
    }

    /* Try to find a matching face in the hashtable */
    bucket = hash(fileName)%NUMFACEBUCKETS;
    otherFace = faceTable[bucket];
    while(otherFace) {
        if(strcmp(otherFace->filename, fileName) == 0)
            break;
        otherFace = otherFace->next;
    }
    if(otherFace) {
        MUMBLE1("Returning cached face: %s\n", otherFace->filename);
        *facep = otherFace;
        return Successful;
    }

    /* No cached match; need to make a new one */
    face = (FTFacePtr)xalloc(sizeof(FTFaceRec));
    if(face == NULL) {
        return AllocError;
    }

    face->filename = (char*)xalloc(strlen(fileName)+1);
    if(face->filename == NULL) {
        xfree(face);
        return AllocError;
    }
    strcpy(face->filename, fileName);

    face->instances = NULL;
    face->active_instance = NULL;

    if(FTcheckForTTCName(fileName, &realFileName, &faceNumber)) {
        ftrc = FT_New_Face(ftypeLibrary, realFileName, faceNumber, &face->face);
        xfree(realFileName);
    } else
        ftrc = FT_New_Face(ftypeLibrary, fileName, 0, &face->face);
    if(ftrc != 0) {
        ErrorF("FreeType: couldn't open face %s: %d\n", fileName, ftrc);
        xfree(face->filename);
        xfree(face);
        return BadFontPath;
    }

    /* Insert face in hashtable and return it */
    face->next = faceTable[bucket];
    faceTable[bucket] = face;
    *facep = face;
    return Successful;
}

static void
FreeTypeFreeFace(FTFacePtr face)
{
    int bucket;
    FTFacePtr otherFace;

    if(!face->instances) {
        bucket = hash(face->filename) % NUMFACEBUCKETS;
        if(faceTable[bucket] == face)
            faceTable[bucket] = face->next;
        else {
            otherFace = faceTable[bucket];
            while(otherFace) {
                if(otherFace->next == face)
                    break;
                otherFace = otherFace->next;
            }
            if(otherFace && otherFace->next)
                otherFace->next = otherFace->next->next;
            else
                ErrorF("FreeType: freeing unknown face\n");
        }
        MUMBLE1("Closing face: %s\n", face->filename);
        FT_Done_Face(face->face);
        xfree(face->filename);
        xfree(face);
    }
}

static int
TransEqual(FTNormalisedTransformationPtr t1, FTNormalisedTransformationPtr t2)
{
    if(t1->scale != t2->scale)
        return 0;
    else if(t1->xres != t2->xres || t1->yres != t2->yres)
        return 0;
    else if(t1->nonIdentity != t2->nonIdentity)
        return 0;
    else if(t1->nonIdentity && t2->nonIdentity) {
        return 
            t1->matrix.xx == t2->matrix.xx &&
            t1->matrix.yx == t2->matrix.yx &&
            t1->matrix.yy == t2->matrix.yy &&
            t1->matrix.xy == t2->matrix.xy;
    } else
        return 1;
}

static int
BitmapFormatEqual(FontBitmapFormatPtr f1, FontBitmapFormatPtr f2)
{
    return
        f1->bit == f2->bit &&
        f1->byte == f2->byte &&
        f1->glyph == f2->glyph;
}

static int
FTInstanceMatch(FTInstancePtr instance,
                 char *fileName, FTNormalisedTransformationPtr trans,
                 int charcell, FontBitmapFormatPtr bmfmt) 
{
    if(strcmp(instance->face->filename, fileName) != 0) {
        return 0;
    } else if(!TransEqual(&instance->transformation, trans)) {
        return 0;
    } else if((charcell && instance->monospaced != FT_CHARCELL) ||
              (!charcell && instance->monospaced == FT_CHARCELL)) {
        return 0;
    } else if(!BitmapFormatEqual(&instance->bmfmt, bmfmt)) {
        return 0;
    } else {
        return 1;
    }
}

static int
FreeTypeActivateInstance(FTInstancePtr instance)
{
    FT_Error ftrc;
    if(instance->face->active_instance == instance)
        return Successful;

    ftrc = FT_Activate_Size(instance->size);
    if(ftrc != 0) {
        instance->face->active_instance = NULL;
        ErrorF("FreeType: couldn't activate instance: %d\n", ftrc);
        return FTtoXReturnCode(ftrc);
    }
    FT_Set_Transform(instance->face->face,
                     instance->transformation.nonIdentity ?
                     &instance->transformation.matrix : 0,
                     0);
                            
    instance->face->active_instance = instance;
    return Successful;
}

static int
FreeTypeOpenInstance(FTInstancePtr *instance_return, 
                     char *fileName, FTNormalisedTransformationPtr trans,
                     int charcell, FontBitmapFormatPtr bmfmt)
{
    FT_Error ftrc;
    int xrc;
    FTInstancePtr instance, otherInstance;
    FTFacePtr face;

    xrc = FreeTypeOpenFace(&face, fileName);
    if(xrc != Successful) {
        return xrc;
    }

    /* Search for a matching instance */
    for(otherInstance = face->instances;
        otherInstance;
        otherInstance = otherInstance->next) {
        if(FTInstanceMatch(otherInstance, fileName, trans, charcell, bmfmt))
            break;
    }
    if(otherInstance) {
        MUMBLE("Returning cached instance\n");
        otherInstance->refcount++;
        *instance_return = otherInstance;
        return Successful;
    }

    /* None matching found */
    instance = (FTInstancePtr)xalloc(sizeof(FTInstanceRec));
    if(instance == NULL) {
        return AllocError;
    }

    instance->refcount = 1;
    instance->face = face;

    instance->monospaced = charcell?FT_CHARCELL:0;
    instance->charcellMetrics = 0;
    instance->width = 0;
  
    instance->transformation = *trans;
    instance->bmfmt = *bmfmt;
    instance->nglyphs = instance->face->face->num_glyphs;
    instance->glyphs = NULL;
    instance->available = NULL;

    ftrc = FT_New_Size(instance->face->face, &instance->size);
    if(ftrc != 0) {
        ErrorF("FreeType: couldn't create size object: %d\n", ftrc);
        FreeTypeFreeFace(instance->face);
        xfree(instance);
        return FTtoXReturnCode(ftrc);
    }
    FreeTypeActivateInstance(instance);
    ftrc = FT_Set_Char_Size(instance->face->face, 
                            (int)(trans->scale*(1<<6)+0.5),
                            (int)(trans->scale*(1<<6)+0.5),
                            trans->xres, trans->yres);
    if(ftrc != 0) {
        FT_Done_Size(instance->size);
        FreeTypeFreeFace(instance->face);
        xfree(instance);
        return FTtoXReturnCode(ftrc);
    }

    /* maintain a linked list of instances */
    instance->next = instance->face->instances;
    instance->face->instances = instance;
    
    *instance_return = instance;
    return Successful;
}

static void
FreeTypeFreeInstance(FTInstancePtr instance)
{
    FTInstancePtr otherInstance;

    if(instance->face->active_instance == instance)
        instance->face->active_instance = NULL;
    instance->refcount--;
    if(instance->refcount <= 0) {
        int i,j;
        
        if(instance->face->instances == instance)
            instance->face->instances = instance->next;
        else {
            for(otherInstance = instance->face->instances;
                otherInstance;
                otherInstance = otherInstance->next)
                if(otherInstance->next == instance) {
                    otherInstance->next = instance->next;
                    break;
                }
        }

        FT_Done_Size(instance->size);
        FreeTypeFreeFace(instance->face);

        if(instance->charcellMetrics) {
            xfree(instance->charcellMetrics);
        }
        if(instance->glyphs) {
            for(i = 0; i < iceil(instance->nglyphs, FONTSEGMENTSIZE); i++) {
                if(instance->glyphs[i]) {
                    for(j = 0; j < FONTSEGMENTSIZE; j++) {
                        if(instance->available[i][j] == 
                           FT_AVAILABLE_RASTERISED)
                            xfree(instance->glyphs[i][j].bits);
                    }
                    xfree(instance->glyphs[i]);
                }
            }
            xfree(instance->glyphs);
        }
        if(instance->available) {
            for(i = 0; i < iceil(instance->nglyphs, FONTSEGMENTSIZE); i++) {
                if(instance->available[i])
                    xfree(instance->available[i]);
            }
            xfree(instance->available);
        }
        xfree(instance);
    }
}

static int
FreeTypeInstanceFindGlyph(unsigned idx, FTInstancePtr instance,
                          CharInfoPtr **glyphs, int ***available,
                          int *found, int *segmentP, int *offsetP)
{
    int segment, offset;

    if(idx > instance->nglyphs) {
        *found = 0;
        return Successful;
    }
  
    if(*available == NULL) {
        *available = 
            (int**)xalloc(sizeof(int*) * iceil(instance->nglyphs, 
                                               FONTSEGMENTSIZE));
        if(*available == NULL)
            return AllocError;
        memset((char*)(*available), 0, 
               sizeof(int*) * iceil(instance->nglyphs, FONTSEGMENTSIZE));
    }

    segment = ifloor(idx, FONTSEGMENTSIZE);
    offset = idx - segment * FONTSEGMENTSIZE;

    if((*available)[segment] == NULL) {
        (*available)[segment] = (int*)xalloc(sizeof(int) * FONTSEGMENTSIZE);
        if((*available)[segment] == NULL)
            return AllocError;
        memset((char*)(*available)[segment], 0, sizeof(int) * FONTSEGMENTSIZE);
    }

    if(*glyphs == NULL) {
        *glyphs = (CharInfoPtr*)xalloc(sizeof(CharInfoPtr)*
                                       iceil(instance->nglyphs, 
                                             FONTSEGMENTSIZE));
        if(*glyphs == NULL)
            return AllocError;
        memset((char*)(*glyphs), 0, 
               sizeof(CharInfoPtr)*iceil(instance->nglyphs, FONTSEGMENTSIZE));
    }

    if((*glyphs)[segment] == NULL) {
        (*glyphs)[segment]=
            (CharInfoPtr)xalloc(sizeof(CharInfoRec) * FONTSEGMENTSIZE);
        if((*glyphs)[segment] == NULL)
            return AllocError;
    }

    *found = 1;
    *segmentP = segment;
    *offsetP = offset;
    return Successful;
}

static int
FreeTypeInstanceGetGlyph(unsigned idx, CharInfoPtr *g, FTInstancePtr instance)
{
    int found, segment, offset;
    FT_Error ftrc;
    int xrc;
    int ***available;
    CharInfoPtr **glyphs;
    
    available = &instance->available;
    glyphs = &instance->glyphs;

    xrc = FreeTypeInstanceFindGlyph(idx, instance, glyphs, available,
                                    &found, &segment, &offset);
    if(xrc != Successful)
        return xrc;

    if(!found || (*available)[segment][offset] == FT_AVAILABLE_NO) {
        *g = 0;
        return Successful;
    } else if((*available)[segment][offset] == FT_AVAILABLE_RASTERISED) {
        *g = &(*glyphs)[segment][offset];
        return Successful;
    }
    
    /* Tough: need to rasterise a new glyph. */
    FreeTypeActivateInstance(instance);
    ftrc = FT_Load_Glyph(instance->face->face, idx, 
                         FT_LOAD_RENDER | FT_LOAD_MONOCHROME);
    if(ftrc != 0)
        return FTtoXReturnCode(ftrc);

    if(instance->monospaced == FT_CHARCELL  && 
       (*available)[segment][offset] == 0)
        memcpy((char*)&(*glyphs)[segment][offset].metrics, 
               (char*)instance->charcellMetrics,
               sizeof(xCharInfo));

    xrc = FreeTypeRasteriseGlyph(&(*glyphs)[segment][offset],
                             instance, instance->monospaced == FT_CHARCELL);
    if(xrc != Successful)
        return xrc;
    else
        (*available)[segment][offset] = FT_AVAILABLE_RASTERISED;

    /* Update the width to match the width of the font */
    if(instance->monospaced) {
        if((*available)[segment][offset] >= FT_AVAILABLE_RASTERISED)
            (*glyphs)[segment][offset].metrics.characterWidth = instance->width;
    }

    /* return the glyph */
    if((*available)[segment][offset] >= FT_AVAILABLE_RASTERISED)
        *g = &(*glyphs)[segment][offset];
    else
        *g = 0;
    return Successful;
}

static int
FreeTypeInstanceGetGlyphMetrics(unsigned idx, 
                                xCharInfo **metrics, FTInstancePtr instance)
{
    CharInfoPtr g;
    int xrc;
    int found, segment, offset;
    
    xrc = FreeTypeInstanceFindGlyph(idx, instance, 
                                    &instance->glyphs, &instance->available, 
                                    &found, &segment, &offset);
    if(xrc != Successful)
        return xrc;

    if(!found) {
        *metrics = 0;
        return Successful;
    }
    
    if(instance->available[segment][offset] == 0) {
        if(instance->monospaced < FT_CHARCELL) {
            xrc = FreeTypeInstanceGetGlyph(idx, &g, instance);
            if(xrc != Successful)
                return xrc;
        } else {
            memcpy((char*)&instance->glyphs[segment][offset].metrics,
                   (char*)instance->charcellMetrics,
                   sizeof(xCharInfo));
            instance->available[segment][offset] = FT_AVAILABLE_YES;
        }
    }
    
    if(instance->available[segment][offset] >= FT_AVAILABLE_YES) {
        *metrics = &instance->glyphs[segment][offset].metrics;
    } else
        *metrics = 0;
    
    return Successful;
}
    
int
FreeTypeRasteriseGlyph(CharInfoPtr tgp, FTInstancePtr instance, 
                       int hasMetrics)
{
#define TRANSFORM_X(x_value) \
  ((int)floor((((double)(x_value)*(double)instance->transformation.matrix.xx)/\
               (TWO_SIXTEENTH*TWO_SIXTH))+0.5))
    
#define TRANSFORM_Y(y_value) \
  ((int)floor((((double)(y_value)*(double)instance->transformation.matrix.yy)/\
               (TWO_SIXTEENTH*TWO_SIXTH))+0.5))

#define TRANSFORM_X_RAW(value) \
  ((int)floor((double)(value)/instance->transformation.scale/TWO_SIXTH/(smetrics.x_ppem/72.0)*1000.0+0.5))

    FTFacePtr face;
    FT_Bitmap *bitmap;
    char *raster = NULL;
    FT_Size_Metrics smetrics;
    FT_Glyph_Metrics *metrics;
    int wd, ht, bpr;            /* width, height, bytes per row */
    int dx, dy;
    int leftSideBearing, rightSideBearing, characterWidth, rawCharacterWidth,
        ascent, descent;
    int i = 0, j;

    face = instance->face;
    smetrics = instance->size->metrics;

    FreeTypeActivateInstance(instance);

    metrics = &face->face->glyph->metrics;
    bitmap = &face->face->glyph->bitmap;

    if(hasMetrics) {
        wd = tgp->metrics.rightSideBearing - tgp->metrics.leftSideBearing;
        ht = tgp->metrics.ascent + tgp->metrics.descent;
        dx = -tgp->metrics.leftSideBearing + 
            TRANSFORM_X(metrics->horiBearingX);
        dy = tgp->metrics.ascent - TRANSFORM_Y(metrics->horiBearingY);
    } else {
        wd = bitmap->width;
        ht = bitmap->rows;
        dx = 0;
        dy = 0;
        /* The X convention is to consider a character with an empty
         * bounding box as undefined.  This convention is broken. */
        if(wd <= 0) wd = 1;
        if(ht <= 0) ht = 1;
    }

    /* Make sure rounding doesn't cause a crash in memcpy below */
    if(wd < bitmap->width)
        wd = bitmap->width;
    if(ht < bitmap->rows)
        ht = bitmap->rows;

    bpr = (((wd + (instance->bmfmt.glyph<<3) - 1) >> 3) & 
           -instance->bmfmt.glyph);
    if(tgp) {
        raster = (char*)xalloc(ht * bpr);
        if(raster == NULL) 
            return AllocError;
        memset(raster, 0, ht * bpr);
    }

    if(dx == 0 && dy == 0 && bpr == bitmap->pitch) {
        memcpy(raster, bitmap->buffer, bitmap->rows * bitmap->pitch);
    } else if(dx == 0) {
        for(i = MAX(0, -dy); i < bitmap->rows && i + dy < ht; i++)
            memcpy(raster + (i + dy) * bpr,
                   bitmap->buffer + i * bitmap->pitch,
                   bitmap->pitch);
    } else {
        for(i = MAX(0, -dy); i < bitmap->rows && i + dy < ht; i++) {
            for(j = MAX(0, -dx); j < bitmap->width && j + dx < wd; j++) {
                int set;
                set = (bitmap->buffer[i * bitmap->pitch + j / 8] &
                       1 << (7 - j % 8));
                if(set)
                    raster[(i + dy) * bpr + (j + dx) / 8] |=
                        1 << (7 - (j + dx) % 8);
            }
        }
    }

    tgp->bits = raster;
    
    if(instance->bmfmt.bit == LSBFirst) {
        BitOrderInvert((unsigned char*)(tgp->bits), ht*bpr);
    }
    
    if(instance->bmfmt.byte != instance->bmfmt.bit) {
        switch(instance->bmfmt.scan) {
        case 1:
            break;
        case 2:
            TwoByteSwap((unsigned char*)(tgp->bits), ht*bpr);
            break;
        case 4:
            FourByteSwap((unsigned char*)(tgp->bits), ht*bpr);
            break;
        default:
            ;
        }
    }
    
    
    if(!hasMetrics) {
        leftSideBearing = TRANSFORM_X(metrics->horiBearingX);
        rightSideBearing = wd + TRANSFORM_X(metrics->horiBearingX);
        
        characterWidth = TRANSFORM_X(metrics->horiAdvance);
        rawCharacterWidth = TRANSFORM_X_RAW(metrics->horiAdvance);
    
        ascent = TRANSFORM_Y(metrics->horiBearingY);
        descent = ht - TRANSFORM_Y(metrics->horiBearingY);

        if(tgp) {
            /* Set the glyph metrics. */
            tgp->metrics.attributes = (unsigned short)((short)rawCharacterWidth);
            tgp->metrics.leftSideBearing = leftSideBearing;
            tgp->metrics.rightSideBearing = rightSideBearing;
            tgp->metrics.characterWidth = characterWidth;
            tgp->metrics.ascent = ascent;
            tgp->metrics.descent = descent;
        }
    }
    return Successful;
#undef TRANSFORM_X
#undef TRANSFORM_Y
#undef TRANSFORM_X_RAW
}

static void
FreeTypeFreeFont(FTFontPtr font)
{
    FreeTypeFreeInstance(font->instance);
    if(font->ranges)
        xfree(font->ranges);
    xfree(font);
}

/* Free a font.  If freeProps is 0, don't free the properties. */

static void
FreeTypeFreeXFont(FontPtr pFont, int freeProps)
{
    FTFontPtr tf;
    
    if(pFont) {
        if((tf = (FTFontPtr)pFont->fontPrivate)) {
            FreeTypeFreeFont(tf);
        }
        if(freeProps && pFont->info.nprops>0) {
            xfree(pFont->info.isStringProp);
            xfree(pFont->info.props);
        }
        DestroyFontRec(pFont);
    }
}


/* Unload a font */

static void
FreeTypeUnloadXFont(FontPtr pFont)
{
    MUMBLE("Unloading\n");
    FreeTypeFreeXFont(pFont, 1);
}

/* Add the font properties, including the Font name, the XLFD
   properties, some strings from the font, and various typographical
   data.  We only provide data readily available in the tables in the
   font for now, altough FIGURE_WIDTH would be a good idea as it is
   used by Xaw. */

static int
FreeTypeAddProperties(FTFontPtr font, FontScalablePtr vals, FontInfoPtr info, 
                      char *fontname, 
                      int rawAverageWidth)
{
    int i, j, maxprops;
    char *sp, *ep, val[MAXFONTNAMELEN], *vp;
    FTFacePtr face;
    FTInstancePtr instance;
    FT_Size_Metrics smetrics;
    FTNormalisedTransformationPtr trans;
    int upm;
    TT_OS2 *os2;
    TT_Postscript *post;
    PS_FontInfoRec t1info_rec, *t1info;
    int xlfdProps = 0;
    int ftrc;

    instance = font->instance;
    face = instance->face;
    smetrics = instance->size->metrics;
    trans = &instance->transformation;
    upm =  face->face->units_per_EM;

    os2 = FT_Get_Sfnt_Table(face->face, ft_sfnt_os2);
    post = FT_Get_Sfnt_Table(face->face, ft_sfnt_post);
    ftrc = FT_Get_PS_Font_Info(face->face, &t1info_rec);
    if(ftrc == 0)
        t1info = &t1info_rec;
    else
        t1info = NULL;

    if(t1info) {
        os2 = NULL;
        post = NULL;
    }

    info->nprops = 0;           /* in case we abort */
  
    strcpy(val, fontname);
    if(FontParseXLFDName(val, vals, FONT_XLFD_REPLACE_VALUE)) {
        xlfdProps = 1;
    } else {
        MUMBLE("Couldn't parse XLFD\n");
        xlfdProps = 0;
    }

    maxprops=
        1 +                     /* NAME */
        (xlfdProps ? 14 : 0) +  /* from XLFD */
        8 +
        (os2 ? 6 : 0) +
        (post || t1info? 3 : 0) +
        2;                      /* type */
    
    info->props = (FontPropPtr)xalloc(maxprops * sizeof(FontPropRec));
    if(info->props == NULL)
        return AllocError;
    
    info->isStringProp = (char*)xalloc(maxprops);
    if(info->isStringProp == NULL) {
        xfree(info->props);
        return AllocError;
    }

    memset((char *)info->isStringProp, 0, maxprops);

    i = 0;

    info->props[i].name = MakeAtom("FONT", 4, TRUE);
    info->props[i].value = MakeAtom(val, strlen(val), TRUE);
    info->isStringProp[i] = 1;
    i++;

    if(*val && *(sp = val + 1)) {
        for (j = 0, sp = val + 1; j < 14; j++) {
            if (j == 13)
                /* Handle the case of the final field containing a subset
                   specification. */
                for (ep = sp; *ep && *ep != '['; ep++);
            else
                for (ep = sp; *ep && *ep != '-'; ep++);
            
            info->props[i].name =
                MakeAtom(xlfd_props[j], strlen(xlfd_props[j]), TRUE);
            
            switch(j) {
            case 6:                   /* pixel size */
                info->props[i].value = 
                    (int)(fabs(vals->pixel_matrix[3]) + 0.5);
                i++;
                break;
            case 7:                   /* point size */
                info->props[i].value = 
                    (int)(fabs(vals->point_matrix[3])*10.0 + 0.5);
                i++;
                break;
            case 8:                   /* resolution x */
                info->props[i].value = vals->x;
                i++;
                break;
            case 9:                   /* resolution y */
                info->props[i].value = vals->y;
                i++;
                break;
            case 11:                  /* average width */
                info->props[i].value = vals->width;
                i++;
                break;
            default:                  /* a string */
                info->props[i].value = MakeAtom(sp, ep - sp, TRUE);
                info->isStringProp[i] = 1;
                i++;
            }
            sp = ++ep;
        }
    }

    info->props[i].name = MakeAtom("RAW_AVERAGE_WIDTH", 17, TRUE);
    info->props[i].value = rawAverageWidth;
    i++;

    info->props[i].name = MakeAtom("FONT_ASCENT", 11, TRUE);
    info->props[i].value = info->fontAscent;
    i++;

    info->props[i].name = MakeAtom("RAW_ASCENT", 15, TRUE);
    info->props[i].value = 
      ((double)face->face->ascender/(double)upm*1000.0);
    i++;

    info->props[i].name = MakeAtom("FONT_DESCENT", 12, TRUE);
    info->props[i].value = info->fontDescent;
    i++;

    info->props[i].name = MakeAtom("RAW_DESCENT", 16, TRUE);
    info->props[i].value = 
      -((double)face->face->descender/(double)upm*1000.0);
    i++;

    j = FTGetEnglishName(face->face, TT_NAME_ID_COPYRIGHT,
                         val, MAXFONTNAMELEN);
    vp = val;
    if (j < 0) {
        if(t1info && t1info->notice) {
            vp = t1info->notice;
            j = strlen(vp);
        }
    }
    if(j > 0) {
        info->props[i].name = MakeAtom("COPYRIGHT", 9, TRUE);
        info->props[i].value = MakeAtom(vp, j, TRUE);
        info->isStringProp[i] = 1;
        i++;
    }
    
    j = FTGetEnglishName(face->face, TT_NAME_ID_FULL_NAME,
                         val, MAXFONTNAMELEN);
    vp = val;
    if (j < 0) {
        if(t1info && t1info->full_name) {
            vp = t1info->full_name;
            j = strlen(vp);
        }
    }
    if(j > 0) {
        info->props[i].name = MakeAtom("FACE_NAME", 9, TRUE);
        info->props[i].value = MakeAtom(vp, j, TRUE);
        info->isStringProp[i] = 1;
        i++;
    }

    j = FTGetEnglishName(face->face, TT_NAME_ID_PS_NAME,
                         val, MAXFONTNAMELEN);
    vp = val;
    if (j < 0) {
        if(t1info && t1info->full_name) {
            vp = t1info->full_name;
            j = strlen(vp);
        }
    }
    if(j > 0) {
        info->props[i].name = MakeAtom("_ADOBE_POSTSCRIPT_FONTNAME", 26, TRUE);
        info->props[i].value = MakeAtom(vp, j, TRUE);
        info->isStringProp[i] = 1;
        i++;
    }

  /* These macros handle the case of a diagonal matrix.  They convert
     FUnits into pixels. */
#define TRANSFORM_FUNITS_X(xval) \
  ((int) \
   floor((((double)(xval)/(double)upm) * \
          ((double)trans->matrix.xx/TWO_SIXTEENTH)*(double)smetrics.x_ppem)+\
         0.5))

#define TRANSFORM_FUNITS_Y(yval) \
  ((int) \
   floor((((double)(yval)/(double)upm) * \
          ((double)trans->matrix.yy/TWO_SIXTEENTH) * (double)smetrics.y_ppem)+\
         0.5))

  /* In what follows, we assume the matrix is diagonal.  In the rare
     case when it is not, the values will be somewhat wrong. */
  
    if(os2) {
        info->props[i].name = MakeAtom("SUBSCRIPT_SIZE",14,TRUE);
        info->props[i].value = 
            TRANSFORM_FUNITS_Y(os2->ySubscriptYSize);
        i++;
        info->props[i].name = MakeAtom("SUBSCRIPT_X",11,TRUE);
        info->props[i].value = 
            TRANSFORM_FUNITS_X(os2->ySubscriptXOffset);
        i++;
        info->props[i].name = MakeAtom("SUBSCRIPT_Y",11,TRUE);
        info->props[i].value = 
            TRANSFORM_FUNITS_Y(os2->ySubscriptYOffset);
        i++;
        info->props[i].name = MakeAtom("SUPERSCRIPT_SIZE",16,TRUE);
        info->props[i].value = 
            TRANSFORM_FUNITS_Y(os2->ySuperscriptYSize);
        i++;
        info->props[i].name = MakeAtom("SUPERSCRIPT_X",13,TRUE);
        info->props[i].value = 
            TRANSFORM_FUNITS_X(os2->ySuperscriptXOffset);
        i++;
        info->props[i].name = MakeAtom("SUPERSCRIPT_Y",13,TRUE);
        info->props[i].value = 
        TRANSFORM_FUNITS_Y(os2->ySuperscriptYOffset);
        i++;
    }

    if(post || t1info) {
        int underlinePosition, underlineThickness;

        if(post) {
            underlinePosition = TRANSFORM_FUNITS_Y(post->underlinePosition);
            underlineThickness = TRANSFORM_FUNITS_Y(post->underlineThickness);
        } else {
            underlinePosition = 
                TRANSFORM_FUNITS_Y(t1info->underline_position);
            underlineThickness = 
                TRANSFORM_FUNITS_Y(t1info->underline_thickness);
        }
        if(underlineThickness <= 0)
            underlineThickness = 1;

        info->props[i].name = MakeAtom("UNDERLINE_THICKNESS",19,TRUE);
        info->props[i].value = underlineThickness;
        i++;

        info->props[i].name = MakeAtom("UNDERLINE_POSITION",18,TRUE);
        info->props[i].value = underlinePosition;
        i++;

        /* The italic angle is often unreliable for Type 1 fonts */
        if(post && trans->matrix.xx == trans->matrix.yy) {
            info->props[i].name = MakeAtom("ITALIC_ANGLE",12,TRUE);
            info->props[i].value = 
                /* Convert from TT_Fixed to 
                   64th of a degree counterclockwise from 3 o'clock */
                90*64+(post->italicAngle >> 10);
            i++;
        }
#undef TRANSFORM_FUNITS_X
#undef TRANSFORM_FUNITS_Y
    }

    info->props[i].name  = MakeAtom("FONT_TYPE", 9, TRUE);
    vp = (char *)FT_Get_X11_Font_Format(face->face);
    info->props[i].value = MakeAtom(vp, strlen(vp), TRUE);
    info->isStringProp[i] = 1;
    i++;
    
    info->props[i].name  = MakeAtom("RASTERIZER_NAME", 15, TRUE);
    info->props[i].value = MakeAtom("FreeType", 10, TRUE);
    info->isStringProp[i] = 1;
    i++;
    
    info->nprops = i;
    return Successful;
}

static int
FreeTypeFontGetGlyph(unsigned code, CharInfoPtr *g, FTFontPtr font)
{
    unsigned idx;
    int i;
    
    /* As a special case, we pass 0 even when it is not in the ranges;
       this will allow for the default glyph, which should exist in any
       TrueType font. */
    
    if(code > 0 && font->nranges) {
        for(i = 0; i < font->nranges; i++)
            if((code >= 
                font->ranges[i].min_char_low+
                (font->ranges[i].min_char_high<<8)) &&
               (code <=
                font->ranges[i].max_char_low + 
                (font->ranges[i].max_char_high<<8)))
                break;
        if(i == font->nranges) {
            *g = 0;
            return Successful;
        }
    }
    
    idx = FTRemap(font->instance->face->face, &font->mapping, code);
    
    /* Only pass the default glyph if there is no first index */
    if(idx == 0 &&
       (code != 0 ||
        (font->mapping.mapping &&
         (font->mapping.mapping->encoding->first != 0 || 
          font->mapping.mapping->encoding->first_col != 0)))) {
        *g = 0;
        return Successful;
    } else {
        return FreeTypeInstanceGetGlyph(idx, g, font->instance);
    }
}

static int
FreeTypeFontGetGlyphMetrics(unsigned code, xCharInfo **metrics, FTFontPtr font)
{
    unsigned idx;
    int i;

    /* As a special case, we pass 0 even when it is not in the ranges;
       this will allow for the default glyph, which should exist in any
       TrueType font. */

    if(code>0 && font->nranges) {
        for(i = 0; i < font->nranges; i++)
            if((code >= 
                font->ranges[i].min_char_low+
                (font->ranges[i].min_char_high<<8)) &&
               (code <=
                font->ranges[i].max_char_low+(font->ranges[i].max_char_high<<8)))
                break;
        if(i == font->nranges) {
            *metrics = 0;
            return Successful;
        }
    }
    
    idx = FTRemap(font->instance->face->face, &font->mapping, code);

    if(idx == 0 && 
       (code != 0 ||
        (font->mapping.mapping &&
         (font->mapping.mapping->encoding->first != 0 || 
          font->mapping.mapping->encoding->first_col != 0)))) {
        *metrics = 0;
        return Successful;
    } else {
        return FreeTypeInstanceGetGlyphMetrics(idx, metrics, font->instance);
    }
}

static int
FreeTypeFontGetDefaultGlyph(CharInfoPtr *g, FTFontPtr font)
{
    /* Disable default glyph generation if there is a first index */
    if(font->mapping.mapping && 
       (font->mapping.mapping->encoding->first || 
        font->mapping.mapping->encoding->first_col)) {
        *g = 0;
        return Successful;
    }
    
    /* Using FreeTypeInstanceGetGlyph(0,...) would cause inconsistencies
       between metrics and glyphs in the unlikely case that 0 is not
       mapped to 0. */
    return FreeTypeFontGetGlyph(0, g, font);
}

static int
FreeTypeLoadFont(FTFontPtr *font_return, char *fileName, 
                 FontScalablePtr vals, FontEntryPtr entry,
                 FontBitmapFormatPtr bmfmt)
{
    int xrc;
    FTFontPtr font;
    FTNormalisedTransformationRec trans;
    int charcell;
    
    font = (FTFontPtr)xalloc(sizeof(FTFontRec));
    if(font == NULL)
        return AllocError;

    /* Compute the transformation matrix.  We use floating-point
       arithmetic for simplicity */
    
    trans.xres = vals->x;
    trans.yres = vals->y;
    
    /* This value cannot be 0. */
    trans.scale = MAX(hypot(vals->point_matrix[0], vals->point_matrix[2]),
                      hypot(vals->point_matrix[1], vals->point_matrix[3]));
    trans.nonIdentity = 0;
    
    /* Try to round stuff.  We want approximate zeros to be exact zeros,
       and if the elements on the diagonal are approximately equal, we
       want them equal.  We do this to avoid breaking hinting. */
    if(DIFFER(vals->point_matrix[0], vals->point_matrix[3])) {
        trans.nonIdentity = 1;
        trans.matrix.xx =
            (int)((vals->point_matrix[0]*(double)TWO_SIXTEENTH)/trans.scale);
        trans.matrix.yy =
            (int)((vals->point_matrix[3]*(double)TWO_SIXTEENTH)/trans.scale);
    } else {
        trans.matrix.xx = trans.matrix.yy =
            ((vals->point_matrix[0] + vals->point_matrix[3])/2*
             (double)TWO_SIXTEENTH)/trans.scale;
    }
    
    if(DIFFER0(vals->point_matrix[1], trans.scale)) {
        trans.matrix.yx =
            (int)((vals->point_matrix[1]*(double)TWO_SIXTEENTH)/trans.scale);
        trans.nonIdentity = 1;
    } else
        trans.matrix.yx = 0;
    
    if(DIFFER0(vals->point_matrix[2], trans.scale)) {
        trans.matrix.xy =
            (int)((vals->point_matrix[2]*(double)TWO_SIXTEENTH)/trans.scale);
        trans.nonIdentity = 1;
    } else
        trans.matrix.xy=0;
    
    /* Check for charcell in XLFD */
    charcell = 0;
    if(entry->name.ndashes == 14) {
        char *p;
        int dashes = 0;
        for(p = entry->name.name; 
            p <= entry->name.name + entry->name.length - 2;
            p++) {
            if(*p == '-') {
                dashes++;
                if(dashes == 11) {
                    if(p[1]=='c' && p[2]=='-')
                        charcell=1;
                    break;
                }
            }
        }
    }
    
    xrc = FreeTypeOpenInstance(&font->instance, 
                               fileName, &trans, charcell, bmfmt);
    if(xrc != Successful)
        return xrc;
    
    if(entry->name.ndashes == 14) {
        if(FTPickMapping(entry->name.name, entry->name.length, fileName,
                         font->instance->face->face, &font->mapping))
            return BadFontFormat;
    } else {
        if(FTPickMapping(0, 0, fileName, 
                         font->instance->face->face, &font->mapping))
            return BadFontFormat;
    }
    

    font->nranges = vals->nranges;
    font->ranges = 0;
    if(font->nranges) {
        font->ranges = (fsRange*)xalloc(vals->nranges*sizeof(fsRange));
        if(font->ranges == NULL) {
            FreeTypeFreeFont(font);
            return AllocError;
        }
        memcpy((char*)font->ranges, (char*)vals->ranges,
               vals->nranges*sizeof(fsRange));
    }
    *font_return = font;
    
    return Successful;
}

/* Given a BBox in FUnits, return a transformed BBox in pixels */
static void
transformBBox(FTNormalisedTransformationPtr transformation, 
              int upm, int x_ppem, int y_ppem,
              int x1, int y1, int x2, int y2,
              int *tx1p, int *ty1p, int *tx2p, int *ty2p)
{
    double 
        xx1, yy1, xx2, yy2, 
        tx11, ty11, tx12, ty12, tx21, ty21, tx22, ty22,
        tx1, ty1, tx2, ty2;
    
    /* Convert arguments to EM units */

    xx1 = ((double)x1/(double)upm);
    yy1 = ((double)y1/(double)upm);
    xx2 = ((double)x2/(double)upm);
    yy2 = ((double)y2/(double)upm);

    /* Apply transformation matrix */

    if(!transformation->nonIdentity) {
        tx1 = xx1;
        ty1 = yy1;
        tx2 = xx2;
        ty2 = yy2;
    } else {
        /* Not an identity matrix, need to compute images of all corners */
        tx11 =
            (transformation->matrix.xx/TWO_SIXTEENTH)*xx1 +
            (transformation->matrix.xy/TWO_SIXTEENTH)*yy1;
        ty11 =
            (transformation->matrix.yx/TWO_SIXTEENTH)*xx1 +
            (transformation->matrix.yy/TWO_SIXTEENTH)*yy1;
        tx12 =
            (transformation->matrix.xx/TWO_SIXTEENTH)*xx1 +
            (transformation->matrix.xy/TWO_SIXTEENTH)*yy2;
        ty12 =
            (transformation->matrix.yx/TWO_SIXTEENTH)*xx1 +
            (transformation->matrix.yy/TWO_SIXTEENTH)*yy2;
        tx21 =
            (transformation->matrix.xx/TWO_SIXTEENTH)*xx2 +
            (transformation->matrix.xy/TWO_SIXTEENTH)*yy1;
        ty21 =
            (transformation->matrix.yx/TWO_SIXTEENTH)*xx2 +
            (transformation->matrix.yy/TWO_SIXTEENTH)*yy1;
        tx22 =
            (transformation->matrix.xx/TWO_SIXTEENTH)*xx2 +
            (transformation->matrix.xy/TWO_SIXTEENTH)*yy2;
        ty22 =
            (transformation->matrix.yx/TWO_SIXTEENTH)*xx2 +
            (transformation->matrix.yy/TWO_SIXTEENTH)*yy2;

        /* Compute the corners of the new bounding box */

        tx1 = MIN(MIN(tx11,tx12),MIN(tx21,tx22));
        ty1 = MIN(MIN(ty11,ty12),MIN(ty21,ty22));
        tx2 = MAX(MAX(tx11,tx12),MAX(tx21,tx22));
        ty2 = MAX(MAX(ty11,ty12),MAX(ty21,ty22));
    }


    /* Convert to device space */
    *tx1p = (int)floor(tx1*(double)x_ppem);
    *ty1p = (int)floor(ty1*(double)y_ppem);
    *tx2p = (int)ceil(tx2*(double)x_ppem);
    *ty2p = (int)ceil(ty2*(double)y_ppem);

    /* Ensure the resulting bounding box is not empty */
    if(*tx1p == *tx2p)
        (*tx2p)++;
    if(*ty1p == *ty2p)
        (*ty2p)++;
}

static int
is_fixed_width(FT_Face face)
{
    PS_FontInfoRec t1info_rec;
    int ftrc;

    if(FT_IS_FIXED_WIDTH(face)) {
        return 1;
    }

    ftrc = FT_Get_PS_Font_Info(face, &t1info_rec);
    if(ftrc == 0 && t1info_rec.is_fixed_pitch) {
        return 1;
    }

    return 0;
}

/* Do all the real work for OpenFont or FontInfo */
/* xf->info is only accessed through info, and xf might be null */

static int
FreeTypeLoadXFont(char *fileName, 
                  FontScalablePtr vals, FontPtr xf, FontInfoPtr info,
                  FontBitmapFormatPtr bmfmt, FontEntryPtr entry)
{
#define TRANSFORM_FUNITS_X(xval) \
  ((int) \
   floor((((double)(xval)/(double)upm) * \
          ((double)instance->transformation.matrix.xx/TWO_SIXTEENTH)*(double)smetrics.x_ppem)+0.5))
#define TRANSFORM_FUNITS_X_DOWN(xval) \
  ((int) \
   floor((((double)(xval)/(double)upm) * \
          ((double)instance->transformation.matrix.xx/TWO_SIXTEENTH)*(double)smetrics.x_ppem)))
#define TRANSFORM_FUNITS_X_UP(xval) \
  ((int) \
   ceil((((double)(xval)/(double)upm) * \
         ((double)instance->transformation.matrix.xx/TWO_SIXTEENTH)*(double)smetrics.x_ppem)))
#define TRANSFORM_FUNITS_Y(yval) \
  ((int) \
   floor((((double)(yval)/(double)upm) * \
          ((double)instance->transformation.matrix.yy/TWO_SIXTEENTH)*(double)smetrics.x_ppem)+0.5))
#define TRANSFORM_FUNITS_Y_DOWN(yval) \
  ((int) \
   floor((((double)(yval)/(double)upm) * \
          ((double)instance->transformation.matrix.yy/TWO_SIXTEENTH) * (double)smetrics.y_ppem)))
#define TRANSFORM_FUNITS_Y_UP(yval) \
  ((int) \
   ceil((((double)(yval)/(double)upm) * \
     ((double)instance->transformation.matrix.yy/TWO_SIXTEENTH) * (double)smetrics.y_ppem)))
#define TRANSFORM_FUNITS_RAW(value) \
  ((long) \
   floor(((double)(value)/(double)upm) * 1000.0 + 0.5))
#define TRANSFORM_FUNITS_RAW_DOWN(value) \
  ((long) \
   floor(((double)(value)/(double)upm) * 1000.0))
#define TRANSFORM_FUNITS_RAW_UP(value) \
  ((long) \
   ceil(((double)(value)/(double)upm) * 1000.0))


    FTFontPtr font;
    FTInstancePtr instance;
    FT_Size_Metrics smetrics;
    FTFacePtr face;
    int xrc, i;
    int charcell, constantWidth;
    long rawWidth, rawAverageWidth, aw, code, lastCode, firstCode;
    int upm, minLsb, maxRsb, ascent, descent, width, averageWidth;
  

    xrc = FreeTypeLoadFont(&font, fileName, vals, entry, bmfmt);
    if(xrc != Successful)
        return xrc;

    instance = font->instance;
    face = instance->face;
    smetrics = instance->size->metrics;

    upm = face->face->units_per_EM;
    charcell = (instance->monospaced == FT_CHARCELL);
    constantWidth = charcell || is_fixed_width(face->face);
    if(constantWidth && instance->monospaced == 0)
        instance->monospaced = FT_MONOSPACED;

    /* There's no way to get the average width right without rasterising
       all of the glyphs.  We make a serious attempt at getting it right
       for monospaced fonts, and try to provide a reasonable
       approximation for others. */

    if(constantWidth)
        aw = face->face->max_advance_width;
    else
        aw = face->face->max_advance_width / 2;

    if(constantWidth)
        averageWidth = 10*TRANSFORM_FUNITS_X(aw);
    else
        averageWidth = TRANSFORM_FUNITS_X(aw*10L);
    rawAverageWidth = TRANSFORM_FUNITS_RAW(aw*10L);

    vals->width = averageWidth;
  
    if(info) {
        info->fontAscent = 
            TRANSFORM_FUNITS_Y(face->face->ascender);
        info->fontDescent = 
            -TRANSFORM_FUNITS_Y(face->face->descender);
        firstCode = 0;
        lastCode = 0xFFFFL;
        if(font->nranges) {
            lastCode = 0;
            /* The ranges information does not have an effect on firstCode,
               as we pass the default glyph at position 0. */
            for(i=0; i<font->nranges; i++) {
                code = font->ranges[i].max_char_low + 
                    (font->ranges[i].max_char_high<<8);
                if(lastCode < code)
                    lastCode = code;
            }
        }

        if(!font->mapping.mapping || 
           font->mapping.mapping->encoding->row_size == 0) {
            /* linear indexing */
            lastCode=MIN(lastCode,
                         font->mapping.mapping ?
                         font->mapping.mapping->encoding->size-1 :
                         0xFF);
            if(font->mapping.mapping && font->mapping.mapping->encoding->first)
                firstCode = font->mapping.mapping->encoding->first;
            info->firstRow = firstCode/0x100;
            info->lastRow = lastCode/0x100;
            info->firstCol =
                (info->firstRow || info->lastRow) ? 0 : (firstCode & 0xFF);
            info->lastCol = info->lastRow ? 0xFF : (lastCode & 0xFF);
        } else {
            /* matrix indexing */
            info->firstRow = font->mapping.mapping->encoding->first;
            info->lastRow = MIN(font->mapping.mapping->encoding->size-1,
                                lastCode/0x100);
            info->firstCol = font->mapping.mapping->encoding->first_col;
            info->lastCol = MIN(font->mapping.mapping->encoding->row_size-1, 
                                lastCode<0x100?lastCode:0xFF);
        }

        /* firstCode and lastCode are not valid in case of a matrix
           encoding */

        transformBBox(&instance->transformation, upm,
                      smetrics.x_ppem, smetrics.y_ppem,
                      charcell? 0 :
                      face->face->bbox.xMin,
                      face->face->bbox.yMin,
                      charcell ?
                      face->face->max_advance_width :
                      face->face->bbox.xMax,
                      face->face->bbox.yMax,
                      &minLsb, &descent, &maxRsb, &ascent);
        descent = -descent;

        width = TRANSFORM_FUNITS_X(face->face->max_advance_width);
        rawWidth = 
            TRANSFORM_FUNITS_RAW(face->face->max_advance_width);
        instance->width = width;

        info->constantWidth=constantWidth;
        info->constantMetrics=charcell;

        info->minbounds.leftSideBearing = minLsb;
        info->minbounds.rightSideBearing = charcell?maxRsb:minLsb;
        info->minbounds.characterWidth = constantWidth?width:-width;
        info->minbounds.ascent = charcell?ascent:-descent;
        info->minbounds.descent = charcell?descent:-ascent;
        info->minbounds.attributes =
            (unsigned short)(short)(constantWidth?rawWidth:-rawWidth);

        info->maxbounds.leftSideBearing = charcell?minLsb:maxRsb;
        info->maxbounds.rightSideBearing = maxRsb;
        info->maxbounds.characterWidth = width;
        info->maxbounds.ascent = ascent;
        info->maxbounds.descent = descent;
        info->maxbounds.attributes = (unsigned short)(short)rawWidth;

        if(charcell && instance->charcellMetrics == 0) {
            instance->charcellMetrics = (xCharInfo*)xalloc(sizeof(xCharInfo));
            if(instance->charcellMetrics == 0) {
                FreeTypeFreeFont(font);
                return AllocError;
            }
            memcpy((char*)instance->charcellMetrics,
                   (char*)&info->maxbounds, sizeof(xCharInfo));
        }

        /* Glyph metrics are accurate */
        info->inkMetrics=1;

        memcpy((char *)&info->ink_maxbounds,
               (char *)&info->maxbounds, sizeof(xCharInfo));
        memcpy((char *)&info->ink_minbounds,
               (char *)&info->minbounds, sizeof(xCharInfo));

        /* XXX - hack */
        info->defaultCh=0;
    }

    if(xf)
        xf->fontPrivate = (void*)font;
  
    if(info) {
        xrc = FreeTypeAddProperties(font, vals, info, entry->name.name, 
                                    rawAverageWidth);
        if (xrc != Successful) {
            FreeTypeFreeFont(font);
            return xrc;
        }
    }

    return Successful;
#undef TRANSFORM_FUNITS_X
#undef TRANSFORM_FUNITS_X_DOWN
#undef TRANSFORM_FUNITS_X_UP
#undef TRANSFORM_FUNITS_Y
#undef TRANSFORM_FUNITS_Y_DOWN
#undef TRANSFORM_FUNITS_Y_UP
#undef TRANSFORM_FUNITS_RAW
#undef TRANSFORM_FUNITS_RAW_DOWN
#undef TRANSFORM_FUNITS_RAW_UP
}

/* Routines used by X11 to get info and glyphs from the font. */

static int
FreeTypeGetMetrics(FontPtr pFont, unsigned long count, unsigned char *chars,
                   FontEncoding charEncoding, unsigned long *metricCount,
                   xCharInfo **metrics)
{
    unsigned int code = 0;
    FTFontPtr tf;
    xCharInfo **mp, *m;


    /* The compiler is supposed to initialise all the fields to 0 */
    static xCharInfo noSuchChar;

    /*  MUMBLE1("Get metrics for %ld characters\n", count);*/

    tf = (FTFontPtr)pFont->fontPrivate;
    mp = metrics;

    while (count-- > 0) {
        switch (charEncoding) {
        case Linear8Bit: 
        case TwoD8Bit:
            code = *chars++;
            break;
        case Linear16Bit: 
        case TwoD16Bit:
            code = (*chars++ << 8);
            code |= *chars++;
            break;
        }

        if(FreeTypeFontGetGlyphMetrics(code, &m, tf) == Successful && m!=0) {
            *mp++ = m;
        } else
            *mp++ = &noSuchChar;
    }
    
    *metricCount = mp - metrics;
    return Successful;
}

static int
FreeTypeGetGlyphs(FontPtr pFont, unsigned long count, unsigned char *chars,
                  FontEncoding charEncoding, unsigned long *glyphCount,
                  CharInfoPtr *glyphs)
{
    unsigned int code = 0;
    FTFontPtr tf;
    CharInfoPtr *gp;
    CharInfoPtr g;

    tf = (FTFontPtr)pFont->fontPrivate;
    gp = glyphs;

    while (count-- > 0) {
        switch (charEncoding) {
        case Linear8Bit: case TwoD8Bit:
            code = *chars++;
            break;
        case Linear16Bit: case TwoD16Bit:
            code = *chars++ << 8; 
            code |= *chars++;
            break;
        }
      
        if(FreeTypeFontGetGlyph(code, &g, tf) == Successful && g!=0) {
            *gp++ = g;
        } else
            if(FreeTypeFontGetDefaultGlyph(&g, tf) == Successful && g!=0)
                *gp++ = g;
    }
    
    *glyphCount = gp - glyphs;
    return Successful;
}

static int
FreeTypeSetUpFont(FontPathElementPtr fpe, FontPtr xf, FontInfoPtr info, 
                  fsBitmapFormat format, fsBitmapFormatMask fmask,
                  FontBitmapFormatPtr bmfmt)
{
    int xrc;
    int image;

    /* Get the default bitmap format information for this X installation.
       Also update it for the client if running in the font server. */
    FontDefaultFormat(&bmfmt->bit, &bmfmt->byte, &bmfmt->glyph, &bmfmt->scan);
    if ((xrc = CheckFSFormat(format, fmask, &bmfmt->bit, &bmfmt->byte,
                             &bmfmt->scan, &bmfmt->glyph,
                             &image)) != Successful) {
        MUMBLE1("Aborting after checking FS format: %d\n", xrc);
        return xrc;
    }

    if(xf) {
        xf->refcnt = 0;
        xf->bit = bmfmt->bit;
        xf->byte = bmfmt->byte;
        xf->glyph = bmfmt->glyph;
        xf->scan = bmfmt->scan;
        xf->format = format;
        xf->get_glyphs = FreeTypeGetGlyphs;
        xf->get_metrics = FreeTypeGetMetrics;
        xf->unload_font = FreeTypeUnloadXFont;
        xf->unload_glyphs = 0;
        xf->fpe = fpe;
        xf->svrPrivate = 0;
        xf->fontPrivate = 0;        /* we'll set it later */
        xf->fpePrivate = 0;
    }

    info->defaultCh = 0;
    info->noOverlap = 0;          /* not updated */
    info->terminalFont = 0;       /* not updated */
    info->constantMetrics = 0;    /* we'll set it later */
    info->constantWidth = 0;      /* we'll set it later */
    info->inkInside = 1;
    info->inkMetrics = 1;
    info->allExist=0;             /* not updated */
    info->drawDirection = LeftToRight; /* we'll set it later */
    info->cachable = 1;           /* we don't do licensing */
    info->anamorphic = 0;         /* can hinting lead to anamorphic scaling? */
    info->maxOverlap = 0;         /* we'll set it later. */
    info->pad = 0;                /* ??? */
    return Successful;
}

/* Functions exported by the backend */

static int
FreeTypeOpenScalable(FontPathElementPtr fpe, FontPtr *ppFont, int flags,
                     FontEntryPtr entry, char *fileName, FontScalablePtr vals,
                     fsBitmapFormat format, fsBitmapFormatMask fmask,
                     FontPtr non_cachable_font)
{
    int xrc;
    FontPtr xf;
    FontBitmapFormatRec bmfmt;

    MUMBLE1("Open Scalable %s, XLFD=",fileName);
#ifdef DEBUG_TRUETYPE
    fwrite(entry->name.name, entry->name.length, 1, stdout);
#endif
    MUMBLE("\n");

    /* Reject ridiculously small values.  Singular matrices are okay. */
    if(MAX(hypot(vals->pixel_matrix[0], vals->pixel_matrix[1]),
           hypot(vals->pixel_matrix[2], vals->pixel_matrix[3]))
       <1.0)
        return BadFontName;

    /* Create an X11 server-side font. */
    xf = CreateFontRec();
    if (xf == NULL)
        return AllocError;

    xrc = FreeTypeSetUpFont(fpe, xf, &xf->info, format, fmask, &bmfmt);
    if(xrc != Successful) {
        DestroyFontRec(xf);
        return xrc;
    }
    /* Load the font and fill its info structure. */
    xrc = FreeTypeLoadXFont(fileName, vals, xf, &xf->info, &bmfmt, entry);
    if(xrc != Successful) {
        /* Free everything up at this level and return the error code. */
        MUMBLE1("Error during load: %d\n",xrc);
        DestroyFontRec(xf);
        return xrc;
    }

    /* Set the font and return. */
    *ppFont = xf;

    return xrc;
}

/* Routine to get requested font info. */

static int
FreeTypeGetInfoScalable(FontPathElementPtr fpe, FontInfoPtr info,
                        FontEntryPtr entry, FontNamePtr fontName,
                        char *fileName, FontScalablePtr vals)
{
    int xrc;
    FontBitmapFormatRec bmfmt;

    MUMBLE("Get info, XLFD= ");
#ifdef DEBUG_TRUETYPE
    fwrite(entry->name.name, entry->name.length, 1, stdout);
#endif
    MUMBLE("\n");

    if(MAX(hypot(vals->pixel_matrix[0], vals->pixel_matrix[1]),
           hypot(vals->pixel_matrix[2], vals->pixel_matrix[3])) < 1.0)
        return BadFontName;

    xrc = FreeTypeSetUpFont(fpe, 0, info, 0, 0, &bmfmt);
    if(xrc != Successful) {
        return xrc;
    }

    bmfmt.glyph <<= 3;

    xrc = FreeTypeLoadXFont(fileName, vals, 0, info, &bmfmt, entry);
    if(xrc != Successful) {
        MUMBLE1("Error during load: %d\n", xrc);
        return xrc;
    }

    return Successful;
}

/* Renderer registration. */

/* Set the capabilities of this renderer. */
#define CAPABILITIES (CAP_CHARSUBSETTING | CAP_MATRIX)

/* Set it up so file names with either upper or lower case can be
   loaded.  We don't support compressed fonts. */
static FontRendererRec renderers[] = {
    {".ttf", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".TTF", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".ttc", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".TTC", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".otf", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".OTF", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".otc", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".OTC", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".pfa", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".PFA", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".pfb", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
    {".PFB", 4, 0, FreeTypeOpenScalable, 0,
     FreeTypeGetInfoScalable, 0, CAPABILITIES},
};
static int num_renderers = sizeof(renderers) / sizeof(renderers[0]);

void
FreeTypeRegisterFontFileFunctions(void)
{
    int i;

    for (i = 0; i < num_renderers; i++)
        FontFileRegisterRenderer(&renderers[i]);
}
