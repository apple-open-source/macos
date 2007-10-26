/* $Xorg: lbxfax.h,v 1.3 2000/08/17 19:46:41 cpqbld Exp $ */
/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992 Sam Leffler
 * Copyright (c) 1991, 1992 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */
/* $XFree86$ */

/*
 * This header file contains various constants and tables needed
 * for FAX G42D compression.
 */

typedef struct tableentry {
    unsigned short length;	/* bit length of g3 code */
    unsigned short code;	/* g3 code */
    short	runlen;		/* run length in bits */
} tableentry;

#define	EOL	0x001	/* EOL code value - 0000 0000 0000 1 */

/* status values returned instead of a run length */

#define	G3CODE_INVALID	-1
#define	G3CODE_INCOMP	-2
#define	G3CODE_EOL	-3
#define	G3CODE_EOF	-4

#define ROUNDUP8(x) ((x + 7) >> 3)

#ifdef __APPLE__
#ifndef TIFFaxWhiteCodes
#define TIFFFaxWhiteCodes Darwin_X_TIFFFaxWhiteCodes
#define TIFFFaxBlackCodes Darwin_X_TIFFFaxBlackCodes
#endif
#endif
extern tableentry TIFFFaxWhiteCodes[];
extern tableentry TIFFFaxBlackCodes[];

/* misc.c */
extern int LbxImageFindDiff ( unsigned char *cp, int bs, int be, int color );
extern void LbxReverseBits ( unsigned char *cp, int n );
