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
    File:       CItemComparer.cpp

    Contains:   Implementation of the CItemComparer class

*/

#include "CItemComparer.h"

//--------------------------------------------------------------------------------
#define super OSObject
    OSDefineMetaClassAndStructors(CItemComparer, OSObject);
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
//      CItemComparer::cItemComparer
//--------------------------------------------------------------------------------
CItemComparer *
CItemComparer::cItemComparer(const void* testItem, const void* keyValue)
{
    CItemComparer *obj = new CItemComparer;
    if (obj && !obj->init(testItem, keyValue)) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      CItemComparer::init
//--------------------------------------------------------------------------------
Boolean
CItemComparer::init(const void* testItem, const void* keyValue)
{
    if (!super::init()) return false;
    fItem = testItem;
    fKey = keyValue;
    return true;
}


//--------------------------------------------------------------------------------
//      CItemComparer::TestItem
//--------------------------------------------------------------------------------
CompareResult CItemComparer::TestItem(const void* criteria) const
{
    XASSERT(fItem);
    XASSERT(criteria);

    if (fItem < criteria)
	return kItemLessThanCriteria;

    else if (fItem > criteria)
	return kItemGreaterThanCriteria;

    else
	return kItemEqualCriteria;
}


//--------------------------------------------------------------------------------
// functions that wrap inline methods so they can be used by external callers
// FXU - For External Use
//--------------------------------------------------------------------------------
void CItemComparer::FXUSetTestItem(const void* testItem)
{
    SetTestItem(testItem);
}

void CItemComparer::FXUSetKeyValue(const void* keyValue)
{
    SetKeyValue(keyValue);
}


