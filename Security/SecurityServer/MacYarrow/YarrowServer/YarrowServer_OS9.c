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
	File:		YarrowServer_OS9.c

	Contains:	Yarrow Server, OS 9 version.

	Written by:	Doug Mitchell

	Copyright: (c) 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		02/29/00	dpm		Created.
 
*/

#include <yarrowUtils.h>
#include "YarrowServer_OS9.h"
#include "entropyFile.h"
#include <debug.h>
#include <yarrow.h>
#include <Errors.h>
#include <Timer.h>		/* Microseconds() */
#include <LowMem.h>		/* LMGetTicks() */

/* the single system-wide yarrow PRNG object */
static PrngRef	prng = NULL;

/*
 * We collect system entropy every ENTROPY_COLLECT_INTERVAL seconds.
 */
#define ENTROPY_COLLECT_INTERVAL	(10 * 60)

/* 
 * When collecting system entropy, try for this many bytes. 
 */
#define SYSTEM_ENTROPY_SIZE			20

/*
 * Maintain an entropy file of this size.
 */
#define ENTROPY_FILE_SIZE			20

/*
 * Microseconds to crunch in prngAllowReseed() 
 */
#define RESEED_TICKS				100


#pragma mark -
#pragma mark * * * Private Functions * * * 

#ifdef __cplusplus
extern "C" {
#endif
OSErr _init(void *initBlk);
void _fini(void);
int main();
#ifdef __cplusplus
}
#endif

static void
systemEntropy(
	UInt8 *buf,
	UInt32 bufSize,
	UInt32 *numBytes,		// RETURNED - number of bytes obtained
	UInt32 *bitsOfEntropy);	// RETURNED - est. amount of entropy


/*
 * Called once on initial library load. 
 */
OSErr 
_init(void *initBlk)
{
	prng_error_status	prtn;
	UInt8				entropyFileData[ENTROPY_FILE_SIZE];
	UInt8				sysEntropyData[SYSTEM_ENTROPY_SIZE];
	UInt32				actLen;
	OSErr				ortn;
	UInt32				entropyBits;
	
	/* set up prng and its lock */
	prtn = prngInitialize(&prng);
	if(prtn) {
		errorLog1("_init: prngInitialize returned %s\n", perrorString(prtn));
		return perrorToOSErr(prtn);
	}

	/* TBD - the mutex */
	
	/*
	 * read entropy file, add contents to system entropy pool.
	 * It's not an error if there is no entropy file; this
	 * should only happen the first time this server runs on a given
	 * system.
	 */
	ortn = readEntropyFile(entropyFileData, 
		ENTROPY_FILE_SIZE,
		&actLen);
	if((ortn == noErr) && (actLen > 0)) {
		prtn = prngInput(prng, 
			entropyFileData,
			actLen,
			ENTROPY_FILE_SOURCE,
			actLen * 8);		// assume total entropy here 
		if(prtn) {
			errorLog1("_init: prngInput returned %s\n", 
				perrorString(prtn));
			return perrorToOSErr(prtn);
		}
	}
	trashMemory(entropyFileData, actLen);
	
	/*
	 * collect system entropy, add to system entropy pool
	 */
	systemEntropy(sysEntropyData,
		SYSTEM_ENTROPY_SIZE,
		&actLen,
		&entropyBits);
	if(actLen > 0) {
		prtn = prngInput(prng, 
			entropyFileData,
			actLen,
			SYSTEM_SOURCE,
			entropyBits);	
		if(prtn) {
			errorLog1("_init: prngInput returned %s\n", 
				perrorString(prtn));
			return perrorToOSErr(prtn);
		}
	}
	trashMemory(sysEntropyData, actLen);
	
	/*
	 * force reseed
	 */
	prtn = prngForceReseed(prng, RESEED_TICKS);
	if(prtn) {
		errorLog1("_init: prngForceReseed returned %s\n", 
			perrorString(prtn));
		return perrorToOSErr(prtn);
	}
	
	/*
	 * get 20 bytes of random data, write to entropy file
	 */
	prtn = prngOutput(prng, entropyFileData, ENTROPY_FILE_SIZE);
	if(prtn) {
		errorLog1("_init: prngOutput returned %s\n", 
			perrorString(prtn));
		return perrorToOSErr(prtn);
	}
	ortn = writeEntropyFile(entropyFileData, ENTROPY_FILE_SIZE, false);
	if(ortn) {
		return ortn;
	}
	/* FIXME -  schedule an entropyCollector() call; */
	
	return noErr;
}

