/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
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


