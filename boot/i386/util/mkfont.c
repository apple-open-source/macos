/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * mkfont ASCII_BITMAP_INPUT [-c (generate .c file)] [BITMAP_OUTPUT]
 *
 * This program parses the ascii bitmap (screen font) description given as
 * input and produces either a binary font file suitable for use with
 * "fbshow" or a .c file containing an initialized font_c_t struct. 
 *
 * If the output file is not specified, it is created in the current directory
 * and given the name FontFamily-FontFace.PointSize (with a .c appended if
 * the -c option is specified); e.g. for 18 point Helvetica Bold, the output
 * file would be: Helvetica-Bold.18.
 *
 * The name of the font_c_t struct created by  the -c option is
 * FontFamily_FontFace_PointSize.
 */
 
#define	EXPORT_BOOLEAN
#import <stdio.h>
#import <libc.h>
#import <ctype.h>
#import <sys/file.h>
#import <mach/boolean.h>
#import "font.h"

#define	BITSINCR	(8*1024*8)		/* in honest-to-god bits! */
#define	MAXLINE		256

typedef enum {
	ENDCHAR_LINE, ENCODING_LINE, DWIDTH_LINE, SWIDTH_LINE, BBX_LINE,
	BITMAP_LINE, DATA_LINE, FONT_LINE, SIZE_LINE, FONTBOUNDINGBOX_LINE,
	CHARS_LINE, STARTCHAR_LINE, ENDFONT_LINE, STARTPROPERTIES_LINE,
	ENDPROPERTIES_LINE, UNKNOWN_LINE, STARTFONT_LINE, COMMENT_LINE
} line_t;

typedef struct {
	char *string;
	line_t linetype;
	int len;
} linetbl_t;

#define	LINETBL(x)		{ #x,	x##_LINE,	(sizeof #x) - 1 }

linetbl_t linetbl[] = {
	LINETBL(ENDCHAR),
	LINETBL(ENCODING),
	LINETBL(DWIDTH),
	LINETBL(SWIDTH),
	LINETBL(BBX),
	LINETBL(BITMAP),
	LINETBL(DATA),
	LINETBL(FONT),
	LINETBL(SIZE),
	LINETBL(FONTBOUNDINGBOX),
	LINETBL(CHARS),
	LINETBL(STARTCHAR),
	LINETBL(ENDFONT),
	LINETBL(STARTPROPERTIES),
	LINETBL(ENDPROPERTIES),
	LINETBL(UNKNOWN),
	LINETBL(STARTFONT),
	LINETBL(COMMENT),
	{ NULL, UNKNOWN_LINE, 0 }
};

static void read_char_description(void);
static void setbits(int bitx, int nbits, int scanbits);
static void fatal(const char *format, ...);
static void parse_error(void);
static void duplicate(char *line);
static line_t linetype(char *linebuf);
static char *getline(char *buf, int buflen, FILE *stream);
static void write_c_file(const char *outfile);
static void write_bin_file(const char *outfile);

const char *program_name;
font_t font;
unsigned char *bits = NULL;
int bitsused;					/* in honest-to-god bits */
int bitsalloc;					/* in honest-to-god bits */
char linebuf[MAXLINE];
FILE *input, *output;

