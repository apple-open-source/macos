/*
 * Copyright 1996, 1997, 1998, 1999 Computing Research Labs,
 * New Mexico State University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COMPUTING RESEARCH LAB OR NEW MEXICO STATE UNIVERSITY BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef lint
#ifdef __GNUC__
static char rcsid[] __attribute__ ((unused)) = "Id: ttf2bdf.c,v 1.25 1999/10/21 16:31:54 mleisher Exp $";
#else
static char rcsid[] = "Id: ttf2bdf.c,v 1.25 1999/10/21 16:31:54 mleisher Exp $";
#endif
#endif

#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <unistd.h>
#endif

#include <string.h>

#include "freetype.h"

/*
 * Include the remapping support.
 */
#include "remap.h"

/**************************************************************************
 *
 * Macros.
 *
 **************************************************************************/

/*
 * The version of ttf2bdf.
 */
#define TTF2BDF_VERSION "2.8"

/*
 * Set the default values used to generate a BDF font.
 */
#ifndef DEFAULT_PLATFORM_ID
#define DEFAULT_PLATFORM_ID 3
#endif

#ifndef DEFAULT_ENCODING_ID
#define DEFAULT_ENCODING_ID 1
#endif

#ifndef DEFAULT_POINT_SIZE
#define DEFAULT_POINT_SIZE 12
#endif

#ifndef DEFAULT_RESOLUTION
#define DEFAULT_RESOLUTION 100
#endif

/*
 * Used as a fallback for XLFD names where the character set/encoding can not
 * be determined.
 */
#ifndef DEFAULT_XLFD_CSET
#define DEFAULT_XLFD_CSET "-FontSpecific-0"
#endif

/*
 * nameID macros for getting strings from the TT font.
 */
#define TTF_COPYRIGHT 0
#define TTF_TYPEFACE  1
#define TTF_PSNAME    6

#ifndef MAX
#define MAX(h,i) ((h) > (i) ? (h) : (i))
#endif

#ifndef MIN
#define MIN(l,o) ((l) < (o) ? (l) : (o))
#endif

/**************************************************************************
 *
 * General globals set from command line.
 *
 **************************************************************************/

/*
 * The program name.
 */
static char *prog;

/*
 * The flag indicating whether messages should be printed or not.
 */
static int verbose = 0;

/*
 * Flags used when loading glyphs.
 */
static int load_flags = TTLOAD_SCALE_GLYPH | TTLOAD_HINT_GLYPH;

/*
 * The default platform and encoding ID's.
 */
static int pid = DEFAULT_PLATFORM_ID;
static int eid = DEFAULT_ENCODING_ID;

/*
 * Default point size and resolutions.
 */
static int point_size = DEFAULT_POINT_SIZE;
static int hres = DEFAULT_RESOLUTION;
static int vres = DEFAULT_RESOLUTION;

/*
 * The user supplied foundry name to use in the XLFD name.
 */
static char *foundry_name = 0;

/*
 * The user supplied typeface name to use in the XLFD name.
 */
static char *face_name = 0;

/*
 * The user supplied weight name to use in the XLFD name.
 */
static char *weight_name = 0;

/*
 * The user supplied slant name to use in the XLFD name.
 */
static char *slant_name = 0;

/*
 * The user supplied width name to use in the XLFD name.
 */
static char *width_name = 0;

/*
 * The user supplied additional style name to use in the XLFD name.
 */
static char *style_name = 0;

/*
 * The user supplied spacing (p = proportional, c = character cell,
 * m = monospace).
 */
static int spacing = 0;

/*
 * The dash character to use in the names retrieved from the font.  Default is
 * the space.
 */
static int dashchar = ' ';

/*
 * Flag, bitmask, and max code for generating a subset of the glyphs in a font.
 */
static int do_subset = 0;
static unsigned short maxcode;
static unsigned long subset[2048];

/*
 * The flag that indicates the remapping table should be used to
 * reencode the font.
 */
static int do_remap = 0;

/**************************************************************************
 *
 * Internal globals.
 *
 **************************************************************************/

/*
 * Structure used for calculating the font bounding box as the glyphs are
 * generated.
 */
typedef struct {
    short minlb;
    short maxlb;
    short maxrb;
    short maxas;
    short maxds;
    short rbearing;
} bbx_t;

static bbx_t bbx;

/*
 * The buffer used to transfer the temporary file to the actual output file.
 */
#define TTF2BDF_IOBUFSIZ 8192
static char iobuf[TTF2BDF_IOBUFSIZ];

/*
 * The Units Per Em value used in numerous places.
 */
static TT_UShort upm;

/*
 * A flag indicating if a CMap was found or not.
 */
static TT_UShort nocmap;

/*
 * The scaling factor needed to compute the SWIDTH (scalable width) value
 * for BDF glyphs.
 */
static double swscale;

/*
 * Mac encoding names used when creating the BDF XLFD font name.
 */
static char *mac_encodings[] = {
    "-MacRoman-0",    "-MacJapanese-0",   "-MacChinese-0",   "-MacKorean-0",
    "-MacArabic-0",   "-MacHebrew-0",     "-MacGreek-0",     "-MacRussian-0",
    "-MacRSymbol-0",  "-MacDevanagari-0", "-MacGurmukhi-0",  "-MacGujarati-0",
    "-MacOriya-0",    "-MacBengali-0",    "-MacTamil-0",     "-MacTelugu-0",
    "-MacKannada-0",  "-MacMalayalam-0",  "-MacSinhalese-0", "-MacBurmese-0",
    "-MacKhmer-0",    "-MacThai-0",       "-MacLaotian-0",   "-MacGeorgian-0",
    "-MacArmenian-0", "-MacMaldivian-0",  "-MacTibetan-0",   "-MacMongolian-0",
    "-MacGeez-0",     "-MacSlavic-0",     "-MacVietnamese-0","-MacSindhi-0",
    "-MacUninterp-0"
};
static int num_mac_encodings = sizeof(mac_encodings) /
                               sizeof(mac_encodings[0]);

