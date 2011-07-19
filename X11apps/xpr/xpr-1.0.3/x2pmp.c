/*
 *	$XConsortium: x2pmp.c,v 1.13 91/07/25 17:56:32 rws Exp $
 */

/* x2pmp.c: Translate xwd window dump format into PMP format for the
 * IBM 3812 PagePrinter.
 */
#include <stdio.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/XWDFile.h>
#include <X11/Xfuncs.h>
#include <errno.h>

#include "pmp.h"
#include "xpr.h"

#define max_(a, b) ((a) > (b) ? (a) : (b))
#define min_(a, b) ((a) < (b) ? (a) : (b))
#define abs_(a)    ((a) < 0 ? -(a) : (a))


/* Local prototypes */
static unsigned char *magnification_table(int scale);
static int bits_set(int n);
static void leave(const char *s);
static void p_move_abs(FILE *p, int x, int y);
static void p_save_cursor(FILE *p, int reg);
static void p_restore_cursor(FILE *p, int reg);
static void p_set_orientation(FILE *p, enum orientation orient);
static void p_bitmap(
  FILE *p,
  unsigned int h, int w,
  unsigned long buflen,
  unsigned char *buf);

static int plane = 0;
#define FONT_HEIGHT 40
#define FONT_HEIGHT_PIXELS (FONT_HEIGHT*75/PPI)
#define FONT_WIDTH 24

