/*
Copyright (c) 2002-2003 by Juliusz Chroboczek

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
/* $XFree86: xc/programs/fonttosfnt/fonttosfnt.h,v 1.4 2003/10/24 20:38:11 tsi Exp $ */

#ifndef _FONTTOSFNT_H_
#define _FONTTOSFNT_H_ 1

#include <stdarg.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define ROUND(x) ((double)(int)((x) + 0.5))

extern int verbose_flag;
extern int glyph_flag;
extern int metrics_flag;
extern int crop_flag;
extern int bit_aligned_flag;
extern int reencode_flag;

#define FONT_SEGMENT_SIZE 128
#define FONT_CODES 0x10000

#define FACE_BOLD 1
#define FACE_ITALIC 2
#define FACE_SYMBOL 4

#define STRIKE_BITMAP(s, i) \
  ((s)->bitmaps[(i)/FONT_SEGMENT_SIZE] ? \
   (s)->bitmaps[(i)/FONT_SEGMENT_SIZE][(i)%FONT_SEGMENT_SIZE] : \
   NULL)

#define SAME_METRICS(b1, b2) \
    ((b1)->advanceWidth == (b2)->advanceWidth && \
     (b1)->horiBearingX == (b2)->horiBearingX && \
     (b1)->horiBearingY == (b2)->horiBearingY && \
     (b1)->width == (b2)->width && \
     (b1)->height == (b2)->height)

/* bit at (x, y) of raster r with stride s */
#define BITREF(r, s, x, y) \
  (((r)[(y) * (s) + (x) / 8] & (1 << (7 - (x) % 8))) != 0)

#define ONE_HALF (1 << 15)
#define TWO_SIXTEENTH (1 << 16)

#define UNITS_PER_EM 2048

#define EPSILON 0.000000001
#define FLOOR(x) ((x) < 0.0 ? -(int)(-(x)) : (x))
#define CEIL(x) FLOOR((x) + 1.0 - EPSILON)

/* Convert a fixed-point value into FUnits */
#define FONT_UNITS(x) \
  FLOOR(((double)(x)) / TWO_SIXTEENTH * UNITS_PER_EM + 0.5)
#define FONT_UNITS_FLOOR(x) \
  FLOOR(((double)(x)) / TWO_SIXTEENTH * UNITS_PER_EM)
#define FONT_UNITS_CEIL(x) \
  CEIL(((double)(x)) / TWO_SIXTEENTH * UNITS_PER_EM)

typedef struct _FontNameEntry {
    int nid;                    /* name id */
    int size;                   /* bytes in value */
    char *value;
} FontNameEntryRec, *FontNameEntryPtr;

typedef struct _Font {
    int numNames;
    struct _FontNameEntry *names;
    int flags;
    int weight;                 /* as in the OS/2 table */
    int width;                  /* as in the OS/2 table */
    int italicAngle;            /* degrees c-clockwise from the vertical */
    int underlinePosition;
    int underlineThickness;
    unsigned foundry;
    struct _Strike *strikes;
} FontRec, *FontPtr;

typedef struct _Strike {
    int sizeX;
    int sizeY;
    struct _Bitmap ***bitmaps;
    struct _Strike *next;
    int numSbits;
    int bitmapSizeTableLocation;
    int indexSubTableLocation;
    struct _IndexSubTable *indexSubTables;
} StrikeRec, *StrikePtr;

typedef struct _Bitmap {
    int index;
    int advanceWidth;
    int horiBearingX;
    int horiBearingY;
    int width;
    int height;
    int stride;
    char *raster;
    int location;
} BitmapRec, *BitmapPtr;

typedef struct _IndexSubTable {
    int location;
    int firstGlyphIndex;
    int lastGlyphIndex;
    int constantMetrics;
    int lastLocation;
    struct _IndexSubTable *next;
} IndexSubTableRec, *IndexSubTablePtr;

typedef struct _Cmap {
    int startCode;
    int endCode;
    int index;
    struct _Cmap *next;
    int maxindex;               /* only in the head segment*/
    int *inverse;
} CmapRec, *CmapPtr;

FontPtr makeFont(void);
StrikePtr makeStrike(FontPtr, int, int);
BitmapPtr makeBitmap(StrikePtr, int,
                     int, int, int, int, int, int,
                     unsigned char*, int);
IndexSubTablePtr makeIndexSubTables(StrikePtr, CmapPtr);
int fontIndex(FontPtr, int);
CmapPtr makeCmap(FontPtr);
int findIndex(CmapPtr, int);
int findCode(CmapPtr, int);
BitmapPtr strikeBitmapIndex(StrikePtr, CmapPtr, int);
void strikeMetrics(StrikePtr, int*, int*, int*, int*, int*);
int glyphMetrics(FontPtr, int, int*, int*, int*, int*, int*);
void fontMetrics(FontPtr, int*, int*, int*, int*, int*);
int maxIndex(CmapPtr);

int readFile(char *filename, FontPtr);
int writeFile(char *filename, FontPtr);

/* util.c */

#define PROP_ATOM 1
#define PROP_INTEGER 2
#define PROP_CARDINAL 3

char *sprintf_reliable(char *f, ...);
char *vsprintf_reliable(char *f, va_list args);
char *makeUTF16(char *);
unsigned makeName(char*);
int macTime(int *, unsigned *);
unsigned faceFoundry(FT_Face);
char *faceEncoding(FT_Face);
int faceFlags(FT_Face);
int faceWeight(FT_Face);
int faceWidth(FT_Face);
int faceItalicAngle(FT_Face);
int degreesToFraction(int, int*, int*);

#endif /* _FONTTOSFNT_H_ */
