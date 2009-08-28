/*
    File:       CList.c

    Contains:   Implementation of the CList class

*/

#include "CList.h"
#include "CListIterator.h"
#include "CItemComparer.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasCListTracing > 0)

enum TraceCodes {
    kLogNew = 1,
    kLogFree,
    kLogInit
};

static
EventTraceCauseDesc TraceEvents[] = {
    {kLogNew,           "CList: new, obj="},
    {kLogFree,          "CList: free, obj="},
    {kLogInit,          "CList: init, obj="}
};
#define XTRACE(x, y, z) IrDALogAdd( x, y, (uintptr_t)z & 0xffff, TraceEvents, true )
#else
#define XTRACE(x, y, z) ((void) 0)
#endif


//--------------------------------------------------------------------------------
#define super CDynamicArray
    OSDefineMetaClassAndStructors(CList, CDynamicArray);
//--------------------------------------------------------------------------------


CList *
CList::cList(ArrayIndex chunkSize)
{
    CList *obj = new CList;
    
    XTRACE(kLogNew, 0, obj);
    
    if (obj && !obj->init(chunkSize)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

Boolean CList::init(ArrayIndex size)
{
    XTRACE(kLogInit, 0, this);
    
    return super::init(kDefaultElementSize, size);
}

void CList::free(void)
{
    XTRACE(kLogFree, 0, this);
    
    super::free();
}

//----------------------------------------------------------------------------
//      CList::At
//----------------------------------------------------------------------------
void* CList::At(ArrayIndex index)
{
    uintptr_t * itemPtr = (uintptr_t *)SafeElementPtrAt(index);
    return (itemPtr == nil) ? nil : (void*) (*itemPtr);

} // CList::At


//----------------------------------------------------------------------------
//      CList::InsertAt
//----------------------------------------------------------------------------
IrDAErr CList::InsertAt(ArrayIndex index, void* item)
{
    uintptr_t data = (uintptr_t) item;

    return InsertElementsBefore(index, &data, 1);

} // CList::InsertBefore


//----------------------------------------------------------------------------
//      CList::Remove
//----------------------------------------------------------------------------
IrDAErr CList::Remove(void* item)
{
    IrDAErr result;
    ArrayIndex index = GetIdentityIndex(item);

    if (index != kEmptyIndex)
	result = RemoveAt(index);
    else
	{
	result = errRangeCheck;
	//XPRINT(("CList::Remove: can't find item in list\n"));
	}

    return result;

} // CList::Remove


//----------------------------------------------------------------------------
//      CList::InsertUnique
//----------------------------------------------------------------------------
Boolean CList::InsertUnique(void* add)
    {
    Boolean result = !Contains(add);
    if (result)
	{
	IrDAErr ignored = InsertLast(add);
	XASSERTNOT(ignored);
	}
    return result;
    }


//----------------------------------------------------------------------------
//      CList::Replace
//----------------------------------------------------------------------------
IrDAErr CList::Replace(void* oldItem, void* newItem)
{
    IrDAErr result;
    ArrayIndex index = GetIdentityIndex(oldItem);

    if (index != kEmptyIndex)
	result = ReplaceAt(index, newItem);
    else
	{
	result = errRangeCheck;
	//XPRINT(("CList::Replace: can't find oldItem in list\n"));
	}

    return result;

} // CList::Replace


//----------------------------------------------------------------------------
//      CList::ReplaceAt
//----------------------------------------------------------------------------
IrDAErr CList::ReplaceAt(ArrayIndex index, void* newItem)
{
    uintptr_t data = (uintptr_t) newItem;

    return ReplaceElementsAt(index, &data, 1);
}

//----------------------------------------------------------------------------
//      CList::GetIdentityIndex
//----------------------------------------------------------------------------
ArrayIndex CList::GetIdentityIndex(void* item)
{
    ArrayIndex index = -1;
    CItemComparer *test =  CItemComparer::cItemComparer(item, nil);     // default comparer tests for identity

    require(test, Fail);
    
    (void) Search(test, index);

    test->release();
    
Fail:
    return index;

} // CList::GetIdentityIndex


//----------------------------------------------------------------------------
//      CList::Search
//----------------------------------------------------------------------------
void* CList::Search(CItemComparer* test, ArrayIndex& index)
/*
    Performs a linear search on the list.
    Returns the index and value of the item for which
    CItemTester::TestItem returns kItemEqualCriteria,
    or kEmptyIndex and nil respectively.
*/
{
    CListIterator *iter = CListIterator::cListIterator(this);
    index = kEmptyIndex;
    void* result = nil;
    void* item = nil;

    require(iter, Fail);
    
    for (item = iter->FirstItem(); iter->More(); item = iter->NextItem())
	if (test->TestItem(item) == kItemEqualCriteria)
	    {
	    result = item;
	    index = iter->fCurrentIndex;
	    break;
	    }

    iter->release();
    
Fail:
    return result;

} // CList::Search

