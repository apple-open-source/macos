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
 *  AuthorizationData.cpp
 *  Authorization
 *
 *  Created by Michael Brouwer on Thu Oct 12 2000.
 *  Copyright (c) 2000 Apple Computer Inc. All rights reserved.
 *
 */

#include "AuthorizationData.h"


namespace Authorization {


//
// Right class
//
Right &
Right::overlay(AuthorizationItem &item)
{
	return static_cast<Right &>(item);
}

Right *
Right::overlay(AuthorizationItem *item)
{
	return static_cast<Right *>(item);
}

Right::Right()
{
	name = "";
	valueLength = 0;
	value = NULL;
	flags = 0;
}

Right::Right(AuthorizationString inName, size_t inValueLength, const void *inValue)
{
	name = inName;
	valueLength = inValueLength;
	value = const_cast<void *>(inValue);
}

Right::~Right()
{
}

bool
Right::operator < (const Right &other) const
{
	return strcmp(name, other.name) < 0;
}


//
// RightSet class
//
const AuthorizationRights RightSet::gEmptyRights = { 0, NULL };

RightSet::RightSet(const AuthorizationRights *rights) :
mRights(const_cast<AuthorizationRights *>(rights ? rights : &gEmptyRights))
{
}

RightSet::RightSet(const RightSet &other)
{
	mRights = other.mRights;
}

RightSet::~RightSet()
{
}

RightSet::const_reference
RightSet::back() const
{
	// @@@ Should this if empty::throwMe()?
	return static_cast<const_reference>(mRights->items[size() - 1]);
}


//
// MutableRightSet class
//
MutableRightSet::MutableRightSet(size_t count, const Right &element) :
mCapacity(count)
{
	mRights = new AuthorizationRights();
	mRights->items = reinterpret_cast<pointer>(malloc(sizeof(Right) * mCapacity));
	if (!mRights->items)
	{
		delete mRights;
		throw std::bad_alloc();
	}

	mRights->count = count;
	for (size_type ix = 0; ix < count; ++ix)
		mRights->items[ix] = element;
}

MutableRightSet::MutableRightSet(const RightSet &other)
{
	size_type count = other.size();
	mCapacity = count;
	mRights = new AuthorizationRights();

	mRights->items = reinterpret_cast<pointer>(malloc(sizeof(Right) * mCapacity));
	if (!mRights->items)
	{
		delete mRights;
		throw std::bad_alloc();
	}

	mRights->count = count;
	for (size_type ix = 0; ix < count; ++ix)
		mRights->items[ix] = other.mRights->items[ix];
}

MutableRightSet::~MutableRightSet()
{
	free(mRights->items);
	delete mRights;
}

MutableRightSet &
MutableRightSet::operator = (const RightSet &other)
{
	size_type count = other.size();
	if (capacity() < count)
		grow(count);

	mRights->count = count;
	for (size_type ix = 0; ix < count; ++ix)
		mRights->items[ix] = other.mRights->items[ix];

	return *this;
}

void
MutableRightSet::swap(MutableRightSet &other)
{
	AuthorizationRights *rights = mRights;
	size_t capacity = mCapacity;
	mRights = other.mRights;
	mCapacity = other.mCapacity;
	other.mRights = rights;
	other.mCapacity = capacity;
}

MutableRightSet::reference
MutableRightSet::back()
{
	// @@@ Should this if empty::throwMe()?
	return static_cast<reference>(mRights->items[size() - 1]);
}

void
MutableRightSet::push_back(const_reference right)
{
	if (size() >= capacity())
		grow(capacity() + 1);

	mRights->items[mRights->count] = right;
	mRights->count++;
}

void
MutableRightSet::pop_back()
{
	// @@@ Should this if empty::throwMe()?
	if (!empty())
		mRights->count--;
}

void
MutableRightSet::grow(size_type min_capacity)
{
	size_type newCapacity = mCapacity * mCapacity;
	if (newCapacity < min_capacity)
		newCapacity = min_capacity;

	void *newItems = realloc(mRights->items, sizeof(*mRights->items) * newCapacity);
	if (!newItems)
		throw std::bad_alloc();

	mRights->items = reinterpret_cast<pointer>(newItems);
	mCapacity = newCapacity;
}


}	// end namespace Authorization
