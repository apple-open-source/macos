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
	File:		MacYarrow_OSX.cpp

	Contains:	Yarrow RNG, OS X version.

	Written by:	Doug Mitchell

	Copyright: (c) 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		02/29/00	dpm		Created.
 
*/

#include "MacYarrow_OSX.h"
#include "entropyFile.h"
#include "systemEntropy.h"
#include <debug.h>
#include <Security/debugging.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

/* moved to Carbon.framework, FIXME */
// #include <CoreServices/../Frameworks/CarbonCore.framework/Headers/Power.h>	/* HardDiskPowered() */

static int HardDiskPowered() { return 1; }
/* end fixme */


#define QUICK_TEST		0

#if		QUICK_TEST

/*
 * We collect system entropy every SYSTEM_ENTROPY_COLLECT_INTERVAL milliseconds.
 */
#define SYSTEM_ENTROPY_COLLECT_INTERVAL		(10 * 1000)

/*
 * Update system entropy file every UPDATE_SYSTEM_ENTROPY_FILE seconds.
 */
#define UPDATE_SYSTEM_ENTROPY_FILE			(30)

#else	/* QUICK_TEST */

/* normal values */

#define SYSTEM_ENTROPY_COLLECT_INTERVAL		(10 * 60 * 1000)
#define UPDATE_SYSTEM_ENTROPY_FILE			(60 * 60)

#endif	/* QUICK_TEST */

/*
 * State of pending timer. 
 */
typedef enum {
	kYTSUninitialized = 0,
	kYTSCollecting,			// while gathering entropy
	kYTSCollectingInit,		// while gathering entropy the first time
	kYTSSleeping			// idle
} yarrowTimerState;

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
#define RESEED_TICKS				1000

/* 
 * The single process-wide yarrow PRNG object and associated timer state.
 * All of the code in this module runs in a single thread, owned by 
 * the YarrowServer object, so no locking is needed.  
 * 
 */
static yarrowTimerState timerState = kYTSUninitialized;
static struct timeval	lastFileUpdate;

static int gDevRandomRef = -1;

/*
 * Reusable init. Currently called from the YarrowServer constructor. 
 */
OSStatus yarrowServerInit(
	const char *entropyFilePath,
	unsigned *firstTimeout)			// RETURNED, first timeout in milliseconds
{
    UInt8               entropyFileData[ENTROPY_FILE_SIZE];
    UInt32              actLen;
    OSErr               ortn;

    /* set up prng */
    gDevRandomRef = open ("/dev/random", O_RDWR);
    if (gDevRandomRef == -1) {
        return ioErr;
    }

    /*
     * read entropy file, add contents to system entropy pool.
     * It's not an error if there is no entropy file; this
     * should only happen the first time this server runs on a given
     * system.
     */
    gettimeofday(&lastFileUpdate, NULL);
    setEntropyFilePath(entropyFilePath);
    ortn = readEntropyFile(entropyFileData,
        ENTROPY_FILE_SIZE,
        &actLen);
    if((ortn == noErr) && (actLen > 0))
        write(gDevRandomRef, entropyFileData, actLen);
    memset(entropyFileData, 0, actLen);

    /*
     * Start collecting system entropy; schedule a timer event to gather
     * it and add it to the pool.
     */
    systemEntropyBegin(SYSTEM_ENTROPY_SIZE);
    *firstTimeout = SYSTEM_ENTROPY_COLLECT_TIME;
    timerState = kYTSCollectingInit;
    
    return noErr;
}


void yarrowServerFini()
{
}

/* 
 * Add some entropy to the pool. The only "known" failure here is a 
 * result of a failure of this library'e early init.
 */
