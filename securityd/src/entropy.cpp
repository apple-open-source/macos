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
#include <errno.h>
#include <security_utilities/logging.h>
#include <sys/sysctl.h>
#include <security_utilities/debugging.h>

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
    mach_timespec_t timings[timingsToCollect];
    size_t size = sizeof(timings);
    if (sysctl(mib, 4, timings, &size, NULL, 0)) {
        Syslog::alert("entropy collection failed (errno=%d)", errno);
        return;
    }
	size /= sizeof(mach_timespec_t); // convert to element count
	if (size > timingsToCollect)
		size = timingsToCollect;	// pure paranoia
    char buffer[timingsToCollect];
    size /= sizeof(mach_timespec_t); // convert to element count
    if (size > timingsToCollect)
        size = timingsToCollect;    // pure paranoia
    for (unsigned n = 0; n < size; n++)
        buffer[n] = timings[n].tv_nsec;	// truncating to LSB
	secdebug("entropy", "Entropy size %d: %02x %02x %02x %02x %02x %02x %02x %02x...",
		(int)size, 
		(unsigned char)buffer[0], (unsigned char)buffer[1], (unsigned char)buffer[2],
		(unsigned char)buffer[3], (unsigned char)buffer[4], (unsigned char)buffer[5],
		(unsigned char)buffer[6], (unsigned char)buffer[7]);
	SECURITYD_ENTROPY_SEED((void *)buffer, size);
    addEntropy(buffer, size);
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

