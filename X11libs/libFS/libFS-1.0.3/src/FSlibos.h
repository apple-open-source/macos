/*
 * Copyright 1990 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software 
 * and its documentation for any purpose is hereby granted without fee, 
 * provided that the above copyright notice appear in all copies and 
 * that both that copyright notice and this permission notice appear 
 * in supporting documentation, and that the names of Network Computing 
 * Devices or Digital not be used in advertising or publicity pertaining 
 * to distribution of the software without specific, written prior 
 * permission. Network Computing Devices or Digital make no representations 
 * about the suitability of this software for any purpose.  It is provided 
 * "as is" without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES AND  DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES 
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, 
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS 
 * SOFTWARE.
 */

/*

Copyright 1987, 1994, 1998  The Open Group

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

*/

/*
 * FSlib networking & os include file
 */

#include <X11/Xfuncs.h>
#include <X11/Xosdefs.h>

#ifndef WIN32

#if defined(__SCO__) || defined(__UNIXWARE__)
#include <stdint.h>	/* For SIZE_MAX */
#endif

/*
 * makedepend screws up on #undef OPEN_MAX, so we define a new symbol
 */

#ifndef FS_OPEN_MAX

#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
#include <limits.h>
#else
#define _POSIX_SOURCE
#include <limits.h>
#undef _POSIX_SOURCE
#endif
#endif
#ifndef SIZE_MAX
# ifdef ULONG_MAX
#  define SIZE_MAX ULONG_MAX
# else 
#  define SIZE_MAX UINT_MAX
# endif
#endif
#ifndef OPEN_MAX
#ifdef SVR4
#define OPEN_MAX 256
#else
#include <sys/param.h>
#ifndef OPEN_MAX
#ifdef __OSF1__
#define OPEN_MAX 256
#else
#ifdef NOFILE
#define OPEN_MAX NOFILE
#else
#if !defined(__UNIXOS2__) && !defined(__QNX__)
#ifdef __GNU__
#define OPEN_MAX (sysconf(_SC_OPEN_MAX))
#else /* !__GNU__ */
#define OPEN_MAX NOFILES_MAX
#endif /* __GNU__ */
#else /* !__UNIXOS2__ && !__QNX__ */
#define OPEN_MAX 256
#endif /* __UNIXOS2__ */
#endif
#endif
#endif
#endif
#endif

#ifdef __GNU__
#define FS_OPEN_MAX 256
#else /*!__GNU__*/
#if OPEN_MAX > 256
#define FS_OPEN_MAX 256
#else
#define FS_OPEN_MAX OPEN_MAX
#endif
#endif /*__GNU__*/

#endif /* FS_OPEN_MAX */

/* Utek leaves kernel macros around in include files (bleah) */

#ifdef dirty
#undef dirty
#endif

#ifdef WORD64
#define NMSKBITS 64
#else
#define NMSKBITS 32
#endif

#define MSKCNT ((FS_OPEN_MAX + NMSKBITS - 1) / NMSKBITS)

#ifdef LONG64
typedef unsigned int FdSet[MSKCNT];
#else
typedef unsigned long FdSet[MSKCNT];
#endif

#if (MSKCNT==1)
#define BITMASK(i) (1 << (i))
#define MASKIDX(i) 0
#endif

#if (MSKCNT>1)
#define BITMASK(i) (1 << ((i) & (NMSKBITS - 1)))
#define MASKIDX(i) ((i) / NMSKBITS)
#endif

#define MASKWORD(buf, i) buf[MASKIDX(i)]
#define BITSET(buf, i) MASKWORD(buf, i) |= BITMASK(i)
#define BITCLEAR(buf, i) MASKWORD(buf, i) &= ~BITMASK(i)
#define GETBIT(buf, i) (MASKWORD(buf, i) & BITMASK(i))

#if (MSKCNT==1)
#define COPYBITS(src, dst) dst[0] = src[0]
#define CLEARBITS(buf) buf[0] = 0
#define MASKANDSETBITS(dst, b1, b2) dst[0] = (b1[0] & b2[0])
#define ORBITS(dst, b1, b2) dst[0] = (b1[0] | b2[0])
#define UNSETBITS(dst, b1) (dst[0] &= ~b1[0])
#define _FSANYSET(src) (src[0])
#endif

#if (MSKCNT==2)
#define COPYBITS(src, dst) { dst[0] = src[0]; dst[1] = src[1]; }
#define CLEARBITS(buf) { buf[0] = 0; buf[1] = 0; }
#define MASKANDSETBITS(dst, b1, b2)  {\
		      dst[0] = (b1[0] & b2[0]);\
		      dst[1] = (b1[1] & b2[1]); }
#define ORBITS(dst, b1, b2)  {\
		      dst[0] = (b1[0] | b2[0]);\
		      dst[1] = (b1[1] | b2[1]); }
#define UNSETBITS(dst, b1) {\
                      dst[0] &= ~b1[0]; \
                      dst[1] &= ~b1[1]; }
#define _FSANYSET(src) (src[0] || src[1])
#endif

