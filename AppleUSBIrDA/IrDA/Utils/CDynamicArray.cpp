/*
    File:       CDynamicArray.cpp

    Contains:   Implementation of the CDynamicArray class


*/

#include "CDynamicArray.h"
#include "CArrayIterator.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasCDynamicArrayTracing > 0)

enum
{
    kLogNew = 1,
    kLogFree,
    kLogInit,
    
    kLogSetSize1,
    kLogSetSize2,
    kLogSetSizeCurrent,
    kLogSetSizeNew,
    
    kLogIOAllocBuffer,
    kLogIOAllocSize,
    kLogIOFreeBuffer,
    kLogIOFreeSize,
    
    kLogRemoveAt,
    kLogRemoveAtObj
};

static
EventTraceCauseDesc gTraceEvents[] = {
    {kLogNew,           "CDynamicArray:  New, obj="},
    {kLogFree,          "CDynamicArray:  Free, obj="},
    {kLogInit,          "CDynamicArray:  Init, elemsize=, chunksize="},
    
    {kLogSetSize1,          "CDynamicArray: set size, obj="},
    {kLogSetSize2,          "CDynamicArray: set size, new count="},
    {kLogSetSizeCurrent,    "CDynamicArray: set size, current alloc="},
    {kLogSetSizeNew,        "CDynamicArray: set size, new alloc="},
    
    {kLogIOAllocBuffer,     "CDynamicArray: ioalloc, buf="},
    {kLogIOAllocSize,       "CDynamicArray: ioalloc, size="},
    {kLogIOFreeBuffer,      "CDynamicArray: iofree, buf="},
    {kLogIOFreeSize,        "CDynamicArray: iofree, size="},
    
    {kLogRemoveAt,          "CDynamicArray: remove at, index=, count="},
    {kLogRemoveAtObj,       "CDynamicArray: remove at, obj="}
};
#define XTRACE(x, y, z) IrDALogAdd( x, y, z, gTraceEvents, true )
#else
#define XTRACE(x, y, z) ((void) 0)
#endif