/*
 * ISO encoding names used when creating the BDF XLFD font name.
 */
static char *iso_encodings[] = {
    "-ASCII-0", "-ISO10646-0", "-ISO8859-1"
};
static int num_iso_encodings = sizeof(iso_encodings) /
                               sizeof(iso_encodings[0]);

/*
 * Microsoft encoding names used when creating the BDF XLFD font name.
 */
static char *ms_encodings[] = {
    "-Symbol-0", "-ISO10646-1", "-ShiftJIS-0", "-GB2312.1980-0", "-Big5-0",
    "-KSC5601.1987-0", "-KSC5601.1992-0"
};
static int num_ms_encodings = sizeof(ms_encodings) /
                              sizeof(ms_encodings[0]);

/*
 * The propery names for all the XLFD properties.
 */
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

/**************************************************************************
 *
 * Freetype globals.
 *
 **************************************************************************/

static TT_Engine engine;
static TT_Face face;
static TT_Face_Properties properties;

static TT_Instance instance;

static TT_Glyph glyph;
static TT_Glyph_Metrics metrics;
static TT_Instance_Metrics imetrics;

static TT_Raster_Map raster;

static TT_CharMap cmap;

/**************************************************************************
 *
 * Freetype related code.
 *
 **************************************************************************/

/*
 * A generic routine to get a name from the TT name table.  This routine
 * always looks for English language names and checks three possibilities:
 * 1. English names with the MS Unicode encoding ID.
 * 2. English names with the MS unknown encoding ID.
 * 3. English names with the Apple Unicode encoding ID.
 *
 * The particular name ID mut be provided (e.g. nameID = 0 for copyright
 * string, nameID = 6 for Postscript name, nameID = 1 for typeface name.
 *
 * If the `dash' flag is non-zero, all dashes (-) in the name will be replaced
 * with the character passed.
 *
 * Returns the number of bytes added.
 */
static int
#ifdef __STDC__
ttf_get_english_name(char *name, int nameID, int dash)
#else
ttf_get_english_name(name, nameID, dash)
char *name;
int nameID, dash;
#endif
{
    TT_UShort slen;
    int i, j, encid, nrec;
    unsigned short nrPlatformID, nrEncodingID, nrLanguageID, nrNameID;
    char *s;

    nrec = TT_Get_Name_Count(face);

    for (encid = 1, j = 0; j < 2; j++, encid--) {
        /*
         * Locate one of the MS English font names.
         */
        for (i = 0; i < nrec; i++) {
            TT_Get_Name_ID(face, i, &nrPlatformID, &nrEncodingID,
                           &nrLanguageID, &nrNameID);
            if (nrPlatformID == 3 &&
                nrEncodingID == encid &&
                nrNameID == nameID &&
                (nrLanguageID == 0x0409 || nrLanguageID == 0x0809 ||
                 nrLanguageID == 0x0c09 || nrLanguageID == 0x1009 ||
                 nrLanguageID == 0x1409 || nrLanguageID == 0x1809)) {
                TT_Get_Name_String(face, i, &s, &slen);
                break;
            }
        }

        if (i < nrec) {
            /*
             * Found one of the MS English font names.  The name is by
             * definition encoded in Unicode, so copy every second byte into
             * the `name' parameter, assuming there is enough space.
             */
            for (i = 1; s != 0 && i < slen; i += 2) {
                if (dash)
                  *name++ = (s[i] == '-' || s[i] == ' ') ? dash : s[i];
                else if (s[i] == '\r' || s[i] == '\n') {
                    if (s[i] == '\r' && i + 2 < slen && s[i + 2] == '\n')
                      i += 2;
                    *name++ = ' ';
                    *name++ = ' ';
                } else
                  *name++ = s[i];
            }
            *name = 0;
            return (slen >> 1);
        }
    }

    /*
     * No MS English name found, attempt to find an Apple Unicode English
     * name.
     */
    for (i = 0; i < nrec; i++) {
        TT_Get_Name_ID(face, i, &nrPlatformID, &nrEncodingID,
                       &nrLanguageID, &nrNameID);
        if (nrPlatformID == 0 && nrLanguageID == 0 &&
            nrNameID == nameID) {
            TT_Get_Name_String(face, i, &s, &slen);
            break;
        }
    }

    if (i < nrec) {
        /*
         * Found the Apple Unicode English name.  The name is by definition
         * encoded in Unicode, so copy every second byte into the `name'
         * parameter, assuming there is enough space.
         */
        for (i = 1; s != 0 && i < slen; i += 2) {
            if (dash)
              *name++ = (s[i] == '-' || s[i] == ' ') ? dash : s[i];
            else if (s[i] == '\r' || s[i] == '\n') {
                if (s[i] == '\r' && i + 2 < slen && s[i + 2] == '\n')
                  i += 2;
                *name++ = ' ';
                *name++ = ' ';
            } else
              *name++ = s[i];
        }
        *name = 0;
        return (slen >> 1);
    }

    return 0;
}