void 
_fini(void)
{
	/* free prng and lock */
	if(prng != NULL) {
		prngDestroy(prng);
		prng = NULL;
	}
}

/* 
 * FIXME - RuntimePPC.dll is referring to this somehow...
 *
int main()
{
	errorLog0("YarrowServer main() called\n");
}
*/

/* 
 * Lock/unlock prngMutex - I guess these are not technically necessary 
 * on OS 9
 */
static void
prngLock()
{

}

static void
prngUnlock()
{

}

/*
 * Get some system entropy. On OS 9 this is pretty lame. 
 */
static void
systemEntropy(
	UInt8 *buf,
	UInt32 bufSize,
	UInt32 *numBytes,		// RETURNED - number of bytes obtained
	UInt32 *bitsOfEntropy)	// RETURNED - est. amount of entropy
{
	UnsignedWide curTime;		/* low 16 bits are pretty good, use 32 */
	unsigned ticks = 0;				/* low 8 bits are OK, use 16 bits */
	UInt8 pool[6];
	UInt8 *pp = pool;
	
	Microseconds(&curTime);		/* low 16 bits are pretty good */
	//ticks = LMGetTicks();
	*pp++ = curTime.lo & 0xff;
	*pp++ = curTime.lo >> 8;
	*pp++ = curTime.lo >> 16;
	*pp++ = curTime.lo >> 24;
	*pp++ = ticks & 0xff;
	*pp   = ticks >> 8;
	if(bufSize > 6) {
		bufSize = 6;
	}
	BlockMove(pool, buf, bufSize);
	*numBytes = bufSize;
	*bitsOfEntropy = 3 * 8;		/* three bytes worth */
}

/*
 * Entropy collector - called every ENTROPY_COLLECT_INTERVAL seconds. 
 */
static void 
entropyCollector()
{
	/* grab some system entropy
	 * add to pool
	 * allow reseed
	 * if enough time has elapsed {
	 *		update seed file
	 * }
	 * schedule another call
	 */
}

#pragma mark -
#pragma mark * * * Public Functions * * * 

/* 
 * Add some entropy to the pool. The only "known" failure here is a 
 * result of a failure of this library'e early init.
 */
OSErr yarrowAddEntropy(
	UInt8	*bytes,
	UInt32	numBytes,
	UInt32	bitsOfEntropy)
{
	UInt8 				sysEntropy[SYSTEM_ENTROPY_SIZE];
	UInt32 				numSysBytes;
	UInt32 				numSysEntropyBits;
	prng_error_status 	prtn;
	OSErr 				ortn = noErr;
	
	if(prng == NULL) {
		return notOpenErr;
	}
	prngLock();
	
	/* add client entropy */
	prtn = prngInput(prng, bytes, numBytes, CLIENT_SOURCE, bitsOfEntropy);
	if(prtn) {
		errorLog1("prngInput returned %s\n", perrorString(prtn));
		ortn = ioErr;
		goto done;
	}
	
	/* and some system entropy too - this prevents client from overwhelming
	 * the entropy pool with its own (untrusted) data */
	systemEntropy(sysEntropy, SYSTEM_ENTROPY_SIZE, &numSysBytes, 
		&numSysEntropyBits);
	prtn = prngInput(prng, sysEntropy, numSysBytes, SYSTEM_SOURCE, 
		numSysEntropyBits);
	if(prtn) {
		errorLog1("prngInput returned %s\n", perrorString(prtn));
		ortn = ioErr;
		goto done;
	}
	prngAllowReseed(prng, RESEED_TICKS);
	
done:
	prngUnlock();
	return ortn;
}

/* 
 * Get some random data. Caller mallocs the memory.
 */
OSErr yarrowGetRandomBytes(
	UInt8	*bytes,	
	UInt32	numBytes)
{
	if(prng == NULL) {
		return notOpenErr;
	}
	prngLock();
	prngOutput(prng, bytes, numBytes);
	prngUnlock();
	return noErr;
}