void x2pmp(FILE *in, FILE *out,
  int scale,
  int p_width, int p_length, int x_pos, int y_pos, /* in pels (units of PPI) */
  char *head, char *foot,
  enum orientation orient,
  int invert)
{
    unsigned char *buffer, *win_name;
    unsigned int win_name_size, width, height, ncolors;
    unsigned int buffer_size, one_plane_size, byte_width, fixed_width;
    int no_of_bits;
    unsigned long swaptest = 1;
    XWDFileHeader header;

    /* Read header from file */
    if (fread((char *)&header, sizeof(header), 1, in) != 1) {
      if (feof(in)) 
	return;
      else
	leave("fread");
    }
    if (*(char *) &swaptest)
      _swaplong((char *) &header, sizeof(header));

    if (header.file_version != XWD_FILE_VERSION) {
	fprintf(stderr,"%s: file format version %d, not %d\n", progname,
		(int)header.file_version, XWD_FILE_VERSION);
    }

    win_name_size = abs_(header.header_size - sizeof(header));
    if ((win_name = (unsigned char *)
	 calloc(win_name_size, (unsigned) sizeof(char))) == NULL)
      leave("Can't calloc window name storage.");

    /* Read window name from file */
    if (fread((char *) win_name, sizeof(char), (int) win_name_size, in) !=
	win_name_size)
      leave("Unable to read window name from dump file.");
    DEBUG(>= 1)
      fprintf(stderr,"win_name =%s\n", win_name);

    width = header.pixmap_width;
    height = header.pixmap_height;
    fixed_width = 8 * (byte_width = header.bytes_per_line);
    one_plane_size = byte_width * height;
    buffer_size = one_plane_size *
      ((header.pixmap_format == ZPixmap)? header.pixmap_depth: 1);
    
    /* Determine orientation and scale if not specified */
    if (orient == UNSPECIFIED)
      orient = (fixed_width <= height)? PORTRAIT: LANDSCAPE;
    if (scale <= 0) {
        int real_height = height;
	if (head) real_height += FONT_HEIGHT_PIXELS << 1;
	if (foot) real_height += FONT_HEIGHT_PIXELS << 1;
	switch(orient) {
	default:
	case PORTRAIT:
	case UPSIDE_DOWN:
	    scale = min_((p_width - 2*x_pos) / fixed_width,
			 (p_length - 2*y_pos) / real_height);
	    break;
	case LANDSCAPE:
	case LANDSCAPE_LEFT:
	    scale = min_((p_length - 2*y_pos) / fixed_width,
			 (p_width - 2*x_pos) / real_height);
	    break;
	}
	if (scale <= 0)
	  leave("PixMap doesn't fit on page.");
	else DEBUG(>1)
	  fprintf(stderr, "scaling by %d to yield %d x %d image\n",
		  scale, fixed_width*scale, height*scale);
    }

    ncolors = header.ncolors;
    if (ncolors) {
	int i;
	XColor *colors = (XColor *)malloc((unsigned) (header.ncolors * sizeof(XColor)));

	if (fread((char *)colors, sizeof(XColor), ncolors, in) != ncolors)
	  leave("Unable to read colormap from dump file.");

	if (*(char *) &swaptest) {
	    for (i = 0; i < ncolors; i++) {
		_swaplong((char *) &colors[i].pixel, (long)sizeof(long));
		_swapshort((char *) &colors[i].red, (long) (3 * sizeof(short)));
	    }
	}
	if (ncolors == 2 && INTENSITY(&colors[0]) > INTENSITY(&colors[1]))
	    invert = !invert;
	free( colors );
    }

    invert = !invert;		/* 3812 puts ink (i.e. black) on 1-bits */

    if ((buffer = (unsigned char *) calloc(buffer_size, 1)) == NULL)
      leave("Can't calloc data buffer.");
    bzero((char *) buffer, (int) buffer_size);

    /* Read bitmap from file */
    if (fread((char *) buffer, sizeof(char), (int) buffer_size, in)
	!= buffer_size)
      leave("Unable to read pixmap from dump file.");

    if (header.bitmap_bit_order == LSBFirst)
    {
	unsigned char bitswap[256], *bp;
	int c;
	for(c = 256; c--;) {
	    bitswap[c] = ((c & 01) << 7) + ((c & 02) << 5) + ((c & 04) << 3) +
	      ((c & 010) << 1) + ((c & 020) >> 1) + ((c & 040) >> 3) +
		((c & 0100) >> 5) + ((c & 0200) >> 7);
	    if (invert)
	      bitswap[c] = ~bitswap[c];
	}
	/* Here's where we do the bitswapping. */
	for(bp = buffer+buffer_size; bp-- > buffer;)
	  *bp = bitswap[*bp];
    }
    else if (invert) {
        unsigned char *bp;
	for(bp = buffer+buffer_size; bp-- > buffer;)
	  *bp = ~*bp;
    }

    /* we don't want the last bits up to the byte/word alignment set */
    if ((no_of_bits = fixed_width - width)) {
	int i, j, mask = ~bits_set(no_of_bits % 8);
	for(i = 0; i < height; i++) {
	    unsigned char *s = buffer + (i+1) * byte_width ;

	    for(j = no_of_bits / 8; j--;)
	      *--s = 0;
	    *--s &= mask;
	}
    }

    DEBUG(>= 1)
      fprintf(stderr,"read %d bytes for a %d (%d bytes) x %d image\n",
	      buffer_size, (int) width, byte_width, (int) height);
    /* Scale the bitmap */
    if (scale > 1) {
	unsigned char *tbl = magnification_table(scale);
	unsigned char *scale_buf;
	int i, j, k;
	
	if ((scale_buf = (unsigned char *)
	     calloc((unsigned) (buffer_size *= scale*scale), sizeof(char)))
	    == NULL)
	  leave("Can't calloc scaled buffer.");
	for(i = 0; i < height; i++) {
	    unsigned char *src, *ss;
	    src = buffer + i * byte_width ;
	    ss = scale_buf + i * scale * scale * byte_width;
	    for(j = 0; j < byte_width; j++) {
		unsigned char *dst = ss+j*scale;
		unsigned char *expansion = tbl+scale*src[j];
		for(k = 0; k < scale; k++, dst += byte_width*scale) {
		    bcopy((char *) expansion, (char *) dst, scale);
		}
	    }
	}
	free((char *) buffer);
	free((char *) tbl);
	buffer = scale_buf;
	byte_width *= scale;
	width *= scale;
	fixed_width *= scale;
	height *= scale;
	one_plane_size *= scale*scale;
    }
    DEBUG(==3) {
	int i, j, k;
	unsigned char *s;

	fprintf(stderr, "dumping %d x %d grid\n", fixed_width, height);
	for(i = 0; i < height; i++) {
	    s = buffer + i * byte_width ;
	    for(j = 0; j < byte_width; j++)
	      for(k = 8; k--;)
		(void) putc((s[j] & 1<<k)? '*': '-', stderr);
	    (void) putc('\n', stderr);
	}
    }
    p_set_orientation(out, orient);
    p_restore_cursor(out, 0);
    p_save_cursor(out, 3);
    if (head != NULL) {
	p_move_abs( out, x_pos + (width - strlen(foot)*FONT_WIDTH) >> 1,
		         y_pos - FONT_HEIGHT );
	fprintf(out, "%s\n", head);
    }
    if (foot != NULL) {
	p_move_abs( out, x_pos + (width - strlen(foot)*FONT_WIDTH) >> 1,
			 y_pos + height + (FONT_HEIGHT << 1) );
	fprintf(out, "%s\n", foot);
    }
    p_move_abs(out, x_pos, y_pos);
    p_bitmap(out, height, fixed_width, (unsigned long) one_plane_size,
	     buffer + plane * one_plane_size);
    free((char *) win_name);
    free((char *) buffer);
}

