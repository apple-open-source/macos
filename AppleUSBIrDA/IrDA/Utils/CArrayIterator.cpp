/*
    File:       CArrayIterator.cpp

    Contains:   Implementation of the CArrayIterator class


*/

#include "CDynamicArray.h"
#include "CArrayIterator.h"

//--------------------------------------------------------------------------------
#define super OSObject
    OSDefineMetaClassAndStructors(CArrayIterator, OSObject);
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
//      CArrayIterator::SwitchArray
//--------------------------------------------------------------------------------
void CArrayIterator::SwitchArray(CDynamicArray* newArray, Boolean itsForward)
{
    // switch from one array to another
    XASSERT(newArray);

    if (fDynamicArray)
	{
	fDynamicArray->fIterator = RemoveFromList();
	fDynamicArray = nil;
	}

    init(newArray, 0, newArray->fSize - 1, itsForward);

} // CArrayIterator::SwitchArray


//--------------------------------------------------------------------------------
//      CArrayIterator::AppendToList
//--------------------------------------------------------------------------------
CArrayIterator* CArrayIterator::AppendToList(CArrayIterator* toList)
{
    if (toList)
	{
	fNextLink = toList->fNextLink;
	fPreviousLink = toList;

	fNextLink->fPreviousLink = this;
	toList->fNextLink = this;
	}
    return this;

} // CArrayIterator::AppendToList


//--------------------------------------------------------------------------------
//      CArrayIterator::RemoveFromList
//--------------------------------------------------------------------------------
CArrayIterator* CArrayIterator::RemoveFromList()
{
    CArrayIterator * returnLink;

    if (fNextLink == this)
	returnLink = nil;
    else
	returnLink = fNextLink;

    fNextLink->fPreviousLink = fPreviousLink;
    fPreviousLink->fNextLink = fNextLink;

    fNextLink = this;
    fPreviousLink = this;

    return returnLink;

} // CArrayIterator::RemoveFromList


//--------------------------------------------------------------------------------
//      CArrayIterator::CArrayIterator(void)
//--------------------------------------------------------------------------------
CArrayIterator *
CArrayIterator::cArrayIterator()
{
    CArrayIterator *obj = new CArrayIterator;
    
    if (obj && !obj->init()) {
	obj->release();
	obj = nil;
    }
    return obj;

} // CArrayIterator::CArrayIterator

//--------------------------------------------------------------------------------
//      CArrayIterator::cArrayIterator(1)
//--------------------------------------------------------------------------------
CArrayIterator *
CArrayIterator::cArrayIterator(CDynamicArray* itsDynamicArray)
{
    CArrayIterator *obj = new CArrayIterator;

    XASSERT(itsDynamicArray);
    // rely on Init to sanity check array bounds
    
    if (obj && !obj->init(itsDynamicArray)) {
	obj->release();
	obj = nil;
    }
    return obj;

} // CArrayIterator::CArrayIterator

//--------------------------------------------------------------------------------
//      CArrayIterator::cArrayIterator(2)
//--------------------------------------------------------------------------------
CArrayIterator *
CArrayIterator::cArrayIterator(CDynamicArray* itsDynamicArray, Boolean itsForward)
{
    CArrayIterator *obj = new CArrayIterator;

    XASSERT(itsDynamicArray);
    // rely on Init to sanity check array bounds
    
    if (obj && !obj->init(itsDynamicArray, itsForward)) {
	obj->release();
	obj = nil;
    }
    return obj;

} // CArrayIterator::CArrayIterator



//--------------------------------------------------------------------------------
//      CArrayIterator::CArrayIterator(4)
//--------------------------------------------------------------------------------
CArrayIterator *
CArrayIterator::cArrayIterator(CDynamicArray* itsDynamicArray, ArrayIndex itsLowBound,
    ArrayIndex itsHighBound, Boolean itsForward)
{
    CArrayIterator *obj = new CArrayIterator;

    XASSERT(itsDynamicArray);
    // rely on Init to sanity check array bounds
    
    if (obj && !obj->init(itsDynamicArray, itsLowBound, itsHighBound, itsForward)) {
	obj->release();
	obj = nil;
    }
    return obj;

} // CArrayIterator::CArrayIterator




