/*
 * Copyright (c) 2000-2006,2011,2014 Apple Inc. All Rights Reserved.
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


//
// cssmlist - CSSM_LIST operational utilities
//
#ifndef _H_CSSMLIST
#define _H_CSSMLIST

#include <security_utilities/utilities.h>
#include <security_utilities/debugging.h>
#include <security_cdsa_utilities/cssmalloc.h>
#include <security_cdsa_utilities/cssmwalkers.h>


namespace Security {

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
	// type control
	CSSM_LIST_ELEMENT_TYPE type() const { return ElementType; }
	bool is(CSSM_LIST_ELEMENT_TYPE t) const { return type() == t; }
	
    // list element chaining
	ListElement * &next() { return ListElement::overlayVar(NextElement); }
	ListElement *next() const { return ListElement::overlay(NextElement); }
	ListElement *last();
	
    // CssmData personality
	explicit ListElement(const CssmData &data);
	explicit ListElement(Allocator &alloc, const CssmData &data);
	explicit ListElement(Allocator &alloc, const std::string &stringData);

	CssmData &data();
	string toString() const	{ return data().toString(); }
	const CssmData &data() const;
	ListElement &operator = (const CssmData &data);
	operator CssmData &() { return data(); }
	operator std::string () const { return toString(); }
	bool operator == (const CssmData &other) const	{ return data() == other; }
	bool operator != (const CssmData &other) const	{ return data() != other; }

    template <class T>
    void extract(T &destination, CSSM_RETURN error = CSSM_ERRCODE_INVALID_DATA)
    { data().extract(destination, error); }
	
    // CssmList (sublist) personality
	explicit ListElement(const CssmList &list);
	CssmList &list();
	const CssmList &list() const;
	TypedList &typedList();
	const TypedList &typedList() const;
	ListElement &operator = (const CssmList &list);
	operator CssmList &() { return list(); }
    operator TypedList &();
	
    // WORDID (number) personality
	explicit ListElement(CSSM_WORDID_TYPE word);
	CSSM_WORDID_TYPE word() const;
	ListElement &operator = (CSSM_WORDID_TYPE word);
    operator CSSM_WORDID_TYPE () const { return word(); }
	
public:
	void *operator new (size_t size, Allocator &alloc)
	{ return alloc.malloc(size); }
	
	void clear(Allocator &alloc);		// free my contents
};

} // end namespace Security

// specialize destroy() to call clear() for cleanup
inline void destroy(ListElement *elem, Allocator &alloc)
{
	elem->clear(alloc);
	alloc.free(elem);
}

namespace Security {


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
	void clear(Allocator &alloc);		// free my contents
};

} // end namespace Security

inline void destroy(CssmList *list, Allocator &alloc)
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
    explicit TypedList(const CSSM_LIST &list) { *(CSSM_LIST *)this = list; }
	TypedList(Allocator &alloc, CSSM_WORDID_TYPE type);
	TypedList(Allocator &alloc, CSSM_WORDID_TYPE type, ListElement *elem1);
	TypedList(Allocator &alloc, CSSM_WORDID_TYPE type, ListElement *elem1,
		ListElement *elem2);
	TypedList(Allocator &alloc, CSSM_WORDID_TYPE type, ListElement *elem1,
		ListElement *elem2, ListElement *elem3);
	TypedList(Allocator &alloc, CSSM_WORDID_TYPE type, ListElement *elem1,
		ListElement *elem2, ListElement *elem3, ListElement *elem4);
	
	bool isProper() const;	// format check (does not throw)
	void checkProper(CSSM_RETURN error = CSSM_ERRCODE_INVALID_SAMPLE_VALUE) const;
	static TypedList &overlay(CSSM_LIST &list)
	{ return static_cast<TypedList &>(list); }
	static const TypedList &overlay(const CSSM_LIST &list)
	{ return static_cast<const TypedList &>(list); }
	
	CSSM_WORDID_TYPE type() const
	{ assert(isProper()); return first()->word(); }
};

inline ListElement::operator TypedList &()
{ return TypedList::overlay(operator CssmList &()); }


//
// Data walkers to parse list elements and lists.
// @@@ Walking lists by recursing over next() is stack intensive. Do this in CssmList walker by loop?
//
namespace DataWalkers {

// ListElement
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
		secinfo("walkers", "invalid list element type (%ux)", (unsigned)elem->type());
		break;
	}
	if (elem->next())		
		walk(operate, elem->next());
	return elem;
}

template <class Action>
ListElement *walk(Action &operate, CSSM_LIST_ELEMENT * &elem)
{ return walk(operate, ListElement::overlayVar(elem)); }

// CssmList
template <class Action>
void enumerate(Action &operate, CssmList &list)
{
	if (!list.empty()) {
		walk(operate, list.first());
		if (operate.needsRelinking)
			list.Tail = list.first()->last();	// re-establish "tail" link
	}
}

template <class Action>
CssmList *walk(Action &operate, CssmList * &list)
{
	operate(list);
	enumerate(operate, *list);
	return list;
}

template <class Action>
void walk(Action &operate, CssmList &list)
{
	operate(list);
	enumerate(operate, list);
}

template <class Action>
void walk(Action &operate, CSSM_LIST &list)
{ walk(operate, CssmList::overlay(list)); }

template <class Action>
CSSM_LIST *walk(Action &operate, CSSM_LIST * &list)
{ return walk(operate, CssmList::overlayVar(list)); }

template <class Action>
TypedList *walk(Action &operate, TypedList * &list)
{ return static_cast<TypedList *>(walk(operate, reinterpret_cast<CssmList * &>(list))); }

template <class Action>
void walk(Action &operate, TypedList &list)
{ walk(operate, static_cast<CssmList &>(list)); }


}	// end namespace DataWalkers
} 	// end namespace Security


#endif //_H_CSSMLIST