static
unsigned char *magnification_table(int scale)
{
    unsigned char *tbl;
    int c;

    if ((tbl = (unsigned char *)
	 calloc((unsigned) (scale*256), sizeof(char))) == NULL)
      leave("Can't calloc magnification table.");
    bzero((char *) tbl, scale*256);
    for(c = 256; c--;) {
	int b = c, bit;
	unsigned char *entry = tbl+c*scale;

	while (b) {
	    int i, last, mask;
	    bit = 1;
	    mask = b;
	    while (! (mask & 1)) {
		bit++;
		mask = mask >> 1;
	    }
	    last = scale*(bit-1);
	    for(i = scale*bit; i-- > last ;)
	      entry[(scale - 1) - i / 8] |= 1 << (i % 8);
	    b &= ~(1 << bit-1);
	}
    }
    return tbl;
}

/* returns 2^n-1, i.e. a number with bits n-1 through 0 set.
 * (zero for n == 0) */
static
int bits_set(int n)
{
    int ans = 0;
    while(n--)
      ans |= 1 << n;
    return ans;
}

static
void leave(const char *s)
{
    fprintf(stderr, "\n%s: ", progname);
    if (errno != 0)
      perror(s);
    else
      fprintf(stderr, "%s", s);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

/* move to coordinates x, y (in pels) */
static
void p_move_abs(FILE *p, int x, int y)
{
    if (x >= 0)  {
	PMP(p, 3);
	(void) putc('\340', p);
	p_wput(x, p);
    }
    if (y >= 0)  {
	PMP(p, 3);
	(void) putc('\341', p);
	p_wput(y, p);
    }
}

/* save current cursor position into (printer) register reg */
static
void p_save_cursor(FILE *p, int reg)
{
    PMP(p, 1);
    (void) putc(reg + '\200', p);
}

/* restore current cursor position from (printer) register reg */
static
void p_restore_cursor(FILE *p, int reg)
{
    PMP(p, 1);
    (void) putc(reg + '\220', p);
}

/* set the page orientation to orient (see pmp.h) */
static
void p_set_orientation(FILE *p, enum orientation orient)
{
    PMP(p, 2);
    fprintf(p, "\322%c", (int) orient);
}

/* generate bitmap */
static
void p_bitmap(
  FILE *p,
  unsigned int h, int w,
  unsigned long buflen,
  unsigned char *buf)
{
    PMP(p, 9);
    (void) fwrite("\365\0", 1, 2, p);
    puthl2(h, p);
    puthl2(w, p);
    puthl3(buflen, p);
    
    while(buflen) {
	int len;
	
	len = min(buflen, MAX_VECTOR_LEN);
	PMP(p, len);
	(void) fwrite((char *) buf, 1, len, p);
	buf += len;
	buflen -= len;
    }
    (void) fflush(p);
}