/**************************************************************************
 *
 * General code.
 *
 **************************************************************************/

/*
 * Create an XLFD name.  Assumes there is enough space in the string passed
 * to fit a reasonably long XLFD name into, up to the 256 byte maximum.
 */
static void
#ifdef __STDC__
make_xlfd_name(char *name, TT_Long awidth, int ismono)
#else
make_xlfd_name(name, awidth, ismono)
char *name;
TT_Long awidth;
int ismono;
#endif
{
    TT_Long i;
    TT_ULong val;
    char *r, *e;
    double dr, dp;

    /*
     * Default the foundry name to "FreeType" in honor of the project and
     * because the foundry name is too difficult to automatically determine
     * from the names in TT fonts. But the user can provide his own.
     */
    if (foundry_name == 0) {
        (void) strcpy(name, "-FreeType");
        name += 9;
    } else {
        *(name++)='-';
        strcpy(name,foundry_name);
        name+=strlen(foundry_name);
    }

    /*
     * Add the typeface name from the font.  The fallback default will be
     * "Unknown".
     */
    *name++ = '-';
    if (face_name == 0) {
        if((i = ttf_get_english_name(name, TTF_TYPEFACE, dashchar)))
          name += i;
        else {
            (void) strcpy(name, "Unknown");
            name += 7;
        }
    } else {
        (void) strcpy(name, face_name);
        name += strlen(face_name);
    }

    /*
     * Add the weight name.  The default will be "Medium".
     */
    if (weight_name != 0) {
        sprintf(name, "-%s", weight_name);
        name += strlen(weight_name) + 1;
    } else {
        if (properties.os2->fsSelection & 0x20) {
            (void) strcpy(name, "-Bold");
            name += 5;
        } else {
            (void) strcpy(name, "-Medium");
            name += 7;
        }
    }

    /*
     * Add the slant name.  The default will be 'R'.
     */
    if (slant_name) {
        sprintf(name, "-%s", slant_name);
        name += strlen(slant_name) + 1;
    } else {
        *name++ = '-';
        if (properties.os2->fsSelection & 0x01)
          *name++ = 'I';
        else
          *name++ = 'R';
    }

    /*
     * Default the setwidth name to "Normal" but user can specify one.
     */
    if (width_name == 0) {
        (void) strcpy(name, "-Normal");
        name += 7;
    } else {
        *(name++)='-';
        strcpy(name,width_name);
        name+=strlen(width_name);
    }

    /*
     * Default the additional style name to NULL but user can specify one.
     */
    *name++ = '-';
    if (style_name != 0) {
        strcpy(name,style_name);
        name+=strlen(style_name);
    }

    /*
     * Determine the pixel size from the point size and resolution.
     */
    dr = (double) vres;
    dp = (double) (point_size * 10);
    val = (unsigned long) (((dp * dr) / 722.7) + 0.5);

    /*
     * Set the pixel size, point size, and resolution.
     */
    sprintf(name, "-%ld-%d-%d-%d", val, point_size * 10, hres, vres);
    name += strlen(name);

    switch (spacing) {
      case 'p': case 'P': spacing = 'P'; break;
      case 'm': case 'M': spacing = 'M'; break;
      case 'c': case 'C': spacing = 'C'; break;
      default: spacing = 0; break;
    }

    /*
     * Set the spacing.
     */
    if (!spacing)
      spacing = (ismono) ? 'M' : 'P';
    *name++ = '-';
    *name++ = spacing;

    /*
     * Add the average width.
     */
    sprintf(name, "-%ld", awidth);
    name += strlen(name);

    /*
     * Check to see if the remapping table specified a registry and encoding
     * and use those if they both exist.
     */
    ttf2bdf_remap_charset(&r, &e);
    if (r != 0 && e != 0) {
        sprintf(name, "-%s-%s", r, e);
        return;
    }

    /*
     * If the cmap for the platform and encoding id was not found, or the
     * platform id is unknown, assume the character set registry and encoding
     * are the XLFD default.
     */
    if (nocmap || pid > 3)
      (void) strcpy(name, DEFAULT_XLFD_CSET);
    else {
        /*
         * Finally, determine the character set registry and encoding from the
         * platform and encoding ID.
         */
        switch (pid) {
          case 0:
            /*
             * Apple Unicode platform, so "Unicode-2.0" is the default.
             */
            (void) strcpy(name, "-Unicode-2.0");
            break;
          case 1:
            /*
             * Macintosh platform, so choose from the Macintosh encoding
             * strings.
             */
            if (eid < 0 || eid >= num_mac_encodings)
              (void) strcpy(name, DEFAULT_XLFD_CSET);
            else
              (void) strcpy(name, mac_encodings[eid]);
            break;
          case 2:
            /*
             * ISO platform, so choose from the ISO encoding strings.
             */
            if (eid < 0 || eid >= num_iso_encodings)
              (void) strcpy(name, DEFAULT_XLFD_CSET);
            else
              (void) strcpy(name, iso_encodings[eid]);
            break;
          case 3:
            /*
             * Microsoft platform, so choose from the MS encoding strings.
             */
            if (eid < 0 || eid >= num_ms_encodings)
              (void) strcpy(name, DEFAULT_XLFD_CSET);
            else
              (void) strcpy(name, ms_encodings[eid]);
            break;
        }
    }
}

