/* $Xorg: misc.h,v 1.4 2001/02/09 02:05:44 xorgcvs Exp $ */
/*
 
Copyright 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices, or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES,
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */
/* $XFree86: xc/programs/xfs/include/misc.h,v 3.8 2001/07/25 15:05:21 dawes Exp $ */

#ifndef _MISC_H_
#define _MISC_H_

#include	<X11/Xosdefs.h>
#include	<X11/Xfuncs.h>
#include	<X11/Xdefs.h>

#include	"assert.h"	/* so its everywhere */

#ifndef NULL
#include	<stddef.h>
#endif

#define	MAXCLIENTS	128

#define	MILLI_PER_SECOND	(1000)
#define	MILLI_PER_MINUTE	(1000 * 60)

#ifndef TRUE
#define	TRUE 1
#define	FALSE 0
#endif

#define	min(a, b)	(((a) < (b)) ? (a) : (b))
#define	max(a, b)	(((a) > (b)) ? (a) : (b))

#include	"os.h"

#define	lowbit(x)	((x) & (~(x) + 1))

/* byte swapping helpers */

/* byte swap a long literal */
#define lswapl(x) ((((x) & 0xff) << 24) |\
		   (((x) & 0xff00) << 8) |\
		   (((x) & 0xff0000) >> 8) |\
		   (((x) >> 24) & 0xff))

/* byte swap a short literal */
#define lswaps(x) ((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))


/* byte swap a long */
#define swapl(x, n) n = ((char *) (x))[0];\
		 ((char *) (x))[0] = ((char *) (x))[3];\
		 ((char *) (x))[3] = n;\
		 n = ((char *) (x))[1];\
		 ((char *) (x))[1] = ((char *) (x))[2];\
		 ((char *) (x))[2] = n;

/* byte swap a short */
#define swaps(x, n) n = ((char *) (x))[0];\
		 ((char *) (x))[0] = ((char *) (x))[1];\
		 ((char *) (x))[1] = n

/* copy long from src to dst byteswapping on the way */
#define cpswapl(src, dst) \
                 ((char *)&(dst))[0] = ((char *) &(src))[3];\
                 ((char *)&(dst))[1] = ((char *) &(src))[2];\
                 ((char *)&(dst))[2] = ((char *) &(src))[1];\
                 ((char *)&(dst))[3] = ((char *) &(src))[0];

/* copy short from src to dst byteswapping on the way */
#define cpswaps(src, dst)\
		 ((char *) &(dst))[0] = ((char *) &(src))[1];\
		 ((char *) &(dst))[1] = ((char *) &(src))[0];


#if 0
extern void NoopDDA(void);
extern char *NameForAtom(Atom atom);
#endif
extern void BitOrderInvert(unsigned char *buf, int nbytes);


#if !defined(UNIXCPP) || defined(ANSICPP)
#define fsCat(x,y) x##_##y
#else
#define fsCat(x,y) x/**/_/**/y
#endif

/* copy a xCharInfo into a XCharInfo */

#define fsPack_XCharInfo(structure, packet) \
    fsCat(packet,left) = (structure)->leftSideBearing; \
    fsCat(packet,right) = (structure)->rightSideBearing; \
    fsCat(packet,width) = (structure)->characterWidth; \
    fsCat(packet,ascent) = (structure)->ascent; \
    fsCat(packet,descent) = (structure)->descent; \
    fsCat(packet,attributes) = (structure)->attributes


/* copy a FontInfoRec into a XFontInfoHeader */

#define fsPack_XFontInfoHeader(structure, packet, clientversion) \
    (packet)->font_header_flags = ((structure)->allExist) ? FontInfoAllCharsExist : 0; \
    (packet)->font_header_draw_direction = ((structure)->drawDirection == LeftToRight) \
               ? LeftToRightDrawDirection : RightToLeftDrawDirection; \
 \
    if ((structure)->inkInside) \
	(packet)->font_header_flags |= FontInfoInkInside; \
 \
    if (clientversion > 1) { \
	(packet)->font_hdr_char_range_min_char_high = (structure)->firstRow; \
        (packet)->font_hdr_char_range_min_char_low = (structure)->firstCol; \
        (packet)->font_hdr_char_range_max_char_high = (structure)->lastRow; \
        (packet)->font_hdr_char_range_max_char_low = (structure)->lastCol; \
        (packet)->font_header_default_char_high = (structure)->defaultCh >> 8; \
        (packet)->font_header_default_char_low = (structure)->defaultCh & 0xff; \
    } else { \
	(packet)->font_hdr_char_range_min_char_high = (structure)->firstCol; \
	(packet)->font_hdr_char_range_min_char_low = (structure)->firstRow; \
	(packet)->font_hdr_char_range_max_char_high = (structure)->lastCol; \
	(packet)->font_hdr_char_range_max_char_low = (structure)->lastRow; \
	(packet)->font_header_default_char_high = (structure)->defaultCh & 0xff; \
	(packet)->font_header_default_char_low = (structure)->defaultCh >> 8; \
    } \
 \
    fsPack_XCharInfo(&(structure)->ink_minbounds, (packet)->font_header_min_bounds); \
    fsPack_XCharInfo(&(structure)->ink_maxbounds, (packet)->font_header_max_bounds); \
 \
    (packet)->font_header_font_ascent = (structure)->fontAscent; \
    (packet)->font_header_font_descent = (structure)->fontDescent


typedef struct {		/* when cloning, need old transport info */
    int trans_id;
    int fd;
    int portnum;
} OldListenRec;

/* os/connection.c */
extern	void	CreateSockets(int old_listen_count, OldListenRec *old_listen);

#endif				/* _MISC_H_ */