OSStatus yarrowAddEntropy(
	UInt8	*bytes,
	UInt32	numBytes,
	UInt32	bitsOfEntropy,
	unsigned *nextTimeout)		// RETURNED, next timeout in ms,  0 means none (leave
								//   timer alone)
{
    OSStatus rCode = noErr;
    
    if (gDevRandomRef == -1) { // did the system not open properly?
        return ioErr;
    }
    
    int result = write (gDevRandomRef, bytes, numBytes);
    if (result == -1) {
        rCode = ioErr;
    }
    
    debug("yarrow", "adding %ld bytes of entropy", numBytes);
    
	/* 
	 * Asynchronously - because this can be time-consuming - 
	 * add some system entropy too. This prevents clients from 
	 * overwhelming the entropy pool with its own (untrusted) data.
	 * Skip this step if we happen to be collecting entropy at the 
	 * moment.
	 */
	if(timerState == kYTSSleeping) {
		systemEntropyBegin(SYSTEM_ENTROPY_SIZE);
		timerState = kYTSCollecting;
		*nextTimeout = SYSTEM_ENTROPY_COLLECT_TIME;
	}
    
	return noErr;
}


/* 
 * Get some random data. Caller mallocs the memory.
 */
OSStatus yarrowGetRandomBytes(
	UInt8	*bytes,	
	UInt32	numBytes)
{
    if (gDevRandomRef == -1) {
        return ioErr;
    }
    
    int result = read (gDevRandomRef, bytes, numBytes);
    if (result == -1) {
        return ioErr;
    } else {
        return noErr;
    }
}


/* 
 * Handle timer event. Returns next timeout in milliseconds.
 */
unsigned yarrowTimerEvent()
{
	UInt8				sysEntropyData[SYSTEM_ENTROPY_SIZE];
	UInt32 				numSysBytes;
	UInt32 				numSysEntropyBits;
	int 				rtn;
	unsigned			nextTimeout;
	
	switch(timerState) {
		case kYTSCollecting:
		case kYTSCollectingInit:
			/* 
			 * Entropy collection in progress; finish the operation,
			 * gather result, add to entropy pool.
			 */
			debug("yarrowtimer", "collecting system entropy");
            nextTimeout = SYSTEM_ENTROPY_COLLECT_INTERVAL;
			if(rtn = systemEntropyCollect(sysEntropyData, SYSTEM_ENTROPY_SIZE, 
					&numSysBytes, &numSysEntropyBits)) {
				errorLog1("systemEntropyCollect() returned %d; aborting\n",
					rtn);
				timerState = kYTSSleeping;
				break;
			}

            unsigned dummy;
            yarrowAddEntropy (sysEntropyData, numSysBytes, 0, &dummy);

			timerState = kYTSSleeping;
			
			/* 
			 * Is it time to update the system entropy file? 
			 */
			struct timeval 	now;
			
			gettimeofday(&now, NULL);
			if( ( (now.tv_sec - lastFileUpdate.tv_sec) > UPDATE_SYSTEM_ENTROPY_FILE) &&
				HardDiskPowered() ) {
				
				UInt8 entropyFileData[ENTROPY_FILE_SIZE];
				OSErr ortn;
	
				debug("yarrow", "writing new entropy file");
                
                yarrowGetRandomBytes (entropyFileData, ENTROPY_FILE_SIZE);

				ortn = writeEntropyFile(entropyFileData, ENTROPY_FILE_SIZE);
				if(ortn) {
					errorLog1("....writeEntropyFile returned %d\n", ortn);
				}
				lastFileUpdate = now;
			}
			break;
			
		case kYTSSleeping:
			/* start to gather entropy */
			debug("yarrowtimer", "start gathering entropy");
			systemEntropyBegin(SYSTEM_ENTROPY_SIZE);
			timerState = kYTSCollecting;
			nextTimeout = SYSTEM_ENTROPY_COLLECT_TIME;
			break;
			
		default:
			errorLog1("yarrowTimerEvent with timerState %d\n", timerState);
			nextTimeout = SYSTEM_ENTROPY_COLLECT_INTERVAL;
			break;
	}
	debug("yarrowtimer", "timer rescheduling for %d msecs", nextTimeout);
	return nextTimeout;
}

