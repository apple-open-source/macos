/*
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
/* $XFree86: xc/lib/font/FreeType/ftfuncs.h,v 1.12 2002/10/01 00:02:10 alanh Exp $ */

/* Number of buckets in the hashtable holding faces */
#define NUMFACEBUCKETS 32

/* Glyphs are held in segments of this size */
#define FONTSEGMENTSIZE 16

/* A structure that holds bitmap order and padding info. */

typedef struct {
    int bit;                    /* bit order */
    int byte;                   /* byte order */
    int glyph;                  /* glyph pad size */
    int scan;                   /* machine word size */
} FontBitmapFormatRec, *FontBitmapFormatPtr;

struct FTSize_s;

/* At the lowest level, there is face; FTFaces are in one-to-one
   correspondence with TrueType faces.  Multiple instance may share
   the same face. */

typedef struct _FTFace {
    char *filename;
    FT_Face face;
    struct _FTInstance *instances;
    struct _FTInstance *active_instance;
    struct _FTFace *next;       /* link to next face in bucket */
} FTFaceRec, *FTFacePtr;

/* A transformation matrix with resolution information */
typedef struct _FTNormalisedTransformation {
    double scale;
    int nonIdentity;            /* if 0, matrix is the identity */
    FT_Matrix matrix;
    int xres, yres;
} FTNormalisedTransformationRec, *FTNormalisedTransformationPtr;

#define FT_MONOSPACED 1
#define FT_CHARCELL 2

#define FT_AVAILABLE_UNKNOWN 0
#define FT_AVAILABLE_NO 1
#define FT_AVAILABLE_YES 2
#define FT_AVAILABLE_RASTERISED 3

/* An instance builds on a face by specifying the transformation
   matrix.  Multiple fonts may share the same instance. */

/* This structure caches bitmap data */
typedef struct _FTInstance {
    FTFacePtr face;             /* the associated face */
    FT_Size size;
    FTNormalisedTransformationRec transformation;
    int monospaced;
    int width;                  /* the width of all glyphs if monospaced */
    xCharInfo *charcellMetrics; /* the metrics if charcell is 1 */
    FontBitmapFormatRec bmfmt;
    unsigned nglyphs;
    CharInfoPtr *glyphs;        /* glyphs and available are used in parallel */
    int **available;
    int refcount;
    struct _FTInstance *next;   /* link to next instance */
} FTInstanceRec, *FTInstancePtr;

/* A font is an instance with coding information; fonts are in
   one-to-one correspondence with X fonts */
typedef struct _FTFont{
    FTInstancePtr instance;
    FTMappingRec mapping;
    int nranges;
    fsRange *ranges;
} FTFontRec, *FTFontPtr;

/* Prototypes for some local functions */

static int FreeTypeOpenFace(FTFacePtr *facep, char *fileName);
static void FreeTypeFreeFace(FTFacePtr face);
static int 
 FreeTypeOpenInstance(FTInstancePtr *instancep,
                      char *fileName, FTNormalisedTransformationPtr trans,
                      int charcell, FontBitmapFormatPtr bmfmt);
static void FreeTypeFreeInstance(FTInstancePtr instance);
static int
 FreeTypeInstanceGetGlyph(unsigned idx, CharInfoPtr *g, FTInstancePtr instance);
static int 
FreeTypeRasteriseGlyph(CharInfoPtr tgp, FTInstancePtr instance, int hasMetrics);
static void FreeTypeFreeFont(FTFontPtr font);
static void FreeTypeFreeXFont(FontPtr pFont, int freeProps);
static void FreeTypeUnloadXFont(FontPtr pFont);
static int
FreeTypeAddProperties(FTFontPtr font, FontScalablePtr vals, FontInfoPtr info, 
                      char *fontname, 
                      int rawAverageWidth);
static int FreeTypeFontGetGlyph(unsigned code, CharInfoPtr *g, FTFontPtr font);
static int FreeTypeFontGetDefaultGlyph(CharInfoPtr *g, FTFontPtr font);
static int
FreeTypeLoadFont(FTFontPtr *fontp, char *fileName,
                 FontScalablePtr vals, FontEntryPtr entry,
                 FontBitmapFormatPtr bmfmt);
