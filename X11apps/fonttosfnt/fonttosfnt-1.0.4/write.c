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
/* $XFree86: xc/programs/fonttosfnt/write.c,v 1.4tsi Exp $ */

#if defined(linux) && !defined(_GNU_SOURCE)
/* for fwrite_unlocked and fread_unlocked */
#define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "X11/Xos.h"

#include "fonttosfnt.h"

#if !defined(I_LOVE_POSIX) && \
    defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#define DO_FWRITE fwrite_unlocked
#define DO_FREAD fread_unlocked
#else
#define DO_FWRITE fwrite
#define DO_FREAD fread
#endif

static int writeDir(FILE*, FontPtr, int, unsigned*);
static int fixupDir(FILE*, FontPtr, int, int*, int*);
static int fixupChecksum(FILE*, int, int);

static int writeEBDT(FILE*, FontPtr);
static int writeEBLC(FILE*, FontPtr);
static int writeOS2(FILE*, FontPtr);
static int writePCLT(FILE*, FontPtr);
static int writecmap(FILE*, FontPtr);
static int writeglyf(FILE*, FontPtr);
static int writehead(FILE*, FontPtr);
static int writehhea(FILE*, FontPtr);
static int writehmtx(FILE*, FontPtr);
static int writeloca(FILE*, FontPtr);
static int writemaxp(FILE*, FontPtr);
static int writename(FILE*, FontPtr);
static int writepost(FILE*, FontPtr);

int max_awidth, min_x, min_y, max_x, max_y;
static CmapPtr current_cmap = NULL;
static int numglyphs, nummetrics;
static int write_error_occurred, read_error_occurred;

/* floor(log2(x)) */
static int
log2_floor(int x) 
{
    int i, j;

    if(x <= 0)
        abort();

    i = 0;
    j = 1;
    while(2 * j < x) {
        i++;
        j *= 2;
    }
    return i;
}

/* 2 ** floor(log2(x)) */
static int
two_log2_floor(int x)
{
    int j;
    
    if(x <= 0)
        abort();

    j = 1;
    while(2 * j < x) {
        j *= 2;
    }
    return j;
}

static void
write_error(int rc)
{
    /* Real Men program in C and don't use exceptions. */
    if(write_error_occurred)
        return;
    write_error_occurred = 1;
    if(rc < 0)
        perror("Couldn't write");
    else
        fprintf(stderr, "Short write.\n");
}

static void
read_error(int rc)
{
    if(read_error_occurred)
        return;
    read_error_occurred = 1;
    if(rc < 0)
        perror("Couldn't read");
    else
        fprintf(stderr, "Short read.\n");
}

static void
writeBYTE(FILE *out, unsigned char val)
{
    int rc;
    rc = DO_FWRITE(&val, 1, 1, out);
    if(rc != 1) write_error(rc);
}

static void
writeBYTEs(FILE *out, unsigned char *chars, int n)
{
    int rc;
    rc = DO_FWRITE(chars, 1, n, out);
    if(rc != n) write_error(rc);
}

static void
writeCHAR(FILE *out, char val)
{
    int rc;
    rc = DO_FWRITE(&val, 1, 1, out);
    if(rc != 1) write_error(rc);
}

static void
writeCHARs(FILE *out, char *chars, int n)
{
    int rc;
    rc = DO_FWRITE(chars, 1, n, out);
    if(rc != n) write_error(rc);
}

static void
writeUSHORT(FILE *out, unsigned short val)
{
    int rc;
    val = htons(val);
    rc = DO_FWRITE(&val, 2, 1, out);
    if(rc != 1) write_error(rc);
}

static void
writeSHORT(FILE *out, short val)
{
    int rc;
    val = htons(val);
    rc = DO_FWRITE(&val, 2, 1, out);
    if(rc != 1) write_error(rc);
}

static void
writeULONG(FILE *out, unsigned int val)
{
    int rc;
    val = htonl(val);
    rc = DO_FWRITE(&val, 4, 1, out);
    if(rc != 1) write_error(rc);
}

static void
writeLONG(FILE *out, int val)
{
    int rc;
    val = htonl(val);
    rc = DO_FWRITE(&val, 4, 1, out);
    if(rc != 1) write_error(rc);
}

static unsigned
readULONG(FILE *out)
{
    int rc;
    unsigned val;
    rc = DO_FREAD(&val, 4, 1, out);
    if(rc != 1) {
        read_error(rc);
        return 0xDEADBEEF;
    }
    return ntohl(val);
}

