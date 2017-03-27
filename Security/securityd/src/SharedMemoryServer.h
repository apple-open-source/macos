/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#ifndef __SHARED_MEMORY_SERVER__
#define __SHARED_MEMORY_SERVER__

#include <stdlib.h>
#include <string>
#include "SharedMemoryCommon.h"

class SharedMemoryServer
{
protected:
	std::string mSegmentName, mFileName;
	size_t mSegmentSize;
    uid_t mUID;

    u_int8_t* mSegment;
	u_int8_t* mDataArea;
	u_int8_t* mDataPtr;
	u_int8_t* mDataMax;

    int mBackingFile;
	
	void WriteOffset (SegmentOffsetType offset);
	void WriteData (const void* data, SegmentOffsetType length);


public:
	SharedMemoryServer (const char* segmentName, SegmentOffsetType segmentSize, uid_t uid = 0, gid_t gid = 0);
	virtual ~SharedMemoryServer ();
	
	void WriteMessage (SegmentOffsetType domain, SegmentOffsetType event, const void *message, SegmentOffsetType messageLength);
	
	const char* GetSegmentName ();
	size_t GetSegmentSize ();
	
	void SetProducerOffset (SegmentOffsetType producerOffset);
};



#endif
