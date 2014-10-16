/*
 * Copyright (c) 2000-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
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
 *  AuthorizationWalkers.h
 *  SecurityCore
 */

#if !defined(__AuthorizationWalkers__)
#define __AuthorizationWalkers__ 1

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>
#include <security_cdsa_utilities/walkers.h>
#include <security_cdsa_utilities/cssmwalkers.h> // char * walker

namespace Security {
namespace DataWalkers {


template <class Action>
void walk(Action &operate, AuthorizationItem &item)
{
	operate(item);
	walk(operate, const_cast<char *&>(item.name));
	operate.blob(item.value, item.valueLength);
	// Ignore reserved
}

template <class Action>
AuthorizationItemSet *walk(Action &operate, AuthorizationItemSet * &itemSet)
{
	operate(itemSet);
	operate.blob(itemSet->items, itemSet->count * sizeof(itemSet->items[0]));
	for (uint32 n = 0; n < itemSet->count; n++)
		walk(operate, itemSet->items[n]);
	return itemSet;
}

template <class Action>
void walk(Action &operate, AuthorizationValue &authvalue)
{
    operate.blob(authvalue.data, authvalue.length);
}

template <class Action>
AuthorizationValueVector *walk(Action &operate, AuthorizationValueVector * &valueVector)
{
    operate(valueVector);
    operate.blob(valueVector->values, valueVector->count * sizeof(valueVector->values[0]));
    for (uint32 n = 0; n < valueVector->count; n++)
        walk(operate, valueVector->values[n]);
    return valueVector;
}



} // end namespace DataWalkers
} // end namespace Security

#endif /* ! __AuthorizationWalkers__ */