int 
writeFile(char *filename, FontPtr font)
{
    int rc;
    FILE *out;
    unsigned tables[15];
    int head_position = 0;
    int full_length;
    int (*(table_writers[15]))(FILE*, FontPtr);
    int i, j;
    int offset[15], length[15];
    StrikePtr strike;

    fontMetrics(font, &max_awidth, &min_x, &min_y, &max_x, &max_y);

    out = fopen(filename, "wb+");
    if(out == NULL)
        return -1;

    current_cmap = makeCmap(font);
    if(current_cmap == NULL) {
        fprintf(stderr, "Couldn't build cmap.\n");
        return -1;
    }

    write_error_occurred = 0;
    read_error_occurred = 0;

    if(glyph_flag >= 2) {
        numglyphs = maxIndex(current_cmap) + 1;
        if(metrics_flag >= 2)
            nummetrics = numglyphs - 1;
        else if(metrics_flag >= 1)
            nummetrics = 1;
        else
            nummetrics = 0;
    } else if(glyph_flag == 1) {
        numglyphs = 1;
        nummetrics = (metrics_flag >= 1) ? 1 : 0;
    } else {
        numglyphs = 0;
        nummetrics = 0;
    }

    strike = font->strikes;
    while(strike) {
        strike->indexSubTables = makeIndexSubTables(strike, current_cmap);
        if(!strike->indexSubTables) {
            fprintf(stderr, "Couldn't build indexSubTable.\n");
            return -1;
        }
        strike = strike->next;
    }

    /* These must be sorted lexicographically */
    i = 0;
    tables[i] = makeName("EBDT"); table_writers[i] = writeEBDT; i++;
    tables[i] = makeName("EBLC"); table_writers[i] = writeEBLC; i++;
    tables[i] = makeName("OS/2"); table_writers[i] = writeOS2; i++;
    tables[i] = makeName("PCLT"); table_writers[i] = writePCLT; i++;
    tables[i] = makeName("cmap"); table_writers[i] = writecmap; i++;
    if(numglyphs >= 1) {
        tables[i] = makeName("glyf");
        table_writers[i] = writeglyf; i++;
    }
    tables[i] = makeName("head"); table_writers[i] = writehead; i++;
    tables[i] = makeName("hhea"); table_writers[i] = writehhea; i++;
    if(nummetrics >= 1) {
        tables[i] = makeName("hmtx"); 
        table_writers[i] = writehmtx; i++;
    }
    if(numglyphs >= 1) {
        tables[i] = makeName("loca"); 
        table_writers[i] = writeloca; i++;
    }
    tables[i] = makeName("maxp"); table_writers[i] = writemaxp; i++;
    tables[i] = makeName("name"); table_writers[i] = writename; i++;
    tables[i] = makeName("post"); table_writers[i] = writepost; i++;

    rc = writeDir(out, font, i, tables);
    if(rc < 0)
        goto fail;

    for(j = 0; j < i; j++) {
        offset[j] = ftell(out);
        if(offset[j] < 0) {
            perror("Couldn't compute table offset");
            goto fail;
        }
        if(tables[j] == makeName("head"))
            head_position = offset[j];
        rc = table_writers[j](out, font);
        if(rc < 0 || write_error_occurred || read_error_occurred)
            goto fail;
        length[j] = ftell(out) - offset[j];
        if(length[j] < 0) {
            perror("Couldn't compute table size");
            goto fail;
        }
        if(length[j] % 4 != 0) {
            /* Pad -- recommended by the spec, and assumed by
               computeChecksum. */
            int k;
            for(k = 0; k < (4 - length[j] % 4); k++) {
                /* This must be 0 -- see computeChecksum. */
                writeBYTE(out, 0);
            }
            if(write_error_occurred || read_error_occurred)
                goto fail;
        }
    }

    rc = fixupDir(out, font, i, offset, length);
    if(rc < 0)
        goto fail;

    full_length = ftell(out);
    if(full_length < 0) {
        perror("Couldn't compute file size");
        goto fail;
    }
    while(full_length % 4 != 0) {
        /* pad for computeChecksum */
        writeBYTE(out, 0);
        full_length++;
    }
    if(write_error_occurred || read_error_occurred)
        goto fail;
    rc = fixupChecksum(out, full_length, head_position);
    if(rc < 0)
        goto fail;
    fclose(out);
    return 0;

 fail:
    unlink(filename);
    return -1;
}

