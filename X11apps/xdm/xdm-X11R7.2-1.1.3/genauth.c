/* $XdotOrg: xc/programs/xdm/genauth.c,v 1.1.4.5 2003/12/20 00:28:31 kaleb Exp $ */
/* $Xorg: genauth.c,v 1.5 2001/02/09 02:05:40 xorgcvs Exp $ */
/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xdm/genauth.c,v 3.23 2003/12/20 19:45:12 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 */

# include   <X11/Xauth.h>
# include   <X11/Xos.h>

# include   "dm.h"
# include   "dm_auth.h"
# include   "dm_error.h"

#include <errno.h>

#include <time.h>
#define Time_t time_t

#ifdef HASXDMAUTH
static unsigned char	key[8];
#endif

#ifdef DEV_RANDOM
extern char	*randomDevice;
#endif

#ifdef HASXDMAUTH

typedef unsigned char auth_cblock[8];	/* block size */

typedef struct auth_ks_struct { auth_cblock _; } auth_wrapper_schedule[16];

extern int _XdmcpAuthSetup(unsigned char *, auth_wrapper_schedule);
extern int _XdmcpAuthDoIt(unsigned char *, unsigned char *, 
    auth_wrapper_schedule, int);
extern void _XdmcpWrapperToOddParity(unsigned char *, unsigned char *);

static void
longtochars (long l, unsigned char *c)
{
    c[0] = (l >> 24) & 0xff;
    c[1] = (l >> 16) & 0xff;
    c[2] = (l >> 8) & 0xff;
    c[3] = l & 0xff;
}

#endif

#ifdef POLL_DEV_RANDOM
#include <poll.h>
static int
pollRandomDevice (int fd)
{
    struct pollfd fds;

    fds.fd = fd;
    fds.events = POLLIN | POLLRDNORM;
    /* Wait up to 5 seconds for entropy to accumulate */
    return poll(&fds, 1, 5000);
}
#else
#define pollRandomDevice(fd) 1
#endif

#if !defined(ARC4_RANDOM)

/* ####################################################################### */

/*
 * Copyright Theodore Ts'o, 1994, 1995, 1996, 1997, 1998, 1999.  All
 * rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

static CARD32 epool[32], erotate, eadd_ptr;

static void
add_entropy (const CARD32 *in, int nwords)
{
	static const CARD32 twist_table[8] = {
		         0, 0x3b6e20c8, 0x76dc4190, 0x4db26158,
		0xedb88320, 0xd6d6a3e8, 0x9b64c2b0, 0xa00ae278 };
	CARD32 i, w;
	int new_rotate;

	while (nwords--) {
		w = *in++;
		w = (w<<erotate | w>>(32-erotate)) & 0xffffffff;
		i = eadd_ptr = (eadd_ptr - 1) & 31;
		new_rotate = erotate + 14;
		if (i)
			new_rotate = erotate + 7;
		erotate = new_rotate & 31;
		w ^= epool[(i + 26) & 31];
		w ^= epool[(i + 20) & 31];
		w ^= epool[(i + 14) & 31];
		w ^= epool[(i + 7) & 31];
		w ^= epool[(i + 1) & 31];
		w ^= epool[i];
		epool[i] = (w >> 3) ^ twist_table[w & 7];
	}
}

/* ####################################################################### */

/*
 * This code implements something close to the MD5 message-digest
 * algorithm. This code is based on code written by Colin Plumb
 * in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 */

