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

#include "SharedMemoryServer.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <machine/byte_order.h>
#include <string>
#include <sys/stat.h>
#include <security_utilities/crc.h>
#include <security_utilities/casts.h>
#include <unistd.h>

/*
    Logically, these should go in /var/run/mds, but we know that /var/db/mds
    already exists at install time.
*/

std::string SharedMemoryCommon::SharedMemoryFilePath(const char *segmentName, uid_t uid) {
    std::string path;
    uid = SharedMemoryCommon::fixUID(uid);
    path = SharedMemoryCommon::kMDSMessagesDirectory;   // i.e. /private/var/db/mds/messages/
    if (uid != 0) {
        path += std::to_string(uid) + "/";              // e.g. /private/var/db/mds/messages/501/
    }

    path += SharedMemoryCommon::kUserPrefix;            // e.g. /var/db/mds/messages/se_
    path += segmentName;                                // e.g. /var/db/mds/messages/501/se_SecurityMessages
    return path;
}

SharedMemoryServer::SharedMemoryServer (const char* segmentName, SegmentOffsetType segmentSize, uid_t uid, gid_t gid) :
    mSegmentName (segmentName), mSegmentSize (segmentSize), mUID(SharedMemoryCommon::fixUID(uid))
{
    const mode_t perm1777 = S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;
    const mode_t perm0755 = S_IRWXU | (S_IRGRP | S_IXGRP) | (S_IROTH | S_IXOTH);
    const mode_t perm0600 = (S_IRUSR | S_IWUSR);

    // make the mds directory, just in case it doesn't exist
    if (mUID == 0) {
        mkdir(SharedMemoryCommon::kMDSDirectory, perm1777);
        mkdir(SharedMemoryCommon::kMDSMessagesDirectory, perm0755);
    } else {
        // Assume kMDSMessagesDirectory was created first by securityd
        std::string uidstr = std::to_string(mUID);
        std::string upath = SharedMemoryCommon::kMDSMessagesDirectory;
        upath += "/" + uidstr;
        mkdir(upath.c_str(), perm0755);
    }
    mFileName = SharedMemoryCommon::SharedMemoryFilePath(segmentName, uid);

    // make the file name
    // clean any old file away
    unlink(mFileName.c_str());

    // open the file
    secdebug("MDSPRIVACY","creating %s",mFileName.c_str ());
    mBackingFile = open (mFileName.c_str (), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (mBackingFile < 0)
    {
        secdebug("MDSPRIVACY","creation of %s failed", mFileName.c_str());
        return;
    }

    int rx = chown(mFileName.c_str (), uid, gid);
    if (rx) {
        secdebug("MDSPRIVACY","chown of %s to %d/%d failed : %d", mFileName.c_str(), uid, gid, rx);
    }

    if (mUID != 0) {
        int rx = fchmod(mBackingFile, perm0600);
        if (rx) {
            secdebug("MDSPRIVACY","chmod of %s to %x failed : %d", mFileName.c_str(), perm0600, rx);
        }
    }

    // set the segment size
    ftruncate (mBackingFile, segmentSize);
    
    // map it into memory
    mSegment = (u_int8_t*) mmap (NULL, mSegmentSize, PROT_READ | PROT_WRITE, MAP_SHARED, mBackingFile, 0);

    if (mSegment == MAP_FAILED) // can't map the memory?
    {
        mSegment = NULL;
        unlink(mFileName.c_str());
    } else {
        mDataPtr = mDataArea = mSegment + sizeof(SegmentOffsetType);
        mDataMax = mSegment + segmentSize;;

        SetProducerOffset (0);
    }
}

SharedMemoryServer::~SharedMemoryServer ()
{
	// go away
	if (mSegment == NULL)
	{
		return;
	}
	
	// get out of memory
	munmap (mSegment, mSegmentSize);

    close(mBackingFile);
	
	// mark the segment for deletion
	unlink (mFileName.c_str ());
}



const SegmentOffsetType
	kSegmentLength = 0,
	kCRCOffset = kSegmentLength + sizeof(SegmentOffsetType),
	kDomainOffset = kCRCOffset + sizeof(SegmentOffsetType),
	kEventTypeOffset = kDomainOffset + sizeof(SegmentOffsetType),
	kHeaderLength = kEventTypeOffset + sizeof(SegmentOffsetType) - kCRCOffset;

void SharedMemoryServer::WriteMessage (SegmentOffsetType domain, SegmentOffsetType event, const void *message, SegmentOffsetType messageLength)
{
    // backing file MUST be right size
    ftruncate (mBackingFile, mSegmentSize);

	// assemble the final message
	ssize_t messageSize = kHeaderLength + messageLength;
	u_int8_t finalMessage[messageSize];
	SegmentOffsetType *fm  = (SegmentOffsetType*) finalMessage;
	fm[0] = OSSwapHostToBigInt32(domain);
	fm[1] = OSSwapHostToBigInt32(event);
	memcpy(&fm[2], message, messageLength);
	
	SegmentOffsetType crc = CalculateCRC(finalMessage, messageSize);
	
	// write the length
	WriteOffset(int_cast<size_t, SegmentOffsetType>(messageSize));
	
	// write the crc
	WriteOffset(crc);
	
	// write the data
	WriteData (finalMessage, int_cast<size_t, SegmentOffsetType>(messageSize));
	
	// write the data count
	SetProducerOffset(int_cast<size_t, SegmentOffsetType>(mDataPtr - mDataArea));
}



const char* SharedMemoryServer::GetSegmentName ()
{
	return mSegmentName.c_str ();
}



size_t SharedMemoryServer::GetSegmentSize ()
{
	return mSegmentSize;
}


void SharedMemoryServer::SetProducerOffset (SegmentOffsetType producerCount)
{
	*((SegmentOffsetType*) mSegment) = OSSwapHostToBigInt32 (producerCount);
}



void SharedMemoryServer::WriteOffset(SegmentOffsetType offset)
{
	u_int8_t buffer[4];
	*((u_int32_t*) buffer) = OSSwapHostToBigInt32(offset);
	WriteData(buffer, 4);
}



void SharedMemoryServer::WriteData(const void* data, SegmentOffsetType length)
{
	// figure out where in the buffer we actually need to write the data
	// figure out how many bytes we can write without overflowing the buffer
	const u_int8_t* dp = (const u_int8_t*) data;
	SegmentOffsetType bytesToEnd = int_cast<ptrdiff_t, SegmentOffsetType>(mDataMax - mDataPtr);
	
	// figure out how many bytes we can write
	SegmentOffsetType bytesToWrite = (length <= bytesToEnd) ? length : bytesToEnd;

	// move the first part of the data, making sure to skip the producer pointer
	memcpy (mDataPtr, dp, bytesToWrite);
	mDataPtr += bytesToWrite;
	dp += bytesToWrite;
	
	// deduct the bytes just written
	length -= bytesToWrite;
	
	if (length != 0) // did we wrap around?
	{
		mDataPtr = mDataArea;
		memcpy (mDataPtr, dp, length);
		mDataPtr += length;
	}
}