#if (MSKCNT==3)
#define COPYBITS(src, dst) { dst[0] = src[0]; dst[1] = src[1]; \
			     dst[2] = src[2]; }
#define CLEARBITS(buf) { buf[0] = 0; buf[1] = 0; buf[2] = 0; }
#define MASKANDSETBITS(dst, b1, b2)  {\
		      dst[0] = (b1[0] & b2[0]);\
		      dst[1] = (b1[1] & b2[1]);\
		      dst[2] = (b1[2] & b2[2]); }
#define ORBITS(dst, b1, b2)  {\
		      dst[0] = (b1[0] | b2[0]);\
		      dst[1] = (b1[1] | b2[1]);\
		      dst[2] = (b1[2] | b2[2]); }
#define UNSETBITS(dst, b1) {\
                      dst[0] &= ~b1[0]; \
                      dst[1] &= ~b1[1]; \
                      dst[2] &= ~b1[2]; }
#define _FSANYSET(src) (src[0] || src[1] || src[2])
#endif

#if (MSKCNT==4)
#define COPYBITS(src, dst) dst[0] = src[0]; dst[1] = src[1]; \
			   dst[2] = src[2]; dst[3] = src[3]
#define CLEARBITS(buf) buf[0] = 0; buf[1] = 0; buf[2] = 0; buf[3] = 0
#define MASKANDSETBITS(dst, b1, b2)  \
                      dst[0] = (b1[0] & b2[0]);\
                      dst[1] = (b1[1] & b2[1]);\
                      dst[2] = (b1[2] & b2[2]);\
                      dst[3] = (b1[3] & b2[3])
#define ORBITS(dst, b1, b2)  \
                      dst[0] = (b1[0] | b2[0]);\
                      dst[1] = (b1[1] | b2[1]);\
                      dst[2] = (b1[2] | b2[2]);\
                      dst[3] = (b1[3] | b2[3])
#define UNSETBITS(dst, b1) \
                      dst[0] &= ~b1[0]; \
                      dst[1] &= ~b1[1]; \
                      dst[2] &= ~b1[2]; \
                      dst[3] &= ~b1[3]
#define _FSANYSET(src) (src[0] || src[1] || src[2] || src[3])
#endif

#if (MSKCNT>4)
#define COPYBITS(src, dst) memmove((caddr_t) dst, (caddr_t) src, sizeof(FdSet))
#define CLEARBITS(buf) bzero((caddr_t) buf, sizeof(FdSet))
#define MASKANDSETBITS(dst, b1, b2)  \
		      { int cri;			\
			for (cri=0; cri<MSKCNT; cri++)	\
		          dst[cri] = (b1[cri] & b2[cri]) }
#define ORBITS(dst, b1, b2)  \
		      { int cri;			\
		      for (cri=0; cri<MSKCNT; cri++)	\
		          dst[cri] = (b1[cri] | b2[cri]) }
#define UNSETBITS(dst, b1) \
		      { int cri;			\
		      for (cri=0; cri<MSKCNT; cri++)	\
		          dst[cri] &= ~b1[cri];  }
#if (MSKCNT==8)
#define _FSANYSET(src) (src[0] || src[1] || src[2] || src[3] || \
			src[4] || src[5] || src[6] || src[7])
#endif
/*
 * If MSKCNT>4 and not 8, then _FSANYSET is a routine defined in FSlibInt.c.
 *
 * #define _FSANYSET(src) (src[0] || src[1] || src[2] || src[3] || src[4] ...)
 */
#endif


#else

#include <X11/Xwinsock.h>
#include <X11/Xw32defs.h>

typedef fd_set FdSet;

#define CLEARBITS(set) FD_ZERO(&set)
#define BITSET(set,s) FD_SET(s,&set)
#define _FSANYSET(set) set.fd_count

#endif

#include <X11/Xtrans/Xtrans.h>
#include <stdlib.h>
#include <string.h>

/*
 * The following definitions can be used for locking requests in multi-threaded
 * address spaces.
 */
#define LockDisplay(dis)
#define LockMutex(mutex)
#define UnlockMutex(mutex)
#define UnlockDisplay(dis)
#define FSfree(ptr) free((ptr))


/*
 * Note that some machines do not return a valid pointer for malloc(0), in
 * which case we provide an alternate under the control of the
 * define MALLOC_0_RETURNS_NULL.  This is necessary because some
 * FSlib code expects malloc(0) to return a valid pointer to storage.
 */

#ifdef MALLOC_0_RETURNS_NULL
#define FSmalloc(size) malloc(((size) > 0 ? (size) : 1))
#define FSrealloc(ptr, size) realloc((ptr), ((size) > 0 ? (size) : 1))
#define FScalloc(nelem, elsize) calloc(((nelem) > 0 ? (nelem) : 1), (elsize))

#else

#define FSmalloc(size) malloc((size))
#define FSrealloc(ptr, size) realloc((ptr), (size))
#define FScalloc(nelem, elsize) calloc((nelem), (elsize))
#endif

#define SearchString(string, char) index((string), (char))
