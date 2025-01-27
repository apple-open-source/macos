/*
 * Copyright (c) 2016-2017 Apple Inc. All Rights Reserved.
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

#ifndef __SHAREDMEMORYCLIENT__
#define __SHAREDMEMORYCLIENT__

#include <string>
#include <stdlib.h>
#include <securityd_client/SharedMemoryCommon.h>
#include <security_utilities/threading.h>

namespace Security
{

    
enum UnavailableReason {kURNone, kURMessageDropped, kURMessagePending, kURNoMessage, kURBufferCorrupt};

class SharedMemoryClient
{
protected:
	std::string mSegmentName;
	size_t mSegmentSize;
	Mutex mMutex;
    uid_t mUID;

	u_int8_t* mSegment;
	u_int8_t* mDataArea;
	u_int8_t* mDataPtr;
	u_int8_t* mDataMax;
	
	SegmentOffsetType GetProducerCount ();

	void ReadData (void* buffer, SegmentOffsetType bytesToRead);
	SegmentOffsetType ReadOffset ();
	
public:
	SharedMemoryClient (const char* segmentName, SegmentOffsetType segmentSize, uid_t uid = 0);
	virtual ~SharedMemoryClient ();
	
	bool ReadMessage (void* message, SegmentOffsetType &length, UnavailableReason &ur);
	
    const char* GetSegmentName() { return mSegmentName.c_str (); }
    size_t GetSegmentSize() { return mSegmentSize; }

    uid_t getUID() const { return mUID; }

    bool uninitialized() { return (mSegment == NULL || mSegment == MAP_FAILED); }
};

};  /* namespace */


#endif