void
main(int argc, const char * const argv[])
{
	const char *infile;
	const char *outfile = NULL;
	char filename[FONTNAMELEN+20];
	boolean_t did_endfont_line, did_font_line, did_size_line,
	    did_fontboundingbox_line, did_chars_line, did_startfont_line;
	int chars, chars_processed, nprops, bytes;
	boolean_t gen_c_file = FALSE;
	const char *argp;
	char c;
	short s1, s2, s3, s4;
	
	program_name = *argv++; argc--;
	if (argc < 1 || argc > 3) {
		fatal("Usage: %s ASCII_BITMAP_INPUT [-c (generate .c file)] "
			"[BITMAP_OUTPUT]", program_name);
	}
	
	infile = *argv++; argc--;
	while(argc) {
		if (**argv == '-') {
			argp = *argv++ + 1; 
			argc--;
			while (c  = *argp++) {
				switch(c) {
				   case 'c':
				   	gen_c_file = TRUE;
					break;
				    default:
				    	fatal("Usage: mkfont "
					   "ASCII_BITMAP_INPUT "
					   "[-c (generate .c file)]");
				}
			}
		}
		else {
			outfile = *argv++; argc--;
		}
	}
	
	if ((input = fopen(infile, "r")) == NULL)
		fatal("Can't open input file %s", infile);
	
	did_endfont_line = FALSE;
	did_font_line = FALSE;
	did_size_line = FALSE;
	did_fontboundingbox_line = FALSE;
	did_chars_line = FALSE;
	did_startfont_line = FALSE;
	chars_processed = 0;
	chars = 0;
	
	while (!did_endfont_line
	 && getline(linebuf, sizeof linebuf, input) != NULL) {
		switch (linetype(linebuf)) {
		case COMMENT_LINE:
		default:
			break;
		case ENDCHAR_LINE:
		case ENCODING_LINE:
		case DWIDTH_LINE:
		case SWIDTH_LINE:
		case BBX_LINE:
		case BITMAP_LINE:
		case DATA_LINE:
			parse_error();
		case STARTFONT_LINE:
			if (did_startfont_line)
				duplicate("STARTFONT");
			did_startfont_line = TRUE;
			break;
		case FONT_LINE:
			if (sscanf(linebuf, "%*s %s", font.font) != 1)
				parse_error();
			if (did_font_line)
				duplicate("FONT");
			did_font_line = TRUE;
			break;
		case SIZE_LINE:
			if (sscanf(linebuf, "%*s %hd", &font.size) != 1)
				parse_error();
			if (did_size_line)
				duplicate("SIZE");
			did_size_line = TRUE;
			break;
		case FONTBOUNDINGBOX_LINE:
			if (sscanf(linebuf, "%*s %hd %hd %hd %hd",
			 &s1, &s2, &s3, &s4) != 4)
				parse_error();
			font.bbx.width = s1;
			font.bbx.height = s2;
			font.bbx.xoff = s3;
			font.bbx.yoff = s4;
			if (did_fontboundingbox_line)
				duplicate("FONTBOUNDINGBOX");
			did_fontboundingbox_line = TRUE;
			break;
		case CHARS_LINE:
			if (sscanf(linebuf, "%*s %d", &chars) != 1)
				parse_error();
			if (did_chars_line)
				duplicate("CHARS");
			did_chars_line = TRUE;
			break;
		case STARTCHAR_LINE:
			read_char_description();
			chars_processed++;
			break;
		case ENDFONT_LINE:
			did_endfont_line = TRUE;
			break;
		case STARTPROPERTIES_LINE:
			if (sscanf(linebuf, "%*s %d", &nprops) != 1)
				parse_error();
			while (nprops-- > 0 && getline(linebuf, sizeof linebuf, input) != NULL)
				continue;
			if (nprops != 0 || getline(linebuf, sizeof linebuf, input) == NULL
			  || linetype(linebuf) != ENDPROPERTIES_LINE)
				parse_error();
			break;
		}
	}
	fclose(input);

	if (! did_font_line || ! did_size_line || ! did_fontboundingbox_line
	 || ! did_chars_line || ! did_endfont_line || ! did_startfont_line)
	 	fatal("Incomplete input file");
	if (chars_processed != chars)
		fatal("Input file missing character descriptions");
	if (bits == NULL)
		fatal("No bitmaps generated!");
	if(gen_c_file) {
		/*
		 * Generate a compilable file. 
		 */
		write_c_file(outfile);
	}
	else {
		write_bin_file(outfile);
	}
}