static int
writeDir(FILE *out, FontPtr font, int numTables, unsigned *tables)
{
    int i, ti;
    i = 0; ti = 1;
    while(2 * ti < numTables) {
        i++;
        ti = 2 * ti;
    }

    writeULONG(out, 0x10000);   /* version */
    writeUSHORT(out, numTables); /* numTables */
    writeUSHORT(out, 16 * ti);  /* searchRange */
    writeUSHORT(out, i);        /* entrySelector */
    writeUSHORT(out, 16 * (numTables - ti)); /* rangeShift */

    /* see fixupDir */
    for(i = 0; i < numTables; i++) {
        writeULONG(out, tables[i]);
        writeULONG(out, 0xDEADFACE); /* checkSum */
        writeULONG(out, 0xDEADFACE); /* offset */
        writeULONG(out, 0xDEADFACE); /* length */
    }
    return 0;
}

static unsigned
computeChecksum(FILE *out, int offset, int length)
{
    int rc;
    int i;
    unsigned sum = 0;

    if(offset % 4 != 0) {
        fprintf(stderr, "Offset %d is not a multiple of 4\n", offset);
        return ~0;
    }

    rc = fseek(out, offset, SEEK_SET);
    if(rc < 0) {
        perror("Couldn't seek");
        return ~0;
    }

    /* This relies on the fact that we always pad tables with zeroes. */
    for(i = 0; i < length; i += 4) {
        sum += readULONG(out);
    }
    return sum;
}

static int
fixupDir(FILE *out, FontPtr font, int numTables, int *offset, int *length)
{
    int rc, i;
    unsigned sum;

    for(i = 0; i < numTables; i++) {
        sum = computeChecksum(out, offset[i], length[i]);
        rc = fseek(out, 12 + 16 * i + 4, SEEK_SET);
        if(rc != 0) {
            perror("Couldn't seek");
            return -1;
        }
        writeULONG(out, sum);
        writeULONG(out, offset[i]);
        writeULONG(out, length[i]);
    }
    return 0;
}

static int
fixupChecksum(FILE *out, int full_length, int head_position)
{
    int rc, checksum;
    checksum = computeChecksum(out, 0, full_length);
    rc = fseek(out, head_position + 8, SEEK_SET);
    if(rc != 0) {
        perror("Couldn't seek");
        return -1;
    }
    writeULONG(out, 0xB1B0AFBA - checksum); /* checkSumAdjustment */
    return 0;
}

    
static int 
writehead(FILE* out, FontPtr font)
{
    int time_hi;
    unsigned time_lo;

    macTime(&time_hi, &time_lo);

    writeULONG(out, 0x00010000);
    writeULONG(out, 0x00010000); /* fontRevision */
    writeULONG(out, 0);         /* checkSumAdjustment -- filled in later */
    writeULONG(out,0x5F0F3CF5); /* magicNumber */
    writeUSHORT(out, 1);        /* flags */
    writeUSHORT(out, UNITS_PER_EM); /* unitsPerEm */

    writeLONG(out, time_hi);    /* created */
    writeULONG(out, time_lo);
    writeLONG(out, time_hi);    /* modified */
    writeULONG(out, time_lo);

    writeUSHORT(out, FONT_UNITS_FLOOR(min_x));
    writeUSHORT(out, FONT_UNITS_FLOOR(min_y));
    writeUSHORT(out, FONT_UNITS_CEIL(max_x));
    writeUSHORT(out, FONT_UNITS_CEIL(max_y));
    writeUSHORT(out, font->flags); /* macStyle */
    writeUSHORT(out, 1);        /* lowestRecPPEM */
    writeSHORT(out, 0);         /* fontDirectionHint */
    writeSHORT(out, 0);         /* indexToLocFormat */
    writeSHORT(out, 0);         /* glyphDataFormat */
    return 0;
}

static int
outputRaster(FILE *out, char *raster, int width, int height, int stride,
             int bit_aligned)
{
    int i, j;
    int len = 0;

    if(!bit_aligned || width % 8 == 0) {
        for(i = 0; i < height; i++) {
            writeCHARs(out, raster + i * stride, (width + 7) / 8);
            len += (width + 7) / 8;
        }
    } else {
        int bit = 0;
        unsigned char v = 0;
        for(i = 0; i < height; i++) {
            for(j = 0; j < width; j++) {
                if(BITREF(raster, stride, j, i))
                    v |= 1 << (7 - bit);
                bit++;
                if(bit >= 8) {
                    writeBYTE(out, v);
                    len++;
                    bit = 0;
                    v = 0;
                }
            }
        }
        if(bit > 0) {
            writeBYTE(out, v);
            len++;
        }
    }
    return len;
}

