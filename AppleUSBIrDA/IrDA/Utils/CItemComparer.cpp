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


