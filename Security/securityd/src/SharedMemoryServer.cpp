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

static const char* kPrefix = "/private/var/db/mds/messages/se_";

SharedMemoryServer::SharedMemoryServer (const char* segmentName, SegmentOffsetType segmentSize) :
	mSegmentName (segmentName), mSegmentSize (segmentSize)
{
	mFileName = kPrefix;
	mFileName += segmentName;
	
	// make the mds directory, just in case it doesn't exist
	mkdir("/var/db/mds", 1777);
	mkdir("/var/db/mds/messages", 0755);
	
	// make the file name
	// clean any old file away
	unlink (mFileName.c_str ());
	
	// open the file
    mBackingFile = open (mFileName.c_str (), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (mBackingFile < 0)
	{
		return;
	}
	
	// set the segment size
   ftruncate (mBackingFile, segmentSize);
	
	// map it into memory
   mSegment = (u_int8_t*) mmap (NULL, mSegmentSize, PROT_READ | PROT_WRITE, MAP_SHARED, mBackingFile, 0);

	if (mSegment == (u_int8_t*) -1) // can't map the memory?
	{
		mSegment = NULL;
		unlink (mFileName.c_str());
	}
	
	mDataPtr = mDataArea = mSegment + sizeof(SegmentOffsetType);
	mDataMax = mSegment + segmentSize;;
	
	SetProducerOffset (0);
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
