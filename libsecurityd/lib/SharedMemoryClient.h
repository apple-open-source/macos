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
	
	u_int8_t* mSegment;
	u_int8_t* mDataArea;
	u_int8_t* mDataPtr;
	u_int8_t* mDataMax;
	
	SegmentOffsetType GetProducerCount ();
	
	void ReadData (void* buffer, SegmentOffsetType bytesToRead);
	SegmentOffsetType ReadOffset ();
	
public:
	SharedMemoryClient (const char* segmentName, SegmentOffsetType segmentSize);
	virtual ~SharedMemoryClient ();
	
	bool ReadMessage (void* message, SegmentOffsetType &length, UnavailableReason &ur);
	
	const char* GetSegmentName ();
	size_t GetSegmentSize ();
};

};


#endif
