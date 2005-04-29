/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// selector - I/O stream multiplexing
//
#include "selector.h"
#include <security_utilities/errors.h>
#include <security_utilities/debugging.h>
#include <algorithm>	// min/max


namespace Security {
namespace UnixPlusPlus {


//
// construct a Selector object.
//
Selector::Selector() : fdMin(INT_MAX), fdMax(-1)
{
    // initially allocate room for FD_SETSIZE file descriptors (usually good enough)
    fdSetSize = FD_SETSIZE / NFDBITS;
    inSet.grow(0, fdSetSize);
    outSet.grow(0, fdSetSize);
    errSet.grow(0, fdSetSize);
}

Selector::~Selector()
{ }


//
// Add a Client to a Selector
//
void Selector::add(int fd, Client &client, Type type)
{
    // plausibility checks
    assert(!client.isActive());		// one Selector per client, and no re-adding
    assert(fd >= 0);
    
    secdebug("selector", "add client %p fd %d type=%d", &client, fd, type);
    
    // grow FDSets if needed
    unsigned int pos = fd / NFDBITS;
    if (pos >= fdSetSize) {
        int newSize = (fd - 1) / NFDBITS + 2;	// as much as needed + 1 spare word
        inSet.grow(fdSetSize, newSize);
        outSet.grow(fdSetSize, newSize);
        errSet.grow(fdSetSize, newSize);
    }
    
    // adjust boundaries
    if (fd < fdMin)
        fdMin = fd;
    if (fd > fdMax)
        fdMax = fd;

    // add client
    Client * &slot = clientMap[fd];
    assert(!slot);
    slot = &client;
    client.mFd = fd;
    client.mSelector = this;
    client.mEvents = type;
    set(fd, type);
}    


//
// Remove a Client from a Selector
//
void Selector::remove(int fd)
{
    // sanity checks
    assert(fd >= 0);
    ClientMap::iterator it = clientMap.find(fd);
    assert(it != clientMap.end());
    assert(it->second->mSelector == this);

    secdebug("selector", "remove client %p fd %d", it->second, fd);

    // remove from FDSets
    set(fd, none);
    
    // remove client
    it->second->mSelector = NULL;
    clientMap.erase(it);
    
    // recompute fdMin/fdMax if needed
    if (isEmpty()) {
        fdMin = INT_MAX;
        fdMax = -1;
    } else if (fd == fdMin) {
        fdMin = clientMap.begin()->first;
    } else if (fd == fdMax) {
        fdMax = clientMap.rbegin()->first;
    }
}


//
// Adjust the FDSets for a single given Client according to a new event Type mask.
//
void Selector::set(int fd, Type type)
{
    assert(fd >= 0);
    inSet.set(fd, type & input);
    outSet.set(fd, type & output);
    errSet.set(fd, type & critical);
    secdebug("selector", "fd %d notifications 0x%x", fd, type);
}


void Selector::operator () ()
{
    if (!clientMap.empty())
        singleStep(0);
}


void Selector::operator () (Time::Absolute stopTime)
{
    if (!clientMap.empty())
        singleStep(stopTime - Time::now());
}


//
// Perform a single pass through the Selector and notify all clients
// that have selected I/O pending at this time.
// There is not time limit on how long this may take; if the clients
// are well written, it won't be too long.
//
void Selector::singleStep(Time::Interval maxWait)
{
    assert(!clientMap.empty());
    secdebug("selector", "select(%d) [%d-%d] for %ld clients",
        fdMax + 1, fdMin, fdMax, clientMap.size());
    for (;;) {	// pseudo-loop - only retries
        struct timeval duration = maxWait.timevalInterval();
#if defined(__APPLE__)
        // ad-hoc fix: MacOS X's BSD rejects times of more than 100E6 seconds
        if (duration.tv_sec > 100000000)
            duration.tv_sec = 100000000;
#endif
        const int size = FDSet::words(fdMax);		// number of active words in sets
        switch (int hits = ::select(fdMax + 1,
                inSet.make(size), outSet.make(size), errSet.make(size),
                &duration)) {
        case -1:		// error
            if (errno == EINTR)
                continue;
            secdebug("selector", "select failed: errno=%d", errno);
            UnixError::throwMe();
        case 0:			// no events
            secdebug("selector", "select returned nothing");
            return;
        default:		// some events
            secdebug("selector", "%d pending descriptors", hits);
            //@@@ This could be optimized as a word-merge scan.
            //@@@ The typical case doesn't benefit from this though, though browsers might
            //@@@ and integrated servers definitely would.
            for (int fd = fdMin; fd <= fdMax && hits > 0; fd++) {
                int types = 0;
                if (inSet[fd])  types |= input;
                if (outSet[fd]) types |= output;
                if (errSet[fd]) types |= critical;
                if (types) {
                    secdebug("selector", "notify fd %d client %p type %d",
                        fd, clientMap[fd], types);
                    clientMap[fd]->notify(fd, types);
                    hits--;
                }
            }
            return;
        }
    }
}


}	// end namespace IPPlusPlus
}	// end namespace Security