//--------------------------------------------------------------------------------
//      CArrayIterator::free
//--------------------------------------------------------------------------------
void
CArrayIterator::free()
{
    if (fDynamicArray) {
	fDynamicArray->fIterator = RemoveFromList();
	fDynamicArray = nil;
    }
	
    super::free();

} // CArrayIterator::free


//--------------------------------------------------------------------------------
//      CArrayIterator::init(void)
//--------------------------------------------------------------------------------
bool
CArrayIterator::init()
{
    if (!super::init()) return false;

    fNextLink = this;
    fPreviousLink = this;
    fHighBound = kEmptyIndex;
    fLowBound = kEmptyIndex;
    fCurrentIndex = kEmptyIndex;
    fIterateForward = kIterateForward;
    fDynamicArray = nil;
    
    return true;

} // CArrayIterator::init

//--------------------------------------------------------------------------------
//      CArrayIterator::init(1)
//--------------------------------------------------------------------------------
Boolean
CArrayIterator::init(CDynamicArray* itsDynamicArray)
{
    return init(itsDynamicArray, 0, itsDynamicArray->fSize - 1, kIterateForward);
}

//--------------------------------------------------------------------------------
//      CArrayIterator::init(2)
//--------------------------------------------------------------------------------
Boolean
CArrayIterator::init(CDynamicArray* itsDynamicArray, Boolean itsForward)
{
    return init(itsDynamicArray, 0, itsDynamicArray->fSize - 1, itsForward);
}

//--------------------------------------------------------------------------------
//      CArrayIterator::init(4)
//--------------------------------------------------------------------------------
Boolean
CArrayIterator::init(CDynamicArray* itsDynamicArray, ArrayIndex itsLowBound,
    ArrayIndex itsHighBound, Boolean itsForward)
{

    if (!super::init()) return false;
    
    require(itsDynamicArray, Fail);

    fNextLink = this;
    fPreviousLink = this;
    fDynamicArray = itsDynamicArray;

    // link me in to the list of iterations in progress
    fDynamicArray->fIterator = AppendToList(fDynamicArray->fIterator);

    // sanity check the bounds
    InitBounds(itsLowBound, itsHighBound, itsForward);
    
    return true;
    
Fail:
    return false;

} // CArrayIterator::init


//--------------------------------------------------------------------------------
//      CArrayIterator::InitBounds
//--------------------------------------------------------------------------------
void CArrayIterator::InitBounds(ArrayIndex itsLowBound, ArrayIndex itsHighBound,
    Boolean itsForward)
{
    fHighBound = (fDynamicArray->fSize > 0) ? MinMax(0, itsHighBound, fDynamicArray->fSize - 1) : kEmptyIndex;
    fLowBound = (fHighBound > kEmptyIndex) ? MinMax(0, itsLowBound, fHighBound) : kEmptyIndex;

    fIterateForward = itsForward;

    Reset();

} // CArrayIterator::Init


//--------------------------------------------------------------------------------
//      CArrayIterator::ResetBounds
//--------------------------------------------------------------------------------
void CArrayIterator::ResetBounds(Boolean goForward)
{
    fHighBound = (fDynamicArray->fSize > 0) ? fDynamicArray->fSize - 1 : kEmptyIndex;
    fLowBound = (fHighBound > kEmptyIndex) ? 0 : kEmptyIndex;

    fIterateForward = goForward;

    Reset();

} // CArrayIterator::Init


//--------------------------------------------------------------------------------
//      CArrayIterator::More
//--------------------------------------------------------------------------------
Boolean CArrayIterator::More()
{
    return (fDynamicArray != nil) ? (fCurrentIndex != kEmptyIndex) : false;

} // CArrayIterator::More


//--------------------------------------------------------------------------------
//      CArrayIterator::Reset
//--------------------------------------------------------------------------------
void CArrayIterator::Reset()
{
    fCurrentIndex = (fIterateForward) ? fLowBound : fHighBound;

} // CArrayIterator::Reset