//--------------------------------------------------------------------------------
#define super OSObject
    OSDefineMetaClassAndStructors(CDynamicArray, OSObject);
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
//      CDynamicArray::cDynamicArray
//--------------------------------------------------------------------------------
CDynamicArray *
CDynamicArray::cDynamicArray(Size elementSize, ArrayIndex chunkSize)
{
    CDynamicArray *obj = new CDynamicArray;
    
    XTRACE(kLogNew, (int)obj >> 16, (short)obj);
    
    if (obj && !obj->init(elementSize, chunkSize)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

//--------------------------------------------------------------------------------
//      CDynamicArray::init
//--------------------------------------------------------------------------------
Boolean
CDynamicArray::init(Size elementSize, ArrayIndex chunkSize)
{

    XTRACE(kLogInit, elementSize, chunkSize);
/*
    put things in a known state.
    note that fArrayBlock is not allocated here, but in ::SetArraySize() below.
*/

    fSize           = 0;
    fElementSize    = elementSize;
    fChunkSize      = chunkSize;
    fAllocatedSize  = 0;
    fArrayBlock     = nil;
    fIterator       = nil;
	
    return super::init();
    
} // CDynamicArray::CDynamicArray


//--------------------------------------------------------------------------------
//      CDynamicArray::free
//--------------------------------------------------------------------------------
void
CDynamicArray::free()
{
    XTRACE(kLogFree, 0, (short)this);
    
    if (fIterator) {
	fIterator->DeleteArray();
	fIterator = nil;
    }
    
    if (fArrayBlock) {
	int len = ComputeByteCount(fAllocatedSize);
	XTRACE(kLogIOFreeBuffer, 0, (short)fArrayBlock);
	XTRACE(kLogIOFreeSize, len >> 16, (short)len);
	IOFree( fArrayBlock, len);
	fArrayBlock = nil;
    }

    super::free();
} // CDynamicArray::free


//--------------------------------------------------------------------------------
//      CDynamicArray::SafeElementPtrAt(ArrayIndex index)
//--------------------------------------------------------------------------------
void* CDynamicArray::SafeElementPtrAt(ArrayIndex index)
{

    return ((fSize > 0) && (index > kEmptyIndex) && (index < fSize))
	? ElementPtrAt(index)
	: nil;

} // CDynamicArray::SafeElementPtrAt


//--------------------------------------------------------------------------------
//      CDynamicArray::RemoveElementsAt
//--------------------------------------------------------------------------------
IrDAErr CDynamicArray::RemoveElementsAt(ArrayIndex index, ArrayIndex count)
/*
    remove count elements starting with index.
    actually, all we do is compress the array by overlaying elements
    from below the target block on top the target block, then
    resetting the array block size.

    Ergo, deletions from the end of the array are more efficient than from
    the middle or front.

    note that the elements themselves are *NOT* delete'd
*/
{
    IrDAErr result = noErr;

    XTRACE(kLogRemoveAt, index, count);
    XTRACE(kLogRemoveAtObj, (int)this >> 16, (short)this);

    // can't remove:    before the start (index < 0)
    //                  zero or negative count (count <= 0)
    //                  after the end (index >= fSize)
    //                  past the end (index + count > fSize)

    // quick return if there is nothing to do
    if (fSize == 0)
	return 0;

    XASSERT(index >= 0);
    XASSERT(count >= 0);
    XASSERT(index < fSize);
    XASSERT((index + count) <= fSize);

    if (count > 0)
	{
	void *indexPtr, *nextElementPtr, *lastElementPtr;

	XASSERT(fArrayBlock);

	indexPtr = ElementPtrAt(index);
	nextElementPtr = ElementPtrAt(index + count);
	lastElementPtr = ElementPtrAt(fSize);

	// removed from middle? Compress the array
	if (nextElementPtr < lastElementPtr)
	    BlockMove(nextElementPtr, indexPtr, (long)lastElementPtr - (long)nextElementPtr);

	// take up slack if necessary.
	result = SetArraySize(fSize - count);
	XREQUIRENOT(result, Fail_SetArraySize);

	fSize -= count;

	// Keep all iterators apprised
	if (fIterator)
	    fIterator->RemoveElementsAt(index, count);
	}

Fail_SetArraySize:

    return result;

} // CDynamicArray::RemoveElementsAt


//--------------------------------------------------------------------------------
//      CDynamicArray::GetElementsAt
//--------------------------------------------------------------------------------
IrDAErr CDynamicArray::GetElementsAt(ArrayIndex index, void* elemPtr, ArrayIndex count)
{
    // can't get:   before the start (index < 0)
    //              after the end (index >= fSize)
    //              negative count (count < 0)
    //              past the end (index + count > fSize)

    XASSERT(index >= 0);
    XASSERT(index < fSize);
    XASSERT(count >= 0);
    XASSERT((index + count) <= fSize);

    XASSERT(fArrayBlock);

    if (count > 0)
	BlockMove(ElementPtrAt(index), elemPtr, ComputeByteCount(count));

    return 0;

} // CDynamicArray::GetElementsAt


//--------------------------------------------------------------------------------
//      CDynamicArray::InsertElementsBefore
//--------------------------------------------------------------------------------
IrDAErr CDynamicArray::InsertElementsBefore(ArrayIndex index, void* elemPtr, ArrayIndex count)
{
    IrDAErr result = noErr;

    // can't insert:    before the start (index < 0)
    //                  negative count (count < 0)

    require(index >= 0, Bogus);
    require(count >= 0, Bogus);

    // put a cap on the index
    // anything past the end of the array gets inserted at the end

    if (index > fSize)
	index = fSize;

    if (count > 0)
	{
	void *indexPtr, *nextIndexPtr, *lastElementPtr;

	result = SetArraySize(fSize + count);
	nrequire(result, Fail_SetArraySize);

	check(fArrayBlock);

	indexPtr = ElementPtrAt(index);
	nextIndexPtr = ElementPtrAt(index + count);
	lastElementPtr = ElementPtrAt(fSize);

	// clear out a hole?
	if (index < fSize)
	    BlockMove(indexPtr, nextIndexPtr, (long)lastElementPtr - (long)indexPtr);

	BlockMove(elemPtr, indexPtr, ComputeByteCount(count));

	fSize += count;

	// Keep all iterators apprised
	if (fIterator)
	    fIterator->InsertElementsBefore(index, count);
	}

Fail_SetArraySize:

    return result;

Bogus:
    return kIrDAErrBadParameter;

} // CDynamicArray::InsertElementsBefore


//--------------------------------------------------------------------------------
//      CDynamicArray::ReplaceElementsAt
//--------------------------------------------------------------------------------
IrDAErr CDynamicArray::ReplaceElementsAt(ArrayIndex index, void* elemPtr, ArrayIndex count)
{
    // can't replace:   before the start (index < 0)
    //                  after the end (index >= fSize)
    //                  negative count (count < 0)
    //                  past the end (index + count > fSize)

    XASSERT(index >= 0);
    XASSERT(index < fSize);
    XASSERT(count >= 0);
    XASSERT((index + count) <= fSize);

    XASSERT(fArrayBlock);

    if (count > 0)
	BlockMove(elemPtr, ElementPtrAt(index), ComputeByteCount(count));

    return 0;

} // CDynamicArray::ReplaceElementsAt


//--------------------------------------------------------------------------------
//      CDynamicArray::Merge
//--------------------------------------------------------------------------------
IrDAErr CDynamicArray::Merge(CDynamicArray* aDynamicArray)
{
    IrDAErr result = errElementSizeMismatch;        // assume the worst

    XREQUIRE(fElementSize == aDynamicArray->fElementSize, Fail_ElementSize);

    if (aDynamicArray->fSize > 0)
	result = InsertElementsBefore(fSize, aDynamicArray->ElementPtrAt(0), aDynamicArray->fSize);
    else
	result = noErr;

Fail_ElementSize:

    return result;

} // CDynamicArray::Merge

//--------------------------------------------------------------------------------
//      CDynamicArray::SetArraySize
//--------------------------------------------------------------------------------
IrDAErr CDynamicArray::SetArraySize(ArrayIndex theSize)
{
    ArrayIndex newAllocatedSize;

    if (theSize == 0)
	{
	    return noErr;               // always leave the inital chunk allocated
	}

    else if ((theSize > fAllocatedSize) || (fAllocatedSize - theSize >= fChunkSize))
	{
	XASSERT(fChunkSize > 0);

	// Set the # of allocated elements to the nearest multiple of fChunkSize
	// Wait until after the NewPtr/ReallocPtr to set fAllocatedSize
	// in case (re)allocation fails

	if (fChunkSize)
	    newAllocatedSize = (theSize + fChunkSize) - (theSize + fChunkSize) % fChunkSize;
	else
	    newAllocatedSize = theSize;


	if (newAllocatedSize != fAllocatedSize) {
	    UInt8 * newArrayBlock;
	    int     new_bytecount, old_bytecount;
	    
	    new_bytecount = ComputeByteCount(newAllocatedSize);
	    old_bytecount = ComputeByteCount(fAllocatedSize);
	    
	    XTRACE(kLogSetSize1, (int)this >> 16, (short)this);
	    XTRACE(kLogSetSize2, theSize >> 16, (short)theSize);
	    XTRACE(kLogSetSizeCurrent, fAllocatedSize   >> 16, (short)fAllocatedSize);
	    XTRACE(kLogSetSizeNew,     newAllocatedSize >> 16, (short)newAllocatedSize);

	    newArrayBlock = (UInt8 *)IOMalloc(new_bytecount);
	    require(newArrayBlock, Fail);
	    
	    XTRACE(kLogIOAllocBuffer, (int)newArrayBlock >> 16, (short)newArrayBlock);
	    XTRACE(kLogIOAllocSize, new_bytecount >> 16, (short)new_bytecount);
	    
	    // if old buffer existed, copy it over to new buffer and free it
	    if (fArrayBlock) {
		int bytecount;          // copy min of old size and new size
		
		bytecount = Min(old_bytecount, new_bytecount);
		BlockMove(fArrayBlock, newArrayBlock, bytecount);
		
		XTRACE(kLogIOFreeBuffer, (int)fArrayBlock >> 16, (short)fArrayBlock);
		XTRACE(kLogIOFreeSize, old_bytecount >> 16, (short)old_bytecount);
		IOFree(fArrayBlock, old_bytecount);
	    }
	    
	    fArrayBlock = newArrayBlock;
	    fAllocatedSize = newAllocatedSize;
	    }
	}

    return noErr;

Fail:
    return errNoMemory;

} // CDynamicArray::SetArraySize


//--------------------------------------------------------------------------------
//      CDynamicArray::SetElementCount
//
//      like SetArraySize, but sets logical size, too
//--------------------------------------------------------------------------------
IrDAErr CDynamicArray::SetElementCount(ArrayIndex theSize)
{

    IrDAErr result = SetArraySize(theSize);
    if ( result == noErr )
	{
	fSize = theSize;
	}

    return result;

} // CDynamicArray::SetElementCount
