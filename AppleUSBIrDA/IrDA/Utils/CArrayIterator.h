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
    File:       ArrayIterator.h

    Contains:   Interface to the CArrayIterator class


*/

#ifndef __CARRAYITERATOR_H
#define __CARRAYITERATOR_H

#include "IrDATypes.h"

enum IterateDirection { kIterateBackward = 0, kIterateForward = 1 };


class CList;
class CSortedList;
class CDynamicArray;


//--------------------------------------------------------------------------------
//      CArrayIterator
//--------------------------------------------------------------------------------
class CArrayIterator : public OSObject
{
    OSDeclareDefaultStructors(CArrayIterator);

public:

	// all four original functions retyped as factories.  probably don't
	// use more than one style.

	static  CArrayIterator* cArrayIterator();
	static  CArrayIterator* cArrayIterator(CDynamicArray* itsDynamicArray);
	static  CArrayIterator* cArrayIterator(CDynamicArray* itsDynamicArray, Boolean itsForward);
	static  CArrayIterator* cArrayIterator(CDynamicArray* itsDynamicArray,
			ArrayIndex itsLowBound, ArrayIndex itsHighBound,
			Boolean itsForward);
			
	void        free();

	bool        init(void);
	Boolean     init(CDynamicArray* itsDynamicArray);
	Boolean     init(CDynamicArray* itsDynamicArray, Boolean itsForward);
	Boolean     init(CDynamicArray* itsDynamicArray, ArrayIndex itsLowBound,
			    ArrayIndex itsHighBound, Boolean itsForward);

	void        InitBounds(ArrayIndex itsLowBound, ArrayIndex itsHighBound, Boolean itsForward);
	void        Reset(void);
	void        ResetBounds(Boolean goForward = true);

	void        SwitchArray(CDynamicArray* newArray, Boolean itsForward = kIterateForward);

	ArrayIndex  FirstIndex(void);
	ArrayIndex  NextIndex(void);
	ArrayIndex  CurrentIndex(void);

	void        RemoveElementsAt(ArrayIndex theIndex, ArrayIndex theCount);
	void        InsertElementsBefore(ArrayIndex theIndex, ArrayIndex theCount);
	void        DeleteArray(void);

	Boolean     More(void);

protected:

	void        Advance(void);

	CDynamicArray*      fDynamicArray;          // the associated dynamic array

	ArrayIndex          fCurrentIndex;          // current index of this iteration
	ArrayIndex          fLowBound;              // lower bound of iteration in progress
	ArrayIndex          fHighBound;             // upper bound of iteration in progress
	Boolean             fIterateForward;        // if iteration is forward or backward

private:

friend class CDynamicArray;
friend class CList;

	CArrayIterator*     AppendToList(CArrayIterator* toList);
	CArrayIterator*     RemoveFromList(void);

	CArrayIterator*     fPreviousLink;          // link to previous iterator
	CArrayIterator*     fNextLink;              // link to next iterator

}; // CArrayIterator


#endif  /*  __CARRAYITERATOR_H  */