static int 
writeEBDT(FILE* out, FontPtr font)
{
    StrikePtr strike;
    BitmapPtr bitmap;
    IndexSubTablePtr table;
    int i;
    int offset;
    int ebdt_start;

    ebdt_start = ftell(out);

    writeULONG(out, 0x00020000); /* version */
    offset = 4;

    strike = font->strikes;
    while(strike) {
        table = strike->indexSubTables;
        while(table) {
            for(i = table->firstGlyphIndex; i <= table->lastGlyphIndex; i++) {
                bitmap = strikeBitmapIndex(strike, current_cmap, i);
                bitmap->location = offset;
                if(bit_aligned_flag && table->constantMetrics) {
                    /* image format 5 */
                    ;
                } else {
                    /* image format 1 or 2 */
                    writeBYTE(out, bitmap->height);
                    writeBYTE(out, bitmap->width);
                    writeCHAR(out, bitmap->horiBearingX);
                    writeCHAR(out, bitmap->horiBearingY);
                    writeBYTE(out, bitmap->advanceWidth);
                    offset += 5;
                }
                offset += outputRaster(out, 
                                       bitmap->raster, 
                                       bitmap->width, bitmap->height,
                                       bitmap->stride,
                                       bit_aligned_flag);
            }
            table->lastLocation = offset;
            table = table->next;
        }
        strike = strike->next;
    }
    if(ftell(out) != ebdt_start + offset)
        abort();
    return 0;
}

static int
writeSbitLineMetrics(FILE *out, StrikePtr strike, int num, int den)
{
    int width_max, x_min, y_min, x_max, y_max;
    strikeMetrics(strike, &width_max, &x_min, &y_min, &x_max, &y_max);

    writeCHAR(out, y_max);      /* ascender */
    writeCHAR(out, y_min);      /* descender */
    writeBYTE(out, width_max);  /* widthMax */
    writeCHAR(out, num);          /* caretSlopeNumerator */
    writeCHAR(out, den);          /* caretSlopeDenominator */
    writeCHAR(out, 0);          /* caretOffset */
    writeCHAR(out, 0);          /* minOriginSB */
    writeCHAR(out, 0);          /* minAdvanceSB */
    writeCHAR(out, 0);          /* maxBeforeBL */
    writeCHAR(out, 0);          /* minAfterBL */
    writeCHAR(out, 0);          /* pad1 */
    writeCHAR(out, 0);          /* pad2 */
    return 0;
}

