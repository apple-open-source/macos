/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All Rights Reserved.
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
// context - CSSM cryptographic context objects
//
#ifndef _H_CONTEXT
#define _H_CONTEXT

#include <security_utilities/utilities.h>
#include <security_utilities/debugging.h>
#include <security_cdsa_utilities/cssmalloc.h>
#include <security_cdsa_utilities/cssmwalkers.h>
#include <security_cdsa_utilities/cssmacl.h>	// to serialize/copy access credentials
#include <security_cdsa_utilities/cssmdates.h>

namespace Security {


//
// Context is a POD overlay for the CSSM_CONTEXT type. It does
// add allocation functions and lots of good stuff.
// Note that if you're outside CSSM proper, you are not supposed to
// memory-manage Context structures on your own. Be a good boy and
// call the CSSM API functions.
// We also provide a POD overlay for CSSM_CONTEXT_ATTRIBUTE, with
// the obvious semantics.
//
class Context : public PodWrapper<Context, CSSM_CONTEXT> {
public:
    Context(CSSM_CONTEXT_TYPE type, CSSM_ALGORITHMS algorithmId);

    uint32 attributesInUse() const { return NumberOfAttributes; }
    CSSM_CONTEXT_TYPE type() const { return ContextType; }
    CSSM_ALGORITHMS algorithm() const { return AlgorithmType; }
    CSSM_CSP_HANDLE cspHandle() const { return CSPHandle; }

    void deleteAttribute(CSSM_ATTRIBUTE_TYPE type);
	size_t copyAttributes(CSSM_CONTEXT_ATTRIBUTE * &attrs, uint32 &count, Allocator &alloc) const;
	
    void copyFrom(const Context &source, Allocator &alloc)
	{ source.copyAttributes(ContextAttributes, NumberOfAttributes, alloc); }

public:
    class Attr : public PodWrapper<Attr, CSSM_CONTEXT_ATTRIBUTE> {
    public:
		Attr() { }
        Attr(const CSSM_CONTEXT_ATTRIBUTE &attr) { (CSSM_CONTEXT_ATTRIBUTE &)*this = attr; }

        template <class T>
        Attr(CSSM_ATTRIBUTE_TYPE typ, T &value, size_t size = 0)
        {
            AttributeType = typ;
			// attribute component pointers are stupidly non-const; allow const input
            Attribute.String = const_cast<char *>(reinterpret_cast<const char *>(&value));
            AttributeLength = size ? size : sizeof(T);
        }

        Attr(CSSM_ATTRIBUTE_TYPE typ, uint32 value)
        {
            AttributeType = typ;
            Attribute.Uint32 = value;
            AttributeLength = 0;
        }
        
        CSSM_ATTRIBUTE_TYPE type() const { return AttributeType; }
        uint32 baseType() const { return AttributeType & CSSM_ATTRIBUTE_TYPE_MASK; }

        operator char * () const
        { assert(baseType() == CSSM_ATTRIBUTE_DATA_STRING); return Attribute.String; }
        operator CssmData & () const
        { assert(baseType() == CSSM_ATTRIBUTE_DATA_CSSM_DATA);
		  return CssmData::overlay(*Attribute.Data); }
        operator CssmCryptoData & () const
        { assert(baseType() == CSSM_ATTRIBUTE_DATA_CRYPTO_DATA);
		  return CssmCryptoData::overlay(*Attribute.CryptoData); }
        operator CssmKey & () const
        { assert(baseType() == CSSM_ATTRIBUTE_DATA_KEY); return CssmKey::overlay(*Attribute.Key); }
        operator AccessCredentials & () const
        { assert(baseType() == CSSM_ATTRIBUTE_DATA_ACCESS_CREDENTIALS);
		  return AccessCredentials::overlay(*Attribute.AccessCredentials); }
        operator uint32 () const
        { assert(baseType() == CSSM_ATTRIBUTE_DATA_UINT32); return Attribute.Uint32; }
        operator CSSM_DL_DB_HANDLE &() const
        {
			assert(baseType() == CSSM_ATTRIBUTE_DATA_DL_DB_HANDLE);
			if (Attribute.DLDBHandle == NULL)
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_DL_DB_HANDLE);
			return *Attribute.DLDBHandle;
		}
        operator CssmDate & () const
        { assert(baseType() == CSSM_ATTRIBUTE_DATA_DATE);
		  return CssmDate::overlay(*Attribute.Date); }
        // @@@ etc. etc. - add yours today!

        void operator = (uint32 value) { Attribute.Uint32 = value; }
        template <class T>
        void operator = (T *ptr) { Attribute.String = reinterpret_cast<char *>(ptr); }

        IFDUMP(void dump() const;)	// debug dump this Attr to stdout (one line)
    };