static void
read_char_description(void)
{
	boolean_t did_endchar_line, did_encoding_line, did_dwidth_line, did_bbx_line,
	 did_bitmap_line;
	int encoding, scanbits, nbits, h;
	bitmap_t bm;
	short s1, s2, s3, s4;
	
	did_endchar_line = FALSE;
	did_encoding_line = FALSE;
	did_dwidth_line = FALSE;
	did_bbx_line = FALSE;
	did_bitmap_line = FALSE;
	nbits = 0;
	bzero(&bm, sizeof bm);
	while (! did_endchar_line && getline(linebuf, sizeof linebuf, input) != NULL) {
		switch (linetype(linebuf)) {
		case FONT_LINE:
		case SIZE_LINE:
		case FONTBOUNDINGBOX_LINE:
		case ENDFONT_LINE:
		case CHARS_LINE:
		case DATA_LINE:
		case STARTPROPERTIES_LINE:
		case ENDPROPERTIES_LINE:
		case STARTFONT_LINE:
			parse_error();
		case ENCODING_LINE:
			if (sscanf(linebuf, "%*s %d", &encoding) != 1)
				parse_error();
			if (did_encoding_line)
				duplicate("ENCODING");
			did_encoding_line = TRUE;
			break;
		case ENDCHAR_LINE:
			did_endchar_line = TRUE;
			break;
		case DWIDTH_LINE:
			if (sscanf(linebuf, "%*s %hd", &s1) != 1)
				parse_error();
			bm.dwidth = s1;
			if (did_dwidth_line)
				duplicate("DWIDTH");
			did_dwidth_line = TRUE;
			break;
		case BBX_LINE:
			if (sscanf(linebuf, "%*s %hd %hd %hd %hd",
			 &s1, &s2, &s3, &s4) != 4)
			 	parse_error();
			bm.bbx.width = s1;
			bm.bbx.height = s2;
			bm.bbx.xoff = s3;
			bm.bbx.yoff = s4;

			if (did_bbx_line)
				duplicate("BBX");
			did_bbx_line = TRUE;
			break;
		case BITMAP_LINE:
			if (! did_bbx_line)
				fatal("BITMAP line not proceeded by BBX line");
			if (did_bitmap_line)
				duplicate("BITMAP");
			did_bitmap_line = TRUE;
			nbits = bm.bbx.width * bm.bbx.height;
			while (bitsused + nbits > bitsalloc) {
				if (bits == NULL) {
					bits = (unsigned char *)malloc(BITSINCR >> 3);
					bitsused = 1;	/* bitx == 0 means no char */
				} else
					bits = (unsigned char *)realloc(bits,
					 (BITSINCR + bitsalloc + 7) >> 3);
				if (bits == NULL)
					fatal("Out of memory");
				bitsalloc += (BITSINCR >> 3) * 8;
			}
			bm.bitx = bitsused;
			for (h = 0; h < bm.bbx.height; h++) {
				if (getline(linebuf, sizeof linebuf, input) == NULL)
					fatal("Unexpected EOF on input");
				if (linetype(linebuf) != DATA_LINE)
					parse_error();
				if (sscanf(linebuf, "%x", &scanbits) != 1)
					parse_error();
				setbits(bm.bitx + h * bm.bbx.width, bm.bbx.width, scanbits);
			}
			break;
		}
	}
	if ( ! did_endchar_line || ! did_encoding_line || ! did_dwidth_line
	 || ! did_bbx_line || ! did_bitmap_line)
	 	parse_error();
	if (encoding >= ENCODEBASE && encoding <= ENCODELAST) {
		font.bitmaps[encoding - ENCODEBASE] = bm;
		bitsused += nbits;
	}
	return;
}

static void
setbits(int bitx, int nbits, int scanbits)
{
	int i, mask;
	
	for (i = 0; i < nbits; i++) {
		mask = 0x80 >> ((bitx + i) & 0x7);
		if (scanbits & (1 << (((nbits + 7) & ~7) - i - 1)))
			bits[(bitx + i) >> 3] |= mask;
		else
			bits[(bitx + i) >> 3] &= ~mask;
	}
}
	
