/*
    File:       CListIterator.h

    Contains:   Interface to the CListIterator class

*/

#ifndef __CLISTITERATOR_H
#define __CLISTITERATOR_H

#include "CArrayIterator.h"


//--------------------------------------------------------------------------------
//      CListIterator
//--------------------------------------------------------------------------------
class CListIterator : public CArrayIterator
{
	OSDeclareDefaultStructors(CListIterator);

public:

    static CListIterator * cListIterator();
    static CListIterator * cListIterator(CDynamicArray* itsList);
    static CListIterator * cListIterator(CDynamicArray* itsList, Boolean itsForward);
    static CListIterator * cListIterator(CDynamicArray* itsList, ArrayIndex itsLowBound,
					    ArrayIndex itsHighBound, Boolean itsForward);
					    

    void*       CurrentItem(void);
    void*       FirstItem(void);
    void*       NextItem(void);

}; // CListIterator


#endif  /*  __CLISTITERATOR_H   */