    // Attributes by position
	Attr *attributes() const { return Attr::overlay(ContextAttributes); }
    Attr &operator [] (unsigned int ix)
    { assert(ix < NumberOfAttributes); return static_cast<Attr &>(ContextAttributes[ix]); }
    const Attr &operator [] (unsigned int ix) const
    { assert(ix < NumberOfAttributes); return static_cast<Attr &>(ContextAttributes[ix]); }

    // general attribute retrieval by type
    Attr *find(CSSM_ATTRIBUTE_TYPE theType) const
    { return find(theType, ContextAttributes, NumberOfAttributes); }

    template <class Elem>
    Elem &get(CSSM_ATTRIBUTE_TYPE type, CSSM_RETURN err) const
    {
        if (Attr *attr = find(type))
            return static_cast<Elem &>(*attr);
        else
            CssmError::throwMe(err);
    }

    template <class Elem>
    Elem *get(CSSM_ATTRIBUTE_TYPE type) const
    {
        if (Attr *attr = find(type))
            // @@@ Invoking conversion operator to Elem & on *attr and taking address of result.
            return &static_cast<Elem &>(*attr);
        else
            return NULL;
    }

    uint32 getInt(CSSM_ATTRIBUTE_TYPE type, CSSM_RETURN err) const
    {
        if (Attr *attr = find(type))
            return static_cast<uint32>(*attr);
        else
            CssmError::throwMe(err);
    }

    uint32 getInt(CSSM_ATTRIBUTE_TYPE type) const
    {
        if (Attr *attr = find(type))
            return static_cast<uint32>(*attr);
        else
            return 0;
    }
    
    bool getInt(CSSM_ATTRIBUTE_TYPE type, uint32 &value) const
    {
        if (Attr *attr = find(type)) {
            value = static_cast<uint32>(*attr);
            return true;
        } else
            return false;
    }
	
public:
	template <class T>
	void replace(CSSM_ATTRIBUTE_TYPE type, const T &newValue) const
	{
		if (Attr *attr = find(type))
			*attr = Attr(type, newValue);
		else
			CssmError::throwMe(CSSMERR_CSSM_ATTRIBUTE_NOT_IN_CONTEXT);
	}

public:
    void *operator new (size_t size, Allocator &alloc) throw(std::bad_alloc)
    { return alloc.malloc(size); }
    void operator delete (void *addr, size_t, Allocator &alloc) throw()
    { return alloc.free(addr); }
    static void destroy(Context *context, Allocator &alloc) throw()
    { alloc.free(context->ContextAttributes); alloc.free(context); }
	
public:
	// Post-IPC context fixup.
	// This can only be called on a Built Context after IPC transmission.
	void postIPC(void *base, CSSM_CONTEXT_ATTRIBUTE *ipcAttributes);

public:
    class Builder;

	// dump to stdout, multiline format
    IFDUMP(void dump(const char *title = NULL,
        const CSSM_CONTEXT_ATTRIBUTE *attrs = NULL) const;)

protected:
    // find an attribute in a plain array of attribute structures (no context)
    static Attr *find(CSSM_ATTRIBUTE_TYPE theType,
    				  const CSSM_CONTEXT_ATTRIBUTE *attrs, unsigned int count);
};


namespace DataWalkers {


template <class Action>
void walk(Action &operate, CSSM_CONTEXT_ATTRIBUTE &attr)
{
	operate(attr);
	if (attr.Attribute.String)	// non-NULL pointer (imprecise but harmless)
		switch (attr.AttributeType & CSSM_ATTRIBUTE_TYPE_MASK) {
		case CSSM_ATTRIBUTE_DATA_CSSM_DATA:
			walk(operate, attr.Attribute.Data); break;
		case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA:
			walk(operate, attr.Attribute.CryptoData); break;
		case CSSM_ATTRIBUTE_DATA_KEY:
			walk(operate, attr.Attribute.Key); break;
		case CSSM_ATTRIBUTE_DATA_STRING:
			walk(operate, attr.Attribute.String); break;
		case CSSM_ATTRIBUTE_DATA_DATE:
			walk(operate, attr.Attribute.Date); break;
		case CSSM_ATTRIBUTE_DATA_RANGE:
			walk(operate, attr.Attribute.Range); break;
		case CSSM_ATTRIBUTE_DATA_ACCESS_CREDENTIALS:
			walk(operate, attr.Attribute.AccessCredentials); break;
		case CSSM_ATTRIBUTE_DATA_VERSION:
			walk(operate, attr.Attribute.Version); break;
		case CSSM_ATTRIBUTE_DATA_DL_DB_HANDLE:
			walk(operate, attr.Attribute.DLDBHandle); break;
		case CSSM_ATTRIBUTE_NONE:
		case CSSM_ATTRIBUTE_DATA_UINT32:
			break;
		default:
			secdebug("walkers", "invalid attribute (%ux) in context", (unsigned)attr.AttributeType);
			break;
		}
}

template <class Action>
void walk(Action &operate, Context::Attr &attr)
{
	walk(operate, static_cast<CSSM_CONTEXT_ATTRIBUTE &>(attr));
}

} // end namespace DataWalkers


