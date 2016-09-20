#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <architecture/byte_order.h>
#include <security_cdsa_utilities/cssmdb.h>
#include "SharedMemoryClient.h"
#include <string>
#include <security_utilities/crc.h>

static const char* kPrefix = "/var/db/mds/messages/se_";

using namespace Security;

SharedMemoryClient::SharedMemoryClient (const char* segmentName, SegmentOffsetType segmentSize)
{
	StLock<Mutex> _(mMutex);
	
	mSegmentName = segmentName;
	mSegmentSize = segmentSize;
    mSegment = (u_int8_t*) MAP_FAILED;
    mDataArea = mDataPtr = 0;

	if (segmentSize < sizeof(u_int32_t))
		return;
	
	// make the name
	int segmentDescriptor;
	{
		std::string name (kPrefix);
		name += segmentName;
		
		// make a connection to the shared memory block
		segmentDescriptor = open (name.c_str (), O_RDONLY, S_IROTH);
		if (segmentDescriptor < 0) // error on opening the shared memory segment?
		{
			// CssmError::throwMe (CSSM_ERRCODE_INTERNAL_ERROR);
			return;
		}
	}

    // check the file size is large enough to support Operations
    struct stat statResult = {};
    int result = fstat(segmentDescriptor, &statResult);
    if(result) {
        UnixError::throwMe(errno);
    }

    off_t sz = statResult.st_size;
    if(sz < sizeof(SegmentOffsetType)) {
        close(segmentDescriptor);
        return;
    }

    if(sz > 4*segmentSize) {
        // File is too ridiculously large. Quit.
        close(segmentDescriptor);
        return;
    }

	// map the segment into place
	mSegment = (u_int8_t*) mmap (NULL, segmentSize, PROT_READ, MAP_SHARED, segmentDescriptor, 0);
	close (segmentDescriptor);

	if (mSegment == MAP_FAILED)
	{
		return;
	}
	
	mDataArea = mSegment + sizeof (SegmentOffsetType);
	mDataMax = mSegment + sz;
	mDataPtr = mDataArea + GetProducerCount ();
}



SharedMemoryClient::~SharedMemoryClient ()
{
	StLock<Mutex> _(mMutex);
	if (mSegment == NULL || mSegment == MAP_FAILED) // error on opening the shared memory segment?
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}
	munmap (mSegment, mSegmentSize);
}



SegmentOffsetType SharedMemoryClient::GetProducerCount ()
{
	if (mSegment == NULL || mSegment == MAP_FAILED) // error on opening the shared memory segment?
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}
    if( ((u_int8_t*) (((u_int32_t*) mSegment) + 1)) > mDataMax) {
        // Check we can actually read this u_int32_t
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
    }

	SegmentOffsetType offset = OSSwapBigToHostInt32 (*(u_int32_t*) mSegment);
	if (&mSegment[offset] >= mDataMax)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	else
		return offset;
}



const char* SharedMemoryClient::GetSegmentName ()
{
	return mSegmentName.c_str ();
}



size_t SharedMemoryClient::GetSegmentSize ()
{
	return mSegmentSize;
}



void SharedMemoryClient::ReadData (void* buffer, SegmentOffsetType length)
{
	if (mSegment == NULL || mSegment == MAP_FAILED) // error on opening the shared memory segment?
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}

	u_int8_t* bptr = (u_int8_t*) buffer;

	SegmentOffsetType bytesToEnd = (SegmentOffsetType)(mDataMax - mDataPtr);
	
	// figure out how many bytes we can read
	SegmentOffsetType bytesToRead = (length <= bytesToEnd) ? length : bytesToEnd;

	// move the first part of the data
	memcpy (bptr, mDataPtr, bytesToRead);
	bptr += bytesToRead;
	
	// see if we have anything else to read
	mDataPtr += bytesToRead;
	
	length -= bytesToRead;
	if (length != 0)
	{
		mDataPtr = mDataArea;
		memcpy(bptr, mDataPtr, length);
		mDataPtr += length;
	}
}



SegmentOffsetType SharedMemoryClient::ReadOffset()
{
	SegmentOffsetType offset;
	ReadData(&offset, sizeof(SegmentOffsetType));
	offset = OSSwapBigToHostInt32 (offset);
	return offset;
}



bool SharedMemoryClient::ReadMessage (void* message, SegmentOffsetType &length, UnavailableReason &ur)
{
	StLock<Mutex> _(mMutex);

	if (mSegment == NULL || mSegment == MAP_FAILED) // error on opening the shared memory segment?
	{
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	}

	ur = kURNone;
	
	size_t offset = mDataPtr - mDataArea;
	if (offset == GetProducerCount())
	{
		ur = kURNoMessage;
		return false;
	}
	
	// get the length of the message in the buffer
	length = ReadOffset();
	
	// we have the possibility that data is correct, figure out where the data is actually located
	// get the length of the message stored there
	if (length == 0 || length >= kPoolAvailableForData)
	{
		if (length == 0)
		{
			ur = kURNoMessage;
		}
		else
		{
			ur = kURBufferCorrupt;
		}

		// something's gone wrong, reset.
		mDataPtr = mDataArea + GetProducerCount ();
		return false;
	}
	
	// read the crc
	SegmentOffsetType crc = ReadOffset();

	// read the data into the buffer
	ReadData (message, length);
	
	// calculate the CRC
	SegmentOffsetType crc2 = CalculateCRC((u_int8_t*) message, length);
	if (crc != crc2)
	{
		ur = kURBufferCorrupt;
		mDataPtr = mDataArea + GetProducerCount ();
		return false;
	}

	return true;
}
