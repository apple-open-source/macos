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
