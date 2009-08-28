/*
    File:       DynamicArray.h

    Contains:   Interface to the CDynamicArray class


*/

#ifndef __CDYNAMICARRAY_H
#define __CDYNAMICARRAY_H

#include "IrDATypes.h"


//--------------------------------------------------------------------------------
//      Forward and external class declarations
//--------------------------------------------------------------------------------

class CArrayIterator;
    // CArrayIterator knows how to traverse a CDynamicArray.
    // In particular, it will bend indexes to account for on-the-fly
    // element insertion and deletion.


enum Parameters {
    kDefaultElementSize = sizeof(void *),   // was 4
    kDefaultChunkSize = sizeof(void *)	    // 4
};

//--------------------------------------------------------------------------------
//      CDynamicArray
//--------------------------------------------------------------------------------

class CDynamicArray : public OSObject
{
    OSDeclareDefaultStructors(CDynamicArray);
    
public:

	    static CDynamicArray *cDynamicArray(Size elementSize = kDefaultElementSize,
						ArrayIndex chunkSize = kDefaultChunkSize);
	    Boolean     init(Size elementSize = kDefaultElementSize, ArrayIndex chunkSize = kDefaultChunkSize);
	    void        free();
	    
    // array manipulation primitives

	    ArrayIndex  GetArraySize(void);
	    IrDAErr     SetArraySize(ArrayIndex theSize);
	    IrDAErr     SetElementCount(ArrayIndex theSize);        // like SetArraySize, but sets logical size, too

	    void*       ElementPtrAt(ArrayIndex index);
	    void*       SafeElementPtrAt(ArrayIndex index);
	    IrDAErr     GetElementsAt(ArrayIndex index, void* elemPtr, ArrayIndex count);
	    IrDAErr     InsertElementsBefore(ArrayIndex startHere, void* elemPtr, ArrayIndex count);
	    IrDAErr     ReplaceElementsAt(ArrayIndex index, void* elemPtr, ArrayIndex count);
	    IrDAErr     RemoveElementsAt(ArrayIndex index, ArrayIndex count);
	    IrDAErr     RemoveAll(void);

    // miscellaneous functions

	    Boolean     IsEmpty(void);

	    IrDAErr     Merge(CDynamicArray* aDynamicArray);

protected:

	Size            ComputeByteCount(ArrayIndex count);

	ArrayIndex      fSize;          // logical size of array

private:

	friend class CArrayIterator;

	Size            fElementSize;   // size of a single element
	ArrayIndex      fChunkSize;     // grow/shrink array by this many elements
	ArrayIndex      fAllocatedSize; // physical size of array
	void*           fArrayBlock;    // element storage
	CArrayIterator* fIterator;      // linked list of iterators active on this array

}; // CDynamicArray


//--------------------------------------------------------------------------------
//      inline functions
//--------------------------------------------------------------------------------

inline ArrayIndex CDynamicArray::GetArraySize()
    { return fSize; }

inline Boolean CDynamicArray::IsEmpty()
    { return (fSize == 0); }

inline void* CDynamicArray::ElementPtrAt(ArrayIndex index)
    { return (void*)((char *)fArrayBlock + (fElementSize * index)); }

inline Size CDynamicArray::ComputeByteCount(ArrayIndex count)
    { return (fElementSize * count); }

inline IrDAErr CDynamicArray::RemoveAll()
    { return RemoveElementsAt(0, fSize); }


#endif  /*  __CDYNAMICARRAY_H   */