static int 
writeEBLC(FILE* out, FontPtr font)
{
    int i, rc, numstrikes, eblc_start, num, den;
    StrikePtr strike;
    IndexSubTablePtr table;

    degreesToFraction(font->italicAngle, &num, &den);

    numstrikes = 0;
    strike = font->strikes;
    while(strike) {
        numstrikes++;
        strike = strike->next;
    }

    eblc_start = ftell(out);
    
    writeULONG(out, 0x00020000); /* version */
    writeULONG(out, numstrikes); /* numSizes */

    /* bitmapSizeTable */
    strike = font->strikes;
    while(strike) {
        strike->bitmapSizeTableLocation = ftell(out);
        writeULONG(out, 0xDEADFACE); /* indexSubTableArrayOffset */
        writeULONG(out, 0xDEADFACE); /* indexTablesSize */
        writeULONG(out, 0xDEADFACE); /* numberOfIndexSubTables */
        writeULONG(out, 0);     /* colorRef */
        writeSbitLineMetrics(out, strike, num, den);
        writeSbitLineMetrics(out, strike, num, den);
        writeUSHORT(out, 0);    /* startGlyphIndex */
        writeUSHORT(out, 0xFFFD); /* endGlyphIndex */
        writeBYTE(out, strike->sizeX); /* ppemX */
        writeBYTE(out, strike->sizeY); /* ppemY */
        writeBYTE(out, 1);      /* bitDepth */
        writeCHAR(out, 1);      /* flags */
        strike = strike->next;
    }

    /* indexSubTableArray, one per strike */
    strike = font->strikes;
    while(strike) {
        int endoffset;
        int numtables = 0;

        strike->indexSubTableLocation = ftell(out);
        table = strike->indexSubTables;
        while(table) {
            table->location = ftell(out);
            writeUSHORT(out, table->firstGlyphIndex);
            writeUSHORT(out, table->lastGlyphIndex);
            writeULONG(out, 0xDEADFACE); /* additionalOffsetToIndexSubtable */
            numtables++;
            table = table->next;
        }
        endoffset = ftell(out);
        rc = fseek(out, strike->bitmapSizeTableLocation, SEEK_SET);
        if(rc != 0) {
            perror("Couldn't seek");
            return -1;
        }
        writeULONG(out, strike->indexSubTableLocation - eblc_start); 
                                              /* indexSubTableArrayOffset */
        writeULONG(out, endoffset - strike->indexSubTableLocation);
                                              /* indexTablesSize */
        writeULONG(out, numtables);           /* numberOfIndexSubTables */
        rc = fseek(out, endoffset, SEEK_SET);
        if(rc != 0) {
            perror("Couldn't seek");
            return -1;
        }
        strike = strike->next;
    }

    /* actual indexSubTables */
    strike = font->strikes;
    while(strike) {
        int vertAdvance, y_min, y_max;
        strikeMetrics(strike, NULL, NULL, &y_min, NULL, &y_max);
        vertAdvance = y_max - y_min;
        table = strike->indexSubTables;
        while(table) {
            int location;
            int data_location;
            int short_offsets;
            int offset;

            location = ftell(out);
            rc = fseek(out, table->location + 4, SEEK_SET);
            if(rc != 0) {
                perror("Couldn't seek");
                return -1;
            }
            /* additionalOffsetToIndexSubtable */
            writeULONG(out, location - strike->indexSubTableLocation);
            rc = fseek(out, location, SEEK_SET);
            if(rc != 0) {
                perror("Couldn't seek");
                return -1;
            }
            data_location =
                strikeBitmapIndex(strike, current_cmap,
                                  table->firstGlyphIndex)->location;
            short_offsets = 1;
            for(i = table->firstGlyphIndex; i <= table->lastGlyphIndex; i++) {
                if(strikeBitmapIndex(strike, current_cmap, i)->location -
                   data_location > 0xFFFF) {
                    short_offsets = 0;
                    break;
                }
            }
            /* indexFormat */
            if(table->constantMetrics)
                writeUSHORT(out, 2);
            else if(short_offsets)
                writeUSHORT(out, 3);
            else
                writeUSHORT(out, 1);
            /* imageFormat */
            if(bit_aligned_flag) {
                if(table->constantMetrics)
                    writeUSHORT(out, 5);
                else
                    writeUSHORT(out, 2);
            } else {
                writeUSHORT(out, 1);
            }
            writeULONG(out, data_location);
            if(table->constantMetrics) {
                int size;
                BitmapPtr bitmap = 
                    strikeBitmapIndex(strike, current_cmap, 
                                      table->firstGlyphIndex);

                size = 
                    strikeBitmapIndex(strike, current_cmap, 
                                      table->firstGlyphIndex + 1)->location -
                    bitmap->location;
                writeULONG(out, size); /* imageSize */
                /* bigMetrics */
                writeBYTE(out, bitmap->height);
                writeBYTE(out, bitmap->width);
                writeCHAR(out, bitmap->horiBearingX);
                writeCHAR(out, bitmap->horiBearingY);
                writeBYTE(out, bitmap->advanceWidth);
                writeCHAR(out, bitmap->horiBearingX); /* vertBearingX */
                writeCHAR(out, bitmap->horiBearingY); /* vertBearingY */
                writeBYTE(out, vertAdvance); /* vertAdvance */
            } else {
                for(i = table->firstGlyphIndex; 
                    i <= table->lastGlyphIndex; i++) {
                    offset = 
                        strikeBitmapIndex(strike, current_cmap, i)->location -
                        data_location;
                    if(short_offsets)
                        writeUSHORT(out, offset);
                    else
                        writeULONG(out, offset);
                }
                /* Dummy glyph of size 0 to mark the end of the table */
                if(short_offsets) {
                    writeUSHORT(out, table->lastLocation - data_location);
                    writeUSHORT(out, table->lastLocation - data_location);
                } else {
                    writeULONG(out, table->lastLocation - data_location);
                    writeULONG(out, table->lastLocation - data_location);
                }
            }
            location = ftell(out);
            while(location % 4 != 0) {
                writeCHAR(out, 0);
                location--;
            }
            table = table->next;
        }
        strike = strike->next;
    }
    return 0;
}