static int
#ifdef __STDC__
generate_font(FILE *out, char *iname, char *oname)
#else
generate_font(out, iname, oname)
FILE *out;
char *iname, *oname;
#endif
{
    int eof, ismono, i;
    FILE *tmp;
    TT_Short maxx, maxy, minx, miny, xoff, yoff, dwidth, swidth;
    TT_Short y_off, x_off;
    TT_UShort sx, sy, ex, ey, wd, ht;
    TT_Long code, idx, ng, aw;
    TT_UShort remapped_code;
    unsigned char *bmap;
    double dw;
    char *xp, xlfd[256];
    char *tmpdir, tmpfile[BUFSIZ];

    /*
     * Open a temporary file to store the bitmaps in until the exact number
     * of bitmaps are known.
     */
    if ((tmpdir = getenv("TMPDIR")) == 0)
      tmpdir = "/tmp";
    sprintf(tmpfile, "%s/ttf2bdf%ld", tmpdir, (long) getpid());
    if ((tmp = fopen(tmpfile, "w")) == 0) {
        fprintf(stderr, "%s: unable to open temporary file '%s'.\n",
                prog, tmpfile);
        return -1;
    }

    /*
     * Calculate the scale factor for the SWIDTH field.
     */
    swscale = ((double) vres) * ((double) point_size);

    /*
     * Calculate the font bounding box again so enough storage for the largest
     * bitmap can be allocated.
     */
    minx = (properties.header->xMin * imetrics.x_ppem) / upm;
    miny = (properties.header->yMin * imetrics.y_ppem) / upm;
    maxx = (properties.header->xMax * imetrics.x_ppem) / upm;
    maxy = (properties.header->yMax * imetrics.y_ppem) / upm;

    maxx -= minx; ++maxx;
    maxy -= miny; ++maxy;

    /*
     * Initialize the flag that tracks if the font is monowidth or not and
     * initialize the glyph width variable that is used for testing for a
     * monowidth font.
     */
    wd = 0xffff;
    ismono = 1;

    /*
     * Use the upward flow because the version of FreeType being used when
     * this was written did not support TT_Flow_Down.  This insures that this
     * routine will not mess up if TT_Flow_Down is implemented at some point.
     */
    raster.flow = TT_Flow_Up;
    raster.width = maxx;
    raster.rows = maxy;
    raster.cols = (maxx + 7) >> 3;
    raster.size = raster.cols * raster.rows;
    raster.bitmap = (void *) malloc(raster.size);

    for (ng = code = 0, eof = 0, aw = 0; eof != EOF && code < 0xffff; code++) {

        /*
         * If a remap is indicated, attempt to remap the code.  If a remapped
         * code is not found, then skip generating the glyph.
         */
        remapped_code = (TT_UShort) code;
        if (do_remap && !ttf2bdf_remap(&remapped_code))
          continue;

        /*
         * If a subset is being generated and the code is greater than the max
         * code of the subset, break out of the loop to avoid doing any more
         * work.
         */
        if (do_subset && remapped_code > maxcode)
          break;

        /*
         * If a subset is being generated and the index is not in the subset
         * bitmap, just continue.
         */
        if (do_subset &&
            !(subset[remapped_code >> 5] & (1 << (remapped_code & 31))))
          continue;

        if (nocmap) {
            if (code >= properties.num_Glyphs)

              /*
               * At this point, all the glyphs are done.
               */
              break;
            idx = code;
        } else
          idx = TT_Char_Index(cmap, code);

        /*
         * If the glyph could not be loaded for some reason, or a subset is
         * being generated and the index is not in the subset bitmap, just
         * continue.
         */

        if (idx <= 0 || TT_Load_Glyph(instance, glyph, idx, load_flags))
          continue;

        (void) TT_Get_Glyph_Metrics(glyph, &metrics);

        /*
         * Clear the raster bitmap.
         */
        (void) memset((char *) raster.bitmap, 0, raster.size);

        /*
         * Grid fit to determine the x and y offsets that will force the
         * bitmap to fit into the storage provided.
         */
        xoff = (63 - metrics.bbox.xMin) & -64;
        yoff = (63 - metrics.bbox.yMin) & -64;

        /*
         * If the bitmap cannot be generated, simply continue.
         */
        if (TT_Get_Glyph_Bitmap(glyph, &raster, xoff, yoff))
          continue;

        /*
         * Determine the DWIDTH (device width, or advance width in TT terms)
         * and the SWIDTH (scalable width) values.
         */
        dwidth = metrics.advance >> 6;
        dw = (double) dwidth;
        swidth = (TT_Short) ((dw * 72000.0) / swscale);

        /*
         * Determine the actual bounding box of the glyph bitmap.  Do not
         * forget that the glyph is rendered upside down!
         */
        sx = ey = 0xffff;
        sy = ex = 0;
        bmap = (unsigned char *) raster.bitmap;
        for (miny = 0; miny < raster.rows; miny++) {
            for (minx = 0; minx < raster.width; minx++) {
                if (bmap[(miny * raster.cols) + (minx >> 3)] &
                    (0x80 >> (minx & 7))) {
                    if (minx < sx)
                      sx = minx;
                    if (minx > ex)
                      ex = minx;
                    if (miny > sy)
                      sy = miny;
                    if (miny < ey)
                      ey = miny;
                }
            }
        }

        /*
         * If the glyph is actually an empty bitmap, set the size to 0 all
         * around.
         */
        if (sx == 0xffff && ey == 0xffff && sy == 0 && ex == 0)
          sx = ex = sy = ey = 0;

        /*
         * Increment the number of glyphs generated.
         */
        ng++;

        /*
         * Test to see if the font is going to be monowidth or not by
         * comparing the current glyph width against the last one.
         */
        if (ismono && (ex - sx) + 1 != wd)
          ismono = 0;

        /*
         * Adjust the font bounding box.
         */
        wd = (ex - sx) + 1;
        ht = (sy - ey) + 1;
        x_off = sx - (xoff >> 6);
        y_off = ey - (yoff >> 6);

        bbx.maxas = MAX(bbx.maxas, ht + y_off);
        bbx.maxds = MAX(bbx.maxds, -y_off);
        bbx.rbearing = wd + x_off;
        bbx.maxrb = MAX(bbx.maxrb, bbx.rbearing);
        bbx.minlb = MIN(bbx.minlb, x_off);
        bbx.maxlb = MAX(bbx.maxlb, x_off);

        /*
         * Add to the average width accumulator.
         */
        aw += wd;

        /*
         * Print the bitmap header.
         */
        fprintf(tmp, "STARTCHAR %04lX\nENCODING %ld\n", code,
                (long) remapped_code);
        fprintf(tmp, "SWIDTH %hd 0\n", swidth);
        fprintf(tmp, "DWIDTH %hd 0\n", dwidth);
        fprintf(tmp, "BBX %hd %hd %hd %hd\n", wd, ht, x_off, y_off);

        /*
         * Check for an error return here in case the temporary file system
         * fills up or the file is deleted while it is being used.
         */
        eof = fprintf(tmp, "BITMAP\n");

        /*
         * Now collect the bits so they can be printed.
         */
        for (miny = sy; eof != EOF && miny >= ey; miny--) {
            for (idx = 0, minx = sx; eof != EOF && minx <= ex; minx++) {
                if (minx > sx && ((minx - sx) & 7) == 0) {
                    /*
                     * Print the next byte.
                     */
                    eof = fprintf(tmp, "%02lX", idx & 0xff);
                    idx = 0;
                }
                if (bmap[(miny * raster.cols) + (minx >> 3)] &
                    (0x80 >> (minx & 7)))
                  idx |= 0x80 >> ((minx - sx) & 7);
            }
            if (eof != EOF)
              /*
               * Because of the structure of the loop, the last byte should
               * always be printed.
               */
              fprintf(tmp, "%02lX\n", idx & 0xff);
        }
        if (eof != EOF)
          fprintf(tmp, "ENDCHAR\n");
    }

    fclose(tmp);

    /*
     * If a write error occured, delete the temporary file and issue an error
     * message.
     */
    if (eof == EOF) {
        (void) unlink(tmpfile);
        fprintf(stderr, "%s: problem writing to temporary file '%s'.\n",
                prog, tmpfile);
        if (raster.size > 0)
          free((char *) raster.bitmap);
        return -1;
    }

    /*
     * If no characters were generated, just unlink the temp file and issue a
     * warning.
     */
    if (ng == 0) {
        (void) unlink(tmpfile);
        fprintf(stderr, "%s: no glyphs generated from '%s'.\n", prog, iname);
        if (raster.size > 0)
          free((char *) raster.bitmap);
        return -1;
    }

    /*
     * Reopen the temporary file so it can be copied to the actual output
     * file.
     */
    if ((tmp = fopen(tmpfile, "r")) == 0) {
        /*
         * Unable to open the file for read, so attempt to delete it and issue
         * an error message.
         */
        (void) unlink(tmpfile);
        fprintf(stderr, "%s: unable to open temporary file '%s' for read.\n",
                prog, tmpfile);
        if (raster.size > 0)
          free((char *) raster.bitmap);
        return -1;
    }

    /*
     * Free up the raster storage.
     */
    if (raster.size > 0)
      free((char *) raster.bitmap);

    /*
     * Calculate the average width.
     */
    aw = (TT_Long) ((((double) aw / (double) ng) + 0.5) * 10.0);

    /*
     * Generate the XLFD font name.
     */
    make_xlfd_name(xlfd, aw, ismono);

    /*
     * Start writing the font out.
     */
    fprintf(out, "STARTFONT 2.1\n");

    /*
     * Add the vanity comments.
     */
    fprintf(out, "COMMENT\n");
    fprintf(out, "COMMENT Converted from TrueType font \"%s\" by \"%s %s\".\n",
            iname, prog, TTF2BDF_VERSION);
    fprintf(out, "COMMENT\n");

    fprintf(out, "FONT %s\n", xlfd);
    fprintf(out, "SIZE %d %d %d\n", point_size, hres, vres);

    /*
     * Generate the font bounding box.
     */
    fprintf(out, "FONTBOUNDINGBOX %hd %hd %hd %hd\n",
            bbx.maxrb - bbx.minlb, bbx.maxas + bbx.maxds,
            bbx.minlb, -bbx.maxds);

    /*
     * Print the properties.
     */
    fprintf(out, "STARTPROPERTIES %hd\n", 19);

    /*
     * Print the font properties from the XLFD name.
     */
    for (i = 0, xp = xlfd; i < 14; i++) {
        /*
         * Print the XLFD property name.
         */
        fprintf(out, "%s ", xlfd_props[i]);

        /*
         * Make sure the ATOM properties are wrapped in double quotes.
         */
        if (i < 6 || i == 10 || i > 11)
          putc('"', out);

        /*
         * Skip the leading '-' in the XLFD name.
         */
        xp++;

        /*
         * Skip until the next '-' or NULL.
         */
        for (; *xp && *xp != '-'; xp++)
          putc(*xp, out);

        /*
         * Make sure the ATOM properties are wrapped in double quotes.
         */
        if (i < 6 || i == 10 || i > 11)
          putc('"', out);

        putc('\n', out);
    }

    /*
     * Make sure to add the FONT_ASCENT and FONT_DESCENT properties
     * because X11 can not live without them.
     */
    fprintf(out, "FONT_ASCENT %hd\nFONT_DESCENT %hd\n",
            (properties.horizontal->Ascender * imetrics.y_ppem) / upm,
            -((properties.horizontal->Descender * imetrics.y_ppem) / upm));

    /*
     * Get the copyright string from the font.
     */
    (void) ttf_get_english_name(xlfd, TTF_COPYRIGHT, 0);
    fprintf(out, "COPYRIGHT \"%s\"\n", xlfd);

    /*
     * Last, print the two user-defined properties _TTF_FONTFILE and
     * _TTF_PSNAME.  _TTF_FONTFILE provides a reference to the original TT
     * font file which some systems can take advantage of, and _TTF_PSNAME
     * provides the Postscript name of the font if it exists.
     */
    (void) ttf_get_english_name(xlfd, TTF_PSNAME, 0);
    fprintf(out, "_TTF_FONTFILE \"%s\"\n_TTF_PSNAME \"%s\"\n", iname, xlfd);

    fprintf(out, "ENDPROPERTIES\n");

    /*
     * Print the actual number of glyphs to the output file.
     */
    eof = fprintf(out, "CHARS %ld\n", ng);

    /*
     * Copy the temporary file to the output file.
     */
    while (eof != EOF && (ng = fread(iobuf, 1, TTF2BDF_IOBUFSIZ, tmp))) {
        if (fwrite(iobuf, 1, ng, out) == 0)
          eof = EOF;
    }
        
    /*
     * Close the temporary file and delete it.
     */
    fclose(tmp);
    (void) unlink(tmpfile);

    /*
     * If an error occured when writing to the output file, issue a warning
     * and return.
     */
    if (eof == EOF) {
        fprintf(stderr, "%s: problem writing to output file '%s'.\n",
                prog, oname);
        if (raster.size > 0)
          free((char *) raster.bitmap);
        return -1;
    }

    /*
     * End the font and do memory cleanup on the glyph and raster structures.
     */
    eof = fprintf(out, "ENDFONT\n");

    return eof;
}

