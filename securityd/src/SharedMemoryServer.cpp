#include "SharedMemoryServer.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <machine/byte_order.h>


SharedMemoryServer::SharedMemoryServer (const char* segmentName, SegmentOffsetType segmentSize) :
	mSegmentName (segmentName), mSegmentSize (segmentSize)
{
	// open the file
	int segmentDescriptor = shm_open (segmentName, O_RDWR | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);
	if (segmentDescriptor < 0)
	{
		return;
	}
	
	// set the segment size
	ftruncate (segmentDescriptor, segmentSize);
	
	// map it into memory
	mSegment = (u_int8_t*) mmap (NULL, mSegmentSize, PROT_READ | PROT_WRITE, MAP_SHARED, segmentDescriptor, 0);
	close (segmentDescriptor);

	if (mSegment == (u_int8_t*) -1) // can't map the memory?
	{
		mSegment = NULL;
		shm_unlink (segmentName);
	}
	
	SetProducerCount (0);
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
	
	// mark the segment for deletion
	shm_unlink (mSegmentName.c_str ());
}



const SegmentOffsetType
	kSegmentLength = 0,
	kDomainOffset = kSegmentLength + sizeof (SegmentOffsetType),
	kEventTypeOffset = kDomainOffset + sizeof (SegmentOffsetType),
	kHeaderLength = kEventTypeOffset + sizeof (SegmentOffsetType);

void SharedMemoryServer::WriteMessage (SegmentOffsetType domain, SegmentOffsetType event, const void *message, SegmentOffsetType messageLength)
{
	// get the current producer count
	SegmentOffsetType pCount = GetProducerCount ();
	
	SegmentOffsetType actualLength = messageLength + kHeaderLength;

	// for now, write a 0 for the length -- this will give clients the opportunity to not process an
	// incomplete message
	WriteOffsetAtOffset (pCount, 0);
	
	// extend the overall write count by enough data to hold the message length and message
	SetProducerCount (pCount + actualLength);
	
	// write the data
	WriteDataAtOffset (pCount + kHeaderLength, message, messageLength);
	
	// write the domain
	WriteOffsetAtOffset (pCount + kDomainOffset, domain);
	
	// write the event type
	WriteOffsetAtOffset (pCount + kEventTypeOffset, event);

	// write the data count
	WriteOffsetAtOffset (pCount, actualLength);
}



const char* SharedMemoryServer::GetSegmentName ()
{
	return mSegmentName.c_str ();
}



size_t SharedMemoryServer::GetSegmentSize ()
{
	return mSegmentSize;
}



SegmentOffsetType SharedMemoryServer::GetProducerCount ()
{
	// the data is stored in the buffer in network byte order
	u_int32_t pCount = *(u_int32_t*) mSegment;
	return OSSwapHostToBigInt32 (pCount);
}



void SharedMemoryServer::SetProducerCount (SegmentOffsetType producerCount)
{
	*((u_int32_t*) mSegment) = OSSwapHostToBigInt32 (producerCount);
}



void SharedMemoryServer::WriteOffsetAtOffset (SegmentOffsetType offset, SegmentOffsetType data)
{
	// convert data to network byte order
	u_int8_t buffer[4];
	*((u_int32_t*) buffer) = OSSwapHostToBigInt32 (data);
	
	WriteDataAtOffset (offset, buffer, sizeof (buffer));
}



void SharedMemoryServer::WriteDataAtOffset (SegmentOffsetType offset, const void* data, SegmentOffsetType length)
{
	// figure out where in the buffer we actually need to write the data
	SegmentOffsetType realOffset = offset % kPoolAvailableForData;
	
	// figure out how many bytes we can write without overflowing the buffer
	SegmentOffsetType bytesToEnd = kPoolAvailableForData - realOffset;
	
	// figure out how many bytes we can write
	SegmentOffsetType bytesToWrite = bytesToEnd < length ? bytesToEnd : length;
	
	// move the first part of the data, making sure to skip the producer pointer
	memmove (mSegment + sizeof (SegmentOffsetType) + realOffset, data, bytesToWrite);
	
	// deduct the bytes just written
	length -= bytesToWrite;
	
	if (length != 0) // did we wrap around?
	{
		memmove (mSegment + sizeof (SegmentOffsetType), ((u_int8_t*) data) + bytesToWrite, length);
	}
}