static int 
writecmap(FILE* out, FontPtr font)
{
    int rc, cmap_start, cmap_end;
    CmapPtr cmap;
    int segcount;

    segcount = 0;
    cmap = current_cmap;
    while(cmap) {
        segcount++;
        cmap = cmap->next;
    }

    segcount++;                 /* dummy segment to end table */

    cmap_start = ftell(out);

    writeUSHORT(out, 0);        /* version */
    writeUSHORT(out, 1);        /* number of encoding tables */
    writeUSHORT(out, 3);        /* platform ID */
    writeUSHORT(out, (font->flags & FACE_SYMBOL) ? 0 : 1);
                                /* encoding ID */
    writeULONG(out, 12);        /* offset to beginning of subtable */

    /* subtable */
    writeUSHORT(out, 4);        /* format */
    writeUSHORT(out, 0xDEAD);   /* length */
    writeUSHORT(out, 0);        /* language */
    /* How baroque can you get? */
    writeUSHORT(out, segcount * 2); /* segCountX2 */
    writeUSHORT(out, 2 * two_log2_floor(segcount)); /* searchRange */
    writeUSHORT(out, 1 + log2_floor(segcount));   /* entrySelector */
    writeUSHORT(out, 2 * (segcount - two_log2_floor(segcount)));   
                                /* rangeShift */

    cmap = current_cmap;
    while(cmap) {
        writeUSHORT(out, cmap->endCode);
        cmap = cmap->next;
    }
    writeUSHORT(out, 0xFFFF);

    writeUSHORT(out, 0);        /* reservedPad */

    cmap = current_cmap;
    while(cmap) {
        writeUSHORT(out, cmap->startCode);
        cmap = cmap->next;
    }
    writeUSHORT(out, 0xFFFF);

    /* idDelta */
    cmap = current_cmap;
    while(cmap) {
        writeUSHORT(out, (cmap->index - cmap->startCode) & 0xFFFF);
        cmap = cmap->next;
    }
    writeUSHORT(out, 1);

    /* idRangeOffset */
    cmap = current_cmap;
    while(cmap) {
        writeUSHORT(out, 0);
        cmap = cmap->next;
    }
    writeUSHORT(out, 0);

    /* glyphIDArray is empty */

    cmap_end = ftell(out);
    rc = fseek(out, cmap_start + 12 + 2, SEEK_SET);
    if(rc != 0) {
        perror("Couldn't seek");
        return -1;
    }
    writeUSHORT(out, cmap_end - cmap_start - 12); /* length */
    rc = fseek(out, cmap_end, SEEK_SET);
    if(rc != 0) {
        perror("Couldn't seek");
        return -1;
    }
    return 0;
}

static int 
writeglyf(FILE* out, FontPtr font)
{
    return 0;
}

int
writehhea(FILE* out, FontPtr font)
{
    int num, den;
    degreesToFraction(font->italicAngle, &num, &den);

    writeULONG(out, 0x00010000); /* version */
    writeSHORT(out, FONT_UNITS_CEIL(max_y)); /* ascender */
    writeSHORT(out, FONT_UNITS_FLOOR(min_y)); /* descender */
    writeSHORT(out, FONT_UNITS(TWO_SIXTEENTH / 20)); /* lineGap */
    writeUSHORT(out, FONT_UNITS(max_awidth)); /* advanceWidthMax */
    writeSHORT(out, FONT_UNITS_FLOOR(min_x)); /* minLeftSideBearing */
    writeSHORT(out, FONT_UNITS_FLOOR(min_x)); /* minRightSideBearing */
    writeSHORT(out, FONT_UNITS_CEIL(max_x)); /* xMaxExtent */
    writeSHORT(out, den);       /* caretSlopeRise */
    writeSHORT(out, num);       /* caretSlopeRun */
    writeSHORT(out, 0);         /* reserved */
    writeSHORT(out, 0);         /* reserved */
    writeSHORT(out, 0);         /* reserved */
    writeSHORT(out, 0);         /* reserved */
    writeSHORT(out, 0);         /* reserved */
    writeSHORT(out, 0);         /* metricDataFormat */
    writeSHORT(out, nummetrics); /* numberOfHMetrics */
    return 0;
}

