// CFCountingAllocator.h

#if !defined(WEBDAV_CFCOUNTINGALLOCATOR__)
#define __WEBDAV_CFCOUNTINGALLOCATOR__ 1

typedef struct
{
	CFIndex totalBytesAllocated;
	UInt32 callsToAlloc, callsToRealloc, callsToDealloc;
	CFMutableArrayRef _ptrs;					/* Do not grok; private structure */
	Boolean keepStatistics;						/* Set this to turn on/off statistics collection.  On by default. */
} _CFCountingAllocator;

CF_EXPORT CFAllocatorRef CFCountingAllocatorCreate(CFAllocatorRef allocator);
CF_EXPORT void CFCountingAllocatorPrintPointers(CFAllocatorRef countingAllocator);
CF_EXPORT CFIndex CFCountingAllocatorBytesCurrentlyAllocated(CFAllocatorRef countingAllocator);

#endif

