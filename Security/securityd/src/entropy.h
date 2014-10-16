/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
// entropy - periodical to collect and seed entropy into /dev/random
//
#ifndef _H_ENTROPY
#define _H_ENTROPY

#include <security_utilities/machserver.h>
#include <security_utilities/timeflow.h>
#include <security_utilities/devrandom.h>

using namespace Security;
using MachPlusPlus::MachServer;


//
// A (one-off) timer object that manages system entropy
//
class EntropyManager : public MachServer::Timer, private DevRandomGenerator {
    // all the parameters you ever (should) want to change :-)
    static const int collectInterval = 600; // collect every 10 minutes
    static const int updateInterval = 3600 * 6; // update file every 6 hours
    static const int timingsToCollect = 40; // how many timings?

public:
	EntropyManager(MachPlusPlus::MachServer &srv, const char *entropyFile);
		
	void action();
    
	MachPlusPlus::MachServer	&server;		// to which we do setTimer()
    
private:
    string mEntropyFilePath;			// absolute path to entropy file
    Time::Absolute mNextUpdate;			// next time for entropy file update
    
    void collectEntropy();				// collect system timings and seed RNG
    void updateEntropyFile();			// update entropy file from RNG if it's time
    
    static const size_t entropyFileSize = 20;	// bytes (effectively one SHA-1 worth)
};

#endif //_H_ENTROPY
