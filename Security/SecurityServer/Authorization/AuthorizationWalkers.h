/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 *  AuthorizationWalkers.h
 *  SecurityCore
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */

#if !defined(__AuthorizationWalkers__)
#define __AuthorizationWalkers__ 1

#include <Security/Authorization.h>
#include <Security/walkers.h>

namespace Security
{

namespace DataWalkers
{

template <class Action>
void walk(Action &operate, AuthorizationItem &item)
{
	walk(operate, item.name);
	operate(item.value, item.valueLength);
	// Ignore reserved
}

template <class Action>
AuthorizationItemSet *walk(Action &operate, AuthorizationItemSet * &itemSet)
{
	operate(itemSet);
	operate(itemSet->items, itemSet->count * sizeof(AuthorizationItem));
	for (uint32 n = 0; n < itemSet->count; n++)
		walk(operate, itemSet->items[n]);
	return itemSet;
}

} // end namespace DataWalkers

} // end namespace Security

#endif /* ! __AuthorizationWalkers__ */
