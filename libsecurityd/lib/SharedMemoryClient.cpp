#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <architecture/byte_order.h>
#include <security_cdsa_utilities/cssmdb.h>
#include "SharedMemoryClient.h"


using namespace Security;

SharedMemoryClient::SharedMemoryClient (const char* segmentName, SegmentOffsetType segmentSize) :
	mSegmentName (segmentName), mSegmentSize (segmentSize), mSegment (NULL)
{
	StLock<Mutex> _(mMutex);
	
	// make a connection to the shared memory block
	int segmentDescriptor = shm_open (segmentName, O_RDONLY, S_IROTH);
	if (segmentDescriptor < 0) // error on opening the shared memory segment?
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}
	
	// map the segment into place
	mSegment = (u_int8_t*) mmap (NULL, segmentSize, PROT_READ, MAP_SHARED, segmentDescriptor, 0);
	close (segmentDescriptor);

	if (mSegment == (u_int8_t*) -1)
	{
		return;
	}
	
	mCurrentOffset = GetProducerCount ();
}



SharedMemoryClient::~SharedMemoryClient ()
{
	StLock<Mutex> _(mMutex);
	if (mSegment == NULL) // error on opening the shared memory segment?
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}
	munmap (mSegment, mSegmentSize);
}



SegmentOffsetType SharedMemoryClient::GetProducerCount ()
{
	if (mSegment == NULL) // error on opening the shared memory segment?
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}
	return OSSwapBigToHostInt32 (*(u_int32_t*) mSegment);
}



const char* SharedMemoryClient::GetSegmentName ()
{
	return mSegmentName.c_str ();
}



size_t SharedMemoryClient::GetSegmentSize ()
{
	return mSegmentSize;
}



bool SharedMemoryClient::ReadBytes (void* buffer, SegmentOffsetType offset, SegmentOffsetType bytesToRead)
{
	if (mSegment == NULL) // error on opening the shared memory segment?
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}

	u_int8_t* bptr = (u_int8_t*) buffer;

	// calculate the number of bytes between offset and the end of the buffer
	if (offset > kPoolAvailableForData)
	{
		// we have an obvious error, do nothing -- it's better than crashing...
		return false;
	}
	
	while (bytesToRead > 0)
	{
		// figure out how many bytes are left in the buffer before wrapping around
		SegmentOffsetType bytesAvailable = kPoolAvailableForData - offset;
		SegmentOffsetType bytesToReadThisTime = bytesToRead > bytesAvailable ? bytesAvailable : bytesToRead;
		
		// move the first load of bytes
		memmove (bptr, mSegment + sizeof (SegmentOffsetType) + offset, bytesToReadThisTime);
		
		// update everything
		bytesToRead -= bytesToReadThisTime;
		offset = 0;
	}
	
	return true;
}



bool SharedMemoryClient::ReadMessage (void* message, SegmentOffsetType &length, UnavailableReason &ur)
{
	StLock<Mutex> _(mMutex);

	if (mSegment == NULL) // error on opening the shared memory segment?
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}

	ur = kURNone;
	
	// if there are more bytes between my count and the producer's, our buffer has been overrun
	SegmentOffsetType offset = GetProducerCount ();
	if (offset == mCurrentOffset)
	{
		ur = kURNoMessage;
		return false;
	}
	
	if (offset - mCurrentOffset > kPoolAvailableForData) // oops, we've been overrun...
	{
		ur = kURMessageDropped;
		mCurrentOffset = GetProducerCount ();
		return false;
	}
	
	// we have the possibility that data is correct, figure out where the data is actually located
	SegmentOffsetType actualOffset = mCurrentOffset % kPoolAvailableForData;
	
	// get the length of the message stored there
	if (!ReadBytes (&length, actualOffset, sizeof (SegmentOffsetType)))
	{
		ur = kURBufferCorrupt;
		mCurrentOffset = GetProducerCount ();
		return false;
	}
	
	length = OSSwapBigToHostInt32 (length);
	
	if (length == 0) // the producer is in the process of writing out the message
	{
		ur = kURMessagePending;
		return false;
	}
	
	if (length > kPoolAvailableForData)
	{
		ur = kURBufferCorrupt;
		mCurrentOffset = GetProducerCount ();
		return false;
	}
	
	// update our pointer
	mCurrentOffset += length;
	
	actualOffset += sizeof (SegmentOffsetType);
	if (actualOffset > kPoolAvailableForData)
	{
		actualOffset -= kPoolAvailableForData;
	}
	
	length -= sizeof (SegmentOffsetType);
	
	// read the data into the buffer
	if (!ReadBytes (message, actualOffset, length))
	{
		ur = kURBufferCorrupt;
		mCurrentOffset = GetProducerCount ();
		return false;
	}
	
	// check again to make sure that we haven't been overrun
	if (GetProducerCount () - mCurrentOffset > kPoolAvailableForData) // oops, we've been overrun...
	{
		ur = kURMessageDropped;
		mCurrentOffset = GetProducerCount ();
		return false;
	}
	
	return true;
}
