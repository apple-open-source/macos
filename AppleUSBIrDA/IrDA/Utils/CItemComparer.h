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
    File:       CItemComparer.h

    Contains:   Interface to the CItemComparer class


*/

#ifndef __CITEMCOMPARER_H
#define __CITEMCOMPARER_H

#include "IrDATypes.h"

enum CompareResult {
    kItemLessThanCriteria = -1,
    kItemEqualCriteria = 0,
    kItemGreaterThanCriteria = 1
};


//  NOTE
//  CItemComparer::TestItem should return
//      kItemLessThanCriteria if (fItem < criteria)
//      kItemGreaterThanCriteria for (fItem > criteria)
//      kItemEqualCriteria for (fItem == criteria)
//  this will keep the CSortedList sorted in ascending order

//  Change the value of itsForward in CArrayIterator::CArrayIterator
//  to FALSE to view a list in descending order.


//--------------------------------------------------------------------------------
//      CItemComparer
//--------------------------------------------------------------------------------
class CItemComparer : public OSObject
{
    OSDeclareDefaultStructors(CItemComparer);

public:
	    static CItemComparer * cItemComparer(const void* testItem = nil, const void* keyValue = nil);

	    Boolean         init(const void* testItem, const void* keyValue = nil);
	    
	    void            SetTestItem(const void* testItem);
	    void            SetKeyValue(const void* keyValue);

    // functions that wrap inline methods so they can be used by external callers
	    void            FXUSetTestItem(const void* testItem);
	    void            FXUSetKeyValue(const void* keyValue);

    virtual CompareResult   TestItem(const void* criteria) const;

protected:

    const void*     fItem;
    const void*     fKey;

private:

}; // CItemComparer

//--------------------------------------------------------------------------------
//      CItemComparer inlines
//--------------------------------------------------------------------------------

inline void CItemComparer::SetTestItem(const void* testItem)
    { fItem = testItem; }

inline void CItemComparer::SetKeyValue(const void* keyValue)
    { fKey = keyValue; }

#endif  /*  __CITEMCOMPARER_H   */
