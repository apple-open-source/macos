/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 	File:		systemEntropy.c
	
 	Contains:	System entropy collector, using 
				sysctl(CTL_KERN:KERN_KDEBUG) trace info

 	Copyright:	(C) 2000 by Apple Computer, Inc., all rights reserved

 	Written by:	Doug Mitchell <dmitch@apple.com>	
*/

#include "systemEntropy.h"
#include "debug.h"

/* support for sysctl */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
//#include <libc.h>
#include <sys/kdebug.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <unistd.h>

/* this should eventually come from private system headers */
#include "kdebug_private.h"

/* time to gather trace info */
#define MS_TO_SLEEP		100

static int set_remove();
static int set_init();
static int set_enable(int val);
static int set_numbufs(int nbufs);

/* start collecting system entropy */
int systemEntropyBegin(UInt32 bufSize)
{
	int rtn;
	
	/* start from clean slate  */
	set_remove();
	
	/* 
	 * This will result in a ENOENT error if we're not root.
	 * That's OK, the kernel will use its default of an 8K 
	 * buffer in that case. 
	 */
	set_numbufs(bufSize);	
	if(rtn = set_init()) {
		return rtn;
	}
	if(rtn = set_enable(1)) {
		return rtn;
	}
	return 0;
}


int systemEntropyCollect(
	UInt8 *buf,
	UInt32 bufSize,
	UInt32 *numBytes,		// RETURNED - number of bytes obtained
	UInt32 *bitsOfEntropy)	// RETURNED - est. amount of entropy
{
	int 		rtn = 0;
	size_t 		mallocdSize;
	UInt8		*cp = buf;
	kd_buf 		*kd = NULL;
	int			i;
	int 		mib[6];
	size_t		numEntries;
	
	*numBytes = 0;
	*bitsOfEntropy = 0;
	

	/*
	 * We use one byte from each entry, which is a kd_buf.
	 * Thus, malloc bufSize kd_bufs. 
	 * FIXME : this should use a secure nonswapping malloc. 
	 */
	mallocdSize = bufSize * sizeof(kd_buf);
	kd = (kd_buf *)malloc(mallocdSize);
	if(kd == NULL) {
		rtn = ENOMEM;
		goto errOut;
	}
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADTR;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;  /* no flags */
	
	/*
	 * Snag the trace buffer, up to caller's limit. 
	 * On call to sysctl, numEntries is byte count, on return,
	 * it's buffer count. 
	 */ 
	numEntries = mallocdSize;
	if (sysctl(mib, 3, kd, &numEntries, NULL, 0) < 0) {
		/* ENOMEM means we didn't have room for everything in
		 * the kernel trace buffer, which is fine */
		int err = errno;
		if(err != ENOMEM) {
			errorLog1("sysctl-KERN_KDREADTR: %d\n", err);
			rtn = err;
			goto errOut;
		}
	}
	if(numEntries == 0) {
		rtn = ENOENT;
		goto errOut;
	}
	
	/* 
	 * First entropy byte is the low byte of the first entry's
	 * timestamp. Subsequent bytes are the deltas between successive
	 * entries' timestamps.
	 */	
	*cp++ = (UInt8)kd[0].timestamp.tv_nsec;
	for (i=1; i<numEntries; i++) {
		*cp++ = kd[i].timestamp.tv_nsec - kd[i-1].timestamp.tv_nsec;
	}
	
	*numBytes = numEntries;
	*bitsOfEntropy = numEntries * 4;		// half random?
	
	/* and finally, turn off tracing */
errOut:
	set_enable(0);
	set_remove();		// ignore errors 
	return rtn;
}

/*
 * The remainder of this file is based on code provided by Joe Sokol.
 * All functions return a UNIX errno, zero on success. 
 */

static int set_remove()
{
	int mib[6];
	size_t needed;
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREMOVE;  /* protocol */
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;  /* no flags */
	
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0) {
		int err = errno;
		errorLog1("sysctl-KERN_KDREMOVE: %d\n", err);
		return err;
	}
	return 0;
}

static int set_init()
{       
	kd_regtype kr;
	int mib[6];
	size_t needed;
	
	kr.type = KDBG_RANGETYPE;
	kr.value1 = 0;
	kr.value2 = -1;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;  /* no flags */
	
	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0) {
		int err = errno;
		errorLog1("sysctl-KERN_KDSETREG: %d\n", err);
		return err;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;  /* no flags */
	
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0) {
		int err = errno;
		errorLog1("sysctl-KERN_KDSETUP: %d\n", err);
		return err;
	}
	return 0;
}

static int set_enable(int val)
{
	int mib[6];
	size_t needed;
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDENABLE;  /* protocol */
	mib[3] = val;
	mib[4] = 0;
	mib[5] = 0;          /* no flags */
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0) {
		int err = errno;
		errorLog1("sysctl-KERN_KDENABLE: %d\n", err);
		return err;
	}
	return 0;
}

static int set_numbufs(int nbufs) 
{
	int mib[6];
	size_t needed;

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETBUF;
	mib[3] = nbufs;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0) {
		int err = errno;
		errorLog2("ERROR: sysctl-KERN_KDSETBUF(%d): %s\n", 
			nbufs, strerror(err));
		return err;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0) {
		int err = errno;
		errorLog1("ERROR: sysctl-KERN_KDSETUP: %s\n", 
			strerror(err));
		return err;
	}
	return 0;
}