static int
#ifdef __STDC__
generate_bdf(FILE *out, char *iname, char *oname)
#else
generate_bdf(out, iname, oname)
FILE *out;
char *iname, *oname;
#endif
{
    TT_Long i;
    TT_UShort p, e;

    /*
     * Get the requested cmap.
     */
    for (i = 0; i < TT_Get_CharMap_Count(face); i++) {
        if (!TT_Get_CharMap_ID(face, i, &p, &e) &&
            p == pid && e == eid)
          break;
    }
    if (i == TT_Get_CharMap_Count(face) && pid == 3 && eid == 1) {
        /*
         * Make a special case when this fails with pid == 3 and eid == 1.
         * Change to eid == 0 and try again.  This captures the two possible
         * cases for MS fonts.  Some other method should be used to cycle
         * through all the alternatives later.
         */
        for (i = 0; i < TT_Get_CharMap_Count(face); i++) {
            if (!TT_Get_CharMap_ID(face, i, &p, &e) &&
                p == pid && e == 0)
              break;
        }
        if (i < TT_Get_CharMap_Count(face)) {
            if (!TT_Get_CharMap(face, i, &cmap))
              eid = 0;
            else
              nocmap = 1;
        }
    } else {
        /*
         * A CMap was found for the platform and encoding IDs.
         */
        if (i < TT_Get_CharMap_Count(face) && TT_Get_CharMap(face, i, &cmap))
          nocmap = 1;
        else
          nocmap = 0;
    }

    if (nocmap && verbose) {
        fprintf(stderr,
                    "%s: no character map for platform %d encoding %d.  ",
                    prog, pid, eid);
        fprintf(stderr, "Generating all glyphs.\n");
    }

    /*
     * Now go through and generate the glyph bitmaps themselves.
     */
    return generate_font(out, iname, oname);
}

