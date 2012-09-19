/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// EntropyManager - manage entropy on the system.
//
// Here is our mission:
// (1) On startup, read the entropy file and seed it into the RNG for initial use
// (2) Periodically, collect entropy from the system and seed it into the RNG
// (3) Once in a while, take entropy from the RNG and write it to the entropy file
//   for use across reboots.
//
// This class will fail to operate if the process has (and retains) root privileges.
// We re-open the entropy file on each use so that we don't work with a "phantom"
// file that some fool administrator removed yesterday.
//
#include "entropy.h"
#include "dtrace.h"
#include <sys/sysctl.h>
#include <mach/clock_types.h>
#include <mach/mach_time.h>
#include <errno.h>
#include <security_utilities/logging.h>
#include <sys/sysctl.h>
#include <security_utilities/debugging.h>
#include <math.h>

/* when true, action() called every 15 seconds */
#define ENTROPY_QUICK_UPDATE	0
#if		ENTROPY_QUICK_UPDATE
#define COLLECT_INTERVAL		15	
#else
#define COLLECT_INTERVAL		collectInterval	
#endif	//ENTROPY_QUICK_UPDATE

using namespace UnixPlusPlus;


//
// During construction, we perform initial entropy file recovery.
//
EntropyManager::EntropyManager(MachPlusPlus::MachServer &srv, const char *entropyFile)
    : DevRandomGenerator(true), server(srv),
    mEntropyFilePath(entropyFile), mNextUpdate(Time::now())
{
    // Read the entropy file and seed the RNG. It is not an error if we can't find one.
    try {
        AutoFileDesc oldEntropyFile(entropyFile, O_RDONLY);
        char buffer[entropyFileSize];
        if (size_t size = oldEntropyFile.read(buffer))
            addEntropy(buffer, size);
    } catch (...) { }
    
    // go through a collect/update/reschedule cycle immediately
    action();
}


//
// Timer action
//
void EntropyManager::action()
{
    collectEntropy();
    updateEntropyFile();
    
    server.setTimer(this, Time::Interval(COLLECT_INTERVAL));	// drifting reschedule (desired)
}


static const double kBytesOfEntropyToCollect = 240;
// that gives us a minimum of 2.16 * 10^609 possible combinations.  It's a finite number to be sure...

static const int kExpectedLoops = 10;

// Calculate the amount of entropy in the buffer (per Shannon's Entropy Calculation)
static double CalculateEntropy(const void* buffer, size_t bufferSize)
{
    double sizef = bufferSize;
    const u_int8_t* charBuffer = (const u_int8_t*) buffer;
    
    // zero the tabulation array
    int counts[256];
    memset(counts, 0, sizeof(counts));

    // tabulate the occurances of each byte in the array
    size_t i;
    for (i = 0; i < bufferSize; ++i)
    {
        counts[charBuffer[i]] += 1;
    }
    
    // calculate the number of bits/byte of entropy
    double entropy = 0.0;
    
    for (i = 0; i < 256; ++i)
    {
        if (counts[i] > 0)
        {
            double p = ((double) counts[i]) / sizef;
            double term = p * -log2(p);
            entropy += term;
        }
    }
    
    double entropicBytes = bufferSize * entropy / 8.0;
    
    return entropicBytes;
}



//
// Collect system timings and seed into the RNG.
// Note that the sysctl will block until the buffer is full or the timeout expires.
// We currently use a 1ms timeout, which almost always fills the buffer and
// does not provide enough of a delay to worry about it. If we ever get worried,
// we could call longTermActivity on the server object to get another thread going.
//

void EntropyManager::collectEntropy()
{
	SECURITYD_ENTROPY_COLLECT();

    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_KDEBUG;
    mib[2] = KERN_KDGETENTROPY;
    mib[3] = 1;	// milliseconds maximum delay
	
	mach_timespec_t buffer[timingsToCollect];
	
	int result;
	
	size_t size = sizeof(mach_timespec_t) * timingsToCollect;
	
    double bytesRemaining = kBytesOfEntropyToCollect;
    
    int loopCount = 0;
    
    while (bytesRemaining >= 0)
    {
        result = sysctl(mib,4, buffer, &size, NULL, 0);
        if (result == -1) {
            Syslog::alert("entropy measurement returned no entropy (errno=%d)", errno);
        }
        else if (size == 0)
        {
            Syslog::alert("entropy measurement returned no entropy.");
        }

        // remove the non-entropic pieces from the buffer
        u_int16_t nonEnt[timingsToCollect];
        
        // treat the received buffer as an array of u_int16 and only take the first two bytes of each
        u_int16_t *rawEnt = (u_int16_t*) buffer;
        
        int i;
        for (i = 0; i < timingsToCollect; ++i)
        {
            nonEnt[i] = *rawEnt;
            rawEnt += 4;
        }
        
        SECURITYD_ENTROPY_SEED((void *)nonEnt, (unsigned int) sizeof(nonEnt));
        addEntropy(nonEnt, sizeof(nonEnt));
        
        double entropyRead = CalculateEntropy(nonEnt, sizeof(nonEnt));
        bytesRemaining -= entropyRead;
        
        loopCount += 1;
    }
    
    if (loopCount > kExpectedLoops)
    {
        Syslog::alert("Entropy collection fulfillment took %d loops", loopCount);
    }
}


//
// (Re)write the entropy file with random data pulled from the RNG
//
void EntropyManager::updateEntropyFile()
{
    if (Time::now() >= mNextUpdate) {
        try {
			SECURITYD_ENTROPY_SAVE((char *)mEntropyFilePath.c_str());
			mNextUpdate = Time::now() + Time::Interval(updateInterval);
            secdebug("entropy", "updating %s", mEntropyFilePath.c_str());
        	char buffer[entropyFileSize];
			random(buffer, entropyFileSize);
            AutoFileDesc entropyFile(mEntropyFilePath.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0600);
            if (entropyFile.write(buffer) != entropyFileSize)
                Syslog::warning("short write on entropy file %s", mEntropyFilePath.c_str());
        } catch (...) {
            Syslog::warning("error writing entropy file %s", mEntropyFilePath.c_str());
        }
    }
}

