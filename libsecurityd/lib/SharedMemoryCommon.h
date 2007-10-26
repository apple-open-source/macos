#ifndef __SHARED_MEMORY_COMMON__
#define __SHARED_MEMORY_COMMON__



#include <sys/types.h>

const unsigned kSegmentSize = 4096;
const unsigned kNumberOfSegments = 8;
const unsigned kSharedMemoryPoolSize = kSegmentSize * kNumberOfSegments;

const unsigned kBytesWrittenOffset = 0;
const unsigned kBytesWrittenLength = 4;
const unsigned kPoolAvailableForData = kSharedMemoryPoolSize - kBytesWrittenLength;

typedef u_int32_t SegmentOffsetType;

#define SECURITY_MESSAGES_NAME "SecurityMessages"

#endif