static int 
writehmtx(FILE* out, FontPtr font)
{
    int rc, i;

    for(i = 0; i <= numglyphs; i++) {
        int code, width, lsb;
        code = findCode(current_cmap, i);
        if(code < 0)
            rc = -1;
        else
            rc = glyphMetrics(font, code, &width, &lsb, NULL, NULL, NULL);
        if(rc < 0) {
            width = UNITS_PER_EM / 3;
            lsb = 0;
        }
        if(i < nummetrics) {
            writeSHORT(out, FONT_UNITS(width));
            writeSHORT(out, FONT_UNITS(lsb));
        } else {
            writeSHORT(out, FONT_UNITS(lsb));
        }
    }
    return 0;
}

static int 
writeloca(FILE* out, FontPtr font)
{
    int i;

    /* All glyphs undefined -- loca table is empty, so offset 0 */
    for(i = 0; i < numglyphs; i++) {
        writeSHORT(out, 0);
    }
    writeSHORT(out, 0);
    return 0;
}

static int 
writemaxp(FILE* out, FontPtr font)
{
    writeLONG(out, 0x00010000); /* version */
    writeUSHORT(out, numglyphs); /* numGlyphs */
    writeUSHORT(out, 0);        /* maxPoints */
    writeUSHORT(out, 0);        /* maxContours */
    writeUSHORT(out, 0);        /* maxCompositePoints */
    writeUSHORT(out, 0);        /* maxCompositeContours */
    writeUSHORT(out, 1);        /* maxZones */
    writeUSHORT(out, 0);        /* maxTwilightPoints */
    writeUSHORT(out, 0);        /* maxStorage */
    writeUSHORT(out, 0);        /* maxFunctionDefs */
    writeUSHORT(out, 0);        /* maxInstructionDefs */
    writeUSHORT(out, 0);        /* maxStackElements */
    writeUSHORT(out, 0);        /* maxSizeOfInstructions */
    writeUSHORT(out, 0);        /* maxComponentElements */
    writeUSHORT(out, 0);        /* maxComponentDepth */
    return 0;
}

static int 
writename(FILE* out, FontPtr font)
{
    int i;
    int offset;

    writeUSHORT(out, 0);        /* format selector */
    writeUSHORT(out, font->numNames);
    writeUSHORT(out, 6 + font->numNames * 12); /* offset to string storage */
    offset = 0;
    for(i = 0; i < font->numNames; i++) {
        writeUSHORT(out, 3);    /* platform id -- Microsoft */
        writeUSHORT(out, 1);    /* encoding -- Unicode */
        writeUSHORT(out, 0x409); /* language id -- American English */
        writeUSHORT(out, font->names[i].nid); /* name id */
        writeUSHORT(out, font->names[i].size); /* string length */
        writeUSHORT(out, offset); /* string offset */
        offset += font->names[i].size;
    }
    for(i = 0; i < font->numNames; i++)
        writeCHARs(out, font->names[i].value, font->names[i].size);
    return 0;
}

static int 
writepost(FILE* out, FontPtr font)
{
    int i, rc, previous_width, width, fixed_pitch;

    fixed_pitch = 1;
    previous_width = -1;
    for(i = 0; i < FONT_CODES; i++) {
        rc = glyphMetrics(font, i, &width, NULL, NULL, NULL, NULL);
        if(rc < 0)
            continue;
        if(previous_width >= 0) {
            if(width != previous_width) {
                fixed_pitch = 0;
                break;
            }
        }
        previous_width = width;
    }
    
    writeULONG(out, 0x00030000); /* FormatType */
    writeULONG(out, font->italicAngle); /* italicAngle */
    writeSHORT(out, FONT_UNITS(font->underlinePosition));
    writeSHORT(out, FONT_UNITS(font->underlineThickness));
    writeULONG(out, fixed_pitch); /* isFixedPitch */
    writeULONG(out, 0);         /* minMemType42 */
    writeULONG(out, 0);         /* maxMemType42 */
    writeULONG(out, 0);         /* minMemType1 */
    writeULONG(out, 0);         /* maxMemType1 */
    return 0;
}

