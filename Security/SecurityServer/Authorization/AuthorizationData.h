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
 *  AuthorizationData.h
 *  Authorization
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */

#ifndef _H_AUTHORIZATIONDATA
#define _H_AUTHORIZATIONDATA  1

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>

// ptrdiff_t needed, so including STL type closest
#include <vector>

namespace Authorization
{


class MutableRightSet;
class RightSet;

class Right : protected AuthorizationItem
{
	friend class MutableRightSet;
	friend class RightSet;
public:
	static Right &overlay(AuthorizationItem &item);
	static Right *overlay(AuthorizationItem *item);
	Right();
	Right(AuthorizationString name, size_t valueLength, const void *value);
	~Right();

	bool operator < (const Right &other) const;
	AuthorizationString rightName() const { return name; }
	size_t argumentLength() const { return valueLength; }
	const void *argument() const { return value; }
};


/* A RightSet is a Container and a Back Insertion Sequence, but it is not a Sequence.  Also it only
   implements the const members of Container and Back Insertion Sequence. */
class RightSet
{
	friend class MutableRightSet;
public:
	// Container required memebers
	typedef Right value_type;
	typedef const Right &const_reference;
	typedef const Right *const_pointer;
	typedef const_pointer const_iterator;
	typedef ptrdiff_t difference_type;
	typedef size_t size_type;

	RightSet(const AuthorizationRights *rights = NULL);
	RightSet(const RightSet &other);
	~RightSet();

	size_type size() const { return mRights->count; }
	size_type max_size() const { return INT_MAX; }
	const_iterator begin() const { return static_cast<const_pointer>(mRights->items); }
	const_iterator end() const { return static_cast<const_pointer>(&mRights->items[mRights->count]); }
	bool empty() const { return size() == 0; }

	// Back Insertion Sequence required memebers
	const_reference back() const;

	// Other convenience members
	operator const AuthorizationRights *() const { return mRights; }
private:
	RightSet &operator = (const RightSet &other);

protected:
	static const AuthorizationRights gEmptyRights;
	AuthorizationRights *mRights;
};


/* A MutableRightSet is a Container and a Back Insertion Sequence, but it is not a Sequence. */
class MutableRightSet : public RightSet
{
public:
	// Container required memebers
	typedef Right &reference;
	typedef Right *pointer;
	typedef pointer iterator;

	MutableRightSet(size_t count = 0, const Right &element = Right());
	MutableRightSet(const RightSet &other);
	~MutableRightSet();

	MutableRightSet &operator = (const RightSet &other);

	iterator begin() { return static_cast<pointer>(mRights->items); }
	iterator end() { return static_cast<pointer>(&mRights->items[mRights->count]); }
	void swap(MutableRightSet &other);

	// Back Insertion Sequence required memebers
	reference back();
	void push_back(const_reference right);
	void pop_back();

	// Other convenience members
	size_type capacity() const { return mCapacity; }
private:
	void grow(size_type min_capacity);

	size_type mCapacity;
};

typedef RightSet AuthItemSet;
typedef MutableRightSet MutableAuthItemSet;

class FindAuthItemByRightName
{
public:
    FindAuthItemByRightName(const char *find_name) : name(find_name) { }

    bool operator()( const Right& right )
    {
        return (!strcmp(name, right.rightName()));
    }
    bool operator()( const AuthorizationItem* item )
    {
        return (!strcmp(name, item->name));
    }
    
private:
    const char *name;
};


}; // namespace Authorization

#endif /* ! _H_AUTHORIZATIONDATA */
