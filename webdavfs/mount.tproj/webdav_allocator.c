// CFCountingAllocator.c

#include <CoreFoundation/CoreFoundation.h>
#include "webdav_allocator.h"
#include <stdio.h>

/*****************************************************************************/

typedef struct {
	void *ptr;
	CFIndex size;
} _CFCountingAllocatorPtrRecord;

/*****************************************************************************/

static CFIndex getIndex(void *info, void *ptr)
{
	CFMutableArrayRef array = ((_CFCountingAllocator *)info)->_ptrs;
	CFIndex i, c = CFArrayGetCount(array);
	for (i = 0; i < c; i++)
	{
		const _CFCountingAllocatorPtrRecord *rec = CFArrayGetValueAtIndex(array, i);
		if (rec->ptr == ptr)
		{
			return i;
		}
	}
	return -1;
}

/*****************************************************************************/

static void *myAllocate(CFIndex size, CFOptionFlags hint, void *info)
{
	_CFCountingAllocator * alloc = (_CFCountingAllocator *)info;
	void *ptr = malloc(size);
	if (alloc->keepStatistics)
	{
		_CFCountingAllocatorPtrRecord * newRec = malloc(sizeof(_CFCountingAllocatorPtrRecord));
		newRec->ptr = ptr;
		newRec->size = size;
		CFArrayAppendValue(alloc->_ptrs, newRec);
		alloc->totalBytesAllocated += size;
		alloc->callsToAlloc++;
	}
#ifdef DEBUG
	fprintf(stderr, "MyAllocate: allocated %d\n", (int)ptr);
#endif

	return ptr;
}

/*****************************************************************************/

static void *myReallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info)
{
	CFIndex i = getIndex(info, ptr);
	_CFCountingAllocator * alloc = (_CFCountingAllocator *)info;
	void *newPtr = realloc(ptr, newsize);
	// Don't test against keepStatistics here; reallocs are always kept or discarded
	// if the original alloc was kept/discarded
	if (i != -1)
	{
		_CFCountingAllocatorPtrRecord * oldRec =
			(_CFCountingAllocatorPtrRecord *)CFArrayGetValueAtIndex(alloc->_ptrs, i);
		alloc->callsToRealloc++;
		if (oldRec->size < newsize)
		{
			alloc->totalBytesAllocated += newsize - oldRec->size;
			oldRec->size = newsize;
		}
		oldRec->ptr = newPtr;
	}
#ifdef DEBUG
	fprintf(stderr, "Realloc: reallocating %d\n", (int)newPtr);
#endif

	return newPtr;
}

/*****************************************************************************/

static void myDeallocate(void *ptr, void *info)
{
	CFIndex i = getIndex(info, ptr);
	if (i != -1)
	{
		_CFCountingAllocator * alloc = (_CFCountingAllocator *)info;
		CFArrayRemoveValueAtIndex(alloc->_ptrs, i);
		alloc->callsToDealloc++;
	}
#ifdef DEBUG
	fprintf(stderr, "myDeallocate: freeing %d\n", (int)ptr);
#endif

	free(ptr);
}

/*****************************************************************************/

static CFIndex myPreferredSize(CFIndex size, CFOptionFlags hint, void *info)
{
	return size;
}

/*****************************************************************************/

static CFIndex getTotalBytes(_CFCountingAllocator *alloc)
{
	CFIndex index, count, totalBytes;
	count = CFArrayGetCount(alloc->_ptrs);
	totalBytes = 0;
	for (index = 0; index < count; index++)
	{
		totalBytes +=
			((_CFCountingAllocatorPtrRecord *)CFArrayGetValueAtIndex(alloc->_ptrs, index))->size;
	}
	return totalBytes;
}

/*****************************************************************************/

static CFStringRef myCopyDescription(const void *info)
{
	_CFCountingAllocator * alloc = (_CFCountingAllocator *)info;
	CFMutableStringRef desc = CFStringCreateMutable(NULL, 0);
	CFStringAppendFormat(desc, NULL,
		CFSTR("%d bytes allocated in %d nodes"), alloc->totalBytesAllocated, alloc->callsToAlloc);
	CFStringAppendFormat(desc, NULL,
		CFSTR("(%d nodes reallocated; %d nodes deallocated)\n"), alloc->callsToRealloc,
		alloc->callsToDealloc);
	CFStringAppendFormat(desc, NULL,
		CFSTR("%d nodes currently allocated, using %d bytes.\n"), CFArrayGetCount(alloc->_ptrs),
		getTotalBytes(alloc));
	return desc;
}

/*****************************************************************************/

CF_EXPORT CFIndex CFCountingAllocatorBytesCurrentlyAllocated(CFAllocatorRef countingAllocator)
{
	CFAllocatorContext ctxt;
	ctxt.version = 0;
	CFAllocatorGetContext(countingAllocator, &ctxt);
	return getTotalBytes((_CFCountingAllocator *)ctxt.info);
}

/*****************************************************************************/

CFAllocatorRef CFCountingAllocatorCreate(CFAllocatorRef allocator)
{
	CFAllocatorContext ctxt;
	_CFCountingAllocator * info = malloc(sizeof(_CFCountingAllocator));
	info->totalBytesAllocated = 0;
	info->callsToAlloc = 0;
	info->callsToRealloc = 0;
	info->callsToDealloc = 0;
	info->keepStatistics = TRUE;
	info->_ptrs = CFArrayCreateMutable(NULL, 0, NULL);
	ctxt.info = (void *)info;
	ctxt.retain = NULL;
	ctxt.release = NULL;
	ctxt.copyDescription = myCopyDescription;
	ctxt.allocate = myAllocate;
	ctxt.reallocate = myReallocate;
	ctxt.deallocate = myDeallocate;
	ctxt.preferredSize = myPreferredSize;
	return CFAllocatorCreate(allocator, &ctxt);
}

/*****************************************************************************/

void CFCountingAllocatorPrintPointers(CFAllocatorRef countingAllocator)
{
	CFAllocatorContext ctxt;
	//	  CFArrayRef array;
	//	  CFIndex i, c;
	CFStringRef desc;

	ctxt.version = 0;
	CFAllocatorGetContext(countingAllocator, &ctxt);
	desc = myCopyDescription(ctxt.info);
	CFShow(desc);
	CFRelease(desc);

#if 0
	array = CFArrayCreateCopy(NULL, ((_CFCountingAllocator *)ctxt.info)->_ptrs);
	c = CFArrayGetCount(array);
	for (i = 0; i < c; i++)
	{
		const _CFCountingAllocatorPtrRecord *ptr = CFArrayGetValueAtIndex(array, i);
		fprintf(stderr, "0x%x (%ld bytes) ", (int)(ptr->ptr), ptr->size);
		if (__CFGenericTypeID(ptr->ptr) < CFXMLParserGetTypeID() && 0 != __CFGenericTypeID(ptr->ptr))
		{
			CFShow(ptr->ptr);
		}
		fprintf(stderr, "\n");
	}
	CFRelease(array);
#endif

}

/*****************************************************************************/
