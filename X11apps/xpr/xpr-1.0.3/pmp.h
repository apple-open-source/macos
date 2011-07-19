/*
 *	$XConsortium: pmp.h,v 1.5 91/02/19 22:13:33 converse Exp $
 */

/* Written by Jose' J. Capo' */
/* (jjc@comet.lcs.mit.edu), June 1987 */

/* WARNING!!!:  Include this header after the standard headers (like */
 /* <stdio.h>) of  the following might cause another header to */
 /* redefine BUFSIZ */
#ifndef BUFSIZ
#define BUFSIZ	1024
#endif

#define MAX_FRAME_LEN	512
#define MAX_VECTOR_LEN	65535

#define DBG(cond)	if (dbg cond)
#define DEBUG(cond)	if (debug cond)
#define min(a,b)	((a) < (b)? (a) : (b))

#define pel2inch(pels)		((float) ((pels) / PPI))
#define ppmask(rounded,thickness)	(0x20 * (rounded) + (thickness))


#define min(a,b)	((a) < (b)? (a) : (b))
#define lo(x)	((x) & 0xFF)
#define hi(x)	(((x) & 0xFF00) >>8)
#define hi2(x)	(((x) & 0xFF0000) >>16)
#define p_wput(w, f)	{\
			   (void) putc(hi(w), (f)); \
			   (void) putc(lo(w), (f));\
		       }
#define PMP(f, len)	{ fprintf(f, "\033[C"); \
			    p_putlh(f, len);\
		      }
#define p_putlh(f, w)	{\
			   (void) putc(lo(w), (f));\
			   (void) putc(hi(w), (f)); \
		       }

/* hi-lo 2-byte integer */
/* int hl2int(unsigned char *) */
#define hl2int(hl2)	((int) (0x100 * *(hl2) + *((hl2)+1)))

/* putlh2(FILE *, int) */
#define puthl2(i, f) { (void) putc(hi((i)), (f));\
		       (void) putc(lo((i)), (f));\
		     }

/* hi-lo 3-byte integer */
/* long hl3long(unsigned char *) */
#define hl3long(hl3)	((long) (0x10000 * *(hl3) + \
				 0x100 * *((hl3)+1) + *((hl3)+2)))

/* puthl3(FILE *, long) */
#define puthl3(l, f) { (void) putc(hi2((l)), (f));\
		       (void) putc(hi((l)), (f));\
		       (void) putc(lo((l)), (f));\
		     }

/* int int2sgn(int) */
#define int2sgn(i)	(((i) > 0x8000 - 1)? (i)- 0x10000: (i))

/* int long3sgn(long) */
#define long3sgn(l)	(((l) > 0x800000 - 1)? (l) - 0x1000000 : (l))
