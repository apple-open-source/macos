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


//
// cssmlist - CSSM_LIST operational utilities
//
#ifndef _H_CSSMLIST
#define _H_CSSMLIST

#include <Security/utilities.h>
#include <Security/cssmalloc.h>
#include <Security/walkers.h>

#ifdef _CPP_CSSMLIST
#pragma export on
#endif

namespace Security
{

class CssmList;
class TypedList;


//
// A POD Wrapper for CSSM_LIST_ELEMENTs.
// List elements are pseudo-polymorphic, so we provide ways to get and
// set their three personalities. It's up to the caller to get this right;
// you mustn't (for example) call the data() method on a list element that
// is not of (element) type CSSM_LIST_ELEMENT_DATUM. To violate this rule
// will get you an assertion (not exception).
//
class ListElement : public PodWrapper<ListElement, CSSM_LIST_ELEMENT> {
public:
    // list element chaining
	CSSM_LIST_ELEMENT_TYPE type() const { return ElementType; }
	ListElement * &next() { return ListElement::overlayVar(NextElement); }
	ListElement *next() const { return ListElement::overlay(NextElement); }
	ListElement *last();
	
    // CssmData personality
	ListElement(const CssmData &data);
    ListElement(CssmAllocator &alloc, string stringData);
	CssmData &data();
	const CssmData &data() const;
	ListElement &operator = (const CssmData &data);
	operator CssmData &() { return data(); }
	operator string () const { return data(); }
	bool operator == (const CssmData &other) const	{ return data() == other; }
	bool operator != (const CssmData &other) const	{ return data() != other; }

    template <class T>
    void extract(T &destination, CSSM_RETURN error = CSSM_ERRCODE_INVALID_DATA)
    { data().extract(destination, error); }
	
    // CssmList (sublist) personality
	ListElement(const CssmList &list);
	CssmList &list();
	const CssmList &list() const;
	ListElement &operator = (const CssmList &list);
	operator CssmList &() { return list(); }
	operator const CssmList &() const { return list(); }
    operator TypedList &();
    operator const TypedList &() const;
	
    // WORDID (number) personality
	ListElement(CSSM_WORDID_TYPE word);
	CSSM_WORDID_TYPE word() const;
	ListElement &operator = (CSSM_WORDID_TYPE word);
    operator CSSM_WORDID_TYPE () const { return word(); }
	bool operator == (CSSM_WORDID_TYPE other) const	{ return word() == other; }
	bool operator != (CSSM_WORDID_TYPE other) const	{ return word() != other; }
	
public:
	void *operator new (size_t size, CssmAllocator &alloc)
	{ return alloc.malloc(size); }
	
	void clear(CssmAllocator &alloc);		// free my contents
};

} // end namespace Security

// specialize destroy() to call clear() for cleanup
inline void destroy(ListElement *elem, CssmAllocator &alloc)
{
	elem->clear(alloc);
	alloc.free(elem);
}

namespace Security
{

//
// A POD Wrapper for CSSM_LIST.
// CssmList does no memory allocations. Retrieval functions return pointers or
// references into existing content, and modifiers modify in-place without any
// attempt to release previous dynamic content. May the Leaking God be with You.
//
class CssmList : public PodWrapper<CssmList, CSSM_LIST> {
public:
	CssmList() { ListType = CSSM_LIST_TYPE_UNKNOWN; Head = Tail = NULL; }
	CssmList(const CssmList &list)	{ *(CssmList *)this = list; }

public:
	CSSM_LIST_TYPE kind() const { return ListType; }	// type() reserved for TypedList
	
	ListElement &operator [] (unsigned ix) const;
	unsigned int length() const;
	ListElement * &first() { return ListElement::overlayVar(Head); }
	ListElement *first() const { return ListElement::overlay(Head); }
	ListElement *last() const { return ListElement::overlay(Tail); }
	bool empty() const { return first() == NULL; }

	CssmList &append(ListElement *elem);
	CssmList &insert(ListElement *elem, ListElement *before);
	CssmList &remove(ListElement *elem);
	CssmList &operator += (ListElement *elem) { return append(elem); }
	CssmList &operator -= (ListElement *elem) { return remove(elem); }
    
    // logically remove the first element (skip it)
    void snip();
	
public:
	void clear(CssmAllocator &alloc);		// free my contents
};

} // end namespace Security

inline void destroy(CssmList *list, CssmAllocator &alloc)
{
	list->clear(alloc);
	alloc.free(list);
}

namespace Security
{

//
// Enhanced overlay for CssmLists whose first element is known to be a wordid.
//
class TypedList : public CssmList {
public:
    TypedList(const CSSM_LIST &list) { *(CSSM_LIST *)this = list; }
	TypedList(CssmAllocator &alloc, CSSM_WORDID_TYPE type);
	TypedList(CssmAllocator &alloc, CSSM_WORDID_TYPE type, ListElement *elem1);
	TypedList(CssmAllocator &alloc, CSSM_WORDID_TYPE type, ListElement *elem1, ListElement *elem2);
	
	bool isProper() const;	// format check (does not throw)
	static TypedList &overlay(CSSM_LIST &list)
	{ return static_cast<TypedList &>(list); }
	static const TypedList &overlay(const CSSM_LIST &list)
	{ return static_cast<const TypedList &>(list); }
	
	CSSM_WORDID_TYPE type() const
	{ assert(isProper()); return first()->word(); }
};

inline ListElement::operator TypedList &()
{ return TypedList::overlay(operator CssmList &()); }

inline ListElement::operator const TypedList &() const
{ return TypedList::overlay(operator const CssmList &()); }


//
// Data walkers to parse list elements and lists.
// @@@ Walking lists by recursing over next() is stack intensive. Do this in CssmList walker by loop?
//
namespace DataWalkers
{

template <class Action>
ListElement *walk(Action &operate, ListElement * &elem)
{
	operate(elem);
	switch (elem->type()) {
	case CSSM_LIST_ELEMENT_DATUM:
		walk(operate, elem->data());
		break;
	case CSSM_LIST_ELEMENT_SUBLIST:
		walk(operate, elem->list());
		break;
	case CSSM_LIST_ELEMENT_WORDID:
		break;
	default:
		assert(false);
	}
	if (elem->next())		
		walk(operate, elem->next());
	return elem;
}

template <class Action>
ListElement *walk(Action &operate, CSSM_LIST_ELEMENT * &elem)
{ walk(operate, ListElement::overlay(elem)); }

template <class Action>
void walk(Action &operate, CssmList &list)
{
	if (!list.empty()) {
		walk(operate, list.first());
		if (operate.needsRelinking)
			list.Tail = list.first()->last();	// re-establish "tail" link
	}
}

template <class Action>
void walk(Action &operate, CSSM_LIST &list)
{ walk(operate, CssmList::overlay(list)); }

template <class Action>
void walk(Action &operate, const CSSM_LIST &list)
{ walk(operate, const_cast<CSSM_LIST &>(list)); }

template <class Action>
void walk(Action &operate, const CssmList &list)
{ walk(operate, const_cast<CssmList &>(list)); }


template <class Action>
CSSM_LIST *walk(Action &operate, CSSM_LIST * &list)
{
	operate(list);
	walk(operate, *list);
	return list;
}

} // end namespace DataWalkers

}; 	// end namespace Security

#ifdef _CPP_CSSMLIST
#pragma export off
#endif

#endif //_H_CSSMLIST