/* The four core functions - F1 is optimized somewhat */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1 (z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define pmd5_step(f, w, x, y, z, data, s) \
	( w += (f(x, y, z) + data) & 0xffffffff,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.
 */
static void 
pmd5_hash (CARD32 *out, const CARD32 in[16])
{
    CARD32 a, b, c, d;

    a = out[0];
    b = out[1];
    c = out[2];
    d = out[3];

    pmd5_step(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    pmd5_step(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    pmd5_step(F1, c, d, a, b, in[2] + 0x242070db, 17);
    pmd5_step(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    pmd5_step(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    pmd5_step(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    pmd5_step(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    pmd5_step(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    pmd5_step(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    pmd5_step(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    pmd5_step(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    pmd5_step(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    pmd5_step(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    pmd5_step(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    pmd5_step(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    pmd5_step(F1, b, c, d, a, in[15] + 0x49b40821, 22);
		
    pmd5_step(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    pmd5_step(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    pmd5_step(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    pmd5_step(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    pmd5_step(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    pmd5_step(F2, d, a, b, c, in[10] + 0x02441453, 9);
    pmd5_step(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    pmd5_step(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    pmd5_step(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    pmd5_step(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    pmd5_step(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    pmd5_step(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    pmd5_step(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    pmd5_step(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    pmd5_step(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    pmd5_step(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);
		
    pmd5_step(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    pmd5_step(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    pmd5_step(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    pmd5_step(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    pmd5_step(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    pmd5_step(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    pmd5_step(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    pmd5_step(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    pmd5_step(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    pmd5_step(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    pmd5_step(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    pmd5_step(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    pmd5_step(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    pmd5_step(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    pmd5_step(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    pmd5_step(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);
		
    pmd5_step(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    pmd5_step(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    pmd5_step(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    pmd5_step(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    pmd5_step(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    pmd5_step(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    pmd5_step(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    pmd5_step(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    pmd5_step(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    pmd5_step(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    pmd5_step(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    pmd5_step(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    pmd5_step(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    pmd5_step(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    pmd5_step(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    pmd5_step(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    out[0] += a;
    out[1] += b;
    out[2] += c;
    out[3] += d;
}

/* ####################################################################### */


static int
sumFile (const char *name, int len, int whence, off_t offset)
{
    int fd, cnt, readlen = 0;
    unsigned char buf[0x1000];

    if ((fd = open (name, O_RDONLY)) < 0) {
	Debug("cannot open entropy source \"%s\", errno=%d\n", name, errno);
	return -1;
    }
    lseek (fd, offset, whence);
    while (readlen < len) {
	if (!(cnt = read (fd, buf, sizeof (buf))))
	    break;
	if (cnt < 0) {
	    close (fd);
	    Debug("cannot read entropy source \"%s\", errno=%d\n", 
		  name, errno);
	    return -1;
	}
	readlen += cnt;
	add_entropy((CARD32 *)buf, (cnt + 3) / 4);
    }
    close (fd);
    Debug("read %d bytes from entropy source \"%s\"\n", readlen, name);
    return readlen;
}

void
AddTimerEntropy (void)
{
    struct timeval now;
    X_GETTIMEOFDAY (&now);
    add_entropy((CARD32 *)&now, sizeof(now)/sizeof(CARD32));
}

#define BSIZ 0x10000

void
AddOtherEntropy (void)
{
    AddTimerEntropy();
    /* XXX -- setup-specific ... use some common ones */
    sumFile ("/var/log/messages", 0x1000, SEEK_END, -0x1000);
    sumFile ("/var/log/syslog", 0x1000, SEEK_END, -0x1000);
    sumFile ("/var/log/debug", 0x1000, SEEK_END, -0x1000);
    sumFile ("/var/log/kern.log", 0x1000, SEEK_END, -0x1000);
    sumFile ("/var/log/daemon.log", 0x1000, SEEK_END, -0x1000);
}

void
AddPreGetEntropy (void)
{
    static off_t offset;
    off_t readlen;

    AddTimerEntropy();
    if ((readlen = sumFile (randomFile, BSIZ, SEEK_SET, offset)) == BSIZ) {
	offset += readlen;
#ifdef FRAGILE_DEV_MEM
	if (!strcmp (randomFile, "/dev/mem")) {
	    if (offset == 0xa0000) /* skip 640kB-1MB ROM mappings */
		offset = 0x100000;
	    else if (offset == 0xf00000) /* skip 15-16MB memory hole */
		offset = 0x1000000;
	}
#endif
	return;
    } else if (readlen >= 0 && offset) {
	if ((offset = sumFile (randomFile, BSIZ, SEEK_SET, 0)) == BSIZ)
	    return;
    }
    LogError("Cannot read randomFile \"%s\"; X cookies may be easily guessable\n", randomFile);
}
#endif /* !ARC4_RANDOM && !DEV_RANDOM */


#ifdef HASXDMAUTH
static void
InitXdmcpWrapper (void)
{
    CARD32 sum[4];

#ifdef	ARC4_RANDOM
    sum[0] = arc4random();
    sum[1] = arc4random();
    *(u_char *)sum = 0;

    _XdmcpWrapperToOddParity((unsigned char *)sum, key);
#else
    unsigned char   tmpkey[8];

#ifdef DEV_RANDOM
    int fd;
    
    if ((fd = open(randomDevice, O_RDONLY)) >= 0) {
	if (pollRandomDevice(fd) && read(fd, tmpkey, 8) == 8) {
	    tmpkey[0] = 0;
	    _XdmcpWrapperToOddParity(tmpkey, key);
	    close(fd);
	    return;	
	} else {
	    close(fd);
	}
    } else {
	LogError("Cannot open randomDevice \"%s\", errno = %d\n", 
	  randomDevice, errno);
    }
#endif    
    /*  Try some pseudo-random number genrator daemon next */
    if (prngdSocket != NULL || prngdPort != 0) {
	    if (get_prngd_bytes(tmpkey, sizeof(tmpkey), prngdPort, 
		    prngdSocket) == 0) {
		    tmpkey[0] = 0;
		    _XdmcpWrapperToOddParity(tmpkey, key);
		    return;
	    }
    }
    /* Fall back if no other source of random number was found */
    AddPreGetEntropy();
    pmd5_hash (sum, epool);
    add_entropy (sum, 1);
    pmd5_hash (sum, epool + 16);
    add_entropy (sum + 2, 1);

    longtochars (sum[0], tmpkey+0);
    longtochars (sum[1], tmpkey+4);
    tmpkey[0] = 0;
    _XdmcpWrapperToOddParity (tmpkey, key);
#endif
}

#endif


int
GenerateAuthData (char *auth, int len)
{
#ifdef HASXDMAUTH
    int		    i, bit;
    auth_wrapper_schedule    schedule;
    unsigned char	    data[8];
    static int	    xdmcpAuthInited;
    long	    ldata[2];

#ifdef ITIMER_REAL
    struct timeval  now;

    X_GETTIMEOFDAY (&now);
    ldata[0] = now.tv_usec;
    ldata[1] = now.tv_sec;
#else
    ldata[0] = time ((long *) 0);
    ldata[1] = getpid ();
#endif
    
    longtochars (ldata[0], data+0);
    longtochars (ldata[1], data+4);
    if (!xdmcpAuthInited)
    {
	InitXdmcpWrapper ();
	xdmcpAuthInited = 1;
    }
    _XdmcpAuthSetup (key, schedule);
    for (i = 0; i < len; i++) {
	auth[i] = 0;
	for (bit = 1; bit < 256; bit <<= 1) {
	    _XdmcpAuthDoIt (data, data, schedule, 1);
	    if ((data[0] + data[1]) & 0x4)
		auth[i] |= bit;
	}
    }
    return 1;
#else /* !XDMAUTH */
#ifdef ARC4_RANDOM
    CARD32 *rnd = (CARD32 *)auth;
    int i;

    for (i = 0; i < len; i += 4)
	rnd[i / 4] = arc4random();
    return 1;
#else /* !ARC4_RANDOM */
    CARD32 tmp[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
#ifdef DEV_RANDOM
    int fd;
    
    if ((fd = open(randomDevice, O_RDONLY)) >= 0) {
	if (pollRandomDevice(fd) && 
	    read(fd, auth, len) == len) {
	    close(fd);
	    return 1;
	}
	close(fd);
	LogError("Cannot read randomDevice \"%s\", errno=%d\n",
		 randomDevice, errno);
    } else 
	LogError("Cannot open randomDevice \"%s\", errno = %d\n", 
		 randomDevice, errno);
#endif /* DEV_RANDOM */
    /*  Try some pseudo-random number genrator daemon next */
    if (prngdSocket != NULL || prngdPort != 0) {
	    if (get_prngd_bytes(auth, len, prngdPort, prngdSocket) == 0) {
		    return 1;
	    }
    }
    /* Fallback if not able to get from /dev/random */
    AddPreGetEntropy();
    pmd5_hash (tmp, epool);
    add_entropy (tmp, 1);
    pmd5_hash (tmp, epool + 16);
    add_entropy (tmp + 2, 1);
    memcpy (auth, tmp, len);
    return 1;
#endif /* !ARC4_RANDOM */
#endif /* !HASXDMAUTH */
}