static void
fatal(const char *format, ...)
{
	va_list ap;
	
	va_start(ap, format);
	fprintf(stderr, "%s: ", program_name);
	vfprintf(stderr, format, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

static int lineNum = 0;

static void
parse_error(void)
{
	fatal("Couldn't parse line %d: <%s>", lineNum, linebuf);
}

static void
duplicate(char *line)
{
	fatal("Duplicate declaration of %s", line);
}

static line_t
linetype(char *linebuf)
{
	linetbl_t *ltp;
	int len;

	for (ltp = linetbl; ltp->string; ltp++) {
		if (strncmp(ltp->string, linebuf, ltp->len) == 0
		 && (linebuf[ltp->len] == '\0' || isspace(linebuf[ltp->len])))
		 	return ltp->linetype;
	}
	len = strlen(linebuf);
	if (len == 0)
		return UNKNOWN_LINE;
	while (--len > 0)
		if (! isxdigit(*linebuf++))
			break;
	return (len == 0) ? DATA_LINE : UNKNOWN_LINE;
}

static char *
getline(char *buf, int buflen, FILE *stream)
{
	char *retval;
	int len;
	
	retval = fgets(buf, buflen, stream);
	lineNum++;

	if (retval == NULL)
		return retval;
		
	len = strlen(buf);
	while (len > 0 && isspace(buf[--len]))
		buf[len] = '\0';
	return retval;
}

/*
 * generate raw binary file which will get map_fd'd into
 * a font_t in fbshow.
 */
static void write_bin_file(const char *outfile)
{
	char filename[FONTNAMELEN+20];
	FILE *output;
	int bytes;
		
	/*
	 * Generate default filename if necessary.
	 */	
	if (outfile == NULL) {
		sprintf(filename, "%s.%d", font.font, font.size);
		outfile = filename;
	}
	if ((output = fopen(outfile, "w")) == NULL)
		fatal("Can't create output file %s", outfile);
		
	/*
	 * Write the font_t.
	 */
	if (fwrite(&font, sizeof font, 1, output) != 1)
		fatal("Write to file %s failed", outfile);

	/*
	 * Now the bit array (declared  in font_t as size 0).
	 */
	bytes = (bitsused + 7) >> 3;
	if (fwrite(bits, sizeof(unsigned char), bytes, output) != bytes) {
		fatal("Write to file %s failed", output);
	}
	fclose(output);
}

/*
 * Generate a compilable file.
 */
#define FONTNAME_STRING_NAME	"fontname"
#define BITS_ARRAY_NAME		"bits_array"

static void write_c_file(const char *outfile)
{
	char structname[FONTNAMELEN+20];
	char filename[FONTNAMELEN+20];
	char fontname[FONTNAMELEN+20];
	char *np;
	FILE  *out;
	char line[120];
	int bytes;
	unsigned char *bitp;
	int loop;
	bitmap_t *bmap;
	
	/*
	 * Generate default filename if necessary.
	 */	
	if (outfile == NULL) {
		sprintf(filename, "%s.%d.c", font.font, font.size);
		outfile = filename;
	}

	if ((out = fopen(outfile, "w")) == NULL)
		fatal("Can't create output file %s", outfile);
	
	/*
	 * generate the name of the font_c_t struct, converting 
	 * possible '-' into '_'.
	 */
	sprintf(structname, "%s_%d", font.font, font.size);
	for(np=structname; *np; np++) {
		if(*np == '-') {
			*np = '_';
		}
	}
	
	/*
	 * Start emitting code. Place fontname and bits_array first to keep
	 * them static.
	 */
	fprintf(out, "/* generated by mkfont */\n\n");
	
	/*
	 * FIXME - maybe this should be passed in in argv...
	 */
	fprintf(out, "#import \"font.h\"\n\n");
		
	/*
	 * The bit array first.
	 */
	fprintf(out, "static const char %s[] = {\n", BITS_ARRAY_NAME);
	bytes = (bitsused + 7) >> 3;
	bitp = bits;
	fprintf(out, "\t");
	for(loop=0; loop<bytes; loop++) {
		fprintf(out, "0x%02x, ", *bitp++);
		if(loop % 8 == 7) {
			/*
			 * Line wrap.
			 */
			fprintf(out, "\n\t");
		}
	}

	fprintf(out, "\n};\n\n");
	
	/*
	 * Finally, the font_c_t itself. 
	 */
	fprintf(out, "const font_c_t %s = {\n", structname); 
	fprintf(out, "\t\"%s-%d\",\n", font.font, font.size);
	fprintf(out, "\t%d,\n", font.size);
	fprintf(out, "\t{%d, %d, %d, %d},\n",
		font.bbx.width, font.bbx.height, 
		font.bbx.xoff, font.bbx.yoff);
		
	/*
	 * bitmap structs.
	 */
	bmap = font.bitmaps;
	fprintf(out, "\t{\n");
	for(loop=0; loop<(ENCODELAST - ENCODEBASE + 1); loop++) {
		fprintf(out, "\t    { {%4d, %4d, %4d, %4d}, %4d, %6d },",
			bmap->bbx.width, bmap->bbx.height,
			bmap->bbx.xoff, bmap->bbx.yoff,
			bmap->dwidth, bmap->bitx);
		bmap++;
		fprintf(out, "\t/* 0x%02x */\n", loop + ENCODEBASE);
	}
	fprintf(out, "\n\t},\n");
	
	/*
	 * and the bits array ptr.
	 */
	fprintf(out, "\t%s,\n", BITS_ARRAY_NAME);
	fprintf(out, "};\n");
	
	/*
	 * now some junk to make our code smaller.
	 */
	fprintf(out, "\n#define %s_BBX_WIDTH\t%d\n",
		    structname, font.bbx.width);
	fprintf(out, "#define %s_BBX_HEIGHT\t%d\n",
		    structname, font.bbx.height);
	fprintf(out, "#define %s_BBX_XOFF\t%d\n",
		    structname, font.bbx.xoff);
	fprintf(out, "#define %s_BBX_YOFF\t%d\n",
		    structname, font.bbx.yoff);

	fclose(out);	
}