//--------------------------------------------------------------------------------
//      CArrayIterator::DeleteArray
//--------------------------------------------------------------------------------
void CArrayIterator::DeleteArray()
{
    // inform everyone else in the list that the array is gone
    if (fNextLink != fDynamicArray->fIterator)
	fNextLink->DeleteArray();

    // we no longer have an array
    fDynamicArray = nil;

} // CArrayIterator::~CArrayIterator


//--------------------------------------------------------------------------------
//      CArrayIterator::Advance
//--------------------------------------------------------------------------------
void CArrayIterator::Advance()
{
    if (fIterateForward)
	{
	if (fCurrentIndex < fHighBound)
	    ++fCurrentIndex;
	else
	    fCurrentIndex = kEmptyIndex;
	}
    else
	{
	if (fCurrentIndex > fLowBound)
	    --fCurrentIndex;
	else
	    fCurrentIndex = kEmptyIndex;
	}

} // CArrayIterator::Advance


//--------------------------------------------------------------------------------
//      CArrayIterator::RemoveElementsAt
//--------------------------------------------------------------------------------
void CArrayIterator::RemoveElementsAt(ArrayIndex theIndex, ArrayIndex theCount)
{
    // tuck the endpoints of the iteration in to match
    if (theIndex < fLowBound)
	fLowBound -= theCount;

    if (theIndex <= fHighBound)
	fHighBound -= theCount;

    if (fIterateForward)
	{
	// If the removed element was !in the range yet to be iterated
	// then bend the fCurrentIndex to account for it.
	if (theIndex <= fCurrentIndex)
	    fCurrentIndex -= theCount;
	}
    else
	{
	// Iterating backwards
	// If the removed element was IN the range yet to be iterated
	// then bend the fCurrentIndex to account for it.
	if (theIndex < fCurrentIndex)
	    fCurrentIndex -= theCount;
	}

    // hand off control to the next link until you hit the last
    // link in the circular chain
    if (fDynamicArray && fNextLink != fDynamicArray->fIterator)
	fNextLink->RemoveElementsAt(theIndex, theCount);

} // CArrayIterator::RemoveElementsAt


//--------------------------------------------------------------------------------
//      CArrayIterator::InsertElementsBefore
//--------------------------------------------------------------------------------
void CArrayIterator::InsertElementsBefore(ArrayIndex theIndex, ArrayIndex theCount)
{
    // bump the endpoints of this iteration out to match
    if (theIndex <= fLowBound)
	fLowBound += theCount;

    if (theIndex <= fHighBound)
	fHighBound += theCount;

    if (fIterateForward)
	{
	// If the inserted element was !in the range yet to be
	// iterated then bend the fCurrentIndex to account for it.
	if (theIndex <= fCurrentIndex)
	    fCurrentIndex += theCount;
	}
    else
	{
	// Iterating backward
	// If the inserted element was IN the range yet to be
	// iterated then bend the fCurrentIndex to account for it.
	if (theIndex < fCurrentIndex)
	    fCurrentIndex += theCount;
	}

    // hand off control to the next link until you hit the last
    // link in the circular chain
    if (fDynamicArray && fNextLink != fDynamicArray->fIterator)
	fNextLink->InsertElementsBefore(theIndex, theCount);

} // CArrayIterator::InsertElementsBefore


//--------------------------------------------------------------------------------
//      CArrayIterator::CurrentIndex
//--------------------------------------------------------------------------------
ArrayIndex CArrayIterator::CurrentIndex()
{
    return (fDynamicArray != nil) ? fCurrentIndex : kEmptyIndex;

} // CArrayIterator::CurrentIndex


//--------------------------------------------------------------------------------
//      CArrayIterator::FirstIndex
//--------------------------------------------------------------------------------
ArrayIndex CArrayIterator::FirstIndex()
{
    Reset();

    return More() ? fCurrentIndex : kEmptyIndex;

} // CArrayIterator::FirstIndex


//--------------------------------------------------------------------------------
//      CArrayIterator::NextIndex
//--------------------------------------------------------------------------------
ArrayIndex CArrayIterator::NextIndex()
{
    Advance();

    return More() ? fCurrentIndex : kEmptyIndex;

} // CArrayIterator::NextIndex