static int 
writeOS2(FILE* out, FontPtr font)
{
    int i;

    writeUSHORT(out, 0x0001);
    writeSHORT(out, FONT_UNITS(max_awidth / 2)); /* xAvgCharWidth; */
    writeUSHORT(out, font->weight);  /* usWeightClass; */
    writeUSHORT(out, font->width); /* usWidthClass; */
    writeSHORT(out, 0);         /* fsType; */
    writeSHORT(out, UNITS_PER_EM / 5); /* ySubscriptXSize; */
    writeSHORT(out, UNITS_PER_EM / 5); /* ySubscriptYSize; */
    writeSHORT(out, 0);         /* ySubscriptXOffset; */
    writeSHORT(out, UNITS_PER_EM / 5); /* ySubscriptYOffset; */
    writeSHORT(out, UNITS_PER_EM / 5); /* ySuperscriptXSize; */
    writeSHORT(out, UNITS_PER_EM / 5); /* ySuperscriptYSize; */
    writeSHORT(out, 0);         /* ySuperscriptXOffset; */
    writeSHORT(out, UNITS_PER_EM / 5); /* ySuperscriptYOffset; */
    writeSHORT(out, FONT_UNITS(font->underlineThickness)); 
    /* yStrikeoutSize; */
    writeSHORT(out, UNITS_PER_EM / 4); /* yStrikeoutPosition; */
    writeSHORT(out, 0);         /* sFamilyClass; */
    for(i = 0; i < 10; i++)
        writeBYTE(out, 0);      /* panose; */
    writeULONG(out, 0xFFFF);    /* ulUnicodeRange1; */
    writeULONG(out, 0xFFFF);    /* ulUnicodeRange2; */
    writeULONG(out, 0x03FF);    /* ulUnicodeRange3; */
    writeULONG(out, 0U);        /* ulUnicodeRange4; */
    writeULONG(out, font->foundry); /* achVendID[4]; */
    writeUSHORT(out, 0x0040);   /* fsSelection; */
    writeUSHORT(out, 0x20);     /* usFirstCharIndex; */
    writeUSHORT(out, 0xFFFD);   /* usLastCharIndex; */
    writeUSHORT(out, FONT_UNITS_CEIL(max_y)); /* sTypoAscender; */
    writeUSHORT(out, -FONT_UNITS_FLOOR(min_y)); /* sTypoDescender; */
    writeUSHORT(out, FONT_UNITS(max_y - min_y));
    /* sTypoLineGap; */
    writeUSHORT(out, FONT_UNITS_CEIL(max_y)); /* usWinAscent; */
    writeUSHORT(out, -FONT_UNITS_FLOOR(min_y)); /* usWinDescent; */
    writeULONG(out, 3);         /* ulCodePageRange1; */
    writeULONG(out, 0);         /* ulCodePageRange2; */
    return 0;
}

static int 
writePCLT(FILE* out, FontPtr font)
{
    char name[16] = "X11 font        ";
    char filename[6] = "X11R00";
    unsigned char charComplement[8] = 
        {0xFF, 0xFF, 0xFF, 0xFF, 0x0B, 0xFF, 0xFF, 0xFE};
    int style, w, strokeWeight, widthType;

    style = 0;
    if(font->flags & FACE_ITALIC)
        style = 1;

    w = (font->weight + 50) / 100;
    if(w < 5)
        strokeWeight = w - 6;
    else if(w == 5)
        strokeWeight = 0;
    else
        strokeWeight = w - 4;

    if(font->width <= 2)
        widthType = -3;
    else if(font->width <= 4)
        widthType = -2;
    else if(font->width <= 6)
        widthType = 0;
    else if(font->width <= 7)
        widthType = 2;
    else
        widthType = 3;

    writeULONG(out, 0x00010000); /* version */
    writeULONG(out, 0);         /* FontNumber */
    writeUSHORT(out, FONT_UNITS(max_awidth)); /* pitch */
    writeUSHORT(out, FONT_UNITS(max_y));    /* xHeight */
    writeUSHORT(out, style);    /* style */
    writeUSHORT(out, 6 << 12);  /* TypeFamily */
    writeUSHORT(out, FONT_UNITS(max_y)); /* CapHeight */
    writeUSHORT(out, 0);        /* SymbolSet */
    writeCHARs(out, name, 16);  /* TypeFace */
    writeBYTEs(out, charComplement, 8); /* CharacterComplement */
    writeCHARs(out, filename, 6); /* FileName */
    writeCHAR(out, strokeWeight); /* StrokeWeight */
    writeCHAR(out, widthType);  /* WidthType */
    writeCHAR(out, 1 << 6);     /* SerifStyle */
    writeCHAR(out, 0);          /* Reserved */
    return 0;
}
