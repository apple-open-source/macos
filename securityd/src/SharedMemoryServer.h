#ifndef __SHARED_MEMORY_SERVER__
#define __SHARED_MEMORY_SERVER__



#include <string>
#include <stdlib.h>
#include <securityd_client/SharedMemoryCommon.h>

class SharedMemoryServer
{
protected:
	std::string mSegmentName;
	size_t mSegmentSize;
	
	u_int8_t* mSegment;

	void WriteOffsetAtOffset (SegmentOffsetType offset, SegmentOffsetType data);
	void WriteDataAtOffset (SegmentOffsetType offset, const void* data, SegmentOffsetType length);

public:
	SharedMemoryServer (const char* segmentName, SegmentOffsetType segmentSize);
	virtual ~SharedMemoryServer ();
	
	void WriteMessage (SegmentOffsetType domain, SegmentOffsetType event, const void *message, SegmentOffsetType messageLength);
	
	const char* GetSegmentName ();
	size_t GetSegmentSize ();
	
	SegmentOffsetType GetProducerCount ();
	void SetProducerCount (SegmentOffsetType producerCount);
};



#endif