#define isdig(cc) ((cc) >= '0' && (cc) <= '9')

/*
 * Routine to parse a subset specification supplied on the command line.
 * The syntax for this specification is the same as the syntax used for
 * the XLFD font names (XLFD documentation, page 9).
 *
 * Example:
 *
 *  "60 70 80_90" means the glyphs at codes 60, 70, and between 80 and
 *  90 inclusive.
 */
static void
#ifdef __STDC__
parse_subset(char *s)
#else
parse_subset(s)
char *s;
#endif
{
    long l, r;

    /*
     * Make sure to clear the flag and bitmap in case more than one subset is
     * specified on the command line.
     */
    maxcode = 0;
    do_subset = 0;
    (void) memset((char *) subset, 0, sizeof(unsigned long) * 2048);

    while (*s) {
        /*
         * Collect the next code value.
         */
        for (l = r = 0; *s && isdig(*s); s++)
          l = (l * 10) + (*s - '0');

        /*
         * If the next character is an '_', advance and collect the end of the
         * specified range.
         */
        if (*s == '_') {
            s++;
            for (; *s && isdig(*s); s++)
              r = (r * 10) + (*s - '0');
        } else
          r = l;

        /*
         * Add the range just collected to the subset bitmap and set the flag
         * that indicates a subset is wanted.
         */
        for (; l <= r; l++) {
            do_subset = 1;
            subset[l >> 5] |= (1 << (l & 31));
            if (l > maxcode)
              maxcode = l;
        }

        /*
         * Skip all non-digit characters.
         */
        while (*s && !isdig(*s))
          s++;
    }
}

