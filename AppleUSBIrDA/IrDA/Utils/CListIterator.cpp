/*
    File:       CListIterator.c

    Contains:   Implementation of the CListIterator class

*/

#include "CListIterator.h"
#include <CList.h>

//--------------------------------------------------------------------------------
#define super CArrayIterator
    OSDefineMetaClassAndStructors(CListIterator, CArrayIterator);
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
//      CListIterator::cListIterator
//--------------------------------------------------------------------------------
CListIterator *
CListIterator::cListIterator()
{
    CListIterator *obj;
    obj = new CListIterator();
    if (obj && !obj->init()) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      CListIterator::cListIterator
//--------------------------------------------------------------------------------
CListIterator *
CListIterator::cListIterator(CDynamicArray* itsList)
{
    CListIterator *obj;
    obj = new CListIterator();
    if (obj && !obj->init(itsList)) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      CListIterator::cListIterator
//--------------------------------------------------------------------------------
CListIterator *
CListIterator::cListIterator(CDynamicArray* itsList, Boolean itsForward)
{
    CListIterator *obj;
    obj = new CListIterator();
    if (obj && !obj->init(itsList, itsForward)) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      CListIterator::CListIterator
//--------------------------------------------------------------------------------
CListIterator *
CListIterator::cListIterator(CDynamicArray* itsList, ArrayIndex itsLowBound,
    ArrayIndex itsHighBound, Boolean itsForward)
{
    CListIterator *obj;
    obj = new CListIterator();
    if (obj && !obj->init(itsList, itsLowBound, itsHighBound, itsForward)) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      CListIterator::CurrentItem
//--------------------------------------------------------------------------------
void* CListIterator::CurrentItem()
{
    if (fDynamicArray)
	return ((CList*)fDynamicArray)->At(fCurrentIndex);
    else
	return nil;

} // CListIterator::CurrentItem


//--------------------------------------------------------------------------------
//      CListIterator::FirstItem
//--------------------------------------------------------------------------------
void* CListIterator::FirstItem()
{
    this->Reset();

    if (this->More())
	return ((CList*)fDynamicArray)->At(fCurrentIndex);
    else
	return nil;

} // CListIterator::FirstItem


//--------------------------------------------------------------------------------
//      CListIterator::NextItem
//--------------------------------------------------------------------------------
void* CListIterator::NextItem()
{
    this->Advance();

    if (this->More())
	return ((CList*)fDynamicArray)->At(fCurrentIndex);
    else
	return nil;

} // CListIterator::NextItem


