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
/* $XFree86: xc/programs/xdm/genauth.c,v 3.15 2002/10/06 18:12:29 herrb Exp $ */

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

static unsigned char	key[8];

#ifdef HASXDMAUTH

typedef unsigned char auth_cblock[8];	/* block size */

typedef struct auth_ks_struct { auth_cblock _; } auth_wrapper_schedule[16];

extern void _XdmcpWrapperToOddParity();

static void
longtochars (long l, unsigned char *c)
{
    c[0] = (l >> 24) & 0xff;
    c[1] = (l >> 16) & 0xff;
    c[2] = (l >> 8) & 0xff;
    c[3] = l & 0xff;
}

#endif

# define FILE_LIMIT	1024	/* no more than this many buffers */

#if !defined(ARC4_RANDOM) && !defined(DEV_RANDOM)
static int
sumFile (char *name, long sum[2])
{
    long    buf[1024*2];
    int	    cnt;
    int	    fd;
    int	    loops;
    int	    reads;
    int	    i;
    int     ret_status = 0;

    fd = open (name, O_RDONLY);
    if (fd < 0) {
	LogError("Cannot open randomFile \"%s\", errno = %d\n", name, errno);
	return 0;
    }
#ifdef FRAGILE_DEV_MEM
    if (strcmp(name, "/dev/mem") == 0) lseek (fd, (off_t) 0x100000, SEEK_SET);
#endif
    reads = FILE_LIMIT;
    sum[0] = 0;
    sum[1] = 0;
    while ((cnt = read (fd, (char *) buf, sizeof (buf))) > 0 && --reads > 0) {
	loops = cnt / (2 * sizeof (long));
	for (i = 0; i < loops; i+= 2) {
	    sum[0] += buf[i];
	    sum[1] += buf[i+1];
	    ret_status = 1;
	}
    }
    if (cnt < 0)
	LogError("Cannot read randomFile \"%s\", errno = %d\n", name, errno);
    close (fd);
    return ret_status;
}
#endif

#ifdef HASXDMAUTH
static void
InitXdmcpWrapper (void)
{

#ifdef	ARC4_RANDOM
    u_int32_t sum[2];

    sum[0] = arc4random();
    sum[1] = arc4random();
    *(u_char *)sum = 0;

    _XdmcpWrapperToOddParity(sum, key);

#elif defined(DEV_RANDOM)
    int fd;
    unsigned char   tmpkey[8];
    
    if ((fd = open(DEV_RANDOM, O_RDONLY)) >= 0) {
	if (read(fd, tmpkey, 8) == 8) {
	    tmpkey[0] = 0;
	    _XdmcpWrapperToOddParity(tmpkey, key);
	    close(fd);
	    return;	
	} else {
	    close(fd);
	}
    }
#else    
    long	    sum[2];
    unsigned char   tmpkey[8];

    if (!sumFile (randomFile, sum)) {
	sum[0] = time ((Time_t *) 0);
	sum[1] = time ((Time_t *) 0);
    }
    longtochars (sum[0], tmpkey+0);
    longtochars (sum[1], tmpkey+4);
    tmpkey[0] = 0;
    _XdmcpWrapperToOddParity (tmpkey, key);
#endif
}

#endif

#ifndef HASXDMAUTH
/* A random number generator that is more unpredictable
   than that shipped with some systems.
   This code is taken from the C standard. */

static unsigned long int next = 1;

static int
xdm_rand(void)
{
    next = next * 1103515245 + 12345;
    return (unsigned int)(next/65536) % 32768;
}

static void
xdm_srand(unsigned int seed)
{
    next = seed;
}
#endif /* no HASXDMAUTH */

void
GenerateAuthData (char *auth, int len)
{
    long	    ldata[2];

#ifdef ITIMER_REAL
    {
	struct timeval  now;

	X_GETTIMEOFDAY (&now);
	ldata[0] = now.tv_usec;
	ldata[1] = now.tv_sec;
    }
#else
    {
#ifndef __UNIXOS2__
	long    time ();
#endif

	ldata[0] = time ((long *) 0);
	ldata[1] = getpid ();
    }
#endif
#ifdef HASXDMAUTH
    {
    	int		    bit;
    	int		    i;
	auth_wrapper_schedule    schedule;
	unsigned char	    data[8];
	static int	    xdmcpAuthInited;
    
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
    }
#else
    {
    	int	    seed;
    	int	    value;
    	int	    i;
	static long localkey[2] = {0,0};
    
	if ( (localkey[0] == 0) && (localkey[1] == 0) ) {
#ifdef ARC4_RANDOM
	    localkey[0] = arc4random();
	    localkey[1] = arc4random();
#elif defined(DEV_RANDOM)
	    int fd;
    
	    if ((fd = open(DEV_RANDOM, O_RDONLY)) >= 0) {
		if (read(fd, (char *)localkey, 8) != 8) {
		    localkey[0] = 1;
		}
		close(fd);
	    } else {
		localkey[0] = 1;
	    }
#else 
    	    if (!sumFile (randomFile, localkey)) {
		localkey[0] = 1; /* To keep from continually calling sumFile() */
    	    }
#endif
	}

    	seed = (ldata[0]+localkey[0]) + ((ldata[1]+localkey[1]) << 16);
    	xdm_srand (seed);
    	for (i = 0; i < len; i++)
    	{
	    value = xdm_rand ();
	    auth[i] = (value & 0xff00) >> 8;
    	}
	value = len;
	if (value > sizeof (key))
	    value = sizeof (key);
    	memmove( (char *) key, auth, value);
    }
#endif
}
