/*
 * Copyright (c) 2001-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _APPLERAID_H
#define _APPLERAID_H

#ifdef KERNEL


#ifdef DEBUG

#define IOASSERT 1   

#define IOLog1 IOLog
//#define IOLog1 IOSleep(100); IOLog
//#define IOLog2 IOLog
//#define IOLogOC IOSleep(100); IOLog	// Open Close
//#define IOLogRW IOSleep(100); IOLog	// Read Write
//#define IOLogUC IOLog			// User Client

#endif DEBUG


#ifndef IOLog1
#define IOLog1(args...)
#endif
#ifndef IOLog2
#define IOLog2(args...) 
#endif
#ifndef IOLogOC
#define IOLogOC(args...) 
#endif
#ifndef IOLogRW
#define IOLogRW(args...) 
#endif
#ifndef IOLogUC
#define IOLogUC(args...) 
#endif


#include <IOKit/IOTypes.h>
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOEventSource.h>
#include <IOKit/IOCommand.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>

class AppleRAID;
class AppleRAIDSet;
class AppleRAIDEventSource;
class AppleRAIDStorageRequest;
class AppleLVMStorageRequest;

#include "AppleRAIDGlobals.h"
#include "AppleRAIDMember.h"
#include "AppleRAIDMemoryDescriptor.h"
#include "AppleRAIDStorageRequest.h"
#include "AppleLVMStorageRequest.h"
#include "AppleRAIDEventSource.h"
#include "AppleRAIDSet.h"
#include "AppleRAIDConcatSet.h"
#include "AppleRAIDMirrorSet.h"
#include "AppleRAIDStripeSet.h"
#include "AppleLVMVolume.h"
#include "AppleLVMGroup.h"
#include "AppleRAIDUserClient.h"
#include "AppleRAIDUserLib.h"


class AppleRAID : public IOService
{
    OSDeclareDefaultStructors(AppleRAID);

private:
    OSDictionary	* raidSets;
    OSDictionary	* raidMembers;
    OSDictionary	* logicalVolumes;
    
    void addSet(AppleRAIDSet * set);
    void removeSet(AppleRAIDSet * set);

    void addMember(AppleRAIDMember * member);
    void removeMember(AppleRAIDMember * member);
    AppleRAIDMember * findMember(const OSString * uuid);

    void startSet(AppleRAIDSet * set);

public:

    bool init(void);
    void free(void);

    AppleRAIDSet * findSet(const OSString * uuid);
    AppleRAIDSet * findSet(AppleRAIDMember * member);

    void addLogicalVolume(AppleLVMVolume * volume);
    void removeLogicalVolume(AppleLVMVolume * volume);
    AppleLVMVolume * findLogicalVolume(const OSString * uuid);

    IOReturn newMember(IORegistryEntry * child);
    IOReturn oldMember(IORegistryEntry * child);
    void recoverMember(IORegistryEntry * child);

    void degradeSet(AppleRAIDSet * set);
    void restartSet(AppleRAIDSet * set, bool bump);

    IOReturn updateSet(char * setBuffer, uint32_t setBufferSize, char * retBuffer, uint32_t * retBufferSize);

    IOReturn getListOfSets(UInt32 inFlags, char * outList, uint32_t * outListSize);
    IOReturn getSetProperties(char * setString, uint32_t setStringSize, char * outProp, uint32_t * outPropSize);
    IOReturn getMemberProperties(char * memberString, uint32_t memberStringSize, char * outProp, uint32_t * outPropSize);

    IOReturn getVolumesForGroup(char * lvgString, uint32_t lvgStringSize, char * outProp, uint32_t * outPropSize);
    IOReturn getVolumeProperties(char * lvString, uint32_t lvStringSize, char * outProp, uint32_t * outPropSize);
    IOReturn getVolumeExtents(char * lvString, uint32_t lvStringSize, char * extentsBuffer, uint32_t * extentsSize);
    IOReturn updateLogicalVolume(char * lveBuffer, uint32_t lveBufferSize, char * retBuffer, uint32_t * retBufferSize);
    IOReturn destroyLogicalVolume(char * lvString, uint32_t lvStringSize, char * retBuffer, uint32_t * retBufferSize);
};

#endif /* KERNEL */

#endif /* ! _APPLERAID_H */