static void
#ifdef __STDC__
usage(int eval)
#else
usage(eval)
int eval;
#endif
{
    fprintf(stderr, "Usage: %s [options below] font.ttf\n", prog);
    fprintf(stderr, "-h\t\tThis message.\n");
    fprintf(stderr, "-v\t\tPrint warning messages during conversion.\n");
    fprintf(stderr,
            "-l \"subset\"\tSpecify a subset of glyphs to generate.\n");
    fprintf(stderr, "-m mapfile\tGlyph reencoding file.\n");
    fprintf(stderr, "-n\t\tTurn off glyph hinting.\n");
    fprintf(stderr,
            "-c c\t\tSet the character spacing (default: from font).\n");
    fprintf(stderr,
            "-f name\t\tSet the foundry name (default: freetype).\n");
    fprintf(stderr,
            "-t name\t\tSet the typeface name (default: from font).\n");
    fprintf(stderr, "-w name\t\tSet the weight name (default: Medium).\n");
    fprintf(stderr, "-s name\t\tSet the slant name (default: R).\n");
    fprintf(stderr, "-k name\t\tSet the width name (default: Normal).\n");
    fprintf(stderr,
            "-d name\t\tSet the additional style name (default: empty).\n");
    fprintf(stderr, "-u char\t\tSet the character to replace '-' in names ");
    fprintf(stderr, "(default: space).\n");
    fprintf(stderr,
            "-pid id\t\tSet the platform ID for encoding (default: %d).\n",
            DEFAULT_PLATFORM_ID);
    fprintf(stderr,
            "-eid id\t\tSet the encoding ID for encoding (default: %d).\n",
            DEFAULT_ENCODING_ID);
    fprintf(stderr, "-p n\t\tSet the point size (default: %dpt).\n",
            DEFAULT_POINT_SIZE);
    fprintf(stderr, "-r n\t\tSet the horizontal and vertical resolution ");
    fprintf(stderr, "(default: %ddpi).\n", DEFAULT_RESOLUTION);
    fprintf(stderr, "-rh n\t\tSet the horizontal resolution ");
    fprintf(stderr, "(default: %ddpi)\n", DEFAULT_RESOLUTION);
    fprintf(stderr, "-rv n\t\tSet the vertical resolution ");
    fprintf(stderr, "(default: %ddpi)\n", DEFAULT_RESOLUTION);
    fprintf(stderr,
            "-o outfile\tSet the output filename (default: stdout).\n");
    exit(eval);
}