//
// Context::Builder - make context attributes the fun way.
//
// A Context (aka CSSM_CONTEXT) has a pointer to an array of context attributes,
// most of which contain pointers to other stuff with pointers to God Knows Where.
// Instead of allocating this all over the heap, a Context::Builder performs
// a two-pass algorithm that places all that stuff into a single heap node.
// Specifically, the builder will allocate and create a vector of CSSM_CONTEXT_ATTRIBUTE
// structures and all their subordinate heap storage.
// A Builder does not deal in Context objects and does not care what you do with your
// CSSM_CONTEXT_ATTRIBUTE array once it's delivered. Since it's a single heap node,
// you can just free() it using the appropriate allocator when you're done with it.
//
// Theory of operation:
// Builder works in two phases, called scan and build. During scan, you call setup()
// with the desired data to be placed into the attribute vector. When done, call make()
// to switch to build phase. Then call put() with the SAME sequence of values as in phase 1.
// Finally, call done() to receive the pointer-and-count values.
// @@@ Add comment about IPC use.
//
using namespace DataWalkers;

class Context::Builder {
protected:
public:
    Builder(Allocator &alloc) : allocator(alloc)
    { slotCount = 0; attributes = NULL; }
    ~Builder() { allocator.free(attributes); }

    Allocator &allocator;

    // switch to build phase
    size_t make();
    // deliver result
    void done(CSSM_CONTEXT_ATTRIBUTE * &attributes, uint32 &count);

public:
    //
    // Phase 1 (scan) dispatch. Call once for each attribute needed.
	//
    template <class T>
    void setup(T p, CSSM_RETURN invalidError = CSSM_OK)
    {
        if (p) {
            slotCount++;
			walk(sizer, unconst_ref_cast(p));
        } else if (invalidError)
            CssmError::throwMe(invalidError);
    }

	void setup(uint32 n, CSSM_RETURN invalidError = CSSM_OK)
	{
		if (n)
			slotCount++;
		else if (invalidError)
			CssmError::throwMe(invalidError);
	}

	void setup(CSSM_SIZE n, CSSM_RETURN invalidError = CSSM_OK)
	{
		if (n)
			slotCount++;
		else if (invalidError)
			CssmError::throwMe(invalidError);
	}

	// dynamic attribute type
    void setup(const CSSM_CONTEXT_ATTRIBUTE &attr)
	{ slotCount++; walk(sizer, const_cast<CSSM_CONTEXT_ATTRIBUTE &>(attr)); }
	void setup(const Context::Attr &attr) { setup(static_cast<const CSSM_CONTEXT_ATTRIBUTE &>(attr)); }

    //
    // Phase 2 (copy) dispatch. Call once for each attribute, in same order as setup().
    //
    template <class T>
    void put(CSSM_ATTRIBUTE_TYPE type, const T *p)
    {
        if (p) {
            assert(slot < slotCount);			// check overflow
            Attr &attribute = attributes[slot++];
            attribute.AttributeType = type;
            attribute.AttributeLength = size(p); //@@@ needed? how/when/what for?
            T *tmp = const_cast<T *>(p);
            attribute = walk(copier, tmp);
        }
    }
    void put(CSSM_ATTRIBUTE_TYPE type, uint32 value)
    {
        if (value) {
            assert(slot < slotCount);			// check overflow
            Attr &attribute = attributes[slot++];
            attribute.AttributeType = type;
            attribute.AttributeLength = 0;		//@@@ unclear what that should be
            attribute = value;					// no heap data (immediate value)
        }
    }
	void put(const CSSM_CONTEXT_ATTRIBUTE &attr)
	{
		assert(slot < slotCount);
		Attr &attribute = attributes[slot++];
		attribute = attr;	// shallow copy
		walk(copier, attribute);	// deep copy
	}
	void put(const Context::Attr &attr) { put(static_cast<const CSSM_CONTEXT_ATTRIBUTE &>(attr)); }
	
private:
    // pass 1 state: collect sizes and counts
    unsigned slotCount;					// count of attribute slots in use
	SizeWalker sizer;					// memory size calculator

    // pass 2 state: build the data set
    Context::Attr *attributes;			// attribute vector and start of block
	CopyWalker copier;					// data copy engine
    uint32 slot;						// writer slot position
};

} // end namespace Security

#endif //_H_CONTEXT