int
#ifdef __STDC__
main(int argc, char *argv[])
#else
main(argc, argv)
int argc;
char *argv[];
#endif
{
    int res;
    char *infile, *outfile, *iname, *oname;
    FILE *out, *mapin;

    if ((prog = strrchr(argv[0], '/')))
      prog++;
    else
      prog = argv[0];

    out = stdout;
    infile = outfile = 0;

    argc--;
    argv++;

    while (argc > 0) {
        if (argv[0][0] == '-') {
            switch (argv[0][1]) {
              case 'v': case 'V':
                verbose = 1;
                break;
              case 'l': case 'L':
                argc--;
                argv++;
                parse_subset(argv[0]);
                break;
              case 'n': case 'N':
                load_flags &= ~TTLOAD_HINT_GLYPH;
                break;
              case 'c': case 'C':
                argc--;
                argv++;
                spacing = argv[0][0];
                break;
              case 't': case 'T':
                argc--;
                argv++;
                face_name = argv[0];
                break;
              case 'w': case 'W':
                argc--;
                argv++;
                weight_name = argv[0];
                break;
              case 's': case 'S':
                argc--;
                argv++;
                slant_name = argv[0];
                break;
              case 'k': case 'K':
                argc--;
                argv++;
                width_name = argv[0];
                break;
              case 'd': case 'D':
                argc--;
                argv++;
                style_name = argv[0];
                break;
              case 'f': case 'F':
                argc--;
                argv++;
                foundry_name = argv[0];
                break;
              case 'u': case 'U':
                argc--;
                argv++;
                dashchar = argv[0][0];
                break;
              case 'p': case 'P':
                res = argv[0][2];
                argc--;
                argv++;
                if (res == 'i' || res == 'I')
                  /*
                   * Set the platform ID.
                   */
                  pid = atoi(argv[0]);
                else
                  /*
                   * Set the point size.
                   */
                  point_size = atoi(argv[0]);
                break;
              case 'e': case 'E':
                /*
                 * Set the encoding ID.
                 */
                argc--;
                argv++;
                eid = atoi(argv[0]);
                break;
              case 'r':
                /*
                 * Set the horizontal and vertical resolutions.
                 */
                if (argv[0][2] == 'h')
                  hres = atoi(argv[1]);
                else if (argv[0][2] == 'v')
                  vres = atoi(argv[1]);
                else
                  hres = vres = atoi(argv[1]);
                argc--;
                argv++;
                break;
              case 'm': case 'M':
                /*
                 * Try to load a remap table.
                 */
                argc--;
                argv++;

                /*
                 * Always reset the `do_remap' variable here in case more than
                 * one map file appears on the command line.
                 */
                do_remap = 0;
                if ((mapin = fopen(argv[0], "r")) == 0)
                  fprintf(stderr, "%s: unable to open the remap table '%s'.\n",
                          prog, argv[0]);
                else {
                    if (ttf2bdf_load_map(mapin) < 0) {
                        fprintf(stderr,
                                "%s: problem loading remap table '%s'.\n",
                                prog, argv[0]);
                        do_remap = 0;
                    } else
                      do_remap = 1;
                    fclose(mapin);
                }
                break;
              case 'o': case 'O':
                /*
                 * Set the output file name.
                 */
                argc--;
                argv++;
                outfile = argv[0];
                break;
              default:
                usage(1);
            }
        } else
          /*
           * Set the input file name.
           */
          infile = argv[0];

        argc--;
        argv++;
    }

    /*
     * Validate the values passed on the command line.
     */
    if (infile == 0) {
        fprintf(stderr, "%s: no input file provided.\n", prog);
        usage(1);
    }
    /*
     * Set the input filename that will be passed to the generator
     * routine.
     */
    if ((iname = strrchr(infile, '/')))
      iname++;
    else
      iname = infile;

    /*
     * Check the platform and encoding IDs.
     */
    if (pid < 0 || pid > 255) {
        fprintf(stderr, "%s: invalid platform ID '%d'.\n", prog, pid);
        exit(1);
    }
    if (eid < 0 || eid > 65535) {
        fprintf(stderr, "%s: invalid encoding ID '%d'.\n", prog, eid);
        exit(1);
    }

    /*
     * Arbitrarily limit the point size to a minimum of 2pt and maximum of
     * 256pt.
     */
    if (point_size < 2 || point_size > 256) {
        fprintf(stderr, "%s: invalid point size '%dpt'.\n", prog, point_size);
        exit(1);
    }

    /*
     * Arbitrarily limit the resolutions to a minimum of 10dpi and a maximum
     * of 1200dpi.
     */
    if (hres < 10 || hres > 1200) {
        fprintf(stderr, "%s: invalid horizontal resolution '%ddpi'.\n",
                prog, hres);
        exit(1);
    }
    if (vres < 10 || vres > 1200) {
        fprintf(stderr, "%s: invalid vertical resolution '%ddpi'.\n",
                prog, vres);
        exit(1);
    }

    /*
     * Open the output file if specified.
     */
    if (outfile != 0) {
        /*
         * Attempt to open the output file.
         */
        if ((out = fopen(outfile, "w")) == 0) {
            fprintf(stderr, "%s: unable to open the output file '%s'.\n",
                    prog, outfile);
            exit(1);
        }
        /*
         * Set the output filename to be passed to the generator routine.
         */
        if ((oname = strrchr(outfile, '/')))
          oname++;
        else
          oname = outfile;
    } else
      /*
       * Set the default output file name to <stdout>.
       */
      oname = "<stdout>";

    /*
     * Intialize Freetype.
     */
    if ((res = TT_Init_FreeType(&engine))) {
        /*
         * Close the output file.
         */
        if (out != stdout) {
            fclose(out);
            (void) unlink(outfile);
        }
        fprintf(stderr, "%s[%d]: unable to initialize renderer.\n",
                prog, res);
        exit(1);
    }

    /*
     * Open the input file.
     */
    if ((res = TT_Open_Face(engine, infile, &face))) {
        if (out != stdout) {
            fclose(out);
            (void) unlink(outfile);
        }
        fprintf(stderr, "%s[%d]: unable to open input file '%s'.\n",
                prog, res, infile);
        exit(1);
    }

    /*
     * Create a new instance.
     */
    if ((res = TT_New_Instance(face, &instance))) {
        (void) TT_Close_Face(face);
        if (out != stdout) {
            fclose(out);
            (void) unlink(outfile);
        }
        fprintf(stderr, "%s[%d]: unable to create instance.\n",
                prog, res);
        exit(1);
    }

    /*
     * Set the instance resolution and point size and the relevant
     * metrics.
     */
    (void) TT_Set_Instance_Resolutions(instance, hres, vres);
    (void) TT_Set_Instance_CharSize(instance, point_size*64);
    (void) TT_Get_Instance_Metrics(instance, &imetrics);

    /*
     * Get the face properties and set the global units per em value for
     * convenience.
     */
    (void) TT_Get_Face_Properties(face, &properties);
    upm = properties.header->Units_Per_EM;

    /*
     * Create a new glyph container.
     */
    if ((res = TT_New_Glyph(face, &glyph))) {
        (void) TT_Done_Instance(instance);
        (void) TT_Close_Face(face);
        if (out != stdout) {
            fclose(out);
            (void) unlink(outfile);
        }
        fprintf(stderr, "%s[%d]: unable to create glyph.\n",
                prog, res);
        exit(1);
    }

    /*
     * Generate the BDF font from the TrueType font.
     */
    res = generate_bdf(out, iname, oname);

    /*
     * Free up the mapping table if one was loaded.
     */
    ttf2bdf_free_map();

    /*
     * Close the input and output files.
     */
    (void) TT_Close_Face(face);
    if (out != stdout) {
        fclose(out);
        if (res < 0)
          /*
           * An error occured when generating the font, so delete the
           * output file.
           */
          (void) unlink(outfile);
    }

    /*
     * Shut down the renderer.
     */
    (void) TT_Done_FreeType(engine);

    exit(res);

    return 0;
}
